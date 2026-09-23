# R0WD0GG Developer Manual

Ground-up implementation guide and API reference

Source snapshot 22 September 2026 · Application version 0.1.0

## 1 How to use this manual

Read chapters 2–5 to build and run the application. Chapters 6–12 explain its contracts, algorithms, execution, model integration and interface. Chapters 13–16 cover testing, maintenance and extension. The companion Source Explorer contains the complete maintained source, original line numbers, existing developer comments and explanatory reading notes beside every line. Its file summaries explain each module’s role. Use the search box to find a symbol, route, property or phrase, then follow the line anchors while reading this manual.

The snapshot manifest records file sizes, physical line counts and SHA-256 hashes. Source code in the explorer is a frozen reference; later edits to the repository will not update it automatically. The companion distinguishes syntax-level reading notes from the original developer comments. Read complete functions and tests when changing behavior; an individual line is often only one part of a larger expression.

Scope is the implemented application, not every feature in the original project proposal. Third-party dependencies, compiled output, model weights, private run history, generated datasets and package-lock dependency trees are not authored application logic. The coverage report identifies included files and exclusions. Historical root-level editing scripts are not part of the supported build.

## 2 What the application does

R0WD0GG is a local educational challenge workbench. A React interface submits either a structured problem, a pasted question, or a bounded plan for the included local fixture. A C++ service validates inputs, runs deterministic algorithms, records evidence and verifies results. Gemma translates natural language into a typed intermediate representation and separately reviews whether the computed answer addresses the original question.

The model does not replace the solver. If the question is interpreted incorrectly, a mathematically correct result can still answer the wrong question. The application therefore separates computational checks from semantic review. A result marked computed has passed the available computation path without a successful positive semantic review; verified requires both kinds of acceptance. Evidence-verified agent results have a different meaning: their flag is grounded in observations from the local fixture, not accepted by an external scoreboard.

| Component | Responsibility | Source |
| --- | --- | --- |
| React and TypeScript | Input, navigation, live state, result display, approvals and local export | frontend/src |
| Crow transport | REST routing, JSON errors, WebSocket snapshots and static assets | backend/src/main.cpp |
| Execution engine | Run lifecycle, policy, evidence, cancellation and persistence | backend/src/engine.cpp |
| Deterministic core | Validation, algorithm dispatch and verification | backend/src/solvers.cpp |
| Gemma adapter | Settings, diagnostics, schemas, inference and response validation | backend/src/gemma.cpp |
| llama.cpp | Loads GGUF weights and exposes local inference | External executable |
| Python fixture | Reproducible loopback challenge observations | scripts/lab_fixture.py |

Mode A flows from question or CTF-IR to validation, computation, computational verification, optional semantic review, and final state. Mode B flows from a proposal to policy validation, any required approval, a bounded tool, an observation, and the next proposal. Every loop has a finite action budget. Neither path provides an arbitrary shell execution tool.

## 3 Repository and dependency map

CMake builds a shared logical core named rowdogg_core from solvers.cpp, engine.cpp and gemma.cpp. The rowdogg executable adds main.cpp and Crow routing. engine_tests and benchmark_runner link the same core. This keeps computation independently testable without a browser or HTTP server.

The backend uses C++20, CMake 3.20 or later and native threads. CMake fetches Crow 1.2.1, standalone Asio 1.30.2, nlohmann/json 3.11.3 and cpp-httplib 0.18.1. Download hashes are pinned in CMakeLists.txt. Crow handles incoming traffic; cpp-httplib handles outgoing loopback calls. nlohmann/json is the common value type, aliased as Json. Asio supplies networking infrastructure for Crow.

On Windows, CMake links ws2_32, mswsock and crypt32 and sets _WIN32_WINNT to Windows 10. ASIO_STANDALONE avoids a Boost dependency. CROW_ENABLE_DEBUG is disabled for the application target. The current workstation uses MinGW GCC 14.2. Linux source support exists, but this workstation’s validation does not establish Linux compatibility.

Frontend package.json declares React 19, React DOM 19, TypeScript 5.8, Vite 6, lucide-react icons, Prettier and Playwright. npm ci installs the exact dependency tree in frontend/package-lock.json. npm run build first type-checks with tsc -b, then generates frontend/dist with Vite. The root package.json is a separate local browser-test dependency setup; it is not the React application manifest.

| Directory | Meaning |
| --- | --- |
| backend/include | Public C++ interfaces and shared run/model types |
| backend/src | Runtime implementation |
| backend/tests | Native solver, policy and benchmark tests |
| frontend/src | React components, types, examples and CSS |
| frontend/public | SVG favicon and pointer assets |
| schemas | Published JSON envelope contracts |
| scripts | Supported launchers, fixture and test drivers |
| data/runs | Local persisted run snapshots, created at runtime |
| data/model.json | Non-secret model connection settings |
| data/benchmark.json | Generated synthetic regression fixtures |
| models | Separately obtained GGUF weights |
| build | Native compilation output and downloaded CMake dependencies |
| tmp | Logs, tracked service records and test reports |

## 4 Build and launch from a clean checkout

Install a C++20 toolchain, CMake 3.20+, Node 20.19+ or a compatible recent release, and Python 3.10+ for the fixture and tests. Ensure cmake, the selected compiler, node, npm and python are available in the terminal. Keep the compiler and CMake generator consistent; a directory configured with one generator should not be reused with a different generator.

Run these commands from the project root on the current Windows toolchain. The initial CMake configure and npm install require network access. Compilation and frontend packaging do not download the model.

```powershell
Set-Location D:\ctf_sample
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
Set-Location frontend
npm ci
npm run build
Set-Location ..
ctest --test-dir build --output-on-failure
```

