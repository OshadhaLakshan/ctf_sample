#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace rowdogg {
// Shared JSON value type at the CTF-IR and API boundaries.
using Json = nlohmann::json;

// Rejects malformed, unsupported, oversized, or inconsistent CTF-IR.
void validateSpec(const Json &spec);
// Computes a deterministic answer using the registered problem type.
Json solve(const Json &spec);
// Independently verifies the answer; never accepts a model's verdict as proof.
Json verify(const Json &spec, const Json &result);
// Lists problem identifiers exposed by this build.
std::vector<std::string> capabilities();
// Encodes bytes to canonical RFC 4648 Base64 for inverse verification.
std::string base64Encode(const std::string &input);
// Strictly decodes canonical Base64 and rejects malformed padding.
std::string base64Decode(const std::string &input);
// Formats a UTC timestamp for immutable audit records.
std::string timestamp();
} // namespace rowdogg
