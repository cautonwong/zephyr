import pytest
import logging

def test_ota_info(dut):
    """Test that 'ota info' displays partition mapping."""
    dut.write("ota info")
    lines = dut.readlines_until(r"MCUboot OTA Partition Mapping", timeout=5.0)
    assert any("MCUboot OTA Partition Mapping" in line for line in lines), \
        "Failed to find OTA info header"

def test_ota_patch_load(dut):
    """Test loading a hex-encoded patch into RAM buffer."""
    # A minimal janpatch control block: [diff_len=0, extra_len=0, seek_offset=0]
    # This is a valid no-op patch
    hex_patch = "000000000000000000000000"
    dut.write(f"ota patch_load {hex_patch}")
    lines = dut.readlines_until(r"Patch loaded", timeout=5.0)
    assert any("Patch loaded" in line for line in lines), \
        "Failed to load patch"

def test_ota_commands_registered(dut):
    """Verify all OTA subcommands are registered."""
    dut.write("ota")
    lines = dut.readlines_until(r"delta_apply", timeout=5.0)
    assert any("delta_apply" in line for line in lines), \
        "delta_apply command not found"
    assert any("patch_load" in line for line in lines), \
        "patch_load command not found"
    assert any("trigger_sim" in line for line in lines), \
        "trigger_sim command not found"

def test_ota_patch_invalid_hex(dut):
    """Test error handling with incomplete hex string."""
    dut.write("ota patch_load 0")
    lines = dut.readlines_until(r"Usage:", timeout=5.0)
    # Should show usage error or fail gracefully
    logging.info("Invalid patch load response received")
