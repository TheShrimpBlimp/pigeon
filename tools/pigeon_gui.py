#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the self-contained Pigeon GUI locally; no audio uploads or device writes.

  python3 tools/pigeon_gui.py                 # macOS / Linux
  py tools/pigeon_gui.py                     # Windows
  python3 tools/pigeon_gui.py --no-open --port 8765

The hosted website uses the same files, served over HTTPS. This launcher is only
for local development/use and binds to loopback, not the network.
"""
import argparse
import functools
import http.server
import socketserver
from pathlib import Path
import webbrowser


class LocalHTTPServer(http.server.ThreadingHTTPServer):
    def server_bind(self):
        # HTTPServer normally reverse-resolves the host name, which can stall
        # offline startup. This tool has a fixed loopback identity.
        socketserver.TCPServer.server_bind(self)
        self.server_name = 'localhost'
        self.server_port = self.server_address[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--no-open', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'web'
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    try:
        server = LocalHTTPServer(('127.0.0.1', args.port), handler)
    except OSError as e:
        parser.exit(1, f'Cannot start local GUI: {e}. Try a different --port.\n')
    url = f'http://127.0.0.1:{server.server_port}/pigeon/'
    print(f'Pigeon GUI: {url}\nOnly the web/ directory is served. Press Ctrl+C to stop.', flush=True)
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
