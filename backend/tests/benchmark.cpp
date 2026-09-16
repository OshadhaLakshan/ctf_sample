#include "solvers.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

// Evaluates deterministic solving and verification against independently labelled fixtures.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: benchmark_runner data/benchmark.json\n";
        return 2;
    }
    std::ifstream input(argv[1]); // Explicit benchmark dataset path.
    rowdogg::Json dataset;        // Parsed labelled fixture collection.
    input >> dataset;
    int solved = 0, verified = 0,
        tamperRejected = 0; // Separate correctness and false-acceptance counters.
    const auto started = std::chrono::steady_clock::now(); // Monotonic evaluation timer.
    for (const auto &record : dataset["records"]) {        // One labelled test case.
        try {
            const auto result = rowdogg::solve(record["spec"]); // Real deterministic engine output.
            bool correct =
                true; // Ground-truth match across independently specified expected fields.
            for (auto iterator = record["expected"].begin(); iterator != record["expected"].end();
                 ++iterator)
                correct &=
                    result.contains(iterator.key()) && result[iterator.key()] == iterator.value();
            solved += correct;
            verified += rowdogg::verify(record["spec"], result)["passed"].get<bool>();
            auto altered = result; // Tamper with one required expected field.
            altered[record["expected"].begin().key()] = "tampered";
            tamperRejected += !rowdogg::verify(record["spec"], altered)["passed"].get<bool>();
        } catch (const std::exception &error) {
            std::cerr << record["id"] << ": " << error.what() << '\n';
        }
    }
    const int count = int(dataset["records"].size()); // Actual dataset denominator.
    const rowdogg::Json report = {
        {"fixtures", count},
        {"correct", solved},
        {"verification_passed", verified},
        {"tampered_results_rejected", tamperRejected},
        {"elapsed_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count()},
        {"scope", "Synthetic deterministic regression; no LLM accuracy claims"}};
    std::cout << report.dump(2) << '\n';
    return solved == count && verified == count && tamperRejected == count ? 0 : 1;
}