Successful output includes build/rowdogg.exe, build/engine_tests.exe, build/benchmark_runner.exe and frontend/dist/index.html. Start from the repository root because relative static and data paths are resolved against the process working directory.

```powershell
./scripts/start.ps1 -WithLab -Port 8085
# Open http://127.0.0.1:8085/
```

Port 8085 avoids the workstation’s Apache service on 8080. The application’s default remains 8080. -WithLab starts the optional fixture on 8090. Production serves the compiled React files directly from Crow and needs no running Node server.

For live frontend development, first stop tracked application services, then start development mode:

```powershell
./scripts/stop.ps1
./scripts/start.ps1 -WithLab -Dev -Port 8085
# Open http://127.0.0.1:3000/
```

Vite forwards API and WebSocket traffic to the configured Crow port. Editing React or CSS updates the development page. Rebuilding frontend/dist is necessary before those changes appear in the production page. C++ changes require a rebuild and service restart. Do not replace a Windows executable while its process is running.

The start script checks that required files exist and selected ports are free, starts hidden processes, writes logs and records process identities in tmp/services.json. The stop script uses tracked identities to avoid stopping unrelated services. A printed start message confirms process creation; use the health endpoint to establish application readiness.

```powershell
Invoke-RestMethod http://127.0.0.1:8085/api/v1/health
./scripts/stop.ps1
./scripts/stop-llama.ps1
```

On Linux, configure without the MinGW generator and launch ./build/rowdogg from the repository root; run python3 scripts/lab_fixture.py separately if needed. The supplied PowerShell service launchers are Windows-oriented. Do not assume the untested platform has passed the Windows test suite.

## 5 Set up llama.cpp and Gemma

The model service is separate from Crow. Installing llama.cpp supplies an inference executable, not the GGUF weights. The current project uses models/gemma-4-E2B-it-Q4_0.gguf from ggml-org/gemma-4-E2B-it-GGUF, revision b4243c1. The recorded file size is 2,841,481,184 bytes and SHA-256 is 8e30dff3ac4c8434c49a7036fa15564bdbb6044e42bf04550bf1a096ad7e6a52. These are snapshot provenance values, not a recommendation to silently substitute a newer model.

Download the matching GGUF from the publisher’s repository, review its license, place it under models, and compare its checksum. Model weights are not included in Git or the documentation bundle. Hardware memory requirements exceed the weight file size because the runtime also needs working memory and a context cache; CPU performance varies by machine.

```powershell
Get-FileHash ./models/gemma-4-E2B-it-Q4_0.gguf -Algorithm SHA256
./scripts/start-llama.ps1
```

The launcher checks PATH and the existing winget package directory for llama-server.exe. If automatic discovery fails, supply explicit absolute paths:

```powershell
./scripts/start-llama.ps1 -ServerPath 'C:\path\llama-server.exe' -ModelPath 'D:\models\model.gguf'
```

The launcher binds 127.0.0.1:8081, sets an 8192-token context, one parallel slot and --jinja, and records the process under tmp/llama-process.json. Inspect tmp/llama-error.log while weights load. In the application, open System, use http://127.0.0.1:8081 as the base URL, leave model ID empty for discovery, save and run Test inference. A ready health check alone does not prove schema-constrained generation works.

The adapter accepts only http://localhost:PORT or http://127.0.0.1:PORT, normalizes localhost to the literal address, strips a trailing /v1, and rejects unexpected URL structure. Model ID is an exact server identifier, not a friendly model-family name. Defaults are 180 seconds and 4096 output tokens; allowed ranges are 10–600 seconds and 256–16384 tokens. A larger output budget does not enlarge llama.cpp’s context window.

| Setting | Source and behavior |
| --- | --- |
| ROWDOGG_PORT | Crow port; default 8080; launch script sets the selected value |
| ROWDOGG_DATA_DIR | Run directory; default data/runs; model settings live in its parent directory |
| ROWDOGG_LLAMA_URL | Startup override for saved inference base URL |
| ROWDOGG_LLAMA_MODEL | Startup override for saved model ID |
| ROWDOGG_LLAMA_API_KEY | Optional bearer secret read from the environment; not returned to the browser |
| ROWDOGG_LAB_ORIGIN | Fixed literal-loopback fixture origin; default http://127.0.0.1:8090 |
| ROWDOGG_FIXTURE_PORT | Port for the Python fixture; default 8090 |
| ROWDOGG_URL | Base URL used by relevant test clients; set to 8085 on this workstation |

Inference uses GET /health, GET /v1/models and POST /v1/chat/completions on llama.cpp. These are model-server endpoints, not Crow routes. Requests use temperature 0, non-streaming responses, role-specific JSON schemas and disabled thinking. Changing model/runtime versions requires rerunning real model acceptance tests.

Publisher and runtime references: https://huggingface.co/ggml-org/gemma-4-E2B-it-GGUF/tree/b4243c1 and https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md. Consult the repository’s pinned versions when comparing documentation.

## 6 CTF intermediate representation

CTF-IR separates interpretation from execution. All structured solver inputs use this envelope:

```json
{
  "version": "1.0",
  "category": "crypto",
  "problem": {"type": "base64_decode"},
  "input": {"text": "SGVsbG8="},
  "parameters": {},
  "output": {"type": "result"}
}
```

version identifies the contract. category groups related solvers. problem.type chooses a fixed implementation. input contains supplied challenge data. parameters contains controls such as source, target or capacity. output declares the output envelope; it is not executable formatting code. The published schema describes the outer wire format, while validateSpec performs authoritative, problem-specific checks. Passing a JSON Schema validator alone is insufficient.

