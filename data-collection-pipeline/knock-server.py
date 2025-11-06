# ws_server.py
import time, json, csv, signal, sys
from datetime import datetime
import tornado.ioloop
import tornado.web
import tornado.websocket

CSV_FILE = None
CSV_WRITER = None

def open_csv():
    global CSV_FILE, CSV_WRITER
    fname = f"sensor_log_{datetime.now().strftime('%Y-%m-%d')}.csv"
    CSV_FILE = open(fname, mode="a", newline="", buffering=1)
    CSV_WRITER = csv.writer(CSV_FILE)
    # Write header if file is empty
    if CSV_FILE.tell() == 0:
        CSV_WRITER.writerow(["mic", "imu"])

def close_csv():
    global CSV_FILE
    if CSV_FILE:
        CSV_FILE.flush()
        CSV_FILE.close()
        CSV_FILE = None

class WS(tornado.websocket.WebSocketHandler):
    # Allow LAN connections (disable CORS for dev)
    def check_origin(self, origin): return True

    def open(self):
        print("client connected")

    def on_message(self, message):
        print(f"[MCU] {message}")
        self.write_message(message)  # echo back

        # --- CSV logging ---
        try:
            parts = message.strip().split(",")
            if len(parts) < 2:
                return  # ignore malformed
            mic_str, imu_str = parts[0].strip(), parts[1].strip()
            mic = float(mic_str)
            imu = float(imu_str)
            CSV_WRITER.writerow([mic, imu])
        except Exception as e:
            print(f"[WARN] Failed to log row: {e}")

    def on_close(self):
        print("client disconnected")

def make_app():
    return tornado.web.Application([
        (r"/ws", WS),
    ])

def shutdown(_sig, _frame):
    print("\nShutting down...")
    close_csv()
    tornado.ioloop.IOLoop.current().stop()

if __name__ == "__main__":
    open_csv()

    # Clean shutdown on Ctrl+C / SIGTERM
    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    app = make_app()
    # listen on all interfaces so your ESP32 on the LAN can reach it
    app.listen(8765, address="0.0.0.0")
    print("WebSocket server on ws://<THIS_PC_IP>:8765/ws")
    tornado.ioloop.IOLoop.current().start()
