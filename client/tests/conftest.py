"""
Integration test fixtures.

Each test session:
 1. checks that vcan0 + can-isotp are available (skips otherwise),
 2. spawns build/src/vecu_uds as a subprocess,
 3. yields a configured udsoncan Client,
 4. tears down the subprocess on exit.

The same vECU instance is reused across the whole session for speed.
Individual tests must avoid leaving the ECU in a non-default session
(use the `clean_state` fixture which forces a session reset between tests).
"""
from __future__ import annotations

import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

# Allow `import diag_tool` from the parent client/ dir.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import isotp
import udsoncan
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.services import DiagnosticSessionControl

from diag_tool import make_config

REPO_ROOT  = Path(__file__).resolve().parents[2]
ECU_BINARY = REPO_ROOT / "build" / "src" / "vecu_uds"
IFACE      = os.environ.get("VECU_IFACE", "vcan0")
RX_ID      = 0x7E0
TX_ID      = 0x7E8


def _vcan_available() -> bool:
    """True if `IFACE` exists. We don't try to create it from tests —
    the user is expected to have run scripts/setup_vcan.sh."""
    try:
        socket.if_nametoindex(IFACE)
        return True
    except OSError:
        return False


def _isotp_kmod_available() -> bool:
    try:
        s = socket.socket(socket.PF_CAN, socket.SOCK_DGRAM, 6)  # 6 = CAN_ISOTP
        s.close()
        return True
    except OSError:
        return False


@pytest.fixture(scope="session")
def ecu():
    if not ECU_BINARY.exists():
        pytest.skip(f"ECU binary missing: {ECU_BINARY} (run cmake --build build)")
    if not _vcan_available():
        pytest.skip(f"interface {IFACE} not available (run scripts/setup_vcan.sh)")
    if not _isotp_kmod_available():
        pytest.skip("kernel can-isotp module not loaded (sudo modprobe can-isotp)")

    proc = subprocess.Popen(
        [str(ECU_BINARY), IFACE],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    # Give the ECU a beat to bind sockets and start threads.
    time.sleep(0.3)
    if proc.poll() is not None:
        out, err = proc.communicate()
        pytest.fail(f"ECU exited prematurely:\nstdout={out!r}\nstderr={err!r}")

    yield proc

    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()


@pytest.fixture
def client(ecu):
    """Fresh udsoncan Client per test, so failures don't leak state."""
    addr = isotp.Address(isotp.AddressingMode.Normal_11bits, rxid=TX_ID, txid=RX_ID)
    conn = IsoTPSocketConnection(IFACE, addr)
    conn.tpsock.set_opts(txpad=0x00)
    with Client(conn, config=make_config()) as cli:
        yield cli


@pytest.fixture
def clean_state(client):
    """Force the ECU back to Default Session between tests."""
    try:
        client.change_session(DiagnosticSessionControl.Session.defaultSession)
    except Exception:
        pass
    yield client