| Problem type | Category | Required input and parameters |
| --- | --- | --- |
| shortest_path, dijkstra | graph | nodes, edges; source and target; nonnegative weights |
| bellman_ford | graph | nodes, edges; source and target; negative weights permitted |
| bfs | graph | nodes, edges; source and target; traversal treats each edge as one hop |
| dfs | graph | nodes, edges; source; returns reachable nodes |
| floyd_warshall | graph | nodes and edges; returns all-pairs distances |
| mst | graph | nodes and edges; undirected graph |
| topological_sort | graph | nodes, edges and directed true; requires acyclic input |
| scc | graph | nodes and edges; components reflect directedness |
| knapsack | algorithm | items as [weight,value]; capacity parameter |
| base64_decode, base64_encode | crypto | text |
| hex_decode, hex_encode | crypto | text |
| xor_decrypt | crypto | text; nonempty raw text key parameter |
| rot13 | crypto | text |
| caesar | crypto | text; shift parameter from -26 to 26 |
| hash_identify | crypto | hexadecimal text; identifies possible digest shapes |
| cidr_subnet | network | cidr string |
| permission_analyze | linux | mode string containing three or four octal digits |
| header_analysis | web | headers object of observed string values |
| pipeline | multi_stage | steps array of complete, non-nested CTF-IR objects |

Graphs have 1–128 nodes and at most 4096 edges. Each edge is [from,to,weight] with zero-based integer endpoints. directed defaults to false. Optional labels must uniquely name every node and are preserved for presentation. Most solvers require weights between 0 and 1,000,000; Bellman–Ford, Floyd–Warshall and MST also accept negative weights down to -1,000,000. Source and target must index existing nodes. Labels do not replace numeric IDs in the computation.

Knapsack accepts at most 100 items, weights from 1 to 10,000, values from 0 to 1,000,000 and capacity from 0 to 10,000. Text inputs are bounded to 65,536 bytes. Browser character counts are not identical to UTF-8 byte counts, so a large non-ASCII paste can reach backend limits earlier.

Strict Base64 requires canonical alphabet and padding and does not silently strip whitespace or accept URL-safe variants. Hex requires valid paired digits. XOR treats JSON string content and its repeated key as bytes; it does not automatically decode a hex ciphertext. Caesar shifts Latin letters and preserves other characters; a negative shift can express decryption. hash_identify does not calculate or crack a digest.

Binary results retain bytes_base64 and byte_count. text is null when decoded bytes are not valid UTF-8. Consumers must not replace that null with guessed or lossy plaintext. CIDR accepts strict IPv4 notation and prefix 0–32, including /0, /31 and /32. /31 and /32 treat all represented addresses as usable. Permission analysis preserves SUID, SGID and sticky-bit distinctions. Header analysis evaluates only the supplied header object; it does not fetch a website.

Pipeline runs up to eight steps. The first step uses explicit data. Later text operations can use "$previous" as input.text to consume the previous verified text result. A binary result cannot feed this text placeholder. Nested pipelines are rejected. The backend accepts a one-step legacy pipeline, while newly generated model pipelines require at least two steps. Every intermediate spec, result and verification record is retained.

## 7 REST and WebSocket API

Use the application origin, currently http://127.0.0.1:8085. All routes below start with /api/v1. POST requests require Content-Type: application/json and a JSON body, including {} where no input fields are needed. Mutation bodies are bounded to 128 KiB. Browser Origin, when present, must match an allowed local application or development origin. Native clients may omit Origin. This is not an authentication system.

Successful route handlers return HTTP 200 with JSON. Invalid JSON or arguments normally return 400, a missing run returns 404, and other caught exceptions return 503 with an error field. Some error responses do not explicitly set JSON Content-Type. A successful state response may include a run.error field describing an execution failure; HTTP success only means the snapshot was retrieved.

| Method and path | Request | Response and side effect |
| --- | --- | --- |
| GET /health | None | Engine status, model diagnostics, capabilities, local lab origin and limits |
| GET /model | None | Public configuration, readiness state, discovered model IDs and selected model |
| POST /model | Model settings object | Validates and persists settings; returns diagnostics |
| POST /model/test | {} | Performs actual diagnostic generation; passed, message and elapsed_ms |
| GET /challenges | None | Run summaries ordered newest first; omits evidence, original request and actions |
| POST /challenges | Challenge request | Persists a new queued run and returns its complete state |
| POST /solve | {"id":"run-id"} | Starts a queued solver run; does not wait for its final answer |
| POST /agent/start | {"id":"run-id"} | Starts a queued agent run |
| POST /agent/stop | {"id":"run-id"} | Requests cooperative stop; returns id and stop_requested |
| POST /agent/approve | {"id":"run-id","action_id":"ACT-1"} | Approves the exact waiting proposal once |
| GET /agent/{id}/state | Path ID | Complete current state, including evidence and original request |
| GET /agent/{id}/evidence | Path ID | Evidence array only |
| GET /agent/{id}/result | Path ID | status, result, verification and semantic_review |
| POST /verify | {"id":"run-id"} | Recomputes checks for a stored structured result; does not rewrite the run |
| WS /live | Text {"id":"run-id"} | One consistent run snapshot per client message |

The result-only route does not include the solution explanation wrapper; use full state for solution and missing_information. There are no server-side delete, login, arbitrary upload or report-download routes. File import happens in the browser; JSON export uses a browser Blob.

Challenge requests contain title, mode, approval, max_steps and at least one usable question, spec or explicit actions plan. mode is solver or agent. approval is manual, assisted or autonomous. max_steps defaults to 12 and must be 1–32. title defaults to Untitled operation and must be 1–120 bytes; question is limited to 65,536 bytes. A supplied spec is validated before persistence. Explicit actions must be nonempty and fit the action budget. Use combinations appropriate to the selected mode rather than relying on unrelated fields being ignored.

