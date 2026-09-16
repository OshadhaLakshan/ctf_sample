#pragma once
#include "solvers.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace rowdogg {
// HTTP adapter for the local llama.cpp OpenAI-compatible endpoint.
class GemmaClient {
  public:
    // Reports live server health with a short timeout.
    bool available() const;
    // Requests one structured JSON response from the specified model role.
    Json request(const std::string &role, const Json &context) const;
};

// Validates tool proposals independently from model instructions or challenge text.
class PolicyEngine {
  public:
    // Restricts every network action to the configured literal-loopback lab origin.
    static void validate(const Json &action);
    // Returns the single operator-configured authorized lab origin.
    static std::string labOrigin();
};

// Owns persistent runs and workers; all mutable run fields are guarded by their mutex.
class CTFEngine {
    struct Run {
        Json state;       // API-visible state and evidence snapshot.
        std::mutex mutex; // Serializes snapshots, state transitions, and disk persistence.
        std::condition_variable changed;    // Wakes a paused worker on approval or stop.
        std::atomic<bool> cancelled{false}; // Stop signal observed between bounded operations.
        bool approved = false;              // One-shot approval consumed by exactly one action.
    };
    std::filesystem::path directory; // Local persistent run directory.
    mutable std::mutex mutex;        // Protects the run index and worker collection.
    std::map<std::string, std::shared_ptr<Run>> runs; // Stable ID-to-run lookup.
    std::vector<std::jthread> workers; // Joined on destruction to avoid detached lifetime bugs.
    GemmaClient gemma;                 // Local inference service adapter.
    // Finds a run or rejects unknown IDs without creating state.
    std::shared_ptr<Run> find(const std::string &id) const;
    // Saves a locked run using replace-on-commit persistence.
    void save(const Run &run) const;
    // Appends a timestamped event and updates the current stage atomically.
    void event(const std::shared_ptr<Run> &run, const std::string &stage,
               const std::string &message, const Json &content = Json::object());
    // Processes Mode A or a bounded Mode B loop in a dedicated worker.
    void execute(const std::shared_ptr<Run> &run, bool agent);

  public:
    // Loads prior runs and marks interrupted executions stopped after restart.
    explicit CTFEngine(std::filesystem::path storage = "data/runs");
    // Stops all workers before member destruction.
    ~CTFEngine();
    // Creates and persists a challenge without executing it.
    Json submit(const Json &request);
    // Starts one pending run; duplicate starts are rejected.
    Json start(const std::string &id, bool agent);
    // Requests cancellation and wakes a manual-approval wait.
    Json stop(const std::string &id);
    // Approves exactly the currently pending action ID.
    Json approve(const std::string &id, const std::string &actionId);
    // Returns a consistent copy suitable for REST and WebSocket delivery.
    Json snapshot(const std::string &id) const;
    // Returns summaries ordered by most recently created first.
    Json list() const;
    // Rechecks stored results without changing original evidence.
    Json reverify(const std::string &id);
    // Returns actual service capabilities and model availability.
    Json health() const;
};
} // namespace rowdogg
