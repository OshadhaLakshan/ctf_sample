#include "solvers.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace rowdogg {
namespace {
// Bounded integer infinity avoids overflow while accumulating graph weights.
constexpr long long INF = 4'000'000'000'000'000LL;
// Canonical alphabet shared by the encoding transformations.
const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Converts a failed precondition into a client-readable validation error.
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::invalid_argument(message);
}

// Reads an integer while preventing JSON floats from being silently truncated.
long long integer(const Json &value, long long minimum, long long maximum,
                  const std::string &name) {
    require(value.is_number_integer(), name + " must be an integer");
    // Checked numeric value used by algorithms after range validation.
    const auto number = value.get<long long>();
    require(number >= minimum && number <= maximum, name + " is out of range");
    return number;
}

// Reads a bounded text input without implicit conversions.
std::string textField(const Json &input, const std::string &key) {
    require(input.contains(key) && input[key].is_string(), key + " must be text");
    // Untrusted source text, bounded to limit work and evidence growth.
    const auto value = input[key].get<std::string>();
    require(value.size() <= 65536, key + " exceeds 64 KiB");
    return value;
}

// Converts strict dotted-decimal IPv4 to a host-order integer.
uint32_t ipv4(const std::string &address) {
    require(std::count(address.begin(), address.end(), '.') == 3 && !address.empty() &&
                address.back() != '.',
            "IPv4 requires exactly four octets");
    // Stream, octet buffer, and packed result used to validate exactly four octets.
    std::istringstream stream(address);
    std::string part;
    uint32_t result = 0;
    for (int index = 0; index < 4; ++index) { // Index identifies the current octet.
        require(bool(std::getline(stream, part, '.')) && !part.empty() && part.size() <= 3,
                "Invalid IPv4 address");
        require(std::all_of(part.begin(), part.end(),
                            [](unsigned char character) { return std::isdigit(character); }),
                "Invalid IPv4 octet");
        require(part.size() == 1 || part[0] != '0', "Ambiguous leading-zero IPv4 octet");
        // Parsed decimal octet must fit one byte.
        const auto octet = std::stoi(part);
        require(octet <= 255, "IPv4 octet exceeds 255");
        result = (result << 8) | uint32_t(octet);
    }
    require(!std::getline(stream, part, '.'), "IPv4 requires four octets");
    return result;
}

// Renders a host-order IPv4 integer as dotted decimal.
std::string address(uint32_t value) {
    return std::to_string(value >> 24) + "." + std::to_string((value >> 16) & 255) + "." +
           std::to_string((value >> 8) & 255) + "." + std::to_string(value & 255);
}

// Produces directed arcs, adding reverse arcs only for undirected graphs.
std::vector<std::tuple<int, int, long long>> arcs(const Json &spec) {
    // Expanded edge list allows all graph algorithms to share direction semantics.
    std::vector<std::tuple<int, int, long long>> result;
    for (const auto &edge :
         spec["input"]["edges"]) { // Each edge has validated endpoints and weight.
        result.emplace_back(edge[0].get<int>(), edge[1].get<int>(), edge[2].get<long long>());
        if (!spec["input"].value("directed", false))
            result.emplace_back(edge[1].get<int>(), edge[0].get<int>(), edge[2].get<long long>());
    }
    return result;
}

// Computes all-pairs shortest paths independently of Dijkstra/Bellman-Ford.
std::vector<std::vector<long long>> floyd(const Json &spec) {
    // Matrix dimensions and edge list are validated before this allocation.
    const int count = spec["input"]["nodes"];
    std::vector distances(count, std::vector<long long>(count, INF));
    for (int node = 0; node < count; ++node)
        distances[node][node] = 0; // Zero-length paths.
    for (const auto &[from, to, weight] : arcs(spec))
        distances[from][to] = std::min(distances[from][to], weight); // Minimum parallel arc.
    for (int via = 0; via < count; ++via)        // Intermediate vertex in the dynamic program.
        for (int from = 0; from < count; ++from) // Origin vertex.
            for (int to = 0; to < count; ++to)   // Destination vertex.
                if (distances[from][via] != INF && distances[via][to] != INF)
                    distances[from][to] =
                        std::min(distances[from][to], distances[from][via] + distances[via][to]);
    for (int node = 0; node < count; ++node)
        require(distances[node][node] >= 0, "Negative cycle detected"); // Reject undefined optima.
    return distances;
}

