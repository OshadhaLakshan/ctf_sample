#include "engine.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <httplib.h>
#include <random>
#include <regex>
#include <set>
#include <stdexcept>

namespace rowdogg {
namespace {
// Generates a collision-resistant ID without incorporating caller-supplied paths.
std::string newId() {
    std::random_device entropy; // Operating-system entropy source where supported.
    std::ostringstream buffer;  // Hex ID, safe as a filename and route component.
    buffer << std::hex << std::setfill('0');
    for (int index = 0; index < 4; ++index)
        buffer << std::setw(8) << entropy(); // Four random words.
    return buffer.str();
}

// Performs one bounded loopback HTTP call; redirects and proxy routing are disabled.
Json executeTool(const Json &action, const Json &previous) {
    PolicyEngine::validate(action);
    const std::string tool = action["action"]; // Validated allowlisted tool identifier.
    Json arguments =
        action["arguments"]; // Copy permits resolving an explicit previous-output reference.
    if (arguments.value("from_previous", false)) {
        if (previous.contains("text") && previous["text"].is_string())
            arguments["text"] = previous["text"];
        else if (previous.contains("body") && previous["body"].is_string())
            arguments["text"] = previous["body"];
        else
            throw std::invalid_argument("Previous evidence has no text output");
    }
    if (tool == "HTTP_GET") {
        const std::string target_origin = PolicyEngine::labOrigin(); // The model cannot replace the operator's lab origin.
        const std::string target_path = arguments.at("path"); // Validated relative path within that lab.
        httplib::Client client(target_origin);
        client.set_connection_timeout(2);
        client.set_read_timeout(5);
        client.set_write_timeout(5);
        client.set_follow_location(false);
        httplib::Headers headers; // Explicit, validated request headers.
        if (arguments.contains("headers"))
            for (auto iterator = arguments["headers"].begin();
                 iterator != arguments["headers"].end(); ++iterator)
                headers.emplace(iterator.key(), iterator.value().get<std::string>());
        std::string body; // Bounded response body captured through a streaming receiver.
        const auto response = client.Get(target_path, headers,
                                         [&](const char *bytes, size_t length) {
                                             if (body.size() + length > 65536)
                                                 return false;
                                             body.append(bytes, length);
                                             return true;
                                         });
        if (!response)
            throw std::runtime_error("Lab request failed, timed out, or exceeded 64 KiB");
        if (response->status >= 300 && response->status < 400)
            throw std::runtime_error("Redirect blocked by target policy");
        Json responseHeaders =
            Json::object(); // Response metadata is evidence, never model instructions.
        for (const auto &[name, value] : response->headers)
            responseHeaders[name] = value;
        return {{"status", response->status},
                {"headers", responseHeaders},
                {"body", body},
                {"source", PolicyEngine::labOrigin() + arguments["path"].get<std::string>()}};
    }
    if (tool == "VERIFY_FLAG") {
        const std::string text =
            arguments.at("text"); // Candidate-bearing text from recorded observations.
        const std::regex pattern(
            R"((?:CTF|FLAG|flag)\{[A-Za-z0-9_!@#$%^&*+.=:?/-]{1,128}\})"); // Fixed bounded pattern
                                                                           // avoids arbitrary regex
                                                                           // execution.
        std::smatch match; // First well-formed candidate.
        if (!std::regex_search(text, match, pattern))
            throw std::invalid_argument("No supported flag format found in evidence");
        return {
            {"flag", match.str()}, {"format_valid", true}, {"challenge_acceptance", "not_checked"}};
    }
    Json spec = {
        {"version", "1.0"},
        {"category", "crypto"},
        {"problem", {{"type", tool == "BASE64_DECODE" ? "base64_decode" : "hex_decode"}}},
        {"input", {{"text", arguments.value("text", "")}}},
        {"parameters", Json::object()},
        {"output", {{"type", "text"}}}}; // Tools reuse the validated deterministic solver registry.
    if (tool == "CIDR_CALCULATE") {
        spec["category"] = "network";
        spec["problem"]["type"] = "cidr_subnet";
        spec["input"] = {{"cidr", arguments.at("cidr")}};
    }
    if (tool == "PERMISSION_ANALYZE") {
        spec["category"] = "linux";
        spec["problem"]["type"] = "permission_analyze";
        spec["input"] = {{"mode", arguments.at("mode")}};
    }
    const auto result =
        solve(spec); // Computed result must pass its verification strategy before observation.
    if (!verify(spec, result)["passed"].get<bool>())
        throw std::runtime_error("Tool output failed verification");
    return result;
}
} // namespace

std::string PolicyEngine::labOrigin() {
    const char *configured = std::getenv(
        "ROWDOGG_LAB_ORIGIN"); // Operator configuration, never an LLM-supplied destination.
    const std::string origin = configured ? configured : "http://127.0.0.1:8090";
    const std::regex allowed(
        R"(^http://127\.0\.0\.1:([0-9]{2,5})$)"); // Literal loopback and explicit port only.
    std::smatch match; // Captured port used to exclude control-plane services.
    if (!std::regex_match(origin, match, allowed))
        throw std::runtime_error("ROWDOGG_LAB_ORIGIN must be http://127.0.0.1:PORT");
    const int port = std::stoi(match[1]); // Numeric service port.
    if (port < 1024 || port > 65535 || port == 8080 || port == 8081 || port == 3000)
        throw std::runtime_error(
            "Lab port must be 1024..65535 and separate from application services");
    return origin;
}

void PolicyEngine::validate(const Json &action) {
    if (!action.is_object() || !action.contains("action") || !action["action"].is_string() ||
        !action.contains("arguments") || !action["arguments"].is_object())
        throw std::invalid_argument("Action requires action:string and arguments:object");
    for (const std::string field :
         {"reason", "expected_observation"}) { // Displayed model fields also have strict contracts.
        if (action.contains(field) &&
            (!action[field].is_string() || action[field].get<std::string>().size() > 4096))
            throw std::invalid_argument(field + " must be text of at most 4096 bytes");
    }
    const std::string tool = action["action"]; // Only this fixed registry may execute.
    const std::set<std::string> tools = {"HTTP_GET",       "BASE64_DECODE",      "HEX_DECODE",
                                         "CIDR_CALCULATE", "PERMISSION_ANALYZE", "VERIFY_FLAG"};
    if (!tools.contains(tool))
        throw std::invalid_argument("Policy rejected tool: " + tool);
    const auto &arguments = action["arguments"]; // Validate each argument before tool dispatch.
    if (arguments.dump().size() > 65536)
        throw std::invalid_argument("Tool arguments exceed 64 KiB");
    const std::set<std::string> allowed =
        tool == "HTTP_GET"             ? std::set<std::string>{"path", "headers"}
        : tool == "CIDR_CALCULATE"     ? std::set<std::string>{"cidr"}
        : tool == "PERMISSION_ANALYZE" ? std::set<std::string>{"mode"}
                                       : std::set<std::string>{"text", "from_previous"};
    for (auto iterator = arguments.begin(); iterator != arguments.end(); ++iterator)
        if (!allowed.contains(iterator.key()))
            throw std::invalid_argument("Unexpected tool argument: " + iterator.key());
    if (tool == "HTTP_GET") {
        if (!arguments.contains("path") || !arguments["path"].is_string())
            throw std::invalid_argument("HTTP_GET requires a relative path");
        const std::string path = arguments["path"]; // Reject absolute URLs, alternate authorities and control characters.
        if (path.empty() || path.size() > 2048 || path.front() != '/' || path.starts_with("//") || path.find('\\') != std::string::npos ||
            std::any_of(path.begin(), path.end(), [](unsigned char character) { return character <= 32 || character == 127; }))
            throw std::invalid_argument("HTTP_GET path must remain within the configured lab");
        if (arguments.contains("headers")) {
            if (!arguments["headers"].is_object() || arguments["headers"].size() > 16)
                throw std::invalid_argument("At most 16 headers allowed");
            for (auto iterator = arguments["headers"].begin();
                 iterator != arguments["headers"].end(); ++iterator) {
                std::string name =
                    iterator.key(); // Lowercase name supports case-insensitive denylisting.
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) {
                    return char(std::tolower(character));
                });
                if (!std::regex_match(name, std::regex("[a-z0-9-]{1,64}")) || name == "host" ||
                    name == "connection" || name == "content-length" ||
                    name == "transfer-encoding" || name.starts_with("proxy-"))
                    throw std::invalid_argument("Policy rejected header name");
                if (!iterator.value().is_string())
                    throw std::invalid_argument("Header values must be text");
                const auto value =
                    iterator.value().get<std::string>(); // Value must not inject another header.
                if (value.size() > 4096 ||
                    std::any_of(value.begin(), value.end(), [](unsigned char character) {
                        return character < 32 || character == 127;
                    }))
                    throw std::invalid_argument("Invalid header value");
            }
        }
    } else if (tool == "CIDR_CALCULATE" || tool == "PERMISSION_ANALYZE") {
        const auto key = tool == "CIDR_CALCULATE" ? "cidr" : "mode"; // Required utility argument.
        if (!arguments.contains(key) || !arguments[key].is_string())
            throw std::invalid_argument("Missing tool text argument");
    } else {
        if (arguments.contains("from_previous") && !arguments["from_previous"].is_boolean())
            throw std::invalid_argument("from_previous must be boolean");
        if (!arguments.value("from_previous", false) &&
            (!arguments.contains("text") || !arguments["text"].is_string()))
            throw std::invalid_argument("Text or from_previous is required");
    }
}

