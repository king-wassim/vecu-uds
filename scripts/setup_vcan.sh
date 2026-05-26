#!/usr/bin/env bash
set -euo pipefail
IFACE="${1:-vcan0}"

lsmod | grep -q '^vcan' || sudo modprobe vcan
ip link show "$IFACE" >/dev/null 2>&1 || sudo ip link add dev "$IFACE" type vcan
ip link show "$IFACE" | grep -q 'state UP' || sudo ip link set up "$IFACE"

ip -details link show "$IFACE"
