#include "engine.hpp"
#include <iostream>
#include <stdexcept>

namespace {
int assertions = 0; // Number of executed checks, reported to CTest.
// Fails the test executable with a useful assertion message.
void expect(bool condition, const std::string &message) {
    ++assertions;
    if (!condition)
        throw std::runtime_error(message);
}
// Ensures malformed input is rejected rather than silently accepted.
template <class Function> void rejects(Function function, const std::string &message) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception &) {
        rejected = true;
    }
    expect(rejected, message);
}
// Creates a complete CTF-IR fixture with explicit output contract.
rowdogg::Json spec(const std::string &category, const std::string &type, rowdogg::Json input,
                   rowdogg::Json parameters = rowdogg::Json::object()) {
    return {{"version", "1.0"}, {"category", category},     {"problem", {{"type", type}}},
            {"input", input},   {"parameters", parameters}, {"output", {{"type", "result"}}}};
}
} // namespace

// Exercises algorithms, independent verification, malformed input, and policy boundaries.
int main() {
    using namespace rowdogg;
    try {
        auto graph =
            spec("graph", "shortest_path",
                 {{"nodes", 5}, {"edges", {{0, 1, 4}, {0, 2, 2}, {2, 3, 3}, {3, 4, 1}, {1, 4, 9}}}},
                 {{"source", 0}, {"target", 4}}); // Known optimum six.
        auto result = solve(graph);               // Claimed result used for tampering checks.
        expect(result["cost"] == 6 && result["path"] == Json({0, 2, 3, 4}),
               "Dijkstra shortest path");
        expect(verify(graph, result)["passed"], "Independent shortest path verification");
        result["cost"] = 5;
        expect(!verify(graph, result)["passed"].get<bool>(), "Reject altered cost");
        result = solve(graph);
        result["path"] = {0, 4};
        expect(!verify(graph, result)["passed"].get<bool>(), "Reject non-existent edge");
        graph["parameters"]["target"] = 0;
        expect(solve(graph)["cost"] == 0, "Identity path");
        graph["input"]["edges"] = Json::array();
        graph["parameters"]["target"] = 4;
        expect(verify(graph, solve(graph))["passed"], "Disconnected graph");
        graph["input"]["edges"] = {{0, 8, 1}};
        rejects([&] { solve(graph); }, "Reject invalid node");
        graph["input"]["edges"] = {{0, 1, -1}};
        rejects([&] { solve(graph); }, "Reject negative Dijkstra edge");
        graph["problem"]["type"] = "bellman_ford";
        graph["input"]["directed"] = true;
        graph["input"]["edges"] = {{0, 1, -1}, {1, 4, 3}};
        expect(solve(graph)["cost"] == 2 && verify(graph, solve(graph))["passed"].get<bool>(),
               "Bellman-Ford negative edges");
        graph["input"]["edges"] = {{0, 1, -1}, {1, 0, -1}};
        rejects([&] { solve(graph); }, "Reject negative cycle");
        auto mst = spec(
            "graph", "mst",
            {{"nodes", 4},
             {"edges", {{0, 1, 1}, {1, 2, 2}, {2, 3, 3}, {0, 3, 9}}}}); // Connected MST fixture.
        expect(solve(mst)["cost"] == 6 && verify(mst, solve(mst))["passed"].get<bool>(),
               "Kruskal versus Prim");
        auto encoding =
            spec("crypto", "base64_decode",
                 {{"text", "Q1RGe2xvY2FsX2ZpcnN0fQ=="}}); // Canonical flag text fixture.
        expect(solve(encoding)["text"] == "CTF{local_first}" &&
                   verify(encoding, solve(encoding))["passed"].get<bool>(),
               "Base64 round trip");
        for (const std::string malformed : {"A", "A===", "!!!!", "Zh==", "AA=A"})
            rejects([&] { base64Decode(malformed); }, "Reject malformed Base64");
        expect(base64Decode("").empty(), "Empty Base64");
        encoding["input"]["text"] = "/w==";
        expect(solve(encoding)["text"].is_null() &&
                   verify(encoding, solve(encoding))["passed"].get<bool>(),
               "Binary output preserved");
        auto network = spec("network", "cidr_subnet",
                            {{"cidr", "192.168.10.42/24"}}); // Subnet boundary fixture.
        expect(solve(network)["network"] == "192.168.10.0" && solve(network)["usable_hosts"] == 254,
               "IPv4 /24");
        network["input"]["cidr"] = "10.0.0.1/31";
        expect(solve(network)["usable_hosts"] == 2, "RFC3021 /31");
        network["input"]["cidr"] = "0.0.0.0/0";
        expect(solve(network)["total_addresses"] == 4294967296ULL, "IPv4 /0 overflow boundary");
        network["input"]["cidr"] = "256.0.0.1/24";
        rejects([&] { solve(network); }, "Reject IPv4 overflow");
        expect(solve(spec("linux", "permission_analyze", {{"mode", "4755"}}))["symbolic"] ==
                   "rwsr-xr-x",
               "SUID permissions");
        auto traversal =
            spec("graph", "bfs",
                 {{"nodes", 4}, {"directed", true}, {"edges", {{0, 1, 1}, {1, 2, 1}, {0, 2, 90}}}},
                 {{"source", 0}, {"target", 2}}); // BFS optimizes hop count, not edge weights.
        expect(solve(traversal)["cost"] == 1 &&
                   verify(traversal, solve(traversal))["passed"].get<bool>(),
               "BFS uses unit edge costs");
        traversal["problem"]["type"] = "dfs";
        expect(solve(traversal)["reachable"] == Json({0, 1, 2}), "DFS reachable closure");
        traversal["problem"]["type"] = "floyd_warshall";
        expect(solve(traversal)["distances"][0][2] == 2 &&
                   solve(traversal)["distances"][0][3].is_null(),
               "All-pairs reachable and unreachable costs");
        traversal["problem"]["type"] = "topological_sort";
        expect(solve(traversal)["order"].size() == 4,
               "Topological order includes isolated vertices");
        traversal["input"]["edges"].push_back({2, 0, 1});
        rejects([&] { solve(traversal); }, "Topological cycle rejected");
        traversal["problem"]["type"] = "scc";
        expect(solve(traversal)["components"].size() == 2,
               "Kosaraju finds separate strongly connected components");
        expect(solve(spec("algorithm", "knapsack", {{"items", {{2, 3}, {3, 4}, {4, 5}, {5, 8}}}},
                          {{"capacity", 5}}))["value"] == 8,
               "0/1 knapsack cannot reuse items");
        for (const std::string type :
             {"hex_encode", "hex_decode", "rot13", "caesar", "xor_decrypt", "base64_encode"}) {
            const auto crypto =
                spec("crypto", type, {{"text", type == "hex_decode" ? "48656C6C6F" : "Hello"}},
                     {{"key", "secret"}, {"shift", -3}}); // Reversible operation coverage.
            expect(verify(crypto, solve(crypto))["passed"], "Inverse verification for " + type);
        }
        encoding["input"]["text"] = "SGVsbG8=";
        result = solve(encoding);
        result["text"] = "forged";
        expect(!verify(encoding, result)["passed"].get<bool>(), "Display must match decoded bytes");
        network["input"]["cidr"] = "1.2.3.4./24";
        rejects([&] { solve(network); }, "Reject trailing IPv4 delimiter");
        expect(solve(spec(
                   "web", "header_analysis",
                   {{"headers", {{"Content-Security-Policy", "default-src 'self'"}}}}))["findings"]
                       .size() == 2,
               "Case-insensitive header inventory");
        expect(solve(spec("crypto", "hash_identify",
                          {{"text", "d41d8cd98f00b204e9800998ecf8427e"}}))["candidates"]
                       .size() == 3,
               "Digest identification explicitly preserves ambiguity");
        PolicyEngine::validate({{"action", "HTTP_GET"}, {"arguments", {{"path", "/challenge"}}}});
        rejects(
            [&] {
                PolicyEngine::validate(
                    {{"action", "SHELL"}, {"arguments", {{"command", "whoami"}}}});
            },
            "Reject shell action");
        for (const std::string path :
             {"http://example.com", "//example.com", "/a\r\nHost:x", "/\\evil"})
            rejects(
                [&] {
                    PolicyEngine::validate(
                        {{"action", "HTTP_GET"}, {"arguments", {{"path", path}}}});
                },
                "Reject network escape");
        rejects(
            [&] {
                PolicyEngine::validate(
                    {{"action", "HTTP_GET"},
                     {"arguments", {{"path", "/"}, {"headers", {{"Host", "example.com"}}}}}});
            },
            "Reject Host override");
        rejects(
            [&] {
                PolicyEngine::validate(
                    {{"action", "HTTP_GET"},
                     {"arguments", {{"path", "/"}, {"url", "http://example.com"}}}});
            },
            "Reject unknown argument");
        rejects(
            [&] {
                PolicyEngine::validate({{"action", "HTTP_GET"},
                                        {"arguments", {{"path", "/"}}},
                                        {"reason", Json::object()}});
            },
            "Reject malformed planner display fields");
        // Deterministic randomized graphs compare the solver with an independent all-pairs oracle.
        for (int seed = 1; seed <= 50; ++seed) {
            Json edges = Json::array(); // Small bounded graphs cover zero weights, parallel routes,
                                        // and disconnected pairs.
            for (int from = 0; from < 7; ++from)
                for (int to = from + 1; to < 7; ++to)
                    if ((seed * 13 + from * 7 + to * 3) % 4)
                        edges.push_back({from, to, (seed + from * 3 + to) % 11});
            const auto fixture = spec("graph", "shortest_path", {{"nodes", 7}, {"edges", edges}},
                                      {{"source", 0}, {"target", 6}});
            expect(verify(fixture, solve(fixture))["passed"], "Random graph independent oracle");
        }
        std::cout << assertions << " assertions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
