// Native imports from the host
void log_to_host(const char *msg, int len);
int get_packet_byte(int index);
int get_packet_len();

// Formats a byte into two hex characters
void byte_to_hex(unsigned char b, char *out) {
    const char hex_chars[] = "0123456789ABCDEF";
    out[0] = hex_chars[(b >> 4) & 0x0F];
    out[1] = hex_chars[b & 0x0F];
}

int main() {
    int len = get_packet_len();
    if (len <= 0 || len > 128) {
        const char *err = "WASM Parser Error: Invalid packet length";
        log_to_host(err, 40);
        return -1;
    }
    
    // Allocate buffer on stack (fully sandboxed and safe)
    char json_buf[512];
    char *p = json_buf;
    
    // Construct simple JSON string output
    const char *header = "{\"raw_payload\":\"";
    while (*header) *p++ = *header++;
    
    for (int i = 0; i < len; i++) {
        unsigned char b = (unsigned char)get_packet_byte(i);
        byte_to_hex(b, p);
        p += 2;
    }
    
    const char *footer = "\",\"status\":\"parsed_by_wasm\"}";
    while (*footer) *p++ = *footer++;
    
    *p = '\0';
    log_to_host(json_buf, p - json_buf);
    
    return 0;
}