```json
{
  "title": "First decoding run",
  "mode": "solver",
  "approval": "assisted",
  "max_steps": 12,
  "spec": {
    "version": "1.0", "category": "crypto",
    "problem": {"type": "base64_decode"},
    "input": {"text": "SGVsbG8="},
    "parameters": {}, "output": {"type": "result"}
  }
}
```

This PowerShell example creates, starts and fetches that run. Submission and execution are deliberately separate so a queued record exists before work starts.

```powershell
$base = 'http://127.0.0.1:8085/api/v1'
$spec = @{
  version='1.0'; category='crypto'
  problem=@{type='base64_decode'}
  input=@{text='SGVsbG8='}; parameters=@{}
  output=@{type='result'}
}
$request = @{title='First decoding run'; mode='solver'; spec=$spec}
$run = Invoke-RestMethod "$base/challenges" -Method Post -ContentType 'application/json' -Body ($request | ConvertTo-Json -Depth 12)
$idBody = @{id=$run.id} | ConvertTo-Json
Invoke-RestMethod "$base/solve" -Method Post -ContentType 'application/json' -Body $idBody
Invoke-RestMethod "$base/agent/$($run.id)/state"
```

The first state read may still show running. Poll while the run is active or request snapshots over WebSocket. For natural language, replace spec with question: "Decode SGVsbG8= from Base64 and return the plaintext." Keep mode solver. This path requires a working model; the structured example can compute without it.

WebSocket sends {"type":"connected"} on open. The browser subsequently sends the selected ID on each subscription tick; the server responds with a snapshot. It is not an unsolicited broadcast stream. Binary messages and messages above 256 bytes are closed; malformed JSON or an invalid ID produces a JSON error. Closing a browser connection does not stop the run. Reconnect or use REST to recover its state.

Static routes are /, /assets/{single-filename}, /favicon.svg, /cursor.svg and /cursor-target.svg. The asset route rejects traversal markers and path separators. Arbitrary client-side deep URLs are not a supported server route; navigation is held in React state.

## 8 State persistence and concurrency

CTFEngine owns an index of shared Run objects. Each Run holds JSON state, a mutex, a condition variable, an atomic cancellation flag and approval state. The index has its own mutex. A worker receives shared ownership so a browser disconnect cannot destroy its execution state. Native jthread workers are owned by the engine and joined at shutdown.

A submitted run gets a random 32-character lowercase hexadecimal ID, creation/update timestamps, queued status, its original request, empty evidence and empty result/check fields. The engine saves it before adding it to the in-memory index. At most 1000 runs are admitted and at most four runs execute concurrently. The current implementation retains completed worker objects until engine destruction; it is a bounded local application, not a production distributed job queue.

| Status | Meaning for the user |
| --- | --- |
| queued | Persisted and ready to start |
| running | Worker is processing a stage |
| awaiting_approval | A specific agent action is waiting for a decision |
| verified | Computation accepted and well-formed semantic review is positive |
| computed | Computed result without successful positive semantic review |
| needs_review | Semantic review identifies a mismatch with the question |
| rejected | Computational checks reject the produced result |
| evidence_verified | Agent flag is grounded in accepted local observations |
| needs_input | Required information is missing or a bounded plan cannot finish |
| stopped | Cancellation or restart recovery ended execution |
| failed | An execution exception prevented completion |

The stage field gives more detailed progress and should not be confused with status. The frontend uses actual server state rather than completing stages merely because an animation elapsed. duration_ms uses a monotonic clock for elapsed time; timestamps provide UTC audit labels.

Each evidence entry records an ID, timestamp, source, message and content. The API appends evidence rather than exposing an edit route. Persistence writes a temporary file, flushes it and replaces the destination atomically; Windows uses MoveFileEx with replacement/write-through flags. This reduces partial-file risk but is not a database transaction across multiple runs, a digital signature or tamper-proof storage.

At startup the engine loads appropriately named JSON files and skips invalid entries. Previously active runs are recovered as stopped rather than silently resumed. Back up data/runs and data/model.json with services stopped for a consistent local copy. Treat this data as potentially sensitive challenge material. Model weights and dependencies are separate, reproducible assets.

Stop sets the cancellation flag and wakes approval waits. Queued runs can be stopped immediately. In-flight model calls have socket cancellation and a total deadline; local HTTP tools have bounded connection/read/write timeouts. Stop is cooperative rather than instantaneous at every instruction. Approval must match the exact current action ID; stale or repeated approvals are rejected. Retrying creates a new run so the earlier evidence remains inspectable.

## 9 Deterministic algorithms and verification

The solver registry exposes 22 identifiers. shortest_path and dijkstra share the Dijkstra strategy. Internal graph helpers expand undirected edges into both directions and construct adjacency representations. Distances use a large long-long sentinel (4 × 10^15) and bounded input weights. Unreachable path results contain an empty path, null cost and reachable false.

| Algorithm | Implementation idea | Typical time and storage |
| --- | --- | --- |
| Dijkstra | Min-priority queue, discard stale queue entries, relax nonnegative edges, retain predecessors | O((V+E) log V), O(V+E) typical |
| BFS | FIFO queue, discover each node once, count edges as hops | O(V+E), O(V+E) |
| Bellman–Ford | Repeated edge relaxation, early exit, final-pass negative-cycle detection | O(VE), O(V+E) |
| Floyd–Warshall | Distance matrix; consider each node as an intermediate | O(V cubed), O(V squared) |
| Kruskal MST | Sort edges and join disjoint components with union-find | O(E log E) sorting; path-compressed sets |
| DFS reachability | Traverse adjacency from the source and return sorted reachable IDs | O(V+E) traversal plus output sorting |
| Topological sort | DFS with active/done colors; reverse completion order | O(V+E), O(V+E) |
| Kosaraju SCC | Finish-order traversal and traversal of the reversed graph | O(V+E), O(V+E) |
| 0/1 knapsack | One capacity array updated from high capacity to low | O(items × capacity), O(capacity) |

