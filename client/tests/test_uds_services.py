"""Golden-path tests: every service we implement, happy case."""
import time

import pytest
from udsoncan.services import DiagnosticSessionControl


def test_tester_present_ok(clean_state):
    clean_state.tester_present()


def test_session_change_to_extended(clean_state):
    resp = clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    assert resp.positive


def test_read_vin(clean_state):
    resp = clean_state.read_data_by_identifier(0xF190)
    vin = resp.service_data.values[0xF190]
    assert vin == "VF1234567890ABCDE"


def test_read_serial(clean_state):
    resp = clean_state.read_data_by_identifier(0xF18C)
    assert resp.service_data.values[0xF18C] == "ECU-DEMO-001"


def test_read_rpm_changes_over_time(clean_state):
    samples = []
    for _ in range(6):
        r = clean_state.read_data_by_identifier(0x010C)
        samples.append(bytes(r.service_data.values[0x010C]))
        time.sleep(0.2)
    # The engine simulator oscillates — at least two distinct values across ~1.2s.
    assert len(set(samples)) >= 2, f"engine looks stuck: {samples}"


def test_read_dtc_returns_seeded_codes(clean_state):
    resp = clean_state.get_dtc_by_status_mask(0x09)
    dtcs = {d.id for d in resp.service_data.dtcs}
    assert 0x030100 in dtcs   # P0301
    assert 0x042000 in dtcs   # P0420


def test_clear_dtc_empties_memory(clean_state):
    clean_state.clear_dtc(group=0xFFFFFF)
    resp = clean_state.get_dtc_by_status_mask(0x09)
    assert resp.service_data.dtcs == []


def test_security_access_with_correct_key(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    clean_state.unlock_security_access(1)
    # No exception = success.


def test_write_did_after_unlock(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    clean_state.unlock_security_access(1)
    clean_state.write_data_by_identifier(0xF200, "WASSIM")
    r = clean_state.read_data_by_identifier(0xF200)
    assert r.service_data.values[0xF200] == "WASSIM"


def test_round_trip_write_then_read_did(clean_state):
    clean_state.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
    clean_state.unlock_security_access(1)
    for name in ["A", "AB", "OWNER-XYZ-1234567890"]:
        clean_state.write_data_by_identifier(0xF200, name)
        r = clean_state.read_data_by_identifier(0xF200)
        assert r.service_data.values[0xF200] == name
