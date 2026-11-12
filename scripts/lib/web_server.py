#!/usr/bin/env python3
# SPDX-FileCopyrightText: Fission359 contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Serves the WebAssembly build of Fission359 locally (scripts/web serve).

The Qt wasm build needs SharedArrayBuffer for its threads, and that only exists with
cross-origin isolation, so every answer carries COOP/COEP (and CORP). The server binds
to 127.0.0.1 only. Standard library only.

Usage: web_server.py <folder> <port>
"""

import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

# MIME types the page needs: .wasm must be application/wasm or the browser refuses it.
EXTRA_MIME_TYPES = {
    ".wasm": "application/wasm",
    ".js": "text/javascript",
}


class WebHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def guess_type(self, path):
        for suffix, mime in EXTRA_MIME_TYPES.items():
            if path.endswith(suffix):
                return mime
        return super().guess_type(path)


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Aufruf: {sys.argv[0]} <Ordner> <Port>", file=sys.stderr)
        return 2
    folder = sys.argv[1]
    try:
        port = int(sys.argv[2])
    except ValueError:
        print(f"Ungültiger Port: {sys.argv[2]}", file=sys.stderr)
        return 2
    handler = partial(WebHandler, directory=folder)
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
    print(f"Liefere {folder} auf http://127.0.0.1:{port} (Strg+C beendet)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