These are conceptual bounds for the named algorithms, not measured performance guarantees for all verification work. For example, the current independent MST verifier uses a heap but scans all edges for each expanded vertex. UI and JSON serialization costs are additional. The union-find uses path compression but should not be described as a union-by-rank implementation.

Dijkstra’s predecessor array reconstructs the selected path; ties can produce any valid optimal path. BFS deliberately ignores supplied weights. Bellman–Ford detects negative cycles reachable from its source. Floyd–Warshall rejects a negative diagonal. Because shortest-path verification uses all-pairs Floyd–Warshall, a negative cycle elsewhere in a graph can also cause verification rejection even when the source cannot reach it. This is a current semantic edge case to test when extending negative-weight support.

MST returns a minimum spanning forest when the graph is disconnected, with connected false. Topological sort rejects cycles. DFS output is sorted reachability rather than the order in which DFS visited nodes. Knapsack returns the maximum value, not the selected item list. Descending capacity updates are essential: ascending updates would allow an item to be reused and change the problem into an unbounded variant.

Shortest-path verification checks reachability, every returned edge, accumulated cost and the independent Floyd–Warshall optimum. MST verification checks original-edge membership, acyclicity, forest size, claimed cost, connected state and an independent Prim optimum. These checks catch a plausible-looking but incorrect path or tree rather than merely checking output shape.

Encoding verification performs inverse transformations and checks byte count and text/byte consistency. For strict Base64, re-encoding the decoded bytes must match the supplied canonical representation. Other utility solvers use deterministic recomputation; they are not independently implemented proofs. Pipeline verification reconstructs each resolved step input, verifies each result, compares stored verification records and checks that the final output matches the last step.

CIDR parses four decimal octets, derives a 32-bit mask, applies network/broadcast arithmetic and uses wider arithmetic for /0 address counts. Leading-zero octets are rejected rather than interpreted ambiguously. Unix mode conversion inspects each permission bit and special-bit combination to form r, w, x, s/S and t/T. Header analysis lowercases names and reports absence of selected headers such as CSP, HSTS and nosniff; those findings alone do not establish a vulnerability.

## 10 Model interpretation and answer review

GemmaClient owns synchronized settings and a settings-file path. It loads saved configuration, applies environment overrides and validates the resulting values. Diagnostics distinguish offline, loading, unauthorized, wrong endpoint, missing model and ready. Model discovery uses the IDs actually returned by /v1/models. The adapter can fall back to successful health information when the model-list route is unavailable, so Test inference remains the stronger readiness check.

Analyst receives the original question and an explicit input guide. Its schema binds each problem type to the appropriate category, required input and parameters. For example, graph problems require numeric nodes and edges rather than arbitrary input.text. Named vertices are mapped to numeric IDs while labels are preserved. Unsupported tasks and missing facts produce clarification responses instead of invented data.

The outer model response has status, spec, missing_information and explanation. Ready responses contain a usable spec. needs_input or unsupported responses have specific questions/reasons and no invented solution. C++ validates the response and the extracted spec. One bounded repair attempt is available after a validation failure. A JSON-shaped model answer is never automatically trusted merely because constrained generation succeeded.

The transport uses a two-second connection timeout, bounded write/read settings and a watchdog that checks cancellation/deadline at short intervals. Redirect following is disabled. It reads the first completion’s final content, rejects truncation and empty output, optionally strips a complete Markdown fence, parses JSON and enforces an object result. The current one-megabyte response limit is checked after receipt; it is not a streaming download cap.

After deterministic verification, Reviewer receives the original question, accepted spec and computed result. It returns answers_question, input_matches_question, confidence, answer_text, reason and explanation. confidence is a model’s self-reported score, not a calibrated probability. For text results, C++ compares answer_text against the actual result text. A reviewer cannot mentally complete a missing transformation and claim that the engine executed it.

When both review booleans are true and computational checks pass, the solver run can be verified. A negative review yields needs_review. An absent or failed review leaves a computed result without semantic endorsement. Failed computational verification yields rejected. The solution wrapper keeps the actual engine answer and a grounded explanation; the model does not overwrite the deterministic result.

The Analyst, Reviewer, Planner and Diagnostic schemas live in gemma.cpp. The published CTF-IR file is intentionally less detailed than the internal generation alternatives. Keep those representations and the backend validator aligned when adding capabilities. A small local model can still miss wording, omit operations or disagree inconsistently; acceptance tests should include failures and ambiguous inputs, not only successful decoding examples.

## 11 Bounded local fixture operations

Mode B supports a fixed tool vocabulary and an explicitly configured literal-loopback fixture origin. The policy validates every proposal before execution, including proposals generated by the model. An action is data until it passes policy and any required approval. This boundary is implemented in C++, not in a prompt.

| Tool | Purpose |
| --- | --- |
| HTTP_GET | Read a relative path from the configured local fixture |
| BASE64_DECODE | Decode supplied or previously observed text |
| HEX_DECODE | Decode hexadecimal text |
| CIDR_CALCULATE | Calculate a supplied IPv4 subnet |
| PERMISSION_ANALYZE | Explain supplied Unix mode bits |
| VERIFY_FLAG | Check bounded flag format and required observation provenance |

HTTP paths must start with a single slash and cannot contain backslashes, control characters or whitespace. Absolute URLs and origin overrides are rejected. Headers are bounded by count and length; Host, connection-management and proxy header overrides are disallowed. The HTTP client does not follow redirects and caps response bodies at 65,536 bytes. The allowlist is not an operating-system container sandbox.

Manual mode requires approval for each action. Assisted mode requires approval for HTTP requests carrying headers. Autonomous mode removes those interaction pauses but retains the same policy, tool set and step budget. An explicit plan runs without a model; a model-driven plan receives prior state and observations one step at a time. Failures are recorded and consume attempts. A finite plan or exhausted budget without a grounded flag ends in needs_input.

The included fixture supplies /challenge with an encoded example, /headers with a header hint, and /vault with a training response conditional on X-Staff: trainee. /redirect and /oversized exercise client rejection behavior. These are controlled regression fixtures, not a general website interaction engine. VERIFY_FLAG needs provenance rooted in a real fixture observation; a model-provided string alone cannot create that provenance. There is no external scoreboard submission.

The current snapshot contains stale Planner prompt wording that mentions an unsupported URL argument and broader authorization. PolicyEngine still enforces relative paths and the configured loopback origin. Treat this as a prompt-contract maintenance defect: align the wording with the enforced policy, then add a regression test. Do not use the historical fix_ssrf.py script to rebuild the application; it is outside the supported build and conflicts with that boundary.

## 12 React interface and visual system

main.tsx mounts App and imports the CSS. App owns page selection, health, run summaries, the selected run, editor state, search/filter state, errors and live connection state. api.ts centralizes fetch calls and JSON error conversion; downloadJson creates a browser-owned Blob URL and revokes it after use. types.ts documents the frontend contract but cannot validate untrusted server data at runtime.

| Module | Developer responsibilities |
| --- | --- |
| App.tsx | Overview, challenge library, operations history, evidence vault, system view and API orchestration |
| ProblemComposer.tsx | Pasted question, saved local draft, clipboard action and visible solver stages |
| ModelConnection.tsx | Editable endpoint/model settings, real diagnostics and generation test |
| components.tsx | Challenge editor, graph/result views, verification presentation and reusable UI |
| examples.ts | Editable inputs for supported demonstrations; never fabricated execution history |
| api.ts | Same-origin API calls and local report export |
| types.ts | Run, evidence, spec, verification and model interface shapes |

Health and history refresh independently of the selected run. The selected run uses WebSocket subscription ticks and REST recovery. Cleanup functions close subscriptions and clear timers when dependencies change or the component unmounts. Pending requests must not overwrite a newly selected run. A transport failure and a failed run are different states and need different UI messages.

The composer stores a draft under the rowdogg-draft localStorage key. It submits the original question with solver mode and displays actual engine stages. It disables model-dependent interpretation when the connection is unavailable. The challenge editor also accepts structured JSON or browser file import. Retry preserves the old request as editable input and creates a fresh operation. Imports are not arbitrary server file access.

The graph view uses a radial layout, limits visible topology to the first 16 nodes and preserves supplied labels. Path highlights follow returned edges and directedness checks. It is a presentation aid, not the authoritative full graph representation. Full JSON remains available for inspecting larger inputs and exact numeric IDs. Verification panels separate computational checks from semantic reasons and self-reported confidence.

The visual system combines a dark maroon/black base, yellow identity accents, cyan interactive focus and restrained scanlines. styles.css establishes most layout/components; cyberpunk.css is loaded later and overrides earlier rules. Later rules of equal specificity win. Consolidate repeated overrides carefully rather than changing a single early rule and assuming it controls the final appearance.

The desktop overview uses explicit grid placement for stats, input/output panels, recent-operation controls and the table. Flexible minmax columns prevent intrinsic text width from forcing overlap. Narrower layouts stack panels; phone rules reduce spacing and adapt navigation. Cursor SVGs use dedicated static routes. Reduced-motion rules limit animation, and keyboard focus must remain visible even with a custom pointer. Do not communicate successful verification using color alone.

The CSS currently imports external Google Fonts, including Share Tech Mono and Rajdhani. Computation and inference are local, but first-time font loading can contact Google. Fallback fonts allow offline rendering; a fully self-contained offline distribution would need licensed local font assets and updated CSS. Legacy decorative components and repeated CSS overrides remain maintenance debt rather than required architecture.

## 13 Tests and acceptance criteria

Run tests at the layer changed. Native tests exercise computation and policy without React. The benchmark compares labelled synthetic inputs and rejects tampered results. Integration tests start real backend/fixture processes with isolated data and a short-lived mock model. Real Gemma tests establish actual inference behavior. Browser checks exercise the user-visible workflow and layout.

```powershell
ctest --test-dir build --output-on-failure
python scripts/test_integration.py
$env:ROWDOGG_URL = 'http://127.0.0.1:8085'
python scripts/test_gemma.py
./build/benchmark_runner.exe data/benchmark.json
$env:ROWDOGG_BROWSER = 'chrome'
node scripts/test_frontend.cjs
```

The browser driver needs an installed Playwright module and browser. If automatic resolution fails, set PLAYWRIGHT_MODULE to the installed package location. Without the Chrome channel setting, install the Playwright Chromium browser appropriate to the pinned package. Inspect the test script for its module search order when adapting another workstation.

Integration ports 8086, 8082 and 8092 must be free. The real app, model and fixture can remain on 8085, 8081 and 8090. Tests use isolated storage; do not point destructive fixture cleanup at production data. scripts/generate_dataset.py rebuilds the 100-case synthetic data file using independent Python labels. Regeneration deliberately changes that generated file, so review its diff.

