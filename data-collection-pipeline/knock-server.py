# ws_server.py
import time, json
import tornado.ioloop
import tornado.web
import tornado.websocket

clients = set()

class WS(tornado.websocket.WebSocketHandler):
    # Allow LAN connections (disable CORS for dev)
    def check_origin(self, origin): return True

    def open(self):
        print("client connected")

    def on_message(self, message):
        reply = "handshake received"
        print(f"[MCU] {message}")
        # echo back the received message
        self.write_message(reply)

    def on_close(self):
        print("client disconnected")

def make_app():
    return tornado.web.Application([
        (r"/ws", WS),
    ])

if __name__ == "__main__":
    app = make_app()
    # listen on all interfaces so your ESP32 on the LAN can reach it
    app.listen(8765, address="0.0.0.0")
    print("WebSocket server on ws://<THIS_PC_IP>:8765/ws")
    tornado.ioloop.IOLoop.current().start()