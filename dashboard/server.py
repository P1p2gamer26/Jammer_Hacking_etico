import json
import os
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Falta pyserial. Ejecuta: python -m pip install pyserial")
    raise

SERIAL_PORT = os.environ.get("RF_PORT", "COM3")
BAUD        = 115200
WEB_PORT    = int(os.environ.get("RF_WEB_PORT", "8080"))
DEV_TTL     = float(os.environ.get("RF_DEV_TTL", "15"))
STATIC_DIR  = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

state = {
    "connected": False,
    "port": SERIAL_PORT,
    "mode": "UNKNOWN",
    "devices": {},
    "log": [],
    "counters": {"scan_cycles": 0, "devices_seen": 0},
}
lock = threading.Lock()
ser = None
ser_lock = threading.Lock()


def log(msg):
    ts = time.strftime("%H:%M:%S")
    with lock:
        state["log"].append(f"[{ts}] {msg}")
        state["log"] = state["log"][-200:]


OUI_VENDOR = {
    "24:0A:C4": "Espressif", "30:AE:A4": "Espressif", "3C:71:BF": "Espressif",
    "5C:CF:7F": "Espressif", "7C:9E:BD": "Espressif", "84:CC:A8": "Espressif",
    "A0:20:A6": "Espressif", "AC:67:B2": "Espressif", "B4:E6:2D": "Espressif",
    "C4:4F:33": "Espressif", "CC:50:E3": "Espressif", "D8:A0:1D": "Espressif",
    "EC:FA:BC": "Espressif", "24:6F:28": "Espressif", "48:3F:DA": "Espressif",
    "88:66:A5": "Apple", "9C:04:EB": "Apple", "64:76:BA": "Apple",
    "A4:83:E7": "Apple", "DC:A9:04": "Apple", "F0:18:98": "Apple",
    "D0:81:7A": "Apple", "04:0C:CE": "Apple", "84:FC:FE": "Apple",
    "3C:5A:37": "Samsung", "5C:0A:5B": "Samsung", "84:25:DB": "Samsung",
    "E8:50:8B": "Samsung", "08:37:3D": "Samsung", "28:39:5E": "Samsung",
    "94:EB:2C": "Google", "F4:F5:D8": "Google", "54:60:09": "Google",
    "3C:5A:B4": "Google", "20:DF:B9": "Google", "A4:77:33": "Google",
    "34:02:86": "Intel", "7C:D3:0A": "Intel", "94:65:2D": "Intel",
    "A0:36:9F": "Intel", "54:35:30": "Intel",
    "28:18:78": "Microsoft", "7C:1E:52": "Microsoft", "C8:3F:26": "Microsoft",
    "00:15:5D": "Microsoft",
    "48:62:76": "Huawei", "5C:7D:5E": "Huawei", "78:D7:52": "Huawei",
    "24:DB:AC": "Huawei", "28:3C:E4": "Huawei",
    "04:CF:8C": "Xiaomi", "28:6C:07": "Xiaomi", "34:CE:00": "Xiaomi",
    "50:64:2B": "Xiaomi", "64:B4:73": "Xiaomi", "78:11:DC": "Xiaomi",
    "8C:BE:BE": "Xiaomi",
}

NAME_TYPE = [
    (("speaker", "soundlink", "flip", "charge", "boom", "echo", "alexa",
      "homepod", "sonos", "soundbar", "party", "pill", "edifier",
      "marshall"), "Parlante"),
    (("airpod", "buds", "headphone", "headset", "beats",
      "sony wf", "sony wh", "sennheiser", "soundcore",
      "freebuds", "earbud"), "Audífonos"),
    (("iphone", "galaxy", "pixel", "redmi", "poco", "moto", "xiaomi", "oppo",
      "huawei", "honor", "oneplus", "realme", "phone"), "Celular"),
    (("macbook", "laptop", "thinkpad", "surface", "lenovo", "dell", "asus",
      "acer", "msi", "desktop", "pc-"), "Computadora"),
    (("bravia", "webos", "roku", "chromecast", "firetv"), "TV"),
    (("watch", "band", "gtr", "gts", "amazfit", "garmin", "galaxy fit"), "Reloj"),
]


def classify_device(addr, name):
    info = {"type": "Desconocido", "vendor": "", "source": ""}
    n = (name or "").lower()
    for keys, tipo in NAME_TYPE:
        if any(k in n for k in keys):
            info["type"] = tipo
            info["source"] = "nombre"
            break
    try:
        b0 = int(addr.split(":")[0], 16)
    except (ValueError, IndexError):
        b0 = 0xFF
    random_addr = (b0 & 0xC0) == 0xC0
    if not random_addr:
        v = OUI_VENDOR.get(addr[:8].upper())
        if v:
            info["vendor"] = v
        if not info["source"]:
            info["source"] = "mac"
    if info["type"] == "Desconocido" and info["vendor"] and info["vendor"] != "Espressif":
        info["type"] = info["vendor"]
    if not info["source"]:
        info["source"] = "mac-random" if random_addr else "desconocido"
    return info


def handle_line(line):
    with lock:
        if line.startswith("DEV "):
            parts = line.split(" ", 3)
            if len(parts) >= 3:
                addr = parts[1]
                try:
                    rssi = int(parts[2])
                except ValueError:
                    rssi = 0
                name = parts[3].strip() if len(parts) > 3 else ""
                d = state["devices"].get(addr) or {
                    "addr": addr, "name": "", "rssi": rssi, "history": [],
                }
                if name:
                    d["name"] = name
                d["rssi"] = rssi
                d.update(classify_device(addr, d.get("name", "")))
                d["last_seen"] = time.time()
                d["history"].append(rssi)
                d["history"] = d["history"][-80:]
                state["devices"][addr] = d
                state["counters"]["devices_seen"] = len(state["devices"])
        elif line.startswith("MODE "):
            state["mode"] = line.split(" ", 1)[1].strip()
        elif line == "SCANEND":
            state["counters"]["scan_cycles"] += 1
    if not line.startswith("DEV ") and line != "SCANEND":
        log(line)


def reader_loop():
    global ser
    while True:
        try:
            if ser is None:
                with ser_lock:
                    ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
                with lock:
                    state["connected"] = True
                log(f"Conectado a {SERIAL_PORT} @ {BAUD}")
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", "replace").strip()
            if line:
                handle_line(line)
        except Exception as exc:
            with lock:
                state["connected"] = False
            log(f"Serial: {exc}")
            with ser_lock:
                try:
                    if ser:
                        ser.close()
                except Exception:
                    pass
                ser = None
            time.sleep(2)


def send_cmd(cmd):
    with ser_lock:
        if not ser:
            return False, "no hay conexion serial"
        try:
            ser.write((cmd + "\n").encode("ascii"))
            log(f">> {cmd}")
            return True, "ok"
        except Exception as exc:
            return False, str(exc)


CMD_MAP = {"scan": "SCAN", "stop": "STOP", "jam": "JAM"}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _send(self, code, body, ctype="application/json"):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode("utf-8")
        elif isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(body)
        except BrokenPipeError:
            pass

    def do_GET(self):
        if self.path.split("?")[0] in ("/", "/index.html"):
            path = os.path.join(STATIC_DIR, "index.html")
            if os.path.exists(path):
                with open(path, "rb") as fh:
                    self._send(200, fh.read(), "text/html; charset=utf-8")
            else:
                self._send(404, "index.html no encontrado", "text/plain")
        elif self.path == "/api/state":
            now = time.time()
            with lock:
                expired = [a for a, d in state["devices"].items()
                           if now - d.get("last_seen", now) > DEV_TTL]
                for a in expired:
                    del state["devices"][a]
                devs = []
                for d in state["devices"].values():
                    dd = dict(d)
                    dd["age"] = round(now - d.get("last_seen", now), 1)
                    devs.append(dd)
                devs.sort(key=lambda x: x["rssi"], reverse=True)
                state["counters"]["devices_seen"] = len(state["devices"])
                payload = {
                    "connected": state["connected"],
                    "port": state["port"],
                    "mode": state["mode"],
                    "devices": devs,
                    "log": state["log"][-60:],
                    "counters": state["counters"],
                    "ports": [p.device for p in list_ports.comports()],
                }
            self._send(200, payload)
        else:
            self._send(404, {"error": "no encontrado"})

    def do_POST(self):
        if self.path == "/api/command":
            length = int(self.headers.get("Content-Length", 0) or 0)
            body = self.rfile.read(length) if length else b"{}"
            try:
                data = json.loads(body or b"{}")
            except json.JSONDecodeError:
                self._send(400, {"error": "json invalido"})
                return
            action = str(data.get("cmd", "")).lower()
            serial_cmd = CMD_MAP.get(action)
            if not serial_cmd:
                self._send(400, {"error": f"cmd desconocido: {action}"})
                return
            ok, msg = send_cmd(serial_cmd)
            self._send(200 if ok else 503, {"ok": ok, "sent": serial_cmd, "detail": msg})
        else:
            self._send(404, {"error": "no encontrado"})


def main():
    threading.Thread(target=reader_loop, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", WEB_PORT), Handler)
    log(f"Dashboard en http://localhost:{WEB_PORT}")
    print(f"\n  Dashboard listo ->  http://localhost:{WEB_PORT}")
    print(f"  Puerto serial   ->  {SERIAL_PORT}\n")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