Existing saved reports record 13 passed browser checks with no browser errors and seven real-model acceptance cases. The prior native suite reported 105 solver assertions; the prior integration run reported 181 assertions, including polling-dependent checks. These are historical snapshot evidence, not a new test run performed while writing this manual. Re-run after source or model changes. The synthetic benchmark is not evidence of success on an unseen real-world challenge corpus.

Minimum release acceptance should cover a direct structured decode without a model; natural-language decoding with the real model; named graph extraction; a two-stage pipeline; a missing-input response; rejection of an omitted operation; manual local-fixture approval; stop and restart recovery; report export; and desktop/phone layouts without overlap. Tests must distinguish a correct computation from a correct interpretation.

## 14 Troubleshooting and known limitations

| Symptom | Check and resolution |
| --- | --- |
| Wrong application on 8080 | Use the selected 8085 URL and launch -Port 8085; do not stop an unrelated service |
| Backend starts but page is missing | Build frontend/dist and launch from the repository root |
| UI says engine unreachable | Inspect backend logs and actual configured port; api.ts still contains a generic 8080 error hint |
| llama-server executable not found | Restart terminal after installation or use explicit -ServerPath |
| Model file missing | Install weights separately and verify the filename, size and checksum |
| Model is loading | Read llama-error.log and wait for readiness before testing inference |
| Ready but inference test fails | Check runtime schema support, exact model ID, response errors and context/token settings |
| Response truncated | Inspect finish reason; increase output budget only within context and memory limits |
| Natural question needs input | Supply the requested missing facts rather than asking the model to invent them |
| Checks pass but review rejects | Compare original wording, extracted spec and computed result; inspect omitted steps |
| Styled changes do not appear | Distinguish Vite development from Crow production and rebuild production assets |
| Cursor falls back to default | Verify /cursor.svg and /cursor-target.svg resolve from the active origin |
| Tests cannot bind a port | Free only the relevant owned test service or change test configuration consistently |
| Start cannot replace executable | Stop the tracked backend before rebuilding on Windows |

The application has no user accounts, TLS deployment layer, database migrations, multi-machine scheduler or general operating-system isolation. Origin checks do not authenticate native local clients. Preserve loopback binding and trusted local data-directory access for this deployment. Hard-coded reserved port lists do not automatically exclude every custom application port; choose distinct app, model and fixture ports.

Implemented utility coverage does not include IPv6, digest computation, JWT inspection, Vigenère, packet capture, process inspection, arbitrary filesystem inspection, HTTP POST, general exploitation modules, mobile clients or SQLite. Model confidence is not measured accuracy. Audit JSON is editable by a local filesystem writer. Several independently recomputed checks share implementation with their solver and therefore share potential bugs.

Known maintenance work includes aligning the stale Planner prompt with policy, removing the generic 8080-only error hint, consolidating overlapping CSS, making offline fonts explicit, and reconciling generation schemas with the published contract. The documentation records these issues; it does not silently modify runtime code.

## 15 Rebuild the architecture step by step

Start with a single harmless structured operation such as Base64 decoding. Define the CTF-IR envelope, validate fields and return a stable JSON result. Add strict byte handling and inverse verification before exposing an HTTP endpoint. Write negative tests for malformed encoding, missing fields, oversized inputs and altered results. This establishes the contract the rest of the system relies on.

Next add graph input helpers and one path solver. Keep parsing, computation and verification separate. Use an independent all-pairs oracle in tests and verification, then introduce MST and its distinct verifier. Add utility algorithms through the same dispatcher. Expand capabilities, schema and fixtures together; never let a advertised type fall through to an unrelated solver.

Build Run and CTFEngine around the working solver. Establish submit/start/snapshot before adding persistence. Add locked snapshots, atomic file replacement, restart recovery, cancellation and bounded workers. Test interruption while waiting and while performing I/O. Keep transport functions thin so core behavior can be tested without Crow.

Add the REST routes and error mapping, then build a minimal React screen that submits a spec and displays the real result. Implement types, API helpers, loading/error states and report export. Add live snapshots and reconnect behavior only after REST works. Introduce the full navigation and responsive visual system after the data flow is stable.

Integrate llama.cpp through a dedicated adapter. Start with diagnostic generation, then schema-constrained Analyst output, strict validation and missing-input handling. Add the independent semantic review as a separate result dimension. Test the actual chosen model, not only mocked JSON responses. Preserve the structured model-free path as a reliable way to isolate model failures from solver failures.

Finally add the bounded local fixture workflow, evidence provenance and approval states. Keep policies outside prompts and test the enforcement directly. Complete browser workflows, accessibility checks and responsive layouts. Build release assets, run the appropriate test layers and record exact source/model versions. This sequence reproduces the architecture without coupling all failures to the first full-stack run.

## 16 Extend and maintain the code

For a new benign solver, update capabilities, validateSpec, solve dispatch and verify strategy in solvers.cpp; add a matching model generation alternative and input-guide entry; update the published schema, frontend examples and labels; add independently labelled fixtures and malformed-input tests. Specify units, integer bounds, binary/text behavior and failure semantics before writing UI copy.

For a new UI feature, first identify whether it needs an existing API response or a new contract. Keep derived display values out of persisted state unless they are part of the audit record. Add loading, empty, failure, cancellation and keyboard states alongside the happy path. Use the actual state vocabulary rather than inventing a second frontend lifecycle. Test both narrow and scrolled desktop layouts.

For an API change, update main.cpp routing, engine behavior, TypeScript shapes, callers, integration tests and this reference together. Keep request and result examples versioned. The current API has no automatic schema migration machinery, so incompatible saved-state changes require a deliberate migration design and backup plan.

