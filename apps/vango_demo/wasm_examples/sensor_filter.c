// Native imports from the host
void log_to_host(const char *msg, int len);
int get_sensor_data(int sensor_id);

#define TEMP_SENSOR_ID      1
#define HUMIDITY_SENSOR_ID  2

#define ALERT_TEMP_THRESHOLD 45 // Alert at 45 Degrees C
#define FILTER_DEPTH        5

// Helper function to format a simple log message
void send_log(const char *prefix, int val) {
    char buf[64];
    char *p = buf;
    const char *s = prefix;
    while (*s) *p++ = *s++;
    
    int temp = val;
    if (temp < 0) {
        *p++ = '-';
        temp = -temp;
    }
    char digits[10];
    int idx = 0;
    do {
        digits[idx++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    while (idx > 0) {
        *p++ = digits[--idx];
    }
    *p = '\0';
    log_to_host(buf, p - buf);
}

int main() {
    int temp_sum = 0;
    
    // Calculate filtered average temperature
    for (int i = 0; i < FILTER_DEPTH; i++) {
        temp_sum += get_sensor_data(TEMP_SENSOR_ID);
    }
    int avg_temp = temp_sum / FILTER_DEPTH;
    
    send_log("WASM Filter: Calculated average temperature: ", avg_temp);
    
    // Check threshold rules dynamically
    if (avg_temp > ALERT_TEMP_THRESHOLD) {
        send_log("!!! WASM ALARM: Temperature exceeded threshold! Current avg: ", avg_temp);
        return 1; // Signal alarm state
    }
    
    return 0; // Signal normal state
}
