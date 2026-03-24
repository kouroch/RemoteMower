# Remote Mower — App Lab Python
# Rev 1.3 · 2026-03-24 18:57 EDT

import asyncio
import json
import time
import websockets

try:
    from arduino.app_utils import *
    BRIDGE_AVAILABLE = True
except ImportError:
    print("[mower] WARNING: arduino.app_utils not found — Bridge calls will be skipped")
    BRIDGE_AVAILABLE = False
    class Bridge:
        @staticmethod
        def call(*args): pass
    class App:
        @staticmethod
        def run(user_loop=None):
            if user_loop: user_loop()

last_cmd_time = 0

async def handle_client(websocket):
    global last_cmd_time
    print(f"[mower] Client connected")
    try:
        async for message in websocket:
            try:
                cmd = json.loads(message)
                if cmd.get("stop"):
                    print("[mower] STOP")
                    Bridge.call("stop")
                    last_cmd_time = 0
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
    print("[mower] Client disconnected")
    Bridge.call("stop")

async def run_server():
    print("[mower] Starting on port 8765...")
    async with websockets.serve(handle_client, "0.0.0.0", 8765):
        print("[mower] Listening on port 8765")
        await asyncio.Future()  # run forever

import threading

_server_thread = None

def _run_in_thread():
    """Run the WebSocket server in a dedicated thread with its own event loop."""
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(run_server())

def user_loop():
    """Called repeatedly by App Lab — start server thread once, then sleep."""
    global _server_thread
    if _server_thread is None:
        _server_thread = threading.Thread(target=_run_in_thread, daemon=True)
        _server_thread.start()
        print("[mower] Server thread started")
    time.sleep(0.5)

if __name__ == "__main__":
    if BRIDGE_AVAILABLE:
        App.run(user_loop=user_loop)
    else:
        # Running standalone — go direct
        asyncio.run(run_server())
