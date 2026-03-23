# Remote Mower — App Lab Python
# Rev 1.1 · 2026-03-23
#
# WebSocket server bridging browser joystick commands to STM32 via Bridge RPC.
# Browser connects to: ws://<uno-q-ip>:8765

import asyncio
import json
import time
import websockets
from arduino.app_utils import *

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
def user_loop():
    asyncio.run(main())

if __name__ == "__main__":
    App.run(user_loop=user_loop)