CTFEngine::CTFEngine(std::filesystem::path storage)
    : directory(std::move(storage)), gemma(directory.parent_path() / "model.json") {
    std::filesystem::create_directories(directory);
    for (const auto &entry :
         std::filesystem::directory_iterator(directory)) { // Recover only committed snapshots.
        if (entry.path().extension() != ".json")
            continue;
        try {
            std::ifstream file(entry.path());   // Existing state snapshot.
            auto run = std::make_shared<Run>(); // New in-memory synchronization wrapper.
            file >> run->state;
            file.close(); // Windows cannot atomically replace a snapshot while this read handle is
                          // open.
            const std::string id =
                run->state.at("id"); // Validate filename/ID agreement before indexing.
            if (entry.path().stem().string() != id ||
                !std::regex_match(id, std::regex("[a-f0-9]{32}")))
                continue;
            if (run->state["status"] == "running" || run->state["status"] == "awaiting_approval") {
                run->state["status"] = "stopped";
                run->state["stage"] = "interrupted";
                run->state["error"] =
                    "Execution interrupted by process restart. Submit a new run to continue.";
                save(*run);
            }
            runs[id] = run;
        } catch (const std::exception
                     &) { /* Corrupt snapshots are preserved on disk for operator inspection. */
        }
    }
}