// Returns a shortest path using a heap, BFS, or Bellman-Ford as requested.
Json shortestPath(const Json &spec) {
    // Algorithm inputs and mutable shortest-distance/predecessor tables.
    const int count = spec["input"]["nodes"], source = spec["parameters"]["source"],
              target = spec["parameters"]["target"];
    const std::string type = spec["problem"]["type"];
    const auto edges = arcs(spec);
    std::vector<long long> distances(count, INF);
    std::vector<int> previous(count, -1);
    distances[source] = 0;
    if (type == "bellman_ford") {
        for (int pass = 0; pass < count;
             ++pass) {            // At most V passes, including negative-cycle detection.
            bool changed = false; // Tracks convergence for early termination.
            for (const auto &[from, to, weight] : edges) { // Current directed arc.
                if (distances[from] != INF && distances[from] + weight < distances[to]) {
                    require(pass < count - 1, "Reachable negative cycle detected");
                    distances[to] = distances[from] + weight;
                    previous[to] = from;
                    changed = true;
                }
            }
            if (!changed)
                break;
        }
    } else if (type == "bfs") {
        // A FIFO frontier yields minimum edge counts in O(V+E), ignoring supplied weights.
        std::vector<std::vector<int>> adjacency(count);
        for (const auto &[from, to, weight] : edges)
            adjacency[from].push_back(to);
        std::queue<int> frontier;
        frontier.push(source);
        while (!frontier.empty()) {
            const int node = frontier.front();
            frontier.pop(); // Next vertex in breadth-first order.
            for (int next : adjacency[node])
                if (distances[next] == INF) {
                    distances[next] = distances[node] + 1;
                    previous[next] = node;
                    frontier.push(next);
                }
        }
    } else {
        // Sparse adjacency lists and a minimum priority queue provide O((V+E) log V) traversal.
        std::vector<std::vector<std::pair<int, long long>>> adjacency(count);
        for (const auto &[from, to, weight] : edges)
            adjacency[from].push_back({to, type == "bfs" ? 1 : weight});
        std::priority_queue<std::pair<long long, int>, std::vector<std::pair<long long, int>>,
                            std::greater<>>
            queue;
        queue.push({0, source});
        while (!queue.empty()) {
            const auto [distance, node] = queue.top();
            queue.pop(); // Cheapest remaining frontier entry.
            if (distance != distances[node])
                continue;
            for (const auto &[next, weight] : adjacency[node]) { // Neighbor and traversal cost.
                if (distance + weight < distances[next]) {
                    distances[next] = distance + weight;
                    previous[next] = node;
                    queue.push({distances[next], next});
                }
            }
        }
    }
    if (distances[target] == INF)
        return {{"path", Json::array()},
                {"cost", nullptr},
                {"reachable", false},
                {"algorithm", type == "shortest_path" ? "dijkstra" : type}};
    // Walk predecessors backwards then restore source-to-target ordering.
    std::vector<int> path;
    for (int node = target; node != -1; node = previous[node])
        path.push_back(node);
    std::reverse(path.begin(), path.end());
    return {{"path", path},
            {"cost", distances[target]},
            {"reachable", true},
            {"algorithm", type == "shortest_path" ? "dijkstra" : type}};
}

// Runs Kruskal's minimum spanning forest with path-compressed union-find.
Json spanningTree(const Json &spec) {
    // Sorted weighted edges and disjoint-set parents.
    const int count = spec["input"]["nodes"];
    auto edges = arcs(spec);
    std::sort(edges.begin(), edges.end(), [](const auto &left, const auto &right) {
        return std::get<2>(left) < std::get<2>(right);
    });
    std::vector<int> parent(count);
    std::iota(parent.begin(), parent.end(), 0);
    // Finds a component representative and compresses the traversal path.
    std::function<int(int)> root = [&](int node) {
        return parent[node] == node ? node : parent[node] = root(parent[node]);
    };
    Json selected = Json::array();                 // Accepted forest edges.
    long long cost = 0;                            // Total accepted weight.
    for (const auto &[from, to, weight] : edges) { // Candidate edge in increasing cost order.
        if (root(from) != root(to)) {
            parent[root(from)] = root(to);
            selected.push_back({from, to, weight});
            cost += weight;
        }
    }
    return {{"edges", selected},
            {"cost", cost},
            {"connected", selected.size() == size_t(count - 1)},
            {"algorithm", "kruskal"}};
}

