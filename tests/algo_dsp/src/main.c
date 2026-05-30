/*
 * Zephyr Advanced Testing & DSP Implementation
 * Features: CMSIS-DSP FFT, PID, ADRC (LESO), Ztest, FFF Mocking
 */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/fff.h>
#include <zephyr/sys/printk.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include <math.h>

/* --- FFF Globals & Mocks --- */
DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(int, hw_adc_read_raw, uint8_t, uint16_t*);

/* ========================================================================= */
/* --- MILESTONE 3: Advanced Control Implementation (ADRC / PID) --- */
/* ========================================================================= */

typedef struct {
    float kp, ki, kd;
    float setpoint;
    float integral;
    float prev_error;
    float limit_min, limit_max;
} pid_controller_t;

float pid_compute(pid_controller_t *pid, float measurement, float dt) {
    float error = pid->setpoint - measurement;
    pid->integral += error * dt;
    
    /* Anti-windup */
    if (pid->integral > pid->limit_max) pid->integral = pid->limit_max;
    if (pid->integral < pid->limit_min) pid->integral = pid->limit_min;

    float derivative = (error - pid->prev_error) / dt;
    float output = (pid->kp * error) + (pid->ki * pid->integral) + (pid->kd * derivative);
    
    /* Output Limiting */
    if (output > pid->limit_max) output = pid->limit_max;
    if (output < pid->limit_min) output = pid->limit_min;

    pid->prev_error = error;
    return output;
}

/* Linear Extended State Observer (LESO) for ADRC */
typedef struct {
    float z1, z2; /* Estimated state and total disturbance */
    float l1, l2; /* Observer gains */
    float b;      /* Input gain */
} leso_t;

void leso_update(leso_t *observer, float y, float u, float dt) {
    float error = observer->z1 - y;
    observer->z1 += (observer->z2 - observer->l1 * error + observer->b * u) * dt;
    observer->z2 += (-observer->l2 * error) * dt;
}

/* ========================================================================= */
/* --- MILESTONE 2: CMSIS-DSP FFT Logic --- */
/* ========================================================================= */

#define FFT_SIZE 1024
static float32_t test_input[FFT_SIZE * 2]; /* Complex: real, imag, real, imag... */
static float32_t test_output[FFT_SIZE];

void generate_sine_wave(float32_t freq, float32_t sampling_rate) {
    for (int i = 0; i < FFT_SIZE; i++) {
        test_input[2 * i] = sinf(2 * PI * freq * i / sampling_rate); /* Real */
        test_input[2 * i + 1] = 0; /* Imag */
    }
}

/* ========================================================================= */
/* --- ZTEST SUITE --- */
/* ========================================================================= */

static void *algo_suite_setup(void) {
    return NULL;
}

static void algo_test_before(void *fixture) {
    FFF_RESET_HISTORY();
    RESET_FAKE(hw_adc_read_raw);
}

/* Test 1: Mocking Hardware (Milestone 1/Write Layer) */
ZTEST(algo_dsp_suite, test_adc_mocking) {
    hw_adc_read_raw_fake.return_val = 0;
    int custom_read(uint8_t ch, uint16_t *val) { *val = 2047; return 0; }
    hw_adc_read_raw_fake.custom_fake = custom_read;

    uint16_t val;
    hw_adc_read_raw(0, &val);
    zassert_equal(val, 2047, "Mock should return 2047");
}

/* Test 2: CMSIS-DSP FFT Accuracy (Milestone 2) */
ZTEST(algo_dsp_suite, test_fft_logic) {
    float32_t sampling_rate = 1000.0f;
    float32_t target_freq = 50.0f;
    
    generate_sine_wave(target_freq, sampling_rate);

    arm_cfft_instance_f32 S;
    arm_status status = arm_cfft_init_f32(&S, FFT_SIZE);
    zassert_equal(status, ARM_MATH_SUCCESS, "FFT Init Failed");

    arm_cfft_f32(&S, test_input, 0, 1);
    arm_cmplx_mag_f32(test_input, test_output, FFT_SIZE);

    /* Find Peak */
    float32_t max_val;
    uint32_t max_idx;
    arm_max_f32(test_output, FFT_SIZE / 2, &max_val, &max_idx);

    float32_t detected_freq = (float32_t)max_idx * sampling_rate / FFT_SIZE;
    printk("Detected Peak Freq: %f Hz\n", (double)detected_freq);
    
    zassert_within(detected_freq, target_freq, 1.0f, "FFT should detect 50Hz peak");
}

#include <zephyr/shell/shell.h>

/* ... existing code ... */

/* Test 3: ADRC/PID Control Loop convergence (Milestone 3) */
ZTEST(algo_dsp_suite, test_control_convergence) {
    pid_controller_t pid = {
        .kp = 10.0f, .ki = 2.0f, .kd = 0.1f,
        .setpoint = 100.0f,
        .limit_min = -1000.0f, .limit_max = 1000.0f
    };

    float process_variable = 0.0f;
    float dt = 0.01f;
    int steps = 2000;

    for (int i = 0; i < steps; i++) {
        float u = pid_compute(&pid, process_variable, dt);
        /* Simple 1st order motor model: y' = u - y */
        process_variable += (u - process_variable) * dt;
    }

    printk("Final PV: %f\n", (double)process_variable);
    zassert_within(process_variable, 100.0f, 1.0f, "PID should converge to setpoint");
}

static int cmd_algo_status(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "ALGO_READY: OK");
    return 0;
}

SHELL_CMD_REGISTER(algo, NULL, "Algorithm commands", cmd_algo_status);

ZTEST_SUITE(algo_dsp_suite, NULL, algo_suite_setup, algo_test_before, NULL, NULL);

