# Remote Mower — App Lab Python
# Rev 1.1 · 2026-03-23
#
# WebSocket server bridging browser joystick commands to STM32 via Bridge RPC.
# Browser connects to: ws://<uno-q-ip>:8765

import asyncio
import json
import threading
import time
import websockets

try:
    from arduino.app_utils import *
    BRIDGE_AVAILABLE = True
except ImportError:
    # Running standalone (not via App Lab) — Bridge not available
    print("[mower] WARNING: arduino.app_utils not found — Bridge calls will be skipped")
    BRIDGE_AVAILABLE = False
    class _FakeBridge:
        def call(self, *args): pass
    Bridge = _FakeBridge()
    class _FakeApp:
        def run(self, user_loop=None):
            if user_loop: user_loop()
    App = _FakeApp()

# ── State ────────────────────────────────────────────────────────────────────
last_cmd_time = 0
HEARTBEAT_TIMEOUT = 0.6

# ── WebSocket handler ────────────────────────────────────────────────────────
async def handle_client(websocket):
    global last_cmd_time
    print(f"[mower] Client connected: {websocket.remote_address}")
    try:
        async for message in websocket:
            try:
                cmd = json.loads(message)
                if cmd.get("stop"):
                    Bridge.call("stop")
                    last_cmd_time = 0
                elif cmd.get("status"):
                    await websocket.send(json.dumps({"status": "ok"}))
                elif "left" in cmd and "right" in cmd:
                    left  = int(max(-255, min(255, cmd["left"])))
                    right = int(max(-255, min(255, cmd["right"])))
                    print(f"[mower] drive L:{left} R:{right}")
                    Bridge.call("drive", left, right)
                    last_cmd_time = time.time()
            except Exception as e:
                print(f"[mower] Error: {e}")
    except Exception:
        pass
    finally:
        print("[mower] Client disconnected — stopping motors")
        Bridge.call("stop")

# ── Heartbeat ────────────────────────────────────────────────────────────────
async def heartbeat():
    while True:
        await asyncio.sleep(0.2)
        if last_cmd_time > 0 and (time.time() - last_cmd_time) > HEARTBEAT_TIMEOUT:
            Bridge.call("stop")

# ── Main async entry ─────────────────────────────────────────────────────────
async def main():
    print("[mower] Starting WebSocket server on port 8765...")
    server = await websockets.serve(handle_client, "0.0.0.0", 8765)
    print("[mower] Listening on port 8765")
    await asyncio.gather(heartbeat(), server.wait_closed())

# ── App Lab entry point ──────────────────────────────────────────────────────
_server_started = False

def user_loop():
    """Called repeatedly by App Lab — start server once, then just sleep."""
    global _server_started
    if not _server_started:
        _server_started = True
        t = threading.Thread(target=lambda: asyncio.run(main()), daemon=True)
        t.start()
    time.sleep(0.5)

if __name__ == "__main__":
    App.run(user_loop=user_loop)
