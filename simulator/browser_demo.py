#!/usr/bin/env python3
"""Serve the real SDL/LVGL simulator through a loopback browser session."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent
BINARIES = {
    "portrait": ROOT / "build-portrait/jr-r1-sim-portrait",
    "landscape": ROOT / "build/jr-r1-sim",
}
ACTIONS = {"menu": 0, "home": 1, "page": 2, "quick": 4, "wake": 6,
           "arm": 9, "cut": 9, "up": 21, "down": 22, "select": 23,
           "back": 24, "minus": 25, "plus": 26}


def command_for(data):
    """Allow only simulator input commands; never accept shell text or paths."""
    if not isinstance(data, dict):
        raise ValueError("Expected an input object")
    kind = data.get("kind")
    if kind == "touch":
        x, y = int(data["x"]), int(data["y"])
        if not (0 <= x < 480 and 0 <= y < 480):
            raise ValueError("Touch outside screen")
        if type(data.get("down")) is not bool:
            raise ValueError("Expected touch state")
        return f"touch {x} {y} {int(data['down'])}\n"
    if kind in ("throttle", "steering", "rx", "tx"):
        value = int(data["value"])
        low, high = (-1000, 1000) if kind in ("throttle", "steering") else (0, 200)
        if not low <= value <= high:
            raise ValueError("Input outside range")
        return f"{kind} {value}\n"
    if kind == "link" and type(data.get("value")) is bool:
        return f"link {int(data['value'])}\n"
    if kind == "key" and data.get("name") in ("mode", "up", "plus", "select", "page", "clear", "down", "minus") and type(data.get("down")) is bool:
        index = ("mode", "up", "plus", "select", "page", "clear", "down", "minus").index(data["name"])
        event = 16 if data.get("cancel") is True else 1 if data["down"] else 2
        return f"action {32+index} {event}\n"
    if kind == "action" and data.get("name") in ACTIONS:
        name = data["name"]
        return f"action {ACTIONS[name]} {0 if name == 'cut' else 1}\n"
    raise ValueError("Unsupported input")


class Demo:
    def __init__(self, directory):
        self.directory = directory
        self.token = secrets.token_urlsafe(32)
        self.processes = {}
        self.locks = {}
        self.logs = []

    def start(self):
        for name, binary in BINARIES.items():
            if not binary.is_file():
                raise RuntimeError(f"Missing {binary}; run make and make portrait first")
            bridge = self.directory / name
            bridge.mkdir()
            state = ROOT / "state-browser" / name
            state.mkdir(parents=True, exist_ok=True)
            env = dict(os.environ, SDL_VIDEODRIVER="dummy",
                       JR_R1_SIM_BRIDGE=str(bridge), JR_R1_SIM_STATE=str(state))
            log = (bridge / "simulator.log").open("wb")
            self.logs.append(log)
            self.processes[name] = subprocess.Popen(
                [str(binary), "--page", "dashboard"], cwd=ROOT, env=env,
                stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
            self.locks[name] = threading.Lock()

    def stop(self):
        for process in self.processes.values():
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            if process.stdin:
                process.stdin.close()
        for log in self.logs:
            log.close()

    def send(self, orientation, command):
        process = self.processes[orientation]
        if process.poll() is not None:
            raise RuntimeError("Simulator stopped; restart the demo")
        with self.locks[orientation]:
            process.stdin.write(command.encode("ascii"))
            process.stdin.flush()


def handler_for(demo, port):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def local_request(self):
            return self.headers.get("Host") in (f"127.0.0.1:{port}", f"localhost:{port}")

        def reply(self, data, content_type="text/plain", status=200):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; img-src 'self' blob:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'self'")
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            if not self.local_request():
                self.reply(b"Local access only", status=403)
                return
            path = urlparse(self.path).path
            if path == "/":
                page = (ROOT / "browser_demo.html").read_text()
                self.reply(page.replace("__SIM_TOKEN__", demo.token).encode(), "text/html; charset=utf-8")
                return
            for orientation in BINARIES:
                if path == f"/frame/{orientation}.bmp":
                    frame = demo.directory / orientation / "frame.bmp"
                    if frame.is_file() and demo.processes[orientation].poll() is None:
                        self.reply(frame.read_bytes(), "image/bmp")
                    else:
                        self.reply(b"Simulator starting or unavailable", status=503)
                    return
            self.reply(b"Not found", status=404)

        def do_POST(self):
            if not self.local_request() or self.headers.get("X-Simulator-Token") != demo.token:
                self.reply(b"Invalid session", status=403)
                return
            orientation = urlparse(self.path).path.removeprefix("/input/")
            if orientation not in BINARIES:
                self.reply(b"Unknown display", status=404)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 2048:
                    raise ValueError("Invalid input length")
                data = json.loads(self.rfile.read(length))
                command = command_for(data)
                demo.send(orientation, command)
            except (ValueError, TypeError, KeyError) as error:
                self.reply(str(error).encode(), status=400)
                return
            except (RuntimeError, BrokenPipeError) as error:
                self.reply(str(error).encode(), status=503)
                return
            self.reply(b"{}", "application/json")
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8767)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="r1-browser-") as temporary:
        demo = Demo(Path(temporary))
        server = None
        try:
            demo.start()
            server = ThreadingHTTPServer(("127.0.0.1", args.port), handler_for(demo, args.port))
            server.daemon_threads = True
            print(f"JR R1 current UI: http://127.0.0.1:{args.port}", flush=True)
            print("Demo settings: simulator/state-browser; Ctrl+C stops both simulators.", flush=True)
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            if server:
                server.server_close()
            demo.stop()


if __name__ == "__main__":
    main()
