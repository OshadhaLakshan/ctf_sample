# R0WD0GG

A local CTF operations application implementing the proposal's **React → Crow/C++20 → CTF-IR → deterministic solvers / policy-gated agent → verification** architecture. The cyberpunk console uses dark maroon panels, yellow identity, cyan controls, subtle scanlines, reduced-motion support, responsive topology visualization, live traces, an evidence vault, and explicit verification states.

## Start on this Windows workstation

The build artifacts are in `build/` and `frontend/dist/` after successful setup.

```powershell
cd D:\ctf_sample
./scripts/start-llama.ps1
./scripts/start.ps1 -WithLab -Port 8085
# Open http://127.0.0.1:8085 (8080 is occupied by Apache on this workstation)
./scripts/stop.ps1
./scripts/stop-llama.ps1
```

For React development, use `./scripts/start.ps1 -WithLab -Dev -Port 8085`, then open **http://127.0.0.1:3000**. It proxies REST and WebSocket traffic to the selected Crow port. The default remains 8080 when `-Port` is omitted; `ROWDOGG_PORT` controls direct backend launches. Launch scripts hide service windows and write logs under `tmp/`.

1. Paste the complete problem into **CTF problem statement**, then choose **Interpret & solve**. Gemma extracts CTF-IR, C++ computes/checks the result, and Gemma cross-checks the original question, extracted inputs, and answer. **System → Model connection** shows diagnostics and an inference test. For a model-free example, click **Run challenge** or open **Challenge lab**.
2. **New operation** accepts a description, `.txt` / `.json` import, CTF-IR, or an action plan.
3. The bundled **Hidden transmission** lab exercises HTTP → Base64 → evidence-backed flag verification.
4. Select **Manual** in the agent editor to approve each proposed action, or use **Stop execution**.
5. Inspect verification, expand raw evidence, reverify a stored result, or export a JSON report.
6. Missing information and review mismatches offer **Edit challenge & retry**; each retry preserves the original audit record.

Example inputs are labelled. No fake execution history, model responses, performance figures, or verification outcomes are shipped in the production data directory.

## Build from source

