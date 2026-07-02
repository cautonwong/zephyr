// Native imports from the host
void log_to_host(const char *msg, int len);
int get_sensor_data(int sensor_id);

int calculate_ratio(int sensor_a, int sensor_b) {
    // In bare-metal C, division by zero halts the CPU with UsageFault.
    // In WASM, the runtime captures it as a trap, allowing host recovery.
    return sensor_a / sensor_b;
}

int main() {
    int val_a = get_sensor_data(10);
    
    const char *start_msg = "WASM Calculator: Running division-by-zero check...";
    log_to_host(start_msg, 50);
    
    // Explicit division by zero to trigger a secure trap.
    int result = calculate_ratio(val_a, 0); 
    
    // This line will NEVER be reached because WASM catches the trap.
    // The main OS thread remains completely safe and running.
    return result;
}
