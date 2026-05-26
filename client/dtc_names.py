"""Tiny SAE J2012 lookup table — only the codes we actually seed/test."""

DTC_NAMES = {
    0x030100: "P0301  Cylinder 1 Misfire",
    0x042000: "P0420  Catalyst System Efficiency Below Threshold (Bank 1)",
    0x017100: "P0171  System Too Lean (Bank 1)",
    0x012800: "P0128  Coolant Thermostat Below Regulating Temperature",
    0x070100: "P0701  Transmission Control System Range/Performance",
}


def status_flags(status: int) -> str:
    flags = []
    if status & 0x01: flags.append("testFailed")
    if status & 0x02: flags.append("testFailedThisCycle")
    if status & 0x04: flags.append("pending")
    if status & 0x08: flags.append("confirmed")
    if status & 0x20: flags.append("testFailedSinceClear")
    if status & 0x80: flags.append("warningIndicator")
    return ", ".join(flags) if flags else "—"


def format_dtc(dtc: int, status: int) -> str:
    name = DTC_NAMES.get(dtc, f"0x{dtc:06X}  (unknown)")
    return f"  {name}   [{status_flags(status)}]"
