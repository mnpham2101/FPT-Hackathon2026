#!/usr/bin/env python3
"""
mock_r4_sender.py — Mock R4 Sender node for 2-node Task 5.1 test blueprint.

Reads config from environment variables (never hardcoded):
  IVI_ECU_HOST   IP of IVI ECU node           (default: 10.99.0.13)
  IVI_ECU_PORT   UDP port on IVI ECU           (default: 47300)
  INTERVAL_MS    Milliseconds between packets  (default: 2000)
  CYCLES         Number of full cycles         (default: 5)
  SCHEMA_VERSION R4 schema version             (default: 1)

Scenario per cycle:
  1. C approaching: 5 packets 40m→20m, warningType=nlos_obstruction
  2. State heartbeat
  3. C leaving:     3 packets 20m→50m
  4. Additive-version test: 1 packet warningType=future_unknown_type
"""

import json
import os
import socket
import sys
import time

IVI_HOST = os.environ.get("IVI_ECU_HOST", "10.99.0.13")
IVI_PORT = int(os.environ.get("IVI_ECU_PORT", "47300"))
INTERVAL_S = int(os.environ.get("INTERVAL_MS", "2000")) / 1000.0
CYCLES = int(os.environ.get("CYCLES", "5"))
SCHEMA_VERSION = int(os.environ.get("SCHEMA_VERSION", "1"))


def make_warning_event(dist_b, dist_c, warning_type, risk):
    return {
        "schemaVersion": SCHEMA_VERSION,
        "type": "warning",
        "warningType": warning_type,
        "riskState": risk,
        "object": {
            "id": "C-001",
            "source": "v2x_relayed",
            "position": {"x": dist_c, "y": 1.5},
            "distance": dist_c,
            "speed": 13.5,
            "confidence": 0.87,
            "state": "tracked"
        },
        "geometry": {
            "ego":      {"x": 0.0,    "y": 0.0},
            "vehicleB": {"x": dist_b, "y": 0.0},
            "vehicleC": {"x": dist_c, "y": 1.5}
        }
    }


def make_state_message(seq):
<<<<<<< HEAD
=======
    # Matches main authority contract: R4Vehicles(ego, vehicleB, vehicleC?)
>>>>>>> origin/feat/phase5-ivi-hmi-complete
    return {
        "schemaVersion": SCHEMA_VERSION,
        "type": "state",
        "seq": seq,
        "vehicles": {
<<<<<<< HEAD
            "ego": {"position": {"x": 0.0,  "y": 0.0}, "speed": 10.0},
            "B":   {"position": {"x": 15.0, "y": 0.0}, "speed": 9.5}
        }
=======
            "ego": {"x": 0.0, "y": 0.0},
            "vehicleB": {"x": 15.0, "y": 0.0},
            "vehicleC": None,
        },
>>>>>>> origin/feat/phase5-ivi-hmi-complete
    }


def send(sock, payload, label):
    data = json.dumps(payload).encode("utf-8")
    sock.sendto(data, (IVI_HOST, IVI_PORT))
    ts = time.strftime("%H:%M:%S")
    preview = json.dumps(payload)[:100]
    print(f"[{ts}] SENT ({label}) {len(data)}B → {IVI_HOST}:{IVI_PORT}")
    print(f"       {preview}...")


def main():
    print(f"=== Mock R4 Sender ===")
    print(f"Target: {IVI_HOST}:{IVI_PORT} | interval={INTERVAL_S*1000:.0f}ms | cycles={CYCLES}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    for cycle in range(1, CYCLES + 1):
        print(f"\n--- Cycle {cycle}/{CYCLES} ---")

        for dist_c in [40.0, 35.0, 30.0, 25.0, 20.0]:
            risk = "low" if dist_c > 30 else ("medium" if dist_c > 25 else "high")
            send(sock, make_warning_event(15.0, dist_c, "nlos_obstruction", risk),
                 f"APPROACH dist_C={dist_c}m risk={risk}")
            time.sleep(INTERVAL_S)

        send(sock, make_state_message(cycle * 10), "STATE heartbeat")
        time.sleep(INTERVAL_S)

        for dist_c in [30.0, 40.0, 50.0]:
            send(sock, make_warning_event(15.0, dist_c, "nlos_obstruction", "low"),
                 f"LEAVING dist_C={dist_c}m")
            time.sleep(INTERVAL_S)

        send(sock, make_warning_event(15.0, 28.0, "future_unknown_type", "medium"),
             "ADDITIVE-VERSION-TEST")
        time.sleep(INTERVAL_S)

    sock.close()
    print(f"\nAll {CYCLES} cycles complete. Exiting cleanly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