CTFEngine::~CTFEngine() {
    for (const auto &[id, run] : runs) {
        run->cancelled = true;
        run->changed.notify_all();
    } // Signal all workers before joining.
    workers.clear();
}

std::shared_ptr<CTFEngine::Run> CTFEngine::find(const std::string &id) const {
    std::lock_guard lock(mutex);      // Protect index lookup against simultaneous submissions.
    const auto found = runs.find(id); // No insertion on missing IDs.
    if (found == runs.end())
        throw std::out_of_range("Run not found");
    return found->second;
}

void CTFEngine::save(const Run &run) const {
    const auto destination =
        directory / (run.state["id"].get<std::string>() + ".json"); // Validated generated filename.
    const auto temporary = destination.string() + ".tmp"; // Staged file for replace-on-commit.
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << run.state.dump(2);
        file.flush();
        if (!file)
            throw std::runtime_error("Cannot persist run state");
    }
#ifdef _WIN32
    if (!MoveFileExW(std::filesystem::path(temporary).c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot commit run state");
#else
    std::filesystem::rename(temporary, destination);
#endif
}

void CTFEngine::event(const std::shared_ptr<Run> &run, const std::string &stage,
                      const std::string &message, const Json &content) {
    std::lock_guard lock(run->mutex); // Evidence and stage transition commit together.
    run->state["stage"] = stage;
    run->state["evidence"].push_back(
        {{"id", "EV-" + std::to_string(run->state["evidence"].size() + 1)},
         {"timestamp", timestamp()},
         {"source", stage},
         {"message", message},
         {"content", content}});
    run->state["updated_at"] = timestamp();
    save(*run);
}

Json CTFEngine::submit(const Json &request) {
    if (!request.is_object() || request.dump().size() > 131072)
        throw std::invalid_argument("Challenge must be a JSON object below 128 KiB");
    const auto title =
        request.value("title", "Untitled operation"); // Display title has no execution authority.
    const auto question = request.value("question", ""); // Untrusted problem statement.
    const auto mode = request.value("mode", "solver");   // Mode A or Mode B selection.
    const auto approval =
        request.value("approval", "assisted"); // Operator-selected approval policy.
    if (request.contains("max_steps") && !request["max_steps"].is_number_integer())
        throw std::invalid_argument("Step budget must be an integer");
    const int maxSteps = request.value("max_steps", 12); // Hard bounded agent budget.
    if (title.empty() || title.size() > 120 || question.size() > 65536)
        throw std::invalid_argument("Title must be 1..120 characters; question at most 64 KiB");
    if (mode != "solver" && mode != "agent")
        throw std::invalid_argument("Mode must be solver or agent");
    if (approval != "manual" && approval != "assisted" && approval != "autonomous")
        throw std::invalid_argument("Invalid approval mode");
    if (maxSteps < 1 || maxSteps > 32)
        throw std::invalid_argument("Step budget must be 1..32");
    if (request.contains("spec") && !request["spec"].is_null())
        validateSpec(request["spec"]);
    if (request.contains("actions")) {
        if (!request["actions"].is_array() || request["actions"].empty() ||
            request["actions"].size() > size_t(maxSteps))
            throw std::invalid_argument("Actions must fit the step budget");
        for (const auto &action : request["actions"])
            PolicyEngine::validate(action);
    }
    if (question.empty() && !request.contains("spec") && !request.contains("actions"))
        throw std::invalid_argument("Provide a question, CTF-IR, or an action plan");
    auto run = std::make_shared<Run>(); // New persistent challenge with its own synchronization.
    run->state = {{"id", newId()},
                  {"title", title},
                  {"question", question},
                  {"mode", mode},
                  {"approval", approval},
                  {"max_steps", maxSteps},
                  {"steps", 0},
                  {"status", "queued"},
                  {"stage", "ready"},
                  {"created_at", timestamp()},
                  {"updated_at", timestamp()},
                  {"evidence", Json::array()},
                  {"actions", Json::array()},
                  {"request", request},
                  {"result", nullptr},
                  {"verification", nullptr},
                  {"semantic_review", {{"status", "not_run"}}}};
    std::lock_guard lock(mutex); // Insert only after validation and durable save.
    if (runs.size() >= 1000)
        throw std::runtime_error(
            "Run limit reached (1000). Archive stored runs before continuing.");
    save(*run);
    runs[run->state["id"].get<std::string>()] = run;
    return run->state;
}

Json CTFEngine::snapshot(const std::string &id) const {
    const auto run = find(id);        // Stable shared ownership outside index lock.
    std::lock_guard lock(run->mutex); // Snapshot includes a consistent evidence chain.
    return run->state;
}

Json CTFEngine::list() const {
    std::lock_guard lock(mutex);    // Stable run index throughout summary construction.
    Json summaries = Json::array(); // Compact dashboard records exclude raw evidence.
    for (const auto &[id, run] : runs) {
        std::lock_guard runLock(run->mutex);
        Json summary = run->state;
        summary.erase("evidence");
        summary.erase("request");
        summary.erase("actions");
        summary["evidence_count"] = run->state["evidence"].size();
        summaries.push_back(summary);
    }
    std::sort(summaries.begin(), summaries.end(), [](const Json &left, const Json &right) {
        return left["created_at"] > right["created_at"];
    });
    return summaries;
}

Json CTFEngine::start(const std::string &id, bool agent) {
    const auto run = find(id);        // Run must be queued and match the requested endpoint.
    std::lock_guard indexLock(mutex); // Serializes worker creation.
    std::lock_guard lock(run->mutex); // Prevent duplicate starts.
    size_t active = 0;                // Count active workers before admitting more work.
    for (const auto &[otherId, other] : runs) {
        if (other == run)
            continue;
        std::lock_guard otherLock(other->mutex);
        if (other->state["status"] == "running" || other->state["status"] == "awaiting_approval")
            ++active;
    }
    if (active >= 4)
        throw std::runtime_error("Maximum four concurrent runs");
    if (run->state["status"] != "queued")
        throw std::invalid_argument("Run has already been started");
    if (agent != (run->state["mode"] == "agent"))
        throw std::invalid_argument("Endpoint does not match challenge mode");
    run->state["status"] = "running";
    save(*run);
    workers.emplace_back(
        [this, run, agent] { execute(run, agent); }); // Worker retains engine-owned run lifetime.
    return run->state;
}

Json CTFEngine::stop(const std::string &id) {
    const auto run = find(id); // Resolve a specific run, never broadcast cancellation.
    run->cancelled = true;
    run->changed.notify_all();
    std::lock_guard lock(run->mutex); // Terminal results remain intact.
    if (run->state["status"] == "queued") {
        run->state["status"] = "stopped";
        save(*run);
    }
    return {{"id", id}, {"stop_requested", true}};
}

Json CTFEngine::approve(const std::string &id, const std::string &actionId) {
    const auto run = find(id); // Approval is bound to a run and the displayed proposal ID.
    std::lock_guard lock(run->mutex);
    if (run->state["status"] != "awaiting_approval" ||
        run->state["pending_action"]["id"] != actionId || run->approved || run->cancelled)
        throw std::invalid_argument("No matching pending action to approve");
    run->approved = true;
    run->changed.notify_all();
    return {{"approved", actionId}};
}

Json CTFEngine::health() const {
    const auto diagnostic =
        gemma.diagnose(); // Report actionable connection state alongside engine health.
    return {{"status", "online"},
            {"engine", "C++20 / Crow"},
            {"model_available", diagnostic["available"]},
            {"model_endpoint", diagnostic["config"]["base_url"]},
            {"model", diagnostic},
            {"lab_origin", PolicyEngine::labOrigin()},
            {"capabilities", capabilities()},
            {"max_steps", 32}};
}

Json CTFEngine::model() const {
    return gemma.diagnose();
}
Json CTFEngine::configureModel(const Json &value) {
    gemma.configure(value);
    return gemma.diagnose();
}
Json CTFEngine::testModel() const {
    return gemma.test();
}

Json CTFEngine::reverify(const std::string &id) {
    const auto state = snapshot(id); // Reverification works on immutable copied input/result.
    if (state["result"].is_null() || !state.contains("spec"))
        throw std::invalid_argument("Only completed deterministic results can be reverified");
    return verify(state["spec"], state["result"]);
}

void CTFEngine::execute(const std::shared_ptr<Run> &run, bool agent) {
    const auto started = std::chrono::steady_clock::now(); // Monotonic duration measurement.
    Json request; // Immutable original request copied under the run lock.
    {
        std::lock_guard lock(run->mutex);
        request = run->state["request"];
    }
    // Stops processing before another operation or state publication after cancellation.
    auto checkpoint = [&] {
        if (run->cancelled)
            throw std::runtime_error("Execution stopped by operator");
    };
    try {
        checkpoint();
        if (!agent) {
            Json spec; // Structured challenge, either user-supplied or model-translated.
            if (request.contains("spec") && !request["spec"].is_null()) {
                spec = request["spec"];
                event(run, "interpret",
                      "Structured CTF-IR received. Model translation not required.");
            } else {
                event(run, "interpret", "Requesting structured interpretation from local Gemma.");
                Json context = {
                    {"question", request.at("question")},
                    {"capabilities", capabilities()}}; // Original text and supported contracts.
                for (int attempt = 0; attempt < 2;
                     ++attempt) { // One bounded repair for schema/parameter extraction errors.
                    spec = gemma.request("Analyst", context, &run->cancelled);
                    checkpoint();
                    try {
                        validateSpec(spec);
                        break;
                    } catch (const std::exception &error) {
                        event(run, "repair",
                              "Model interpretation did not satisfy CTF-IR: " +
                                  std::string(error.what()),
                              spec);
                        if (attempt == 1)
                            throw NeedsInput(Json::array(
                                {"Gemma could not produce a supported CTF-IR: " +
                                 std::string(error.what()) +
                                 ". Add missing parameters or edit the structured input."}));
                        context["previous_spec"] = spec;
                        context["validation_error"] = error.what();
                    }
                }
            }
            checkpoint();
            validateSpec(spec);
            event(run, "validate", "CTF-IR accepted by the C++ schema validator.", spec);
            {
                std::lock_guard lock(run->mutex);
                run->state["spec"] = spec;
                save(*run);
            }
            checkpoint();
            const auto result = solve(spec); // Deterministic C++ result, independent of the model.
            event(run, "execute", "Deterministic solver completed.", result);
            checkpoint();
            const auto verification =
                verify(spec, result); // Independent strategy or explicitly labeled recomputation.
            event(run, "verify",
                  verification["passed"].get<bool>() ? "All deterministic checks passed."
                                                     : "Verification rejected the result.",
                  verification);
            checkpoint();
            Json review = {
                {"status", "unavailable"},
                {"reason",
                 "Local Gemma is offline. Semantic review was not performed."}}; // Offline state is
                                                                                 // never
                                                                                 // represented as a
                                                                                 // positive review.
            { // Publish the computed answer before potentially slow semantic review.
                std::lock_guard lock(run->mutex);
                run->state["result"] = result;
                run->state["verification"] = verification;
                save(*run);
            }
            if (verification["passed"].get<bool>() && gemma.available()) {
                event(run, "review",
                      "Requesting semantic review after deterministic verification.");
                try {
                    review = gemma.request("Reviewer",
                                           {{"question", request.value("question", "")},
                                            {"spec", spec},
                                            {"result", result},
                                            {"verification", verification}},
                                           &run->cancelled);
                    if (!review.contains("answers_question") ||
                        !review["answers_question"].is_boolean() ||
                        !review.contains("confidence") || !review["confidence"].is_number() ||
                        review["confidence"].get<double>() < 0 ||
                        review["confidence"].get<double>() > 1 || !review.contains("reason") ||
                        !review["reason"].is_string())
                        throw std::runtime_error("Invalid semantic review schema");
                    if (!review.contains("input_matches_question") ||
                        !review["input_matches_question"].is_boolean())
                        throw std::runtime_error("Invalid input cross-check verdict");
                    if (!review.contains("explanation") || !review["explanation"].is_string())
                        throw std::runtime_error("Invalid explanation schema");
                    if (!review.contains("answer_text") || !review["answer_text"].is_string())
                        throw std::runtime_error("Invalid reviewed answer schema");
                    if (result.contains("text") && result["text"].is_string() &&
                        review["answer_text"] != result["text"]) {
                        review["answers_question"] = false;
                        review["reason"] =
                            "Gemma's stated answer differs from the actual C++ output. " +
                            review["reason"].get<std::string>();
                    }
                    review["status"] = "completed";
                    event(run, "review", review["reason"].get<std::string>(), review);
                } catch (const std::exception &error) {
                    review = {{"status", "failed"}, {"reason", error.what()}};
                }
            }
            checkpoint();
            std::lock_guard lock(
                run->mutex); // Atomically publish final result and separate review verdict.
            run->state["result"] = result;
            run->state["verification"] = verification;
            run->state["semantic_review"] = review;
            const bool reviewed =
                review.value("status", "") ==
                "completed"; // Semantic review never overrides deterministic correctness.
            const bool matches = reviewed && review.value("answers_question", false) &&
                                 review.value("input_matches_question", false);
            run->state["status"] = !verification["passed"].get<bool>() ? "rejected"
                                   : matches                           ? "verified"
                                   : reviewed                          ? "needs_review"
                                                                       : "computed";
            run->state["solution"] = {
                {"answer", result},
                {"explanation", matches
                                    ? review.value("explanation", review.value("reason", ""))
                                    : "C++ computed the result shown below. Consult the separate "
                                      "verification and semantic review before submitting it."},
                {"question_crosschecked", matches}};
            run->state["stage"] = "complete";
        } else {
            Json previous =
                Json::object(); // Latest actual tool observation supplied to the planner.
            const int limit =
                request.value("max_steps", 12); // Enforced regardless of LLM instructions.
            const bool scripted = request.contains(
                "actions"); // Explicit plans work without a model; UI labels this mode.
            event(run, "analyze",
                  scripted ? "Executing supplied action plan under C++ policy enforcement."
                           : "Starting local Gemma action planner.");
            bool solved = false; // Set only by a flag grounded in prior observed evidence.
            for (int step = 0; step < limit;
                 ++step) { // Bounded number of planning/execution attempts.
                checkpoint();
                if (scripted && step >= int(request["actions"].size()))
                    break;
                Json action; // One proposal, with no authority until policy and approval pass.
                if (scripted)
                    action = request["actions"][step];
                else {
                    Json state;
                    {
                        std::lock_guard lock(run->mutex);
                        state = run->state;
                    }
                    action = gemma.request("Planner",
                                           {{"question", request.at("question")},
                                            {"state", state},
                                            {"observation", previous},
                                            {"lab_origin", PolicyEngine::labOrigin()}},
                                           &run->cancelled);
                }
                checkpoint();
                PolicyEngine::validate(action);
                action["id"] = "ACT-" + std::to_string(step + 1);
                event(run, "plan", "Action passed tool, target, and parameter policy checks.",
                      action);
                const bool needsApproval =
                    request.value("approval", "assisted") == "manual" ||
                    (request.value("approval", "assisted") == "assisted" &&
                     action["action"] == "HTTP_GET" &&
                     action["arguments"].contains(
                         "headers")); // Custom headers require operator review in Assisted mode.
                if (needsApproval) {
                    std::unique_lock lock(
                        run->mutex); // Wait releases this lock for API approvals and stop requests.
                    run->state["status"] = "awaiting_approval";
                    run->state["pending_action"] = action;
                    run->approved = false;
                    save(*run);
                    run->changed.wait(lock, [&] { return run->approved || run->cancelled; });
                    checkpoint();
                    run->approved = false;
                    run->state["status"] = "running";
                    run->state.erase("pending_action");
                    save(*run);
                }
                checkpoint();
                Json observation; // Actual tool response; fabricated model evidence is never
                                  // appended here.
                try {
                    observation = executeTool(action, previous);
                } catch (const std::exception &error) {
                    event(run, "tool_error", error.what(), action);
                    {
                        std::lock_guard lock(run->mutex);
                        run->state["steps"] = step + 1;
                        run->state["actions"].push_back(
                            {{"proposal", action}, {"error", error.what()}, {"status", "failed"}});
                        save(*run);
                    }
                    if (scripted)
                        throw;
                    previous = {{"error", error.what()}};
                    continue;
                }
                checkpoint();
                // Only observations derived from an actual HTTP response earn lab provenance.
                observation["grounded_in_lab"] =
                    action["action"] == "HTTP_GET" ||
                    (action["arguments"].value("from_previous", false) &&
                     previous.value("grounded_in_lab", false));
                if (action["action"] == "VERIFY_FLAG") {
                    const std::string flag = observation["flag"]; // Candidate must already appear
                                                                  // in a successful observation.
                    std::lock_guard lock(run->mutex);
                    bool grounded =
                        false; // Input/model assertions alone cannot establish flag evidence.
                    for (const auto &evidence : run->state["evidence"])
                        if (evidence["source"] == "observe" &&
                            evidence["content"].value("grounded_in_lab", false) &&
                            evidence["content"].dump().find(flag) != std::string::npos)
                            grounded = true;
                    if (!grounded)
                        throw std::runtime_error("Flag has no supporting tool evidence");
                    solved = true;
                }
                event(run, "observe",
                      action["action"].get<std::string>() + " produced recorded evidence.",
                      observation);
                {
                    std::lock_guard lock(run->mutex);
                    run->state["steps"] = step + 1;
                    run->state["actions"].push_back(
                        {{"proposal", action}, {"status", "completed"}});
                    save(*run);
                }
                previous = observation;
                if (solved)
                    break;
                // Limit loopback lab requests to at most four per second and honor stop promptly.
                for (int slice = 0; slice < 5; ++slice) {
                    checkpoint();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
            checkpoint();
            std::lock_guard lock(run->mutex); // Publish evidence-backed completion separately from
                                              // challenge acceptance.
            run->state["result"] = previous;
            run->state["status"] = solved ? "evidence_verified" : "needs_input";
            run->state["stage"] = solved ? "complete" : "budget_or_plan_exhausted";
            run->state["verification"] = {
                {"passed", solved},
                {"method",
                 "Flag format + observed evidence provenance (not scoreboard acceptance)"},
                {"checks",
                 Json::array({{{"name", "flag_format"}, {"passed", solved}},
                              {{"name", "supporting_tool_evidence"}, {"passed", solved}}})}};
        }
    } catch (const NeedsInput &missing) {
        event(run, "needs_input", "Additional challenge information is required.",
              {{"questions", missing.details}});
        std::lock_guard lock(run->mutex);
        run->state["status"] = run->cancelled ? "stopped" : "needs_input";
        run->state["missing_information"] = missing.details;
    } catch (const std::exception &error) {
        try {
            event(run, run->cancelled ? "stopped" : "failed", error.what());
        } catch (...) { /* Preserve the original execution error when persistence also fails. */
        }
        std::lock_guard lock(run->mutex);
        run->state["status"] = run->cancelled ? "stopped" : "failed";
        run->state["error"] = error.what();
        run->state.erase("pending_action");
    }
    std::lock_guard lock(
        run->mutex); // Persist terminal timing after every success/failure/cancellation path.
    run->state["duration_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
    run->state["updated_at"] = timestamp();
    try {
        save(*run);
    } catch (...) {
        run->state["persistence_error"] = "Final state could not be saved";
    }
}
} // namespace rowdogg
