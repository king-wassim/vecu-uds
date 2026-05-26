"""Negative paths — every NRC we expect the ECU to produce."""
import pytest
from udsoncan.exceptions import NegativeResponseException
from udsoncan.services import DiagnosticSessionControl


def expect_nrc(call, expected_code):
    with pytest.raises(NegativeResponseException) as ei:
        call()
    assert ei.value.response.code == expected_code, (
        f"expected NRC 0x{expected_code:02X}, got 0x{ei.value.response.code:02X} "
        f"({ei.value.response.code_name})"
    )


def test_unknown_did_returns_nrc_31(clean_state):
    # udsoncan refuses to even send a read for a DID not in the codec config,
    # so we forge the request by hand and parse the negative response.
    clean_state.conn.empty_rxqueue()
    clean_state.conn.send(bytes([0x22, 0xAB, 0xCD]))
    resp = clean_state.conn.wait_frame(timeout=2.0, exception=True)
    # Expected negative response: 0x7F 0x22 0x31
    assert len(resp) == 3, f"unexpected response: {resp.hex()}"
    assert resp[0] == 0x7F and resp[1] == 0x22, f"not a NegResp: {resp.hex()}"
    assert resp[2] == 0x31, f"expected NRC 0x31, got 0x{resp[2]:02X}"


def test_write_did_without_security_returns_nrc_33(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    expect_nrc(lambda: clean_state.write_data_by_identifier(0xF200, "X"), 0x33)


def test_write_did_in_default_session_returns_nrc_7f(clean_state):
    # Default session → WriteDID rejected as not supported in this session.
    expect_nrc(lambda: clean_state.write_data_by_identifier(0xF200, "X"), 0x7F)


def test_security_wrong_key_returns_nrc_35(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    seed_resp = clean_state.request_seed(1)
    # Send a deliberately wrong key.
    expect_nrc(lambda: clean_state.send_key(1, b"\x00\x00\x00\x00"), 0x35)


def test_security_send_key_without_seed_returns_nrc_24(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    expect_nrc(lambda: clean_state.send_key(1, b"\x00\x00\x00\x00"), 0x24)
