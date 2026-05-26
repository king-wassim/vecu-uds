"""ISO 14229 S3 timer behaviour."""
import time

from udsoncan.services import DiagnosticSessionControl


def test_s3_timer_returns_to_default_after_5s(clean_state):
    # Switch to extended, do nothing for >5 s, then verify we are back
    # to default (write would now be rejected with NRC 0x7F instead of 0x33).
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    clean_state.unlock_security_access(1)
    # Quick sanity: write works while unlocked.
    clean_state.write_data_by_identifier(0xF200, "TMP")

    time.sleep(6.0)   # > S3_TIMEOUT_MS (5000ms)

    from udsoncan.exceptions import NegativeResponseException
    try:
        clean_state.write_data_by_identifier(0xF200, "TMP2")
        assert False, "ECU should have reverted to Default Session"
    except NegativeResponseException as e:
        # Either NRC 0x7F (not supported in default session) or 0x33 (re-locked).
        assert e.response.code in (0x33, 0x7F), \
            f"unexpected NRC after S3 timeout: 0x{e.response.code:02X}"
