#!/usr/bin/env python3
"""
diag_tool.py — UDS diagnostic client for the vecu-uds virtual ECU.

Usage:
    ./diag_tool.py read-vin
    ./diag_tool.py read-serial
    ./diag_tool.py read-dtc
    ./diag_tool.py clear-dtc
    ./diag_tool.py unlock --level 1
    ./diag_tool.py write-name "WASSIM"
    ./diag_tool.py monitor

All commands take --iface (default vcan0) and --txid/--rxid (default 0x7E0/0x7E8).
Set DIAG_VERBOSE=1 for udsoncan logs.
"""
from __future__ import annotations

import argparse
import curses
import logging
import os
import struct
import sys
import time
from contextlib import contextmanager

import isotp
import udsoncan
from udsoncan import DataFormatIdentifier, DataIdentifier
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.exceptions import NegativeResponseException, TimeoutException
from udsoncan.services import DiagnosticSessionControl, SecurityAccess

from dtc_names import format_dtc

# ---- DIDs we know about, taught to udsoncan's codec layer. ----
#
# udsoncan.DidCodec's bare constructor expects a struct format STRING
# (e.g. ">H"), not an int byte count, so we use a tiny RawCodec helper
# whenever we just want N raw bytes through.

class RawCodec(udsoncan.DidCodec):
    """Pass-through codec of fixed length N bytes."""
    def __init__(self, n: int):
        self.n = n
    def encode(self, val) -> bytes:
        b = bytes(val)
        if len(b) != self.n:
            raise ValueError(f"expected {self.n} bytes, got {len(b)}")
        return b
    def decode(self, payload: bytes) -> bytes:
        return bytes(payload)
    def __len__(self) -> int:
        return self.n


class VarAsciiCodec(udsoncan.DidCodec):
    """Variable-length ASCII codec, used only for ENCODE (WriteDID 0xF200).
    Reading 0xF200 back is done via raw send/recv (see read_owner_raw) because
    udsoncan's codec layer requires a fixed length for read responses."""
    def __init__(self, max_len: int):
        self.max_len = max_len
    def encode(self, val: str) -> bytes:
        b = val.encode("ascii")
        if len(b) > self.max_len:
            raise ValueError(f"too long ({len(b)} > {self.max_len})")
        return b
    def decode(self, payload: bytes) -> str:
        return payload.decode("ascii", errors="replace")
    def __len__(self) -> int:
        # Best-effort default for codec __len__ contract; the read path
        # for 0xF200 bypasses udsoncan and reads raw bytes.
        return self.max_len


DIDS = {
    0xF190: udsoncan.AsciiCodec(17),   # VIN
    0xF18C: udsoncan.AsciiCodec(12),   # ECU serial
    0xF187: udsoncan.AsciiCodec(8),    # spare part number
    0xF200: VarAsciiCodec(32),         # owner name (encode only, read via raw)
    0x010C: RawCodec(2),               # RPM raw
    0x010D: RawCodec(1),               # Speed km/h
    0x0105: RawCodec(1),               # Coolant raw byte (= °C + 40)
}


def read_owner_raw(cli: Client) -> str:
    """Read DID 0xF200 (variable-length owner name) by hand-crafting the
    UDS request and parsing the response, bypassing udsoncan's codec layer."""
    cli.conn.empty_rxqueue()
    cli.conn.send(bytes([0x22, 0xF2, 0x00]))
    raw = cli.conn.wait_frame(timeout=2.0, exception=True)
    # Expected positive response: 0x62 0xF2 0x00 <name bytes...>
    if len(raw) < 3 or raw[0] != 0x62 or raw[1] != 0xF2 or raw[2] != 0x00:
        raise RuntimeError(f"unexpected raw response: {raw.hex()}")
    return raw[3:].decode("ascii", errors="replace")


# ---- Security algorithm: demo XOR with 0xDEADBEEF ----
def security_algo(level: int, seed: bytes, params=None) -> bytes:
    if len(seed) != 4:
        raise ValueError(f"seed must be 4 bytes, got {len(seed)}")
    s = int.from_bytes(seed, "big")
    k = s ^ 0xDEADBEEF
    return k.to_bytes(4, "big")


def make_config(security_level: int = 1) -> dict:
    cfg = dict(udsoncan.configs.default_client_config)
    cfg["data_identifiers"] = DIDS
    cfg["security_algo"] = security_algo
    cfg["security_algo_params"] = {}
    cfg["request_timeout"] = 2.0
    return cfg


@contextmanager
def open_client(iface: str, rxid: int, txid: int):
    addr = isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=rxid, txid=txid)
    conn = IsoTPSocketConnection(iface, addr)
    conn.tpsock.set_opts(txpad=0x00)
    with Client(conn, config=make_config()) as cli:
        yield cli


# ---- sub-commands ----

def cmd_read_vin(cli: Client, _args) -> int:
    resp = cli.read_data_by_identifier(0xF190)
    print(f"VIN: {resp.service_data.values[0xF190]}")
    return 0


def cmd_read_serial(cli: Client, _args) -> int:
    resp = cli.read_data_by_identifier(0xF18C)
    print(f"Serial: {resp.service_data.values[0xF18C]}")
    return 0