Requires a C++20 compiler, CMake 3.20+, Node 20.19+ or a compatible recent Node release, and Python 3.10+ for fixtures/tests. This workstation uses MinGW GCC 14.2. Linux is also supported by the source; Linux builds have not been exercised on this Windows host.

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
cd frontend
npm ci
npm run build
cd ..
ctest --test-dir build --output-on-failure
```

On Linux, omit `-G "MinGW Makefiles"` and run `./build/rowdogg` from the repository root. In a second terminal run `python3 scripts/lab_fixture.py` if using the sample lab. CMake fetches hash-pinned Crow 1.2.1, standalone Asio 1.30.2, nlohmann/json 3.11.3, and cpp-httplib 0.18.1. Initial builds require network access; runtime CTF computation is local. `package-lock.json` pins frontend dependencies.

The built React frontend is also served directly by Crow at `/`, eliminating a development server for normal use. No Node backend is used.

## Local Gemma

Model weights and llama.cpp are **not committed to Git**. On this workstation, the winget llama.cpp installation is discovered automatically and Gemma 4 E2B Q4_0 has been downloaded into `models/gemma-4-E2B-it-Q4_0.gguf`. Start it with `./scripts/start-llama.ps1`. The script runs the server hidden, binds port 8081, and saves logs to `tmp/llama-error.log`. Other installations can supply `-ServerPath` and `-ModelPath`.

Downloaded source: [ggml-org/Gemma 4 E2B GGUF](https://huggingface.co/ggml-org/gemma-4-E2B-it-GGUF/tree/b4243c1), revision `b4243c1`, 2,841,481,184 bytes. SHA-256 verified: `8e30dff3ac4c8434c49a7036fa15564bdbb6044e42bf04550bf1a096ad7e6a52`.

Equivalent command:

```text
llama-server -m /absolute/path/to/gemma.gguf --host 127.0.0.1 --port 8081 -c 8192
```

The C++ adapter uses `/health`, `/v1/models`, and `/v1/chat/completions`. It discovers the actual model ID and requests role-specific schema-constrained JSON with thinking disabled. C++ validates responses, attempts one bounded schema repair, and reports missing inputs without inventing data. Endpoint, model ID, timeout, and token budget are editable under **System** and saved to `data/model.json`. Loading, authentication, transport, truncated responses, and invalid JSON have distinct diagnostics. See the official [llama.cpp server documentation](https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md).

Structured CTF-IR and explicit action plans do not require a model. Without Gemma, a correct deterministic result is labelled **Checks passed**. Natural-language runs require the model; a failed or negative semantic review never confers verified status. `python scripts/test_gemma.py` exercises real inference and saves complete responses to `tmp/gemma-report.json`; mock contract tests are separate.

## Implemented functionality

| Layer             | Implementation                                                                                                                                                                            |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Frontend          | React + TypeScript, dashboard, editor/import, mode selection, search, history, topology, live logs, approvals, stop, verification, export, responsive layout, keyboard focus handling     |
| API               | Crow REST + WebSocket, loopback binding, origin checks, bounded JSON contracts                                                                                                            |
| Mode A            | Structured or Gemma-interpreted input → validation → deterministic computation → verification → optional Gemma review                                                                     |
| Mode B            | Explicit or Gemma-generated plan → policy → manual/assisted/autonomous approval → bounded tool → persisted evidence → replanning, up to 32 actions                                        |
| State             | Per-run synchronized state, evidence snapshots, atomic replace on save, restart recovery; interrupted runs become stopped                                                                 |
| Graph             | BFS, DFS reachability, Dijkstra, Bellman–Ford, Floyd–Warshall, Kruskal MST, topological sort, Kosaraju SCC                                                                                |
| Algorithms        | 0/1 knapsack dynamic programming, union-find, heaps, queues, adjacency lists                                                                                                              |
| Encoding          | Strict Base64, hex, repeating-key XOR, ROT13, Caesar, digest-shape identification                                                                                                         |
| Multi-stage       | Up to eight non-nested CTF-IR steps; `$previous` carries verified text between decoding operations; intermediate results and checks remain inspectable                                    |
| Network / systems | IPv4 CIDR including /0, /31, /32; Unix permissions with SUID/SGID/sticky bits; observed HTTP header analysis                                                                              |
| Tools             | HTTP_GET, BASE64_DECODE, HEX_DECODE, CIDR_CALCULATE, PERMISSION_ANALYZE, VERIFY_FLAG                                                                                                      |
| Tests             | Solver/policy unit tests, independent-oracle graph tests, real API integration, mock model contracts, restart recovery, browser workflow tests, 100 synthetic labelled benchmark fixtures |

### Verification semantics

- Shortest paths: edge/path validity, accumulated cost, reachability, and an independent **Floyd–Warshall** optimum.
- Spanning trees: original-edge membership, forest structure, claimed cost, and independent **Prim** optimum.
- Reversible encodings: inverse transformation, byte count, and UTF-8 display consistency. Binary output is preserved as Base64, not corrupted text.
- Other utility solvers: explicitly labelled **deterministic recomputation**, rather than a claim of an independent optimality proof.
- Agent flags: fixed bounded flag-format check plus provenance rooted in a real lab HTTP response. A model cannot earn lab provenance by submitting an invented encoded flag. This does **not** establish challenge scoreboard acceptance.
- Semantic review is shown separately. `verified` requires deterministic checks and a well-formed positive model review. A model confidence value is labelled self-reported; no confidence score is fabricated when the model is absent.

### Scope and extension boundaries

This is a working local implementation of the core research architecture, not a claim that the full six-month research programme or every cybersecurity utility in the proposal is complete. The implemented registry above is the authoritative feature list.

IPv6 calculations, MD5/SHA digest computation, JWT inspection, Vigenère, packet capture, process or arbitrary filesystem inspection, HTTP POST, general exploitation modules, mobile clients, SQLite, and a curated 500–1000 challenge research corpus remain extensions. Only a single explicitly configured literal-loopback HTTP lab origin is supported. The tool allowlist is an execution boundary, **not an OS container sandbox**. HTTPS and public/private-network targets are not enabled. Research claims comparing real LLM baselines require a separately collected evaluation.

## API

All mutations require `Content-Type: application/json`. Use `id` from challenge submission with subsequent execution endpoints.

| Method     | Route                         | Purpose                                                    |
| ---------- | ----------------------------- | ---------------------------------------------------------- |
| GET        | `/api/v1/health`              | Actual model health and solver registry                    |
| GET / POST | `/api/v1/model`              | Diagnose or save local model connection settings           |
| POST       | `/api/v1/model/test`          | Perform actual schema-constrained diagnostic inference     |
| GET / POST | `/api/v1/challenges`          | List summaries / persist a challenge                       |
| POST       | `/api/v1/solve`               | Start a queued Mode A challenge: `{ "id": "..." }`         |
| POST       | `/api/v1/agent/start`         | Start a queued Mode B challenge                            |
| POST       | `/api/v1/agent/stop`          | Request stop; in-flight calls finish or hit their timeout  |
| POST       | `/api/v1/agent/approve`       | Approve exactly `{ "id": "...", "action_id": "ACT-1" }`    |
| GET        | `/api/v1/agent/{id}/state`    | Complete persisted state                                   |
| GET        | `/api/v1/agent/{id}/evidence` | Timestamped audit observations                             |
| GET        | `/api/v1/agent/{id}/result`   | Result and separate verification/review                    |
| POST       | `/api/v1/verify`              | Recheck a stored deterministic result                      |
| WS         | `/api/v1/live`                | Send `{ "id": "..." }` to receive an atomic state snapshot |

The browser requests bounded live snapshots over WebSocket; it falls back to REST if the connection fails. Four runs may execute concurrently. At most 1000 persisted runs are admitted. Stop is cooperative; lab calls have a five-second read timeout. Model calls have a configurable total deadline (default 180 seconds); cancellation interrupts their HTTP socket. The runtime does not execute shell commands. Only trusted local users should have write access to the data directory.

Configuration is operator-controlled through environment variables:

- `ROWDOGG_DATA_DIR`: state directory, default `data/runs` relative to the process working directory.
- `ROWDOGG_LAB_ORIGIN`: default `http://127.0.0.1:8090`. Must be literal loopback with port 1024–65535, excluding application ports 3000, 8080, and 8081. Redirects, proxy headers, Host overrides, header control characters, and absolute request URLs are rejected.

## Tests and benchmark

The isolated API integration suite requires ports 8086, 8082, and 8092 to be free. The real app, model, and lab can stay running on their separate ports.

```powershell
ctest --test-dir build --output-on-failure
python scripts/test_integration.py
$env:ROWDOGG_URL = 'http://127.0.0.1:8085'
python scripts/test_gemma.py
./build/benchmark_runner.exe data/benchmark.json
python scripts/generate_dataset.py
```

The checked-in 100-fixture dataset is explicitly **synthetic regression data**. It uses Python Bellman–Ford, `base64`, `ipaddress`, and `stat.filemode` as independent labels. The C++ benchmark checks expected fields, verification acceptance, and rejection of deliberately altered results. It makes no claim about natural-language interpretation accuracy or real-world CTF success rates.

Integration tests run real Crow and tool processes in an isolated temporary data directory. The llama.cpp adapter is tested using a short-lived local mock on port 8082; it is stopped at test completion and is never installed as the user's model service.

Browser checks: start the app and fixture, then run `node scripts/test_frontend.cjs` with `PLAYWRIGHT_MODULE` pointing to an installed Playwright package, or install Playwright in the frontend. `ROWDOGG_BROWSER=chrome` uses installed Chrome; without that variable, install Playwright Chromium. Screenshots and reports are saved under `tmp/`.

## Developer map

- `backend/src/solvers.cpp`: validation, deterministic solver registry, independent checks.
- `backend/src/engine.cpp`: policy engine, controlled tools, state/evidence orchestration.
- `backend/src/gemma.cpp`: model settings, diagnosis, schema-constrained inference, cancellation, and role prompts.
- `backend/src/main.cpp`: transport only: Crow routes, JSON errors, WebSocket and built frontend.
- `frontend/src/App.tsx`: actual API state and application views.
- `frontend/src/ProblemComposer.tsx`: pasted-question intake and live pipeline progress.
- `frontend/src/ModelConnection.tsx`: saved connection settings, discovery, and inference test.
- `frontend/src/components.tsx`: graph, verification, result, and challenge-editor components.
- `frontend/src/examples.ts`: editable challenge inputs, never mock results.
- `frontend/src/styles.css` and `cyberpunk.css`: responsive layout, reference-inspired palette, and restrained motion.
- `schemas/`: machine-readable wire contracts; C++ performs additional semantic validation.
- `docs/ARCHITECTURE.md`: trust boundaries, states, and extension checklist.

Functions, model fields, state variables, and algorithm data structures carry maintenance comments. When adding a solver, update the capability list, validator, dispatcher, verification strategy, schema, labelled fixtures, and tests together. Do not silently downgrade an unsupported type or turn an unavailable model into a simulated response.
