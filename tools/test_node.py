#!/usr/bin/env python3
"""CAN integration test for the canhub firmware (run on the Pi with can0 up)."""
import struct
import sys
import time

import can

ID_HEARTBEAT, ID_CMD, ID_ACK = 0x100, 0x200, 0x201
CMD_LED_SET, CMD_LED_TOGGLE, CMD_PING = 0x01, 0x02, 0x03


def wait_for(bus, arb_id, timeout=2.0):
    end = time.time() + timeout
    while time.time() < end:
        msg = bus.recv(timeout=max(0.0, end - time.time()))
        if msg and msg.arbitration_id == arb_id and not msg.is_error_frame:
            return msg
    return None


def decode_hb(msg):
    uptime_ms, counter, state, led = struct.unpack("<IHBB", bytes(msg.data))
    return {"uptime_ms": uptime_ms, "counter": counter, "state": state, "led": led}


def send_cmd(bus, op, arg=0):
    bus.send(can.Message(arbitration_id=ID_CMD, data=[op, arg], is_extended_id=False))
    return wait_for(bus, ID_ACK)


def check(name, ok):
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    return ok


def main():
    results = []
    with can.Bus(channel="can0", interface="socketcan") as bus:
        hb1 = wait_for(bus, ID_HEARTBEAT)
        hb2 = wait_for(bus, ID_HEARTBEAT)
        results.append(check("heartbeat present", hb1 is not None and hb2 is not None))
        if hb1 and hb2:
            a, b = decode_hb(hb1), decode_hb(hb2)
            dt = b["uptime_ms"] - a["uptime_ms"]
            results.append(check(f"heartbeat period ~500 ms (got {dt})", 400 <= dt <= 600))
            results.append(check("counter increments", (b["counter"] - a["counter"]) & 0xFFFF == 1))
            results.append(check("can_state error-active", b["state"] == 0))

        ack = send_cmd(bus, CMD_LED_SET, 1)
        results.append(check("LED set ack", ack is not None and bytes(ack.data) == bytes([CMD_LED_SET, 0])))
        hb = wait_for(bus, ID_HEARTBEAT)
        results.append(check("heartbeat reports LED on", hb is not None and decode_hb(hb)["led"] == 1))

        ack = send_cmd(bus, CMD_LED_TOGGLE)
        results.append(check("toggle ack", ack is not None and bytes(ack.data) == bytes([CMD_LED_TOGGLE, 0])))
        hb = wait_for(bus, ID_HEARTBEAT)
        results.append(check("heartbeat reports LED off", hb is not None and decode_hb(hb)["led"] == 0))

        ack = send_cmd(bus, CMD_PING)
        results.append(check("ping ack", ack is not None and bytes(ack.data) == bytes([CMD_PING, 0])))

        ack = send_cmd(bus, 0x77)
        results.append(check("unknown op rejected (0xFF)", ack is not None and bytes(ack.data) == bytes([0x77, 0xFF])))

    passed = sum(results)
    print(f"\n{passed}/{len(results)} passed")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
