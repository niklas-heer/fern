#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
import ssl
import sys

port = int(sys.argv[1])
cert_path = sys.argv[2]
key_path = sys.argv[3]


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/health":
            body = b"ok"
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(body)
            return
        body = b"not found"
        self.send_response(404)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        body_len = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(body_len)
        if self.path == "/echo":
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(body)
            return
        not_found = b"not found"
        self.send_response(404)
        self.send_header("Content-Length", str(len(not_found)))
        self.send_header("Connection", "close")
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(not_found)

    def log_message(self, fmt, *args):
        return


def main():
    server = HTTPServer(("127.0.0.1", port), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile=cert_path, keyfile=key_path)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
