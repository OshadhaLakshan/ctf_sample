"""Test real local Gemma inference through Crow; never substitute a mock response."""
import json
import os
import time
import urllib.request
from pathlib import Path

# Fixed local application endpoint; llama.cpp must already be running separately.
BASE = os.environ.get('ROWDOGG_URL', 'http://127.0.0.1:8080') + '/api/v1'
ROOT = Path(__file__).resolve().parents[1]
OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))
# Each case has an independently known answer, or deliberately missing information.
CASES = [
    ('base64', 'Decode the Base64 signal Q1RGe3RydXN0X2J1dF92ZXJpZnl9 and return the plaintext.', 'text', 'CTF{trust_but_verify}'),
    ('graph', 'Find the minimum-cost path from Start to Finish in an undirected graph. The vertices are Start, Relay, Finish. Edges: Start--Relay weight 2; Relay--Finish weight 3; Start--Finish weight 9. Return the path and total cost.', 'cost', 5),
    ('subnet', 'For IPv4 address 10.42.13.37/20, calculate the network address, broadcast address and usable host range.', 'network', '10.42.0.0'),
    ('pipeline', 'Decode NDg2NTZjNmM2Zg== using Base64 first. The resulting text is hexadecimal: decode that hexadecimal next. Return the final plaintext.', 'text', 'Hello'),
    ('clarification', 'Decrypt my Caesar ciphertext. I have not provided the ciphertext or the shift yet.', None, None),
    ('unsupported', 'Factor this 4096-bit RSA modulus to recover a private key. No modulus or exponent has been supplied.', None, None),
]


def call(path, data=None):
    """Make one real loopback API request with time for CPU inference."""
    request = urllib.request.Request(BASE + path, data=None if data is None else json.dumps(data).encode(), headers={'Content-Type': 'application/json'})
    with OPENER.open(request, timeout=610) as response:
        return json.load(response)


def main():
    """Save all actual model responses and fail if any expected end-to-end behavior fails."""
    report = {'model': call('/model'), 'diagnostic': call('/model/test', {}), 'cases': []}
    for name, question, field, expected in CASES:
        created = call('/challenges', {'title': 'Gemma acceptance: ' + name, 'question': question, 'mode': 'solver'})
        call('/solve', {'id': created['id']})
        deadline = time.monotonic() + 610
        while True:
            state = call('/agent/' + created['id'] + '/state')
            if state['status'] not in ('queued', 'running', 'awaiting_approval') or time.monotonic() > deadline:
                break
            time.sleep(1)
        passed = state['status'] == 'needs_input' if field is None else state['status'] == 'verified' and state.get('result', {}).get(field) == expected and state['verification']['passed'] and state['solution']['question_crosschecked']
        report['cases'].append({'case': name, 'passed': passed, 'expected': expected, 'state': state})
        (ROOT / 'tmp' / 'gemma-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(f"{name}: {'PASS' if passed else 'FAIL'} / {state['status']} / {state.get('duration_ms')} ms", flush=True)
    # An intentionally incomplete plan must not gain approval when Gemma can mentally finish it.
    incomplete = {'version': '1.0', 'category': 'crypto', 'problem': {'type': 'base64_decode'}, 'input': {'text': 'NDg2NTZjNmM2Zg=='}, 'parameters': {}, 'output': {'type': 'result'}}
    created = call('/challenges', {'title': 'Gemma acceptance: omitted second step', 'question': CASES[3][1], 'spec': incomplete})
    call('/solve', {'id': created['id']})
    deadline = time.monotonic() + 190
    while True:
        state = call('/agent/' + created['id'] + '/state')
        if state['status'] != 'running' or time.monotonic() > deadline:
            break
        time.sleep(1)
    passed = state['status'] == 'needs_review' and not state['solution']['question_crosschecked']
    report['cases'].append({'case': 'omitted_step_rejected', 'passed': passed, 'state': state})
    (ROOT / 'tmp' / 'gemma-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f"omitted_step_rejected: {'PASS' if passed else 'FAIL'} / {state['status']}", flush=True)
    assert all(case['passed'] for case in report['cases']), 'See tmp/gemma-report.json for actual responses and failed cases'


if __name__ == '__main__':
    main()