def cmd_read_dtc(cli: Client, _args) -> int:
    # Mask 0x09 = testFailed + confirmedDTC
    resp = cli.get_dtc_by_status_mask(0x09)
    dtcs = resp.service_data.dtcs
    if not dtcs:
        print("No active DTCs.")
        return 0
    print(f"Active DTCs ({len(dtcs)}):")
    for d in dtcs:
        print(format_dtc(d.id, d.status.get_byte_as_int()))
    return 0


def cmd_clear_dtc(cli: Client, _args) -> int:
    cli.clear_dtc(group=0xFFFFFF)
    print("DTCs cleared.")
    return 0


def cmd_unlock(cli: Client, args) -> int:
    cli.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    # Show seed → key for the user
    seed_resp = cli.request_seed(args.level)
    seed = seed_resp.service_data.seed
    key  = security_algo(args.level, bytes(seed))
    seed_i = int.from_bytes(bytes(seed), "big")
    key_i  = int.from_bytes(key, "big")
    print(f"Seed: 0x{seed_i:08X} -> Key: 0x{key_i:08X}")
    cli.send_key(args.level, key)
    print(f"Security unlocked (level {args.level}).")
    return 0


def cmd_write_name(cli: Client, args) -> int:
    cli.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    cli.unlock_security_access(1)
    cli.write_data_by_identifier(0xF200, args.name)
    print(f"Owner name written: {args.name!r}")
    # Read back to confirm — bypass udsoncan codec for variable-length DID.
    confirmed = read_owner_raw(cli)
    print(f"Confirmed: {confirmed!r}")
    return 0


def cmd_monitor(cli: Client, _args) -> int:
    def draw(stdscr):
        curses.curs_set(0)
        stdscr.nodelay(True)
        while True:
            try:
                rpm_raw = bytes(cli.read_data_by_identifier(0x010C).service_data.values[0x010C])
                spd_raw = bytes(cli.read_data_by_identifier(0x010D).service_data.values[0x010D])
                clt_raw = bytes(cli.read_data_by_identifier(0x0105).service_data.values[0x0105])
                rpm = (struct.unpack(">H", rpm_raw)[0]) // 4
                speed = spd_raw[0]
                coolant = clt_raw[0] - 40
            except Exception as e:
                stdscr.addstr(0, 0, f"error: {e}")
                stdscr.refresh()
                time.sleep(0.5)
                continue

            stdscr.erase()
            stdscr.addstr(0, 0, "+----- vECU monitor -----+")
            stdscr.addstr(1, 0, f"| RPM     : {rpm:5d}        |")
            stdscr.addstr(2, 0, f"| Speed   : {speed:3d} km/h    |")
            stdscr.addstr(3, 0, f"| Coolant : {coolant:3d} C       |")
            stdscr.addstr(4, 0, "+------------------------+")
            stdscr.addstr(6, 0, "(q to quit)")
            stdscr.refresh()

            time.sleep(0.2)
            ch = stdscr.getch()
            if ch in (ord("q"), ord("Q"), 27):
                return
    curses.wrapper(draw)
    return 0


# ---- entry point ----

def main(argv=None) -> int:
    if os.environ.get("DIAG_VERBOSE"):
        logging.basicConfig(level=logging.DEBUG)

    p = argparse.ArgumentParser(prog="diag_tool",
                                description="UDS diagnostic client for vecu-uds")
    p.add_argument("--iface", default="vcan0")
    p.add_argument("--txid", type=lambda v: int(v, 0), default=0x7E0,
                   help="CAN ID for tester→ECU (default 0x7E0)")
    p.add_argument("--rxid", type=lambda v: int(v, 0), default=0x7E8,
                   help="CAN ID for ECU→tester (default 0x7E8)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("read-vin")
    sub.add_parser("read-serial")
    sub.add_parser("read-dtc")
    sub.add_parser("clear-dtc")

    p_unlock = sub.add_parser("unlock")
    p_unlock.add_argument("--level", type=int, default=1)

    p_write = sub.add_parser("write-name")
    p_write.add_argument("name")

    sub.add_parser("monitor")

    args = p.parse_args(argv)

    handlers = {
        "read-vin":    cmd_read_vin,
        "read-serial": cmd_read_serial,
        "read-dtc":    cmd_read_dtc,
        "clear-dtc":   cmd_clear_dtc,
        "unlock":      cmd_unlock,
        "write-name":  cmd_write_name,
        "monitor":     cmd_monitor,
    }

    try:
        with open_client(args.iface, args.rxid, args.txid) as cli:
            return handlers[args.cmd](cli, args)
    except NegativeResponseException as e:
        print(f"ECU responded with NRC: {e.response.code_name} (0x{e.response.code:02X})",
              file=sys.stderr)
        return 2
    except TimeoutException as e:
        print(f"Timeout: {e}", file=sys.stderr)
        return 3
    except OSError as e:
        print(f"I/O error ({args.iface}): {e}", file=sys.stderr)
        print("Hint: sudo modprobe can-isotp && ./scripts/setup_vcan.sh", file=sys.stderr)
        return 4


if __name__ == "__main__":
    sys.exit(main())
