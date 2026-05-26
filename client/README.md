# client/ — Python diagnostic tool

`diag_tool.py` speaks UDS to the vecu-uds ECU over kernel `can-isotp`.

## Install

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Make sure the kernel modules are loaded and `vcan0` exists:

```bash
sudo modprobe vcan can-isotp
../scripts/setup_vcan.sh
```

Start the ECU in another terminal:

```bash
../build/src/vecu_uds vcan0
```

## Commands

| Command                  | What it does                                      |
|--------------------------|---------------------------------------------------|
| `read-vin`               | Reads DID 0xF190 (VIN, 17 ASCII)                  |
| `read-serial`            | Reads DID 0xF18C (ECU serial)                     |
| `read-dtc`               | ReadDTCInformation sub 0x02 mask 0x09, formats   |
| `clear-dtc`              | ClearDiagnosticInformation, group 0xFFFFFF        |
| `unlock --level 1`       | SecurityAccess seed/key (XOR with 0xDEADBEEF)     |
| `write-name "WASSIM"`    | Extended session + unlock + WriteDID 0xF200       |
| `monitor`                | curses dashboard (RPM, speed, coolant)            |

All commands take `--iface VCAN`, `--txid HEX`, `--rxid HEX` overrides.

Set `DIAG_VERBOSE=1` to dump udsoncan internals.
