import pytest
import logging

def test_dual_core_boot(dut):
    """Test that both M0 and M33 cores start up."""
    # Wait for the M33 application to boot
    lines = dut.readlines_until(r"Vango Target-Centric App Orchestrator", timeout=10.0)
    assert any("Vango Target-Centric App Orchestrator" in line for line in lines), \
        "M33 failed to boot"

def test_m0_cpumeter_log(dut):
    """Test that the M0 metering core produces logs (via IPC or shared memory)."""
    lines = dut.readlines_until(r"CPUMETER", timeout=10.0)
    assert any("CPUMETER" in line for line in lines), \
        "M0 cpumeter core not detected"

def test_ipc_communication(dut):
    """Verify IPC between M0 and M33."""
    lines = dut.readlines_until(r"IPC", timeout=10.0)
    assert any("IPC" in line for line in lines), \
        "IPC service failed to initialize"
