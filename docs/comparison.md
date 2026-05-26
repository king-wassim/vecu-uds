# vecu-uds vs. industrial ECU

This project is a learning artefact, not a production ECU. The table below
maps what we did against what a real Bosch / Continental / Vitesco ECU
would have, so the gap is explicit — both for interviewers and for me.

| Aspect              | This project                  | Industrial ECU (typical)                                        |
|---------------------|-------------------------------|-----------------------------------------------------------------|
| OS                  | Linux POSIX (WSL2)            | AUTOSAR Classic (OSEK) or Linux + safety RTOS partition         |
| Bus                 | Linux vcan0                   | CAN HS / CAN-FD / CAN-XL, LIN, FlexRay, Automotive Ethernet     |
| ISO-TP              | Custom (educational) + kernel | Vector CANbedded / EB tresos stack, certified                   |
| UDS service breadth | ~7 services                   | 20+ services, ASPICE / ISO 26262 certified                      |
| Security            | XOR-with-constant demo        | HSM + manufacturer-signed crypto, OEM key management            |
| Diag tool           | Python CLI                    | CANoe / CANalyzer / ODX-based tester                            |
| Validation          | pytest (~20 cases)            | HARA + ISTQB + fuzz + HIL / dyno benches                        |
| Threading           | 4 pthreads, mutex-protected   | OSEK tasks with declared WCET, no dynamic allocation            |
| Memory              | malloc on demand              | Static allocation only, RAM map reviewed in safety case         |
| Bootloader / OTA    | None                          | UDS programming session, signed images, A/B partitions          |

## What this project explicitly does **not** do

- **No WCET guarantee.** We run on Linux best-effort. A real ECU schedules
  tasks with bounded worst-case execution time, often verified by static
  analysis (aiT, RapiTime).
- **No ISO 26262 safety case.** No ASIL classification, no FMEDA, no
  redundancy, no diagnostic coverage targets. This is purely
  formation-grade software.
- **No bootloader / no OTA programming.** The "ECUReset" service does a
  logical reset of the state machine — it doesn't reboot a real CPU
  and chainload an application image.
- **No OBD-II conformance.** We borrow PIDs (0x010C/D, etc.) but don't
  pass SAE J1979 / ISO 15031 compliance tests.
- **No multi-bus routing.** Real diagnostic gateways forward UDS requests
  between buses (e.g. tester on Ethernet → CAN-FD body bus).

## What translates 1:1 to industry

- Socket-level CAN handling (`PF_CAN` / `CAN_RAW` / `CAN_ISOTP`) is the
  same API the kernel-level diag stacks of real Linux gateways use.
- The seed/key state machine, the lock-out timer after 3 fails, the
  session-vs-security gating, the NRC vocabulary — all carry over
  unchanged. Only the crypto algorithm differs.
- pthread producer/consumer with mutex + cond, helgrind discipline — same
  pattern used in any concurrent server, automotive or not.
- The validation discipline (one pytest per NRC) is exactly the
  diagnostic-validation work I did during the KPIT internship.

## "What I would build next" (v2)

1. **Bootloader stub.** Implement service 0x34 (RequestDownload), 0x36
   (TransferData), 0x37 (TransferExit) with a fake "flash" file.
2. **OBD-II compliance.** Replace our demo PIDs with the proper SAE J1979
   subset and pass a conformance suite.
3. **AUTOSAR PoC.** Reimplement the dispatcher on FreeRTOS with a fixed
   pool of message buffers and a measured WCET.
4. **DoIP.** Add a UDS-over-IP front-end so the same dispatcher serves
   both CAN and Ethernet testers.
