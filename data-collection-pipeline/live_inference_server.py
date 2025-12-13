import os
import signal
import subprocess
import threading
from pathlib import Path

import tornado.ioloop
import tornado.web
import tornado.websocket

# ---------- Paths / globals ----------

# repo root: one level above data-collection-pipeline
ROOT = Path(__file__).resolve().parent.parent
EI_INFER_PATH = ROOT / "model" / "build" / "ei_infer.exe"

infer_proc = None
batch_count = 0

# confidence threshold for declaring a knock
KNOCK_THRESHOLD = 0.8


def start_infer_process():
    """Start the ei_infer.exe process and spawn a reader thread for its stdout."""
    global infer_proc

    if not EI_INFER_PATH.exists():
        print(f"[ERR] ei_infer.exe not found at: {EI_INFER_PATH}")
        return

    print(f"[INF] Starting EI process: {EI_INFER_PATH}")

    # Use binary pipes; we'll encode/decode ourselves (more reliable on Windows)
    infer_proc = subprocess.Popen(
        [str(EI_INFER_PATH)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,      # unbuffered
        text=False      # binary mode
    )

    def reader():
        """Print everything the EI process writes to stdout and detect knocks."""
        for raw in infer_proc.stdout:
            try:
                line = raw.decode("utf-8", errors="ignore").rstrip()
            except AttributeError:
                line = raw.rstrip()
            if not line:
                continue

            # print("[EI]", line)

            # Expect lines like: "PRED idle=0.123 knock=0.877 ..."
            if line.startswith("PRED"):
                parts = line.split()[1:]  # skip "PRED"
                scores = {}
                for p in parts:
                    if "=" not in p:
                        continue
                    label, val = p.split("=", 1)
                    try:
                        scores[label] = float(val)
                    except ValueError:
                        continue

                knock_score = scores.get("knock")
                if knock_score is not None and knock_score >= KNOCK_THRESHOLD:
                    print(">>> KNOCK DETECTED (score = {:.3f})".format(knock_score))

    threading.Thread(target=reader, daemon=True).start()


def send_to_infer(mic, imu):
    global infer_proc

    if infer_proc is None or infer_proc.stdin is None:
        return

    # If the process has already exited, don't keep writing
    if infer_proc.poll() is not None:
        print(f"[WARN] EI process not running (exit code {infer_proc.returncode})")
        return

    try:
        msg = f"{mic},{imu}\n".encode("utf-8")
        infer_proc.stdin.write(msg)
        infer_proc.stdin.flush()
    except OSError as e:
        print(f"[WARN] Failed to write to EI stdin: {e}")


# ---------- Tornado WebSocket server ----------

class WS(tornado.websocket.WebSocketHandler):
    def check_origin(self, origin):
        # allow connections from anywhere on the LAN
        return True

    def open(self):
        print("client connected")

    def on_message(self, message):
        global batch_count

        # message can be bytes or str depending on client
        if isinstance(message, bytes):
            text = message.decode("utf-8", errors="ignore")
        else:
            text = message

        batch_count += 1
        print(f"Data received: Batch {batch_count}")

        # MCU is sending batched text with 1000 lines of "mic,imu"
        for line in text.splitlines():
            line = line.strip()
            if not line:
                continue

            parts = line.split(",")
            if len(parts) < 2:
                continue

            mic_str = parts[0].strip()
            imu_str = parts[1].strip()

            try:
                mic = float(mic_str)
                imu = float(imu_str)
            except ValueError:
                # malformed line, skip
                continue

            # send each sample to the EI process
            send_to_infer(mic, imu)

    def on_close(self):
        print("client disconnected")


def make_app():
    return tornado.web.Application([
        (r"/ws", WS),
    ])


# ---------- Shutdown handling ----------

def shutdown(_sig, _frame):
    global infer_proc
    print("\nShutting down live inference server...")

    if infer_proc is not None:
        try:
            infer_proc.terminate()
        except Exception:
            pass

    tornado.ioloop.IOLoop.current().stop()


if __name__ == "__main__":
    start_infer_process()

    # clean shutdown on Ctrl+C / SIGTERM
    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    app = make_app()
    app.listen(8765, address="0.0.0.0")
    print("Live inference WebSocket server on ws://<THIS_PC_IP>:8765/ws")
    tornado.ioloop.IOLoop.current().start()