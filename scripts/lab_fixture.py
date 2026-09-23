"""A deliberately tiny loopback-only CTF fixture for the shipped agent example."""
import base64
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# Fixed educational flag contains no external secret or credential.
FLAG = "CTF{evidence_over_assumptions}"


class LabHandler(BaseHTTPRequestHandler):
    """Serve bounded, deterministic observations for local integration exercises."""

    def do_GET(self):
        """Return the encoded signal, a controlled redirect, or a scoped 404."""
        # Response values are derived solely from these explicit fixture routes.
        status = 200
        body = b""
        if self.path == "/challenge":
            body = base64.b64encode(FLAG.encode())
        elif self.path == "/headers":
            body = b"Staff access requires an X-Staff: trainee header. Try /vault."
        elif self.path == "/vault":
            status = 200 if self.headers.get("X-Staff") == "trainee" else 403
            body = FLAG.encode() if status == 200 else b"Missing lab staff header"
        elif self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "http://example.invalid/")
            self.end_headers()
            return
        elif self.path == "/oversized":
            body = b"X" * 70000
        else:
            status = 404
            body = b"Fixture route not found"
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Lab", "R0WD0GG local fixture")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        """Keep local fixture logs concise and deterministic."""
        print("LAB", format % args, flush=True)


if __name__ == "__main__":
    import os
    port = int(os.environ.get('ROWDOGG_FIXTURE_PORT', '8090')) # Separate test fixtures from the user's active lab.
    # The fixture never binds to a public network interface.
    server = ThreadingHTTPServer(("127.0.0.1", port), LabHandler)
    print(f"Local CTF fixture: http://127.0.0.1:{port}/challenge", flush=True)
    server.serve_forever()
