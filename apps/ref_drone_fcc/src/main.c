
#include <zephyr/kernel.h>
#include <common_core.h>

LOG_MODULE_REGISTER(ref_drone_fcc, LOG_LEVEL_INF);

/* Linear Active Disturbance Rejection Control (ADRC) / LESO observer stub */
struct leso_observer {
    float beta1;
    float beta2;
    float z1; /* Estimator for attitude */
    float z2; /* Estimator for total disturbance (wind shear, etc) */
};

void run_leso_update(struct leso_observer *leso, float measured_pitch, float control_u)
{
    float error = measured_pitch - leso->z1;
    leso->z1 += (leso->z2 + leso->beta1 * error + control_u) * 0.005f; /* 5ms loop */
    leso->z2 += (leso->beta2 * error) * 0.005f;
    
    LOG_INF("LESO Estimator -> Pitch: %.3f rad, Disturbance z2 (wind): %.3f rad/s^2",
            leso->z1, leso->z2);
}

int main(void)
{
    common_core_init("ref_drone_fcc");
    
    LOG_INF("Drone FCC flight loops armed.");
    
    struct leso_observer pitch_leso = {
        .beta1 = 120.0f,
        .beta2 = 3600.0f,
        .z1 = 0.0f,
        .z2 = 0.0f
    };
    
    float mock_sensor_pitch = 0.05f; /* 50mrad error */
    float control_signal = 0.1f;
    
    for (int i = 0; i < 5; i++) {
        run_leso_update(&pitch_leso, mock_sensor_pitch, control_signal);
        mock_sensor_pitch += 0.002f; /* Simulating wind shear force */
        k_sleep(K_MSEC(50));
    }
    
    return 0;
}
