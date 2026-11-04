"""UDP relay that records SA-MP traffic so a working client can be diffed.

Usage:
  python relay.py serve  --listen 2646 --target 195.18.27.241:2646 --out cap.jsonl
  python relay.py analyze --file cap.jsonl

The relay accepts a client (the Android SA-MP app pointed at this PC) and
forwards its datagrams to the real server, logging both directions as hex.
`analyze` decrypts the SA-MP layer and prints the ClientJoin RPC found in the
client->server stream.
"""

import argparse
import json
import re
import select
import socket
import sys
import time
from pathlib import Path

TABLE_RE = re.compile(r"0x([0-9A-Fa-f]{2})")
SRC = Path(__file__).resolve().parents[1] / "SampIOS" / "vendor" / "raknet" / "SAMP" / "samp_netencr.cpp"


def load_table():
    text = SRC.read_text(encoding="utf-8", errors="ignore")
    start = text.index("sampEncrTable")
    block = text[start : start + 4000]
    values = [int(m, 16) for m in TABLE_RE.findall(block)][:256]
    if len(values) != 256:
        raise SystemExit(f"encryption table parse failed: {len(values)} bytes")
    return values


def decrypt(table, wire):
    inverse = [0] * 256
    for plain_byte, wire_byte in enumerate(table):
        inverse[wire_byte] = plain_byte
    out = bytearray()
    for b in wire[1:]:  # first byte is the checksum
        out.append(inverse[b])
    return bytes(out)


def serve(args):
    listen = ("0.0.0.0", args.listen)
    target = (args.target_host, args.target_port)
    front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    front.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    front.bind(listen)

    upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    upstream.bind(("0.0.0.0", 0))

    peers = {}   # client addr -> upstream sockaddr
    clients = {}  # upstream sockaddr -> client addr
    sock_list = [front, upstream]
    out = open(args.out, "w", encoding="utf-8")
    print(f"relay {listen} -> {target}, log {args.out}", flush=True)

    deadline = time.time() + args.seconds
    try:
        while time.time() < deadline:
            ready, _, _ = select.select(sock_list, [], [], 0.5)
            for sock in ready:
                data, addr = sock.recvfrom(65535)
                if sock is front:
                    if addr not in peers:
                        # one dedicated upstream port per client
                        up = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        up.bind(("0.0.0.0", 0))
                        sock_list.append(up)
                        peers[addr] = up
                        clients[up] = addr
                        print(f"client {addr} -> upstream {up.getsockname()}", flush=True)
                    peer = peers[addr]
                    peer.sendto(data, target)
                    record(out, "c2s", addr, data)
                else:
                    client = clients.get(sock)
                    if client is None:
                        continue
                    front.sendto(data, client)
                    record(out, "s2c", client, data)
    finally:
        out.close()
        for peer in peers.values():
            peer.close()
        front.close()
        upstream.close()
    print("relay stopped", flush=True)


def record(handle, direction, addr, data):
    handle.write(json.dumps({
        "t": round(time.time(), 3),
        "dir": direction,
        "addr": f"{addr[0]}:{addr[1]}",
        "hex": data.hex(),
    }) + "\n")
    handle.flush()


def analyze(args):
    table = load_table()
    packets = [json.loads(line) for line in Path(args.file).read_text().splitlines() if line.strip()]
    print(f"{len(packets)} datagrams")

    signature = bytes.fromhex("f90f0000 01".replace(" ", ""))
    for packet in packets:
        raw = bytes.fromhex(packet["hex"])
        plain = decrypt(table, raw) if packet["dir"] == "c2s" else raw
        index = plain.find(signature)
        if index >= 0:
            body = plain[index - 2 : index + 120]
            print(f"\n[{packet['dir']}] {packet['addr']} t={packet['t']} offset={index}")
            print("  hex :", body.hex())
            print("  ascii:", "".join(chr(b) if 32 <= b < 127 else "." for b in body))

    # also show the plain (decrypted) head of the first few client packets
    print("\nfirst client datagrams (decrypted, first 48 bytes):")
    shown = 0
    for packet in packets:
        if packet["dir"] != "c2s":
            continue
        plain = decrypt(table, bytes.fromhex(packet["hex"]))
        print(f"  {len(plain):4d} {plain[:48].hex()}")
        shown += 1
        if shown >= 8:
            break


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)

    serve_p = sub.add_parser("serve")
    serve_p.add_argument("--listen", type=int, default=2646)
    serve_p.add_argument("--target", default="195.18.27.241:2646")
    serve_p.add_argument("--out", default="cap.jsonl")
    serve_p.add_argument("--seconds", type=int, default=300)

    analyze_p = sub.add_parser("analyze")
    analyze_p.add_argument("--file", default="cap.jsonl")

    args = parser.parse_args()
    if args.mode == "serve":
        host, _, port = args.target.partition(":")
        args.target_host = host
        args.target_port = int(port or 2646)
        serve(args)
    else:
        analyze(args)


if __name__ == "__main__":
    sys.exit(main())