Comment why a variable or function exists, which invariant it maintains, and what units/bounds apply. Avoid comments that simply repeat an assignment. Keep ownership and threading notes beside shared state, explain independent verification choices, and identify any same-implementation recomputation honestly. The Source Explorer supplies exact line navigation and reading notes; the original source comments remain the primary local maintenance explanations.

## 17 Source coverage and reading order

Use the Source Explorer in this order: CMakeLists.txt; solvers.hpp and solvers.cpp; engine.hpp and engine.cpp; gemma.cpp; main.cpp; frontend types.ts and api.ts; App.tsx and its components; the two stylesheets; schemas; launchers; and tests. Configuration files, SVG assets and the original architecture/README documentation are also included so the source snapshot can be studied without the running application.

Every included physical line has its exact source text, a stable line anchor and a reading note. Blank lines and delimiters are identified explicitly. Comments retain their original wording. Syntax-level notes explain declarations, branches, calls, JSX, CSS properties and configuration records; module summaries and this manual explain behavior across those lines. This is a source-reading reference, not a claim of formal verification of every statement.

coverage.json lists included files with hashes and counts and records excluded generated/private/historical material. The ZIP bundle contains the Word manual, Markdown manual, standalone HTML explorer, coverage manifest and a plain-text source snapshot. It intentionally excludes model weights, installed dependencies, build products and saved personal challenge runs. Keep the complete bundle together when sharing it with a future developer.

## 18 Result contracts and shared vocabulary

Every solver result is a JSON object, normally with an algorithm label identifying the implementation. The label is explanatory metadata, not a second dispatcher. The following table records the payload fields consumers should inspect; optional notes can appear alongside them.

| Solver family | Result fields | Interpretation |
| --- | --- | --- |
| Shortest path and BFS | path, cost, reachable, algorithm | Ordered numeric vertices; null cost when unreachable; BFS cost counts hops |
| MST | edges, cost, connected, algorithm | Selected original edge triples, forest weight and whether a full spanning tree exists |
| Floyd–Warshall | distances, algorithm | Square matrix; null cells indicate unreachable pairs |
| DFS | reachable, algorithm | Sorted array of reachable node IDs; here reachable is an array, not a boolean |
| Topological sort | order, algorithm | Valid vertex order; algorithm label is dfs_postorder |
| SCC | components, algorithm | Arrays of node IDs, sorted within each component; algorithm is kosaraju |
| Knapsack | value, algorithm | Maximum attainable value; no chosen-item reconstruction |
| Reversible text transforms | text, bytes_base64, byte_count, algorithm | text is a string or null; bytes_base64 preserves the actual output bytes |
| Digest identification | candidates, bits, note, algorithm | Possible algorithms based on hexadecimal shape, with ambiguity stated |
| IPv4 CIDR | network, broadcast, netmask, prefix, total_addresses, usable_hosts, first_host, last_host, algorithm | Address strings and numeric counts computed from the supplied prefix |
| Unix permissions | symbolic, suid, sgid, sticky, world_writable, algorithm | Nine-character permissions and boolean special-bit findings |
| Header analysis | headers, findings, note, algorithm | Lowercase header map and advisory missing-header strings |
| Pipeline | Final step fields plus steps and algorithm | Each steps entry includes resolved spec, result and verification; algorithm becomes verified_pipeline |

Verification has passed, checks and method. checks is an array of objects with name and passed. A passing verification refers to the supplied structured spec. It does not by itself establish that a natural-language question was completely represented. Read semantic_review separately and avoid equating a non-null result with success.

An HTTP tool observation contains status, headers, body and source. That status is an HTTP integer, unlike the run’s string lifecycle status. VERIFY_FLAG returns flag, format_valid and challenge_acceptance, whose value is not_checked. Engine-level provenance checks are additional to this tool’s format result. Generic frontend JSON objects therefore need contextual interpretation; a shared property name can have different types in different envelopes.

| Name or C++ construct | Meaning in this codebase |
| --- | --- |
| spec | Validated or proposed CTF-IR object; check which stage owns it |
| request | Original API payload retained for audit and retry |
| run and run->state | Synchronized execution object and its API-visible JSON snapshot |
| input and parameters | Data and controls extracted from a spec |
| previous | Last pipeline result or tool observation; not arbitrary model memory |
| proof and checked | Verification records derived from actual computation |
| adjacency and reverse | Forward and reversed graph neighbor lists |
| distance and INF | Best known costs and the sentinel for an unreached vertex |
| parent or predecessor array | Information needed to reconstruct a path or disjoint-set relationship; inspect local scope |
| color | DFS unvisited, active and completed traversal state |
| order | Traversal completion/order vector, subsequently reversed or sorted as required |
| best | Knapsack values indexed by available capacity |
| const reference | Read access without copying through that binding; does not make the entire shared object immutable |
| auto and structured binding | Compiler-inferred type and destructuring of a tuple/pair/object element |
| lambda capture by reference | Callback uses existing objects; their lifetime must outlast callback execution |
| lock_guard and unique_lock | Scoped mutex ownership; unique_lock also supports condition-variable waits |
| shared_ptr and jthread | Shared lifetime for run objects and owned/joined worker execution |
| optional TypeScript property | A field may be absent; components must handle that case |
| useState and useRef | Render-driving state versus a stable mutable reference |
| useEffect cleanup | Unsubscribes or clears resources when dependencies change or the component unmounts |
| CSS custom property | Reusable theme token inherited through the cascade |

The same identifier can be reused in different scopes. The source explorer preserves those scopes and line numbers so the reader can distinguish, for example, an HTTP status, a run status and a model response status. The variable’s declaration, nearby developer comment and enclosing function are more authoritative than its name alone.
