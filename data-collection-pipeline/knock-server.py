# ws_server.py
import time, json, csv, signal, sys
from datetime import datetime
import tornado.ioloop
import tornado.web
import tornado.websocket

CSV_FILE = None
CSV_WRITER = None
global numData, t0
global test_numbers

def open_csv():
    global CSV_FILE, CSV_WRITER, t0
    t0 = time.time()
    fname = f"sensor_log_{datetime.now().strftime('%Y-%m-%d')}.csv"
    CSV_FILE = open(fname, mode="a", newline="", buffering=1)
    CSV_WRITER = csv.writer(CSV_FILE)
    # Write header if file is empty
    if CSV_FILE.tell() == 0:
        CSV_WRITER.writerow(["mic", "imu"])
        CSV_WRITER.writerow(["start time", t0])

def close_csv():
    global CSV_FILE, numData, t0
    tfinal = time.time()
    elapsed = tfinal - t0
    if CSV_FILE:
        CSV_WRITER.writerow(["sampling time", elapsed])
        CSV_WRITER.writerow(["num data", numData])
        frequency = numData / elapsed if elapsed > 0 else 0
        CSV_WRITER.writerow(["sampling frequency", frequency])
        CSV_FILE.flush()
        CSV_FILE.close()
        CSV_FILE = None

class WS(tornado.websocket.WebSocketHandler):
    # Allow LAN connections (disable CORS for dev)
    def check_origin(self, origin): return True

    def open(self):
        print("client connected")

    def on_message(self, message):
        global numData
        global test_numbers

        # # debugging purposes; delete later************
        # val = int(message) if message.isdigit() else -1
        # if val >= 0:
        #     test_numbers.append(val)
        # if len(test_numbers) > 1 and test_numbers[-1] - test_numbers[-2] != 1:
        #     print(f"[WARN] Missing number! {test_numbers[-2]} -> {test_numbers[-1]}")
        # if val % 100 == 0:
        #     print(f"[MCU] {message}")
    
        # # part of original code; stop deleting********
        self.write_message(message) # echo back

        # --- CSV logging ---
        try:
            parts = message.strip().split(",")
            if len(parts) < 2:
                return  # ignore malformed
            mic_str, imu_str = parts[0].strip(), parts[1].strip()
            mic = float(mic_str)
            imu = float(imu_str)
            CSV_WRITER.writerow([mic, imu])
            numData += 1
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
    numData = 0
    test_numbers = []
    
    # Clean shutdown on Ctrl+C / SIGTERM
    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    app = make_app()
    # listen on all interfaces so your ESP32 on the LAN can reach it
    app.listen(8765, address="0.0.0.0")
    print("WebSocket server on ws://<THIS_PC_IP>:8765/ws")
    tornado.ioloop.IOLoop.current().start()
