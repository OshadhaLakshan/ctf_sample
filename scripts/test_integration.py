"""Exercise real Crow endpoints, persisted workers, approvals, and a mock model contract."""
import base64
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

# Repository paths and loopback-only API targets are fixed for this integration suite.
ROOT = Path(__file__).resolve().parents[1]
BASE = "http://127.0.0.1:8080/api/v1"
OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))
SPEC = {"version": "1.0", "category": "graph", "problem": {"type": "shortest_path"}, "input": {"nodes": 3, "edges": [[0, 1, 2], [1, 2, 3], [0, 2, 9]]}, "parameters": {"source": 0, "target": 2}, "output": {"type": "path_and_cost"}}
ACTIONS = [{"action": "HTTP_GET", "arguments": {"path": "/challenge"}}, {"action": "BASE64_DECODE", "arguments": {"from_previous": True}}, {"action": "VERIFY_FLAG", "arguments": {"from_previous": True}}]
ASSERTIONS = 0


def check(condition, message):
    """Count checks and stop at the first actionable integration failure."""
    global ASSERTIONS
    ASSERTIONS += 1
    if not condition:
        raise AssertionError(message)


def call(path, payload=None, expected=200, headers=None):
    """Send one JSON request and verify both transport status and JSON decoding."""
    # Caller-provided headers let the suite test origin/CSRF boundaries.
    request_headers = {"Content-Type": "application/json", **(headers or {})}
    request = urllib.request.Request(BASE + path, data=json.dumps(payload).encode() if payload is not None else None, headers=request_headers)
    try:
        response = OPENER.open(request, timeout=8)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read().decode()
        check(response.status == expected, f"{path}: expected {expected}, got {response.status}: {body}")
        return json.loads(body)


def await_status(run_id, statuses, timeout=12):
    """Poll boundedly for an engine transition, failing with the final state."""
    # Monotonic deadline avoids system-clock adjustment affecting a test timeout.
    deadline = time.monotonic() + timeout
    state = {}
    while time.monotonic() < deadline:
        state = call(f"/agent/{run_id}/state")
        if state["status"] in statuses:
            return state
        time.sleep(0.06)
    raise AssertionError(f"Timed out waiting for {statuses}: {state}")


def start_run(mode="solver", **fields):
    """Create a fresh challenge and start its actual C++ pipeline."""
    # Each test supplies only the input contract needed by the selected mode.
    run = call("/challenges", {"title": "Integration test", "mode": mode, **fields})
    call("/agent/start" if mode == "agent" else "/solve", {"id": run["id"]})
    return run["id"]


class ModelMock(BaseHTTPRequestHandler):
    """Tests llama.cpp HTTP contracts; this is explicitly not a real Gemma model."""

    def do_GET(self):
        """Respond to the local model health probe."""
        self.send_json({"data": [{"id": "test-gemma"}]} if self.path == '/v1/models' else {"status": "ok"})

    def do_POST(self):
        """Produce role-specific JSON with deliberate malformed and malicious cases."""
        # Decode the actual engine prompt to verify its role/context integration.
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        role = payload["messages"][0]["content"].split("R0WD0GG's ")[-1].split('.')[0]
        context = json.loads(payload["messages"][1]["content"])
        question = context.get("question", "")
        if role == "Analyst":
            answer = {"version": "broken"} if "malformed" in question else SPEC
        elif role == "Planner":
            answer = {"action": "SHELL", "arguments": {"command": "echo blocked"}} if "blocked" in question else ACTIONS[context["state"]["steps"]]
        elif role == "Diagnostic":
            answer = {"ok": True}
        else:
            answer = {"answers_question": "yes"} if "invalid_review" in question else {"answers_question": "mismatch" not in question, "input_matches_question": "mismatch" not in question, "confidence": 0.9, "answer_text": "", "reason": "Mock contract review only", "explanation": "Mock contract explanation"}
        self.send_json({"choices": [{"message": {"content": json.dumps(answer)}}]})

    def send_json(self, data):
        """Write a valid, length-delimited HTTP JSON response."""
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        """Suppress expected local mock traffic to keep test results readable."""
        pass


def main():
    """Launch isolated services, execute assertions, and always stop test-owned processes."""
    # Tests never overwrite user run history and never reuse an unknown process.
    storage = tempfile.mkdtemp(prefix="integration-", dir=ROOT / "tmp")
    environment = {**os.environ, "ROWDOGG_DATA_DIR": storage, "ROWDOGG_LLAMA_URL": "http://127.0.0.1:8082"} # Keep the real model on 8081 untouched.
    binary = ROOT / "build" / ("rowdogg.exe" if os.name == "nt" else "rowdogg")
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    engine = None
    lab = None
    mock = None
    log = open(ROOT / "tmp" / "integration.log", "w", encoding="utf-8")
    try:
        # Refuse occupied ports to avoid testing or terminating somebody else's service.
        import socket
        for port in (8080, 8082, 8090):
            with socket.socket() as probe:
                check(probe.connect_ex(("127.0.0.1", port)) != 0, f"Port {port} must be free for isolated integration tests")
        engine = subprocess.Popen([str(binary)], cwd=ROOT, env=environment, stdout=log, stderr=log, creationflags=flags)
        lab = subprocess.Popen([sys.executable, "scripts/lab_fixture.py"], cwd=ROOT, stdout=log, stderr=log, creationflags=flags)
        # Wait for the engine to bind before issuing assertions.
        for attempt in range(50):
            try:
                health = call("/health")
                break
            except (OSError, urllib.error.URLError):
                time.sleep(0.1)
        else:
            raise RuntimeError("Engine failed to start; see tmp/integration.log")
        check(not health["model_available"], "Offline model must be reported honestly")
        run_id = start_run(spec=SPEC)
        state = await_status(run_id, {"computed", "failed"})
        check(state["status"] == "computed" and state["result"]["cost"] == 5, "Real deterministic solve")
        check(state["verification"]["passed"] and state["semantic_review"]["status"] == "unavailable", "Offline semantic review cannot be fabricated")
        check(call("/verify", {"id": run_id})["passed"], "Stored result reverification")
        check(len(call(f"/agent/{run_id}/evidence")) >= 4, "Evidence retrieval")
        check(call(f"/agent/{run_id}/result")["result"]["cost"] == 5, "Result retrieval")
        call("/solve", {"id": run_id}, 400)
        call("/challenges", {"spec": {"version": "invalid"}}, 400)
        call("/challenges", {"spec": SPEC}, 400, {"Origin": "https://untrusted.invalid"})
        call("/agent/missing/state", expected=404)
        call("/challenges", {"mode": "agent", "actions": [{"action": "SHELL", "arguments": {}}]}, 400)
        agent_id = start_run("agent", actions=ACTIONS, approval="manual")
        for step in range(3):
            state = await_status(agent_id, {"awaiting_approval"})
            check(state["steps"] == step, "Manual action cannot execute before approval")
            call("/agent/approve", {"id": agent_id, "action_id": "ACT-stale"}, 400)
            call("/agent/approve", {"id": agent_id, "action_id": state["pending_action"]["id"]})
            # Await a different stage/action before checking the next approval.
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                state = call(f"/agent/{agent_id}/state")
                if state["steps"] > step:
                    break
                time.sleep(0.05)
        state = await_status(agent_id, {"evidence_verified", "failed"})
        check(state["status"] == "evidence_verified" and state["result"]["flag"] == "CTF{evidence_over_assumptions}", "Real HTTP/decoding/evidence chain")
        check(state["result"]["challenge_acceptance"] == "not_checked", "No scoreboard acceptance claim")
        stopped_id = start_run("agent", actions=ACTIONS, approval="manual")
        await_status(stopped_id, {"awaiting_approval"})
        call("/agent/stop", {"id": stopped_id})
        check(await_status(stopped_id, {"stopped"})["steps"] == 0, "Stop wakes manual approval without executing")
        for path in ("/redirect", "/oversized"):
            failed_id = start_run("agent", actions=[{"action": "HTTP_GET", "arguments": {"path": path}}])
            check(await_status(failed_id, {"failed"})["status"] == "failed", "Redirect and response size limits")
        fabricated_id = start_run("agent", actions=[{"action": "BASE64_DECODE", "arguments": {"text": base64.b64encode(b"CTF{invented}").decode()}}, {"action": "VERIFY_FLAG", "arguments": {"from_previous": True}}])
        check("no supporting" in await_status(fabricated_id, {"failed"})["error"], "Model-provided bytes cannot fabricate lab provenance")
        # Run an actual local HTTP mock only for adapter/schema contract tests.
        mock = ThreadingHTTPServer(("127.0.0.1", 8082), ModelMock)
        threading.Thread(target=mock.serve_forever, daemon=True).start()
        natural_id = start_run(question="Find the shortest route")
        check(await_status(natural_id, {"verified", "failed"})["status"] == "verified", "Analyst and reviewer HTTP contracts")
        malformed_id = start_run(question="malformed model output")
        check(await_status(malformed_id, {"needs_input"})["status"] == "needs_input", "Malformed CTF-IR prompts clarification after bounded repair")
        check(call('/model')['selected_model'] == 'test-gemma', 'Model discovery uses actual server ID')
        check(call('/model/test', {})['passed'], 'Diagnostic JSON generation contract')
        call('/model', {'base_url': 'http://remote.invalid:8081'}, 400)
        mismatch_id = start_run(spec=SPEC, question='mismatch question')
        check(await_status(mismatch_id, {'needs_review'})['solution']['question_crosschecked'] is False, 'Semantic mismatch never marked verified')
        review_id = start_run(spec=SPEC, question="invalid_review")
        check(await_status(review_id, {"computed", "failed"})["semantic_review"]["status"] == "failed", "Malformed review cannot confer verified status")
        planned_id = start_run("agent", question="Find the flag in the local challenge", max_steps=3)
        check(await_status(planned_id, {"evidence_verified", "failed"})["status"] == "evidence_verified", "Model planner action loop contract")
        blocked_id = start_run("agent", question="blocked planner request")
        check(await_status(blocked_id, {"failed"})["steps"] == 0, "Malicious model action never executes")
        interrupted_id = start_run("agent", actions=ACTIONS, approval="manual")
        await_status(interrupted_id, {"awaiting_approval"})
        engine.terminate()
        engine.wait(timeout=5)
        engine = subprocess.Popen([str(binary)], cwd=ROOT, env=environment, stdout=log, stderr=log, creationflags=flags)
        time.sleep(0.5)
        check(call(f"/agent/{run_id}/state")["result"]["cost"] == 5, "Completed result survives process restart")
        check(call(f"/agent/{interrupted_id}/state")["status"] == "stopped", "Interrupted run never resumes without operator action")
        print(f"{ASSERTIONS} integration assertions passed (mock-model contract tests, not Gemma accuracy)")
    finally:
        if mock:
            mock.shutdown()
            mock.server_close()
        for process in (engine, lab):
            if process and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
        log.close()


if __name__ == "__main__":
    main()
