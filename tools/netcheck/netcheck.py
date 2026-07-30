#!/usr/bin/env python3
"""Baseline connectivity check. Env: ROLE, LISTEN_PORT?, NEXT_HOP_HOST?, NEXT_HOP_PORT?, HZ, PAD, START_DELAY_S."""
import os, socket, threading, time

ROLE   = os.environ.get("ROLE", "node")
LISTEN = os.environ.get("LISTEN_PORT")                  # relay/sink nodes only
NH, NP = os.environ.get("NEXT_HOP_HOST"), os.environ.get("NEXT_HOP_PORT")
HZ     = float(os.environ.get("HZ", "1"))               # 1 Hz: log stays readable and live
PAD    = int(os.environ.get("PAD", "0"))                # payload padding, for the MTU check
DELAY  = float(os.environ.get("START_DELAY_S", "20"))   # let the downstream nodes come up first

def log(tag, msg): print(f"[{tag}] {ROLE} {msg}", flush=True)

def route_check():
    """Reachability without ICMP: connect() forces a route lookup and sends nothing."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((NH, int(NP)))
        log("NET", f"route to {NH}:{NP} OK, egress address {s.getsockname()[0]}")
    except OSError as e:
        log("ERR", f"no route to {NH}:{NP} - {e}")
    finally:
        s.close()

def receiver():
    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.bind(("0.0.0.0", int(LISTEN)))     # never bind a named interface - the bridge NIC's name is not guaranteed
    log("NET", f"listening on udp/{LISTEN}")
    fwd, n = socket.socket(socket.AF_INET, socket.SOCK_DGRAM), 0
    while True:
        data, src = rx.recvfrom(65535); n += 1
        log("RX", f"#{n} from {src[0]}:{src[1]} len={len(data)} body={data[:96].decode('ascii','replace')}")
        if NH:                            # relay: stamp, then forward
            out = data + f"|{ROLE}".encode()
            fwd.sendto(out, (NH, int(NP)))
            log("TX", f"#{n} relayed to {NH}:{NP} len={len(out)}")

def sender():
    time.sleep(DELAY)
    tx, i = socket.socket(socket.AF_INET, socket.SOCK_DGRAM), 0
    while True:                           # runs forever, so the log is alive whenever it is opened
        out = f"seq={i}|{ROLE}".encode() + b"x" * PAD
        try:
            tx.sendto(out, (NH, int(NP)))
            log("TX", f"#{i} to {NH}:{NP} len={len(out)}")
        except OSError as e:
            log("ERR", f"send failed - {e}")
        i += 1; time.sleep(1 / HZ)

if NH: route_check()
if LISTEN: threading.Thread(target=receiver, daemon=True).start()
if NH and not LISTEN: sender()            # source node
while True: time.sleep(3600)              # keep the pod Running so View Log stays open
