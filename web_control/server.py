#!/usr/bin/env python3
"""
server.py — Mower WebSocket control server
Runs on UNO Q Linux. Bridges browser WebSocket commands to STM32 via
Arduino_RPClite msgpack RPC over /dev/ttySTM0.

Usage: python3 server.py
"""

import asyncio
import json
import logging
import time
import msgpack
import serial
import websockets

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("mower")

SERIAL_PORT  = "/dev/ttySTM0"
SERIAL_BAUD  = 115200
WS_PORT      = 8765
HEARTBEAT_MS = 600   # send stop if no command for this long

# Shared state
ser: serial.Serial | None = None
last_cmd_time: float = 0.0
connected_clients: set = set()


def open_serial() -> serial.Serial | None:
    try:
        s = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=0.1)
        log.info("Opened serial %s @ %d baud", SERIAL_PORT, SERIAL_BAUD)
        return s
    except serial.SerialException as e:
        log.error("Cannot open serial: %s", e)
        return None


def rpc_notify(method: str, args: list) -> bytes:
    """Build Arduino_RPClite notify frame: [2, method_name, [args]]"""
    return msgpack.packb([2, method, args], use_bin_type=True)


def send_drive(left: int, right: int) -> None:
    global ser
    if ser is None or not ser.is_open:
        ser = open_serial()
    if ser is None:
        log.warning("Serial not available — dropping drive(%d, %d)", left, right)
        return
    try:
        data = rpc_notify("drive", [int(left), int(right)])
        ser.write(data)
    except serial.SerialException as e:
        log.error("Serial write error: %s", e)
        ser = None


def send_stop() -> None:
    global ser
    if ser is None or not ser.is_open:
        ser = open_serial()
    if ser is None:
        log.warning("Serial not available — dropping stop()")
        return
    try:
        data = rpc_notify("stop", [])
        ser.write(data)
    except serial.SerialException as e:
        log.error("Serial write error: %s", e)
        ser = None


async def heartbeat_task() -> None:
    """Send stop to STM32 if no command received within HEARTBEAT_MS."""
    while True:
        await asyncio.sleep(0.1)
        if connected_clients:
            elapsed_ms = (time.monotonic() - last_cmd_time) * 1000
            if elapsed_ms > HEARTBEAT_MS:
                send_stop()


async def handle_client(websocket) -> None:
    global last_cmd_time

    client_addr = websocket.remote_address
    log.info("Client connected: %s", client_addr)
    connected_clients.add(websocket)

    try:
        async for message in websocket:
            try:
                cmd = json.loads(message)
            except json.JSONDecodeError:
                log.warning("Invalid JSON from %s: %r", client_addr, message)
                continue

            if cmd.get("stop"):
                send_stop()
                last_cmd_time = time.monotonic()
                log.debug("stop()")

            elif cmd.get("status"):
                elapsed_ms = (time.monotonic() - last_cmd_time) * 1000
                status = {
                    "serial": ser is not None and ser.is_open,
                    "clients": len(connected_clients),
                    "last_cmd_ms": round(elapsed_ms),
                }
                await websocket.send(json.dumps(status))

            elif "left" in cmd and "right" in cmd:
                left  = max(-255, min(255, int(cmd["left"])))
                right = max(-255, min(255, int(cmd["right"])))
                send_drive(left, right)
                last_cmd_time = time.monotonic()
                log.debug("drive(%d, %d)", left, right)

            else:
                log.warning("Unknown command from %s: %r", client_addr, cmd)

    except websockets.exceptions.ConnectionClosedError:
        pass
    finally:
        connected_clients.discard(websocket)
        log.info("Client disconnected: %s", client_addr)
        if not connected_clients:
            log.info("No clients — sending stop")
            send_stop()


async def main() -> None:
    global ser, last_cmd_time

    ser = open_serial()
    last_cmd_time = time.monotonic()

    asyncio.create_task(heartbeat_task())

    log.info("WebSocket server listening on ws://0.0.0.0:%d", WS_PORT)
    async with websockets.serve(handle_client, "0.0.0.0", WS_PORT):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    asyncio.run(main())
