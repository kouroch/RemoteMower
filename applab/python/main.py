# Remote Mower — App Lab Python
# Rev 1.0 · 2026-03-23
#
# WebSocket server that bridges browser joystick commands to the STM32
# via the Arduino Bridge RPC.
#
# Usage (auto-started by App Lab):
#   App.run(user_loop=setup_websocket)
#
# Browser connects to: ws://<uno-q-ip>:8765
# Commands: {"left": 200, "right": 200} or {"stop": true}

import asyncio
import json
import threading
import time
import websockets
from arduino.app_utils import *

# ── State ────────────────────────────────────────────────────────────────────
last_cmd_time = 0
HEARTBEAT_TIMEOUT = 0.6  # seconds — stop motors if no command received

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
                    last_cmd_time = 0  # reset so heartbeat doesn't re-stop
                elif cmd.get("status"):
                    await websocket.send(json.dumps({"status": "ok", "bridge": "connected"}))
                elif "left" in cmd and "right" in cmd:
                    left  = int(max(-255, min(255, cmd["left"])))
                    right = int(max(-255, min(255, cmd["right"])))
                    Bridge.call("drive", left, right)
                    last_cmd_time = time.time()
            except (json.JSONDecodeError, KeyError) as e:
                print(f"[mower] Bad command: {e}")
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print(f"[mower] Client disconnected — stopping motors")
        Bridge.call("stop")

# ── Heartbeat watchdog ───────────────────────────────────────────────────────
async def heartbeat():
    """Stop motors if no drive command received for HEARTBEAT_TIMEOUT seconds."""
    while True:
        await asyncio.sleep(0.2)
        if last_cmd_time > 0 and (time.time() - last_cmd_time) > HEARTBEAT_TIMEOUT:
            Bridge.call("stop")
            # Don't reset last_cmd_time — only a new drive command resets it

# ── WebSocket server ─────────────────────────────────────────────────────────
async def websocket_main():
    print("[mower] WebSocket server starting on port 8765...")
    async with websockets.serve(handle_client, "0.0.0.0", 8765):
        print("[mower] WebSocket server listening on port 8765")
        await asyncio.gather(heartbeat(), asyncio.Future())  # run forever

def run_websocket_server():
    asyncio.run(websocket_main())

# ── App Lab entry point ──────────────────────────────────────────────────────
def setup_websocket():
    """Called once by App.run() as the user_loop."""
    # Start WebSocket server in a background thread
    t = threading.Thread(target=run_websocket_server, daemon=True)
    t.start()
    # Keep the App Lab loop alive
    while True:
        time.sleep(1)

if __name__ == "__main__":
    App.run(user_loop=setup_websocket)
