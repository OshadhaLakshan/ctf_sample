"""Generate 100 reproducible, labelled fixtures using independent Python references."""
import base64
import ipaddress
import json
import random
import stat
from pathlib import Path

# Repository root anchors output regardless of the invoking working directory.
ROOT = Path(__file__).resolve().parents[1]
# A local PRNG seed makes the benchmark reproducible without process-global randomness.
RNG = random.Random(42042)


def spec(category, problem, input_data, parameters=None):
    """Create a complete CTF-IR fixture accepted by the C++ boundary."""
    return {"version": "1.0", "category": category, "problem": {"type": problem}, "input": input_data, "parameters": parameters or {}, "output": {"type": "result"}}


def generate():
    """Create an explicitly synthetic regression set, not a claimed research evaluation."""
    # Each record carries provenance and an independently computed ground-truth field.
    records = []
    for index in range(25):
        # Small directed acyclic graphs include a guaranteed reachable path.
        nodes = 3 + index % 6
        edges = [[node, node + 1, RNG.randint(1, 15)] for node in range(nodes - 1)]
        for source in range(nodes):
            for target in range(source + 2, nodes):
                if RNG.random() < 0.45:
                    edges.append([source, target, RNG.randint(0, 20)])
        # Python Bellman-Ford supplies independent expected costs for C++ Dijkstra.
        distances = [float("inf")] * nodes
        distances[0] = 0
        for _ in range(nodes - 1):
            for source, target, weight in edges:
                distances[target] = min(distances[target], distances[source] + weight)
        records.append({"id": f"graph-{index:03}", "category": "graph", "difficulty": "synthetic-small", "question": f"Find the minimum cost from 0 to {nodes - 1}.", "spec": spec("graph", "shortest_path", {"nodes": nodes, "directed": True, "edges": edges}, {"source": 0, "target": nodes - 1}), "expected": {"cost": distances[-1]}, "reference": "Python Bellman-Ford"})
        # Encoding expected values are known source text, not copied from C++ output.
        plaintext = f"CTF{{fixture_{index:03}_evidence}}"
        encoded = base64.b64encode(plaintext.encode()).decode()
        records.append({"id": f"encoding-{index:03}", "category": "crypto", "difficulty": "synthetic-small", "question": "Decode the Base64 signal.", "spec": spec("crypto", "base64_decode", {"text": encoded}), "expected": {"text": plaintext}, "reference": "Python base64"})
        # Include /0, /31, /32 and other boundary prefixes.
        prefix = [0, 8, 16, 20, 24, 28, 30, 31, 32][index % 9]
        cidr = f"10.{index}.13.37/{prefix}"
        network = ipaddress.ip_network(cidr, strict=False)
        records.append({"id": f"network-{index:03}", "category": "network", "difficulty": "synthetic-boundary", "question": f"Calculate the subnet for {cidr}.", "spec": spec("network", "cidr_subnet", {"cidr": cidr}), "expected": {"network": str(network.network_address), "broadcast": str(network.broadcast_address), "total_addresses": network.num_addresses, "usable_hosts": network.num_addresses if prefix >= 31 else network.num_addresses - 2}, "reference": "Python ipaddress"})
        # POSIX permission references include SUID, SGID and sticky-bit cases.
        mode = RNG.randrange(0o10000)
        records.append({"id": f"linux-{index:03}", "category": "linux", "difficulty": "synthetic-boundary", "question": f"Analyze Unix mode {mode:04o}.", "spec": spec("linux", "permission_analyze", {"mode": f"{mode:04o}"}), "expected": {"symbolic": stat.filemode(mode)[1:]}, "reference": "Python stat.filemode"})
    (ROOT / "data" / "benchmark.json").write_text(json.dumps({"description": "100 synthetic deterministic regression fixtures; not a curated CTF corpus or model evaluation.", "seed": 42042, "records": records}, indent=2), encoding="utf-8")
    print(f"Generated {len(records)} labelled fixtures")


if __name__ == "__main__":
    generate()
