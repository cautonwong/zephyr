
#include <zephyr/kernel.h>
#include <common_core.h>

LOG_MODULE_REGISTER(ref_v2g_charger, LOG_LEVEL_INF);

struct pid_controller {
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
};

float run_pid(struct pid_controller *pid, float setpoint, float feedback)
{
    float error = setpoint - feedback;
    pid->integral += error * 0.01f; /* 10ms sampling */
    float derivative = (error - pid->prev_error) / 0.01f;
    pid->prev_error = error;
    
    return (pid->kp * error) + (pid->ki * pid->integral) + (pid->kd * derivative);
}

int main(void)
{
    common_core_init("ref_v2g_charger");
    
    LOG_INF("V2G Bi-directional Charger Controller initialized.");
    
    struct pid_controller charge_pid = {
        .kp = 0.5f,
        .ki = 0.1f,
        .kd = 0.02f,
        .integral = 0.0f,
        .prev_error = 0.0f
    };
    
    float target_current_a = 32.0f;
    float current_feedback_a = 28.5f;
    
    for (int step = 0; step < 5; step++) {
        float control_output = run_pid(&charge_pid, target_current_a, current_feedback_a);
        LOG_INF("PID Control step %d -> Duty Cycle Adjust: %.4f (Current feedback: %.2f A)", 
                step, control_output, current_feedback_a);
        current_feedback_a += (control_output * 2.0f); /* Simulating system response */
        k_sleep(K_MSEC(100));
    }
    
    return 0;
}
