#include "engine.hpp"
#include <crow.h>
#include <fstream>
#include <iostream>

namespace {
// Converts domain exceptions to consistent JSON API errors.
template <class Function> crow::response respond(Function function) {
    try {
        crow::response response(200, function().dump()); // Successful JSON response.
        response.set_header("Content-Type", "application/json");
        response.set_header("Cache-Control", "no-store");
        return response;
    } catch (const nlohmann::json::exception &error) {
        return crow::response(
            400, rowdogg::Json({{"error", std::string("Invalid JSON contract: ") + error.what()}})
                     .dump());
    } catch (const std::invalid_argument &error) {
        return crow::response(400, rowdogg::Json({{"error", error.what()}}).dump());
    } catch (const std::out_of_range &error) {
        return crow::response(404, rowdogg::Json({{"error", error.what()}}).dump());
    } catch (const std::exception &error) {
        return crow::response(503, rowdogg::Json({{"error", error.what()}}).dump());
    }
}

// Validates browser origins before mutations and WebSocket upgrades.
bool localOrigin(const crow::request &request) {
    const auto origin =
        request.get_header_value("Origin"); // Non-browser local API clients omit Origin.
    return origin.empty() || origin == "http://127.0.0.1:3000" ||
           origin == "http://localhost:3000" || origin == "http://127.0.0.1:8080" ||
           origin == "http://localhost:8080";
}

// Rejects oversized or cross-origin mutations before dispatching engine operations.
rowdogg::Json body(const crow::request &request) {
    if (!localOrigin(request))
        throw std::invalid_argument("Origin rejected");
    if (!request.get_header_value("Content-Type").starts_with("application/json"))
        throw std::invalid_argument("Content-Type must be application/json");
    if (request.body.size() > 131072)
        throw std::invalid_argument("Request exceeds 128 KiB");
    return rowdogg::Json::parse(request.body);
}
} // namespace

// Runs the loopback-only Crow REST/WebSocket service and optional built frontend.
int main() {
    const char *storage = std::getenv(
        "ROWDOGG_DATA_DIR"); // Optional operator-controlled path isolates tests and deployments.
    rowdogg::CTFEngine engine(
        storage ? storage
                : "data/runs"); // Process-wide owner of persistent state and execution workers.
    crow::SimpleApp app;        // Crow HTTP/WebSocket router; no second backend runtime.
    CROW_ROUTE(app, "/api/v1/health")([&] { return respond([&] { return engine.health(); }); });
    CROW_ROUTE(app, "/api/v1/challenges").methods(crow::HTTPMethod::GET)([&] {
        return respond([&] { return engine.list(); });
    });
    CROW_ROUTE(app, "/api/v1/challenges")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] { return engine.submit(body(request)); });
        });
    CROW_ROUTE(app, "/api/v1/solve")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] { return engine.start(body(request).at("id"), false); });
        });
    CROW_ROUTE(app, "/api/v1/agent/start")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] { return engine.start(body(request).at("id"), true); });
        });
    CROW_ROUTE(app, "/api/v1/agent/stop")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] { return engine.stop(body(request).at("id")); });
        });
    CROW_ROUTE(app, "/api/v1/agent/approve")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] {
                const auto payload = body(request);
                return engine.approve(payload.at("id"), payload.at("action_id"));
            });
        });
    CROW_ROUTE(app, "/api/v1/agent/<string>/state")(
        [&](const std::string &id) { return respond([&] { return engine.snapshot(id); }); });
    CROW_ROUTE(app, "/api/v1/agent/<string>/evidence")([&](const std::string &id) {
        return respond([&] { return engine.snapshot(id)["evidence"]; });
    });
    CROW_ROUTE(app, "/api/v1/agent/<string>/result")([&](const std::string &id) {
        return respond([&] {
            const auto state = engine.snapshot(id);
            return rowdogg::Json{{"status", state["status"]},
                                 {"result", state["result"]},
                                 {"verification", state["verification"]},
                                 {"semantic_review", state["semantic_review"]}};
        });
    });
    CROW_ROUTE(app, "/api/v1/verify")
        .methods(crow::HTTPMethod::POST)([&](const crow::request &request) {
            return respond([&] { return engine.reverify(body(request).at("id")); });
        });
    CROW_WEBSOCKET_ROUTE(app, "/api/v1/live")
        .max_payload(256)
        .onaccept([](const crow::request &request, void **) { return localOrigin(request); })
        .onopen([](crow::websocket::connection &connection) {
            connection.send_text("{\"type\":\"connected\"}");
        })
        .onmessage(
            [&](crow::websocket::connection &connection, const std::string &message, bool binary) {
                // Each subscription tick returns one consistent state; clients reconnect using REST
                // fallback.
                if (binary || message.size() > 256) {
                    connection.close("Invalid subscription");
                    return;
                }
                try {
                    const auto payload = rowdogg::Json::parse(message);
                    connection.send_text(engine.snapshot(payload.at("id")).dump());
                } catch (const std::exception &error) {
                    connection.send_text(rowdogg::Json({{"error", error.what()}}).dump());
                }
            })
        .onclose([](crow::websocket::connection &,
                    const std::string
                        &) { /* State remains owned by the engine when a browser disconnects. */ });
    CROW_ROUTE(app, "/")([] {
        crow::response response;
        response.set_static_file_info("frontend/dist/index.html");
        return response;
    });
    CROW_ROUTE(app, "/assets/<string>")([](const std::string &filename) {
        crow::response response;
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos)
            return crow::response(400);
        response.set_static_file_info("frontend/dist/assets/" + filename);
        return response;
    });
    CROW_ROUTE(app, "/favicon.svg")([] {
        crow::response response;
        response.set_static_file_info("frontend/public/favicon.svg");
        return response;
    });
    std::cout << "R0WD0GG API listening on http://127.0.0.1:8080\n";
    app.bindaddr("127.0.0.1").port(8080).concurrency(4).loglevel(crow::LogLevel::Warning).run();
}
