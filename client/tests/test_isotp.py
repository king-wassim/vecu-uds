"""ISO-TP transport tests — verifies multi-frame works end-to-end."""
from diag_tool import read_owner_raw


def test_isotp_sf_payload_short(clean_state):
    # TesterPresent fits in SF (2-byte request, 2-byte response).
    clean_state.tester_present()


def test_isotp_response_uses_multiframe_for_vin(clean_state):
    # VIN response = 3 header bytes + 17 ASCII = 20 bytes → must be FF+CFs.
    r = clean_state.read_data_by_identifier(0xF190)
    assert len(r.service_data.values[0xF190]) == 17


def test_isotp_multiframe_write_request(clean_state):
    # Write a 20-char owner name → request is multi-frame (3 header + 20 = 23 bytes).
    from udsoncan.services import DiagnosticSessionControl
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    clean_state.unlock_security_access(1)
    payload = "OWNER-XYZ-1234567890"  # 20 chars
    clean_state.write_data_by_identifier(0xF200, payload)
    # Read back via raw to handle variable-length DID.
    assert read_owner_raw(clean_state) == payload
