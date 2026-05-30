import pytest
import logging

def test_shell_algo_status(dut):
    """
    Test that the 'algo' shell command returns the correct status.
    """
    # Wait for the boot banner to ensure the system is up
    # Note: 'dut' is the Device Under Test (the native_sim process)
    
    # Send the 'algo' command
    lines = dut.readlines_until(r"ALGO_READY: OK", timeout=5.0)
    
    # Assert that we found the expected output
    assert any("ALGO_READY: OK" in line for line in lines), "Failed to find ALGO_READY: OK in shell output"
    logging.info("Shell interaction successful: ALGO_READY: OK found.")