// Applies reversible byte transformations with explicit input contracts.
std::string transform(const Json &spec) {
    // Type, source text, and parameters form the validated transformation request.
    const std::string type = spec["problem"]["type"], input = spec["input"]["text"];
    const auto &parameters = spec["parameters"];
    std::string result; // Output bytes, later required to be valid JSON UTF-8 text.
    if (type == "base64_decode")
        return base64Decode(input);
    if (type == "base64_encode")
        return base64Encode(input);
    if (type == "hex_decode") {
        require(input.size() % 2 == 0, "Hex input must have even length");
        for (size_t index = 0; index < input.size(); index += 2) { // Offset of each hex byte.
            require(std::isxdigit(static_cast<unsigned char>(input[index])) &&
                        std::isxdigit(static_cast<unsigned char>(input[index + 1])),
                    "Invalid hex digit");
            result += char(std::stoi(input.substr(index, 2), nullptr, 16));
        }
    } else if (type == "hex_encode") {
        constexpr char digits[] = "0123456789abcdef"; // Lowercase canonical hexadecimal alphabet.
        for (unsigned char byte : input) {
            result += digits[byte >> 4];
            result += digits[byte & 15];
        } // Encode each byte.
    } else if (type == "xor_decrypt") {
        const auto key = textField(parameters, "key"); // Repeating nonempty key.
        require(!key.empty(), "XOR key cannot be empty");
        for (size_t index = 0; index < input.size(); ++index)
            result += char(input[index] ^ key[index % key.size()]); // Byte offset.
    } else {
        const int shift =
            type == "rot13"
                ? 13
                : int(integer(parameters.at("shift"), -26, 26, "shift")); // Signed Caesar shift.
        for (unsigned char character :
             input) { // Current ASCII character; other bytes are preserved.
            const int base = character >= 'a' && character <= 'z'   ? 'a'
                             : character >= 'A' && character <= 'Z' ? 'A'
                                                                    : 0;
            result += base ? char(base + (character - base + shift + 26) % 26) : char(character);
        }
    }
    return result;
}
} // namespace

std::string timestamp() {
    // System clock converted with thread-safe platform-specific UTC formatting.
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream stream; // ISO 8601 output buffer.
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::vector<std::string> capabilities() {
    return {
        "shortest_path",   "dijkstra",      "bellman_ford",     "bfs",         "dfs",
        "floyd_warshall",  "mst",           "topological_sort", "scc",         "knapsack",
        "base64_decode",   "base64_encode", "hex_decode",       "hex_encode",  "xor_decrypt",
        "rot13",           "caesar",        "hash_identify",    "cidr_subnet", "permission_analyze",
        "header_analysis", "pipeline"};
}

std::string base64Encode(const std::string &input) {
    std::string output;                // Canonical encoded text including padding.
    unsigned accumulator = 0;          // Sliding bit buffer; unsigned overflow is defined.
    int bits = -6;                     // Number of available bits beyond a complete Base64 symbol.
    for (unsigned char byte : input) { // Next raw byte.
        accumulator = (accumulator << 8) | byte;
        bits += 8;
        while (bits >= 0) {
            output += alphabet[(accumulator >> bits) & 63];
            bits -= 6;
        }
    }
    if (bits > -6)
        output += alphabet[((accumulator << 8) >> (bits + 8)) & 63];
    while (output.size() % 4)
        output += '=';
    return output;
}

std::string base64Decode(const std::string &input) {
    require(input.size() % 4 == 0, "Base64 length must be divisible by four");
    std::string output;                  // Decoded bytes before round-trip canonical validation.
    unsigned accumulator = 0;            // Sliding six-bit buffer.
    int bits = -8;                       // Pending bit count relative to a byte.
    bool padding = false;                // No alphabet characters may follow padding.
    for (const char character : input) { // Current encoded symbol.
        if (character == '=') {
            padding = true;
            continue;
        }
        const auto value =
            alphabet.find(character); // Alphabet index, or npos for malformed symbols.
        require(!padding && value != std::string::npos, "Invalid Base64 symbol or padding");
        accumulator = (accumulator << 6) | unsigned(value);
        bits += 6;
        if (bits >= 0) {
            output += char((accumulator >> bits) & 255);
            bits -= 8;
        }
    }
    require(base64Encode(output) == input, "Non-canonical Base64 padding");
    return output;
}

void validateSpec(const Json &spec) {
    require(spec.is_object() && spec.value("version", "") == "1.0", "CTF-IR version must be 1.0");
    require(spec.contains("input") && spec["input"].is_object() && spec.contains("parameters") &&
                spec["parameters"].is_object(),
            "input and parameters must be objects");
    require(spec.contains("problem") && spec["problem"].is_object() &&
                spec["problem"].contains("type") && spec["problem"]["type"].is_string(),
            "problem.type is required");
    require(spec.contains("output") && spec["output"].is_object() &&
                spec["output"].contains("type") && spec["output"]["type"].is_string(),
            "output.type is required");
    // Explicit category/type mapping prevents misrouted model proposals.
    const std::string type = spec["problem"]["type"], category = spec.value("category", "");
    const auto supported = capabilities();
    require(std::find(supported.begin(), supported.end(), type) != supported.end(),
            "Unsupported problem type: " + type);
    const std::set<std::string> graphTypes = {
        "shortest_path",  "dijkstra", "bellman_ford",     "bfs", "dfs",
        "floyd_warshall", "mst",      "topological_sort", "scc"};
    const auto &input = spec["input"];
    if (type == "pipeline") {
        require(category == "multi_stage", "Pipeline requires category multi_stage");
        require(input.size() == 1 && input.contains("steps"), "Pipeline input must contain only steps; put all operations inside that array");
        require(input.contains("steps") && input["steps"].is_array() && !input["steps"].empty() &&
                    input["steps"].size() <= 8,
                "Pipeline requires 1..8 steps");
        size_t index = 0; // Step index prevents unresolved input on the first operation.
        for (const auto &step : input["steps"]) {
            require(step.is_object() && step.contains("problem") && step["problem"].is_object() &&
                        step["problem"].value("type", "") != "pipeline",
                    "Nested pipelines are not supported");
            validateSpec(step);
            if (step["input"].contains("text") && step["input"]["text"] == "$previous")
                require(index > 0, "First step needs explicit input text");
            ++index;
        }
        return;
    }
    if (graphTypes.contains(type)) {
        require(category == "graph", "Graph problem requires category graph");
        const int count =
            int(integer(input.at("nodes"), 1, 128,
                        "nodes")); // Bounded vertex count keeps verification predictable.
        if (input.contains("labels")) {
            require(input["labels"].is_array() && input["labels"].size() == size_t(count),
                    "labels must match node count");
            std::set<std::string>
                unique; // Preserve original graph names without ambiguous mappings.
            for (const auto &label : input["labels"]) {
                require(label.is_string() && !label.get<std::string>().empty() &&
                            label.get<std::string>().size() <= 64,
                        "Node labels must be 1..64 bytes");
                unique.insert(label.get<std::string>());
            }
            require(unique.size() == size_t(count), "Node labels must be unique");
        }
        require(input.contains("edges") && input["edges"].is_array() &&
                    input["edges"].size() <= 4096,
                "edges must be an array of at most 4096 edges");
        require(!input.contains("directed") || input["directed"].is_boolean(),
                "directed must be boolean");
        for (const auto &edge : input["edges"]) { // Validate each endpoint and signed weight before
                                                  // algorithm dispatch.
            require(edge.is_array() && edge.size() == 3, "Each edge must be [from,to,weight]");
            integer(edge[0], 0, count - 1, "edge source");
            integer(edge[1], 0, count - 1, "edge target");
            integer(edge[2],
                    type == "bellman_ford" || type == "floyd_warshall" || type == "mst" ? -1'000'000
                                                                                        : 0,
                    1'000'000, "weight");
        }
        if (type == "shortest_path" || type == "dijkstra" || type == "bellman_ford" ||
            type == "bfs") {
            integer(spec["parameters"].at("source"), 0, count - 1, "source");
            integer(spec["parameters"].at("target"), 0, count - 1, "target");
        }
        if (type == "dfs")
            integer(spec["parameters"].at("source"), 0, count - 1, "source");
        if (type == "mst")
            require(!input.value("directed", false), "MST requires an undirected graph");
        if (type == "topological_sort")
            require(input.value("directed", false), "Topological sort requires a directed graph");
    } else if (type == "cidr_subnet") {
        require(category == "network", "CIDR requires category network");
        textField(input, "cidr");
    } else if (type == "permission_analyze") {
        require(category == "linux", "Permissions require category linux");
        textField(input, "mode");
    } else if (type == "header_analysis") {
        require(category == "web", "Headers require category web");
        require(input.contains("headers") && input["headers"].is_object() &&
                    input["headers"].size() <= 100,
                "headers must be an object with at most 100 entries");
    } else if (type == "knapsack") {
        require(category == "algorithm", "Knapsack requires category algorithm");
        integer(spec["parameters"].at("capacity"), 0, 10000, "capacity");
        require(input.contains("items") && input["items"].is_array() &&
                    input["items"].size() <= 100,
                "items must have at most 100 entries");
        for (const auto &item : input["items"]) {
            require(item.is_array() && item.size() == 2, "Items are [weight,value]");
            integer(item[0], 1, 10000, "weight");
            integer(item[1], 0, 1000000, "value");
        }
    } else {
        require(category == "crypto", "Encoding problem requires category crypto");
        textField(input, "text");
    }
}

Json solve(const Json &spec) {
    validateSpec(spec);
    // Validated dispatch key and input object shared by the solver branches.
    const std::string type = spec["problem"]["type"];
    const auto &input = spec["input"];
    if (type == "pipeline") {
        Json previous = Json::object(),
             steps = Json::array(); // Actual step outputs and their independent checks.
        for (auto step : input["steps"]) {
            if (step["input"].contains("text") && step["input"]["text"] == "$previous") {
                require(previous.contains("text") && previous["text"].is_string(),
                        "Previous step did not produce UTF-8 text; specify a compatible operation");
                step["input"]["text"] = previous["text"];
            }
            const auto output =
                solve(step); // Each resolved spec passes the same authoritative validation.
            const auto proof = verify(step, output);
            require(proof["passed"].get<bool>(), "Pipeline step verification failed");
            steps.push_back({{"spec", step}, {"result", output}, {"verification", proof}});
            previous = output;
        }
        previous["steps"] = steps;
        previous["algorithm"] = "verified_pipeline";
        return previous;
    }
    if (type == "shortest_path" || type == "dijkstra" || type == "bellman_ford" || type == "bfs")
        return shortestPath(spec);
    if (type == "mst")
        return spanningTree(spec);
    if (type == "floyd_warshall") {
        Json matrix = Json::array(); // Null represents an unreachable pair in JSON.
        for (const auto &row : floyd(spec)) {
            Json values = Json::array();
            for (auto distance : row)
                values.push_back(distance == INF ? Json(nullptr) : Json(distance));
            matrix.push_back(values);
        }
        return {{"distances", matrix}, {"algorithm", type}};
    }
    if (type == "dfs" || type == "scc" || type == "topological_sort") {
        // Adjacency, reverse graph, and color state support traversal and SCC decomposition.
        const int count = input["nodes"];
        std::vector<std::vector<int>> adjacency(count), reverse(count);
        std::vector<int> color(count, 0), order;
        for (const auto &[from, to, weight] : arcs(spec)) {
            adjacency[from].push_back(to);
            reverse[to].push_back(from);
        }
        // Depth-first visit records postorder and rejects cycles for topological sorting.
        std::function<void(int)> visit = [&](int node) {
            color[node] = 1;
            for (int next : adjacency[node]) {
                if (type == "topological_sort")
                    require(color[next] != 1, "Directed cycle: no topological ordering");
                if (!color[next])
                    visit(next);
            }
            color[node] = 2;
            order.push_back(node);
        };
        if (type == "dfs") {
            visit(spec["parameters"]["source"]);
            std::sort(order.begin(), order.end());
            return {{"reachable", order}, {"algorithm", "dfs"}};
        }
        for (int node = 0; node < count; ++node)
            if (!color[node])
                visit(node); // Visit all components.
        std::reverse(order.begin(), order.end());
        if (type == "topological_sort")
            return {{"order", order}, {"algorithm", "dfs_postorder"}};
        std::fill(color.begin(), color.end(), 0);
        Json components = Json::array(); // Strong components in condensation order.
        // Reverse-graph traversal collects one strongly connected component.
        std::function<void(int, std::vector<int> &)> collect = [&](int node,
                                                                   std::vector<int> &component) {
            color[node] = 1;
            component.push_back(node);
            for (int next : reverse[node])
                if (!color[next])
                    collect(next, component);
        };
        for (int node : order)
            if (!color[node]) {
                std::vector<int> component;
                collect(node, component);
                std::sort(component.begin(), component.end());
                components.push_back(component);
            }
        return {{"components", components}, {"algorithm", "kosaraju"}};
    }
    if (type == "knapsack") {
        const int capacity = spec["parameters"]["capacity"]; // Maximum admissible weight.
        std::vector<long long> best(capacity + 1, 0);        // One-dimensional 0/1 dynamic program.
        for (const auto &item : input["items"])
            for (int weight = capacity; weight >= item[0].get<int>(); --weight)
                best[weight] = std::max(best[weight], best[weight - item[0].get<int>()] +
                                                          item[1].get<long long>());
        return {{"value", best[capacity]}, {"algorithm", "0/1 dynamic programming"}};
    }
    if (type == "cidr_subnet") {
        const std::string cidr = input["cidr"]; // Original IPv4/prefix expression.
        const auto slash = cidr.find('/');      // Prefix separator.
        require(slash != std::string::npos, "CIDR must include /prefix");
        const auto suffix = cidr.substr(slash + 1); // Strict decimal prefix text.
        require(!suffix.empty() && suffix.size() <= 2 &&
                    std::all_of(suffix.begin(), suffix.end(),
                                [](unsigned char character) { return std::isdigit(character); }),
                "Invalid CIDR prefix");
        const int prefix = std::stoi(suffix); // Prefix length controls network/host bits.
        require(prefix <= 32, "IPv4 prefix must be 0..32");
        const uint32_t ip = ipv4(cidr.substr(0, slash)),
                       mask = prefix == 0 ? 0 : 0xffffffffU << (32 - prefix), network = ip & mask,
                       broadcast = network | ~mask;
        const uint64_t total = uint64_t(1) << (32 - prefix); // 64-bit storage permits /0.
        return {{"network", address(network)},
                {"broadcast", address(broadcast)},
                {"netmask", address(mask)},
                {"prefix", prefix},
                {"total_addresses", total},
                {"usable_hosts", prefix >= 31 ? total : total - 2},
                {"first_host", address(prefix >= 31 ? network : network + 1)},
                {"last_host", address(prefix >= 31 ? broadcast : broadcast - 1)},
                {"algorithm", "ipv4 bitmask"}};
    }
    if (type == "permission_analyze") {
        const std::string mode = input["mode"]; // Three or four octal permission digits.
        require(mode.size() >= 3 && mode.size() <= 4 &&
                    std::all_of(mode.begin(), mode.end(),
                                [](char digit) { return digit >= '0' && digit <= '7'; }),
                "Mode must be 3 or 4 octal digits");
        const int bits = std::stoi(mode, nullptr, 8); // Packed Unix permission bits.
        std::string symbolic; // Human-readable rwx triplets with special bits.
        for (int shift : {6, 3, 0}) {
            symbolic += bits & (4 << shift) ? 'r' : '-';
            symbolic += bits & (2 << shift) ? 'w' : '-';
            symbolic += bits & (1 << shift) ? 'x' : '-';
        }
        if (bits & 04000)
            symbolic[2] = bits & 0100 ? 's' : 'S';
        if (bits & 02000)
            symbolic[5] = bits & 0010 ? 's' : 'S';
        if (bits & 01000)
            symbolic[8] = bits & 0001 ? 't' : 'T';
        return {{"symbolic", symbolic},
                {"suid", bool(bits & 04000)},
                {"sgid", bool(bits & 02000)},
                {"sticky", bool(bits & 01000)},
                {"world_writable", bool(bits & 0002)},
                {"algorithm", "posix mode analysis"}};
    }
    if (type == "header_analysis") {
        Json normalized = Json::object(),
             findings = Json::array(); // Case-insensitive header map and advisory findings.
        for (auto iterator = input["headers"].begin(); iterator != input["headers"].end();
             ++iterator) {
            std::string key = iterator.key();
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char character) { return char(std::tolower(character)); });
            require(iterator.value().is_string(), "Header values must be text");
            normalized[key] = iterator.value();
        }
        for (const std::string header :
             {"content-security-policy", "x-content-type-options", "strict-transport-security"})
            if (!normalized.contains(header))
                findings.push_back("Missing " + header);
        return {{"headers", normalized},
                {"findings", findings},
                {"note", "Header observations are not proof of exploitability."},
                {"algorithm", "header inventory"}};
    }
    if (type == "hash_identify") {
        const std::string text =
            input["text"]; // Digest candidate; length alone cannot establish its algorithm.
        require(std::all_of(text.begin(), text.end(),
                            [](unsigned char character) { return std::isxdigit(character); }),
                "Hash candidate must contain hex digits");
        Json candidates =
            Json::array(); // Ambiguous candidates, deliberately not asserted identities.
        if (text.size() == 32)
            candidates = {"MD5", "NTLM", "other 128-bit digest"};
        if (text.size() == 40)
            candidates = {"SHA-1", "other 160-bit digest"};
        if (text.size() == 64)
            candidates = {"SHA-256", "other 256-bit digest"};
        return {{"candidates", candidates},
                {"bits", text.size() * 4},
                {"note", "Length-based candidates only; algorithm identity is unverified."},
                {"algorithm", "digest shape analysis"}};
    }
    const auto output =
        transform(spec); // Reversible transformation output may contain arbitrary bytes.
    Json result = {
        {"algorithm", type}, {"bytes_base64", base64Encode(output)}, {"byte_count", output.size()}};
    try {
        Json candidate = output;
        candidate.dump();
        result["text"] = output;
    } catch (const Json::exception &) {
        result["text"] = nullptr;
        result["note"] = "Binary result: inspect bytes_base64.";
    }
    return result;
}

