import json, subprocess, threading, socket, time, http.server, struct, sys
from pathlib import Path

config = json.loads(Path(__file__).parent.joinpath("config.json").read_text())
HTTP_PORT = config["http_port"]
TCP_BASE = config["tcp_base_port"]
FPS = config.get("fps", 5)
STREAMS = config["streams"]

FRAME_SIZE = 160 * 128 * 2

def to_column_major(data):
    out = bytearray(FRAME_SIZE)
    for y in range(128):
        for x in range(160):
            src = (y * 160 + x) * 2
            dst = (x * 128 + y) * 2
            out[dst]     = data[src + 1]
            out[dst + 1] = data[src]
    return bytes(out)

class StreamState:
    def __init__(self, cfg):
        self.cfg = cfg
        self.proc = None
        self.latest_frame = None
        self.lock = threading.Lock()
        self.ref_count = 0
        self.frame_count = 0

streams = [StreamState(s) for s in STREAMS]

def ffmpeg_cmd(url):
    return [
        "ffmpeg", "-hide_banner",
        "-i", url,
        "-vf", f"scale=160:128:flags=neighbor,fps={FPS}",
        "-sws_flags", "neighbor",
        "-f", "rawvideo",
        "-pix_fmt", "rgb565le",
        "-"
    ]

def start_stream(stream):
    if stream.proc is not None:
        return
    name = stream.cfg["name"]
    print(f"[FFMPEG] Starting: {name}")
    stream.proc = subprocess.Popen(ffmpeg_cmd(stream.cfg["url"]),
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   bufsize=FRAME_SIZE * 4)
    stream.latest_frame = None
    stream.frame_count = 0
    def stderr_reader():
        for line in iter(stream.proc.stderr.readline, b''):
            print(f"[FFMPEG:{name}] {line.decode('utf-8', errors='replace').strip()}")
    threading.Thread(target=stderr_reader, daemon=True).start()
    def reader():
        while stream.proc and stream.proc.poll() is None:
            data = stream.proc.stdout.read(FRAME_SIZE)
            if not data or len(data) < FRAME_SIZE:
                print(f"[READER:{name}] EOF or short read ({len(data) if data else 0} bytes)")
                break
            stream.frame_count += 1
            transposed = to_column_major(data)
            with stream.lock:
                stream.latest_frame = transposed
            if stream.frame_count <= 3 or stream.frame_count % 30 == 0:
                print(f"[READER:{name}] Frame #{stream.frame_count}: {len(data)} bytes "
                      f"first_bytes={data[0]:02X}{data[1]:02X}{data[2]:02X}{data[3]:02X}")
        print(f"[READER:{name}] FFmpeg exited after {stream.frame_count} frames")
        if stream.proc:
            stream.proc.kill()
            stream.proc.wait(timeout=2)
        stream.proc = None
        stream.latest_frame = None
        stream.ref_count = 0
    threading.Thread(target=reader, daemon=True).start()

def stop_stream(stream):
    name = stream.cfg["name"]
    print(f"[FFMPEG] Stopping: {name} (sent {stream.frame_count} frames)")
    if stream.proc:
        stream.proc.kill()
        stream.proc.wait(timeout=3)
        stream.proc = None
    stream.latest_frame = None

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/streams":
            data = [{
                "id": s.cfg["id"],
                "name": s.cfg["name"],
                "port": TCP_BASE + i,
                "online": s.proc is not None
            } for i, s in enumerate(streams)]
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(data, ensure_ascii=False).encode("utf-8"))
        elif self.path == "/api/testframe":
            data = bytearray()
            for y in range(128):
                for x in range(160):
                    r = (x * 31) // 159
                    g = (y * 63) // 127
                    b = 31 - ((x * 31) // 159)
                    pixel = (r << 11) | (g << 5) | b
                    data.append(pixel & 0xFF)
                    data.append((pixel >> 8) & 0xFF)
            data = to_column_major(bytes(data))
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_response(404)
            self.end_headers()
    def log_message(self, *a):
        pass

def tcp_server(idx):
    stream = streams[idx]
    port = TCP_BASE + idx
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", port))
    s.listen(5)
    print(f"  TCP {stream.cfg['name']} -> 0.0.0.0:{port}")
    while True:
        conn, addr = s.accept()
        print(f"  [+] {stream.cfg['name']} client from {addr[0]}")
        stream.ref_count += 1
        if stream.ref_count == 1:
            print(f"  [*] FFmpeg start: {stream.cfg['name']}")
            start_stream(stream)
        def handler(conn, addr, stream):
            sent = 0
            try:
                conn.settimeout(5)
                while True:
                    with stream.lock:
                        frame = stream.latest_frame
                    if frame:
                        pkt = struct.pack(">I", len(frame)) + frame
                        conn.sendall(pkt)
                        sent += 1
                        if sent <= 3 or sent % 30 == 0:
                            print(f"[TCP:{stream.cfg['name']}] Sent frame #{sent} to {addr[0]} ({len(frame)} bytes)")
                    time.sleep(1.0 / FPS * 0.9)
            except Exception as e:
                print(f"[TCP:{stream.cfg['name']}] Client {addr[0]} disconnected: {e}")
            finally:
                conn.close()
                stream.ref_count -= 1
                print(f"[TCP:{stream.cfg['name']}] {addr[0]} left ({stream.ref_count} remain, sent {sent} frames)")
                if stream.ref_count == 0:
                    print(f"  [*] FFmpeg stop: {stream.cfg['name']}")
                    stop_stream(stream)
        threading.Thread(target=handler, args=(conn, addr, stream), daemon=True).start()

if __name__ == "__main__":
    print("Stream Server starting...")
    print(f"  HTTP API: 0.0.0.0:{HTTP_PORT}/api/streams")
    print(f"  FPS: {FPS}")
    print(f"  FRAME SIZE: {FRAME_SIZE} bytes")
    for i in range(len(streams)):
        threading.Thread(target=tcp_server, args=(i,), daemon=True).start()
    httpd = http.server.HTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    httpd.serve_forever()