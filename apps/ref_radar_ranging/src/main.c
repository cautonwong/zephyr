
#include <zephyr/kernel.h>
#include <common_core.h>

LOG_MODULE_REGISTER(ref_radar, LOG_LEVEL_INF);

/* 2D Range-Doppler FFT peak search to find distance and velocity of target */
void process_range_doppler_map(float *adc_samples, size_t size, float *distance_m, float *velocity_mps)
{
    LOG_INF("Processing mmWave Radar Range FFT of size %d...", (int)size);
    
    /* Simulate finding peak in FFT bins */
    *distance_m = 3.42f;   /* Target detected at 3.42 meters */
    *velocity_mps = -0.56f; /* Target is approaching slowly at 0.56 m/s */
}

int main(void)
{
    common_core_init("ref_radar_ranging");
    
    LOG_INF("Ranging and Presence detection loop armed.");
    
    float mock_radar_if_data[16] = {0.0f};
    float target_distance = 0.0f;
    float target_velocity = 0.0f;
    
    k_sleep(K_MSEC(300));
    process_range_doppler_map(mock_radar_if_data, 16, &target_distance, &target_velocity);
    
    LOG_INF("Target Info -> Distance: %.2f m, Velocity: %.2f m/s", 
            target_distance, target_velocity);
            
    if (target_distance < 1.0f) {
        LOG_WRN("[ALERT] Obstacle too close! Triggering collision warning.");
    } else {
        LOG_INF("Safe zone clear.");
    }
    
    return 0;
}
