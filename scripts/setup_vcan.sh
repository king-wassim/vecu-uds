#!/usr/bin/env bash
# setup_vcan.sh — prepare a virtual CAN interface for vecu-uds
#
# Usage:   ./scripts/setup_vcan.sh [ifname]
# Default: ifname = vcan0
#
# Requires sudo (modprobe + ip link).
# Idempotent: safe to run multiple times.

set -euo pipefail

IFACE="${1:-vcan0}"

# 1. Load the vcan kernel module if not already loaded.
#    On standard WSL2 Ubuntu 22.04 the Microsoft kernel ships vcan as a module
#    since ~5.15. If modprobe fails, a custom kernel rebuild is required:
#    https://learn.microsoft.com/windows/wsl/wsl-config#wsl-2-settings
if ! lsmod | grep -q '^vcan'; then
    echo ">> Loading vcan kernel module..."
    sudo modprobe vcan
fi

# 2. Create the virtual interface if missing.
if ! ip link show "$IFACE" >/dev/null 2>&1; then
    echo ">> Creating interface $IFACE..."
    sudo ip link add dev "$IFACE" type vcan
fi

# 3. Bring it up if it's down.
if ! ip link show "$IFACE" | grep -q 'state UP'; then
    echo ">> Bringing $IFACE up..."
    sudo ip link set up "$IFACE"
fi

# 4. Show the result.
ip -details link show "$IFACE"
echo "OK: $IFACE ready"
