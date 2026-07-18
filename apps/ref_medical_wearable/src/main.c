
#include <zephyr/kernel.h>
#include <common_core.h>

LOG_MODULE_REGISTER(ref_medical, LOG_LEVEL_INF);

/* PPG signal heart rate variance (HRV) analysis stub */
void process_ppg_hrv(uint32_t *rr_intervals_ms, size_t count)
{
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += rr_intervals_ms[i];
    }
    float mean_rr = sum / count;
    
    /* Calculate SDNN (Standard Deviation of NN intervals) */
    float variance_sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = rr_intervals_ms[i] - mean_rr;
        variance_sum += diff * diff;
    }
    float sdnn = sqrtf(variance_sum / count);
    
    LOG_INF("HRV SDNN: %.2f ms (Mean R-R interval: %.2f ms, Beats checked: %d)",
            sdnn, mean_rr, (int)count);
}

int main(void)
{
    common_core_init("ref_medical_wearable");
    
    LOG_INF("PPG Vital Signs Engine running.");
    
    uint32_t mock_rr_intervals[] = {800, 812, 795, 805, 820};
    
    k_sleep(K_MSEC(400));
    process_ppg_hrv(mock_rr_intervals, 5);
    
    return 0;
}