Json verify(const Json &spec, const Json &result) {
    validateSpec(spec);
    Json checks = Json::array(); // Named checks emitted for the verification UI.
    // Adds an independently established check result.
    auto check = [&](const std::string &name, bool passed) {
        checks.push_back({{"name", name}, {"passed", passed}});
    };
    const std::string type = spec["problem"]["type"]; // Selects the verification strategy.
    check("schema_valid", true);
    try {
        if (type == "pipeline") {
            const auto &records = result.at(
                "steps"); // Claimed intermediate results, checked against the original plan.
            require(records.is_array() && records.size() == spec["input"]["steps"].size(),
                    "Pipeline step count mismatch");
            Json previous =
                Json::object(); // Previous verified output used to reconstruct dependent inputs.
            for (size_t index = 0; index < records.size(); ++index) {
                Json resolved = spec["input"]["steps"][index];
                if (resolved["input"].contains("text") && resolved["input"]["text"] == "$previous")
                    resolved["input"]["text"] = previous.at("text");
                check("step_" + std::to_string(index + 1) + "_input",
                      records[index].at("spec") == resolved);
                const auto checked = verify(
                    resolved, records[index].at(
                                  "result")); // Recompute checks rather than trusting saved flags.
                check("step_" + std::to_string(index + 1) + "_verified", checked["passed"]);
                check("step_" + std::to_string(index + 1) + "_check_record",
                      checked == records[index].at("verification"));
                previous = records[index].at("result");
            }
            previous["steps"] = records;
            previous["algorithm"] = "verified_pipeline";
            check("final_output_matches_chain", previous == result);
        } else if (type == "shortest_path" || type == "dijkstra" || type == "bfs" ||
                   type == "bellman_ford") {
            Json oracleSpec = spec; // BFS optimality is measured in unit edges.
            if (type == "bfs")
                for (auto &edge : oracleSpec["input"]["edges"])
                    edge[2] = 1;
            const auto distances = floyd(oracleSpec); // Independent all-pairs optimum.
            const int source = spec["parameters"]["source"], target = spec["parameters"]["target"];
            const auto &path = result.at("path"); // Claimed ordered vertices.
            if (distances[source][target] == INF) {
                check("unreachable_confirmed", result.at("reachable") == false && path.empty() &&
                                                   result.at("cost").is_null());
            } else {
                bool valid = path.is_array() && !path.empty() && path.front() == source &&
                             path.back() == target;
                long long sum = 0; // Independently accumulated path weight.
                for (size_t index = 1; index < path.size(); ++index) { // Path-edge offset.
                    long long weight = INF; // Least parallel-edge weight consistent with the path.
                    for (const auto &[from, to, cost] : arcs(oracleSpec))
                        if (path[index - 1] == from && path[index] == to)
                            weight = std::min(weight, cost);
                    if (weight == INF)
                        valid = false;
                    else
                        sum += weight;
                }
                check("path_exists", valid);
                check("cost_correct", valid && result.at("cost") == sum);
                check("optimal_floyd_warshall", result.at("cost") == distances[source][target]);
                check("reachability_consistent", result.at("reachable") == true);
            }
        } else if (type == "mst") {
            // Prim's independent forest cost checks Kruskal's claimed optimum.
            const int count = spec["input"]["nodes"];
            std::vector<bool> used(count, false);
            long long cost = 0;
            for (int start = 0; start < count; ++start)
                if (!used[start]) {
                    std::priority_queue<std::pair<long long, int>,
                                        std::vector<std::pair<long long, int>>, std::greater<>>
                        queue;
                    queue.push({0, start});
                    while (!queue.empty()) {
                        const auto [weight, node] = queue.top();
                        queue.pop();
                        if (used[node])
                            continue;
                        used[node] = true;
                        cost += weight;
                        for (const auto &[from, to, edgeWeight] : arcs(spec))
                            if (from == node && !used[to])
                                queue.push({edgeWeight, to});
                    }
                }
            Json forest = spec;
            forest["input"]["edges"] =
                result.at("edges"); // Validate claimed edges as an acyclic spanning forest.
            validateSpec(forest);
            bool originals = true;
            long long claimedCost = 0;
            for (const auto &edge : result.at("edges")) {
                bool found = false;
                for (const auto &original : spec["input"]["edges"])
                    if (edge == original || (edge[0] == original[1] && edge[1] == original[0] &&
                                             edge[2] == original[2]))
                        found = true;
                originals &= found;
                claimedCost += edge[2].get<long long>();
            }
            check("edges_exist", originals);
            check("forest_structure",
                  spanningTree(forest)["edges"].size() == result["edges"].size() &&
                      result["edges"].size() == spanningTree(spec)["edges"].size());
            check("cost_correct", result.at("cost") == claimedCost);
            check("optimal_prim", result.at("cost") == cost);
            check("connectivity_correct",
                  result.at("connected") == (result["edges"].size() == size_t(count - 1)));
        } else if (spec["category"] == "crypto" && type != "hash_identify") {
            const std::string source = spec["input"]["text"],
                              output = base64Decode(result.at(
                                  "bytes_base64")); // Raw claimed bytes for inverse checking.
            bool inverse = false; // True only if an inverse operation recovers the original bytes.
            if (type == "base64_decode")
                inverse = base64Encode(output) == source;
            else if (type == "base64_encode")
                inverse = base64Decode(output) == source;
            else {
                Json reverse = spec;
                reverse["input"]["text"] = output;
                if (type == "hex_decode")
                    reverse["problem"]["type"] = "hex_encode";
                if (type == "hex_encode")
                    reverse["problem"]["type"] = "hex_decode";
                if (type == "caesar")
                    reverse["parameters"]["shift"] = -spec["parameters"]["shift"].get<int>();
                std::string expected = source;
                if (type == "hex_decode")
                    std::transform(
                        expected.begin(), expected.end(), expected.begin(),
                        [](unsigned char character) { return char(std::tolower(character)); });
                inverse = transform(reverse) == expected;
            }
            check("inverse_round_trip", inverse);
            check("byte_count_correct", result.at("byte_count") == output.size());
            bool display =
                result.at("text").is_null(); // Null is only valid for non-UTF-8 byte sequences.
            if (display) {
                try {
                    Json(output).dump();
                    display = false;
                } catch (const Json::exception &) {
                }
            } else
                display = result.at("text") == output;
            check("display_matches_bytes", display);
        } else {
            // These utility solvers expose deterministic recomputation, not an independent
            // optimality claim.
            check("deterministic_recomputation", solve(spec) == result);
        }
    } catch (const std::exception &) {
        check("result_structure_valid", false);
    }
    bool passed = true; // Aggregate verdict requires every check to pass.
    for (const auto &entry : checks)
        passed &= entry["passed"].get<bool>();
    return {{"passed", passed},
            {"checks", checks},
            {"method", type == "shortest_path" || type == "dijkstra" || type == "bellman_ford" ||
                               type == "bfs"
                           ? "Independent Floyd-Warshall oracle"
                       : type == "mst"      ? "Independent Prim oracle + structural checks"
                       : type == "pipeline" ? "Per-step verification + chain provenance"
                       : spec["category"] == "crypto" && type != "hash_identify"
                           ? "Inverse transformation"
                           : "Deterministic recomputation"}};
}
} // namespace rowdogg
