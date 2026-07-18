
#include <zephyr/kernel.h>
#include <common_core.h>
#include <math.h>

LOG_MODULE_REGISTER(ref_bms_eis, LOG_LEVEL_INF);

/* Simulated Electrochemical Impedance Spectroscopy (EIS) algorithm */
void calculate_eis_impedance(float frequency_hz, float amplitude_ma)
{
    LOG_INF("Running EIS measurement at frequency: %.2f Hz, excitation amplitude: %.2f mA", 
            frequency_hz, amplitude_ma);
            
    /* Simulate phase shift theta and voltage amplitude Response */
    float phase_shift_rad = -0.523; /* -30 degrees shift */
    float voltage_amplitude_mv = 25.0;
    
    /* Calculate Real and Imaginary impedance parts: Z = Z_real + j * Z_imag */
    float impedance_magnitude = voltage_amplitude_mv / amplitude_ma; /* Ohms */
    float z_real = impedance_magnitude * cos(phase_shift_rad);
    float z_imag = impedance_magnitude * sin(phase_shift_rad);
    
    LOG_INF("EIS Result for %.2f Hz -> Magnitude: %.4f Ohm, Z_real: %.4f Ohm, Z_imag: %.4f Ohm",
            frequency_hz, impedance_magnitude, z_real, z_imag);
}

int main(void)
{
    common_core_init("ref_bms_eis");
    
    LOG_INF("BMS Electrochemical Impedance Spectroscopy service started.");
    
    float frequencies[] = {1.0, 10.0, 100.0, 1000.0};
    for (size_t i = 0; i < 4; i++) {
        k_sleep(K_MSEC(200));
        calculate_eis_impedance(frequencies[i], 10.0);
    }
    
    return 0;
}
