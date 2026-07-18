
#include <zephyr/kernel.h>
#include <common_core.h>

LOG_MODULE_REGISTER(ref_6dof_sensor, LOG_LEVEL_INF);

/* Simple complementary Kalman filter step */
void run_kalman_filter_step(float accel_angle, float gyro_rate, float dt, float *angle)
{
    /* Simple complementary filter mapping */
    float filter_coef = 0.98f;
    *angle = filter_coef * (*angle + gyro_rate * dt) + (1.0f - filter_coef) * accel_angle;
    LOG_INF("Fused Angle -> Pitch: %.4f rad (accel: %.3f, gyro: %.3f)",
            *angle, accel_angle, gyro_rate);
}

int main(void)
{
    common_core_init("ref_6dof_sensor");
    
    LOG_INF("6DoF Sensor Fusion service running.");
    
    float fused_pitch = 0.0f;
    float dt = 0.01f; /* 10ms */
    
    float mock_accel[] = {0.05f, 0.06f, 0.07f, 0.05f, 0.04f};
    float mock_gyro[] = {1.2f, 1.1f, 1.0f, 0.9f, 0.8f};
    
    for (size_t i = 0; i < 5; i++) {
        run_kalman_filter_step(mock_accel[i], mock_gyro[i], dt, &fused_pitch);
        k_sleep(K_MSEC(100));
    }
    
    return 0;
}
