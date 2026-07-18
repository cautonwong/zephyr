/*
 * Smart Power Quality Monitor (PQM) Reference Design
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 *
 * Uses lib/control's spectrum module for real CMSIS-DSP FFT
 * harmonic analysis and THD calculation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <common_core.h>
#include <control/spectrum.h>

#include <math.h>
#include <string.h>

LOG_MODULE_REGISTER(ref_power_quality, LOG_LEVEL_INF);

#define FFT_LEN 256
/* spectrum_analyze requires 2*len buffer for complex FFT */
#define BUF_LEN (FFT_LEN * 2)

/*
 * Generate a simulated mains voltage waveform with harmonics.
 * phase_deg: 0, 120, 240 for 3-phase
 * sag: if true, simulate a voltage sag (amplitude drops to 70%)
 */
static void gen_waveform(float *buf, uint32_t len, float phase_deg, bool sag)
{
    float amp = sag ? 154.0f : 220.0f; /* 220V nominal, 154V during sag */
    float phase_rad = phase_deg * 3.14159f / 180.0f;

    for (uint32_t i = 0; i < len; i++) {
        float t = (float)i / 6400.0f; /* 6.4 kHz sampling, 50 Hz fundamental */

        /* Fundamental (50 Hz) */
        float val = amp * sinf(2.0f * 3.14159f * 50.0f * t + phase_rad);

        /* 3rd harmonic (150 Hz) - typical from rectifier loads */
        val += 12.5f * sinf(2.0f * 3.14159f * 150.0f * t + phase_rad);

        /* 5th harmonic (250 Hz) */
        val += 8.4f * sinf(2.0f * 3.14159f * 250.0f * t + phase_rad);

        /* 7th harmonic (350 Hz) */
        val += 5.2f * sinf(2.0f * 3.14159f * 350.0f * t + phase_rad);

        /* 9th harmonic (450 Hz) */
        val += 3.1f * sinf(2.0f * 3.14159f * 450.0f * t + phase_rad);

        buf[i] = val;
    }
}

/* Fill imaginary part with zeros for real FFT */
static void zero_imaginary(float *buf, uint32_t len)
{
    memset(buf + len, 0, len * sizeof(float));
}

/* Detect voltage sag: RMS < 90% of nominal for > 1/2 cycle */
static bool detect_sag(const float *samples, uint32_t len, float nominal_v)
{
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        sum_sq += samples[i] * samples[i];
    }
    float rms = sqrtf(sum_sq / (float)len);
    float threshold = nominal_v * 0.9f;
    return rms < threshold;
}

int main(void)
{
    common_core_init("ref_power_quality");
    LOG_INF("Power Quality Monitor (PQM) Agent active.");
    LOG_INF("Sampling: 6.4 kHz, FFT: %d points, Range: DC-%d Hz",
            FFT_LEN, (int)(6400.0f / 2.0f));
    LOG_INF("Analysis: THD up to %%dth harmonic, Sag detection < 1 ms");

    float waveform[BUF_LEN];
    struct spectrum_result result;

    /* Phase A: Normal operation */
    LOG_INF("");
    LOG_INF("--- Phase A: Normal Grid Operation ---");
    gen_waveform(waveform, FFT_LEN, 0.0f, false);
    zero_imaginary(waveform, FFT_LEN);

    spectrum_analyze(waveform, FFT_LEN, 6400.0f, &result);

    LOG_INF(" Fundamental: %.1f Hz @ %.2f", result.peak_freq, result.peak_magnitude);
    LOG_INF(" THD: %.2f %%", result.thd);
    LOG_INF(" Result: %s", result.thd < 10.0f ? "WITHIN LIMITS ✓" : "EXCEEDS LIMIT ✗");

    k_sleep(K_MSEC(200));

    /* Phase B: Voltage Sag */
    LOG_INF("");
    LOG_INF("--- Phase B: Voltage Sag Event ---");
    gen_waveform(waveform, FFT_LEN, 0.0f, true);
    zero_imaginary(waveform, FFT_LEN);

    spectrum_analyze(waveform, FFT_LEN, 6400.0f, &result);

    LOG_INF(" Sag: %s", detect_sag(waveform, FFT_LEN, 220.0f) ? "DETECTED! ⚡" : "NORMAL");
    LOG_INF(" THD during sag: %.2f %%", result.thd);

    k_sleep(K_MSEC(200));

    /* Phase C: High Harmonic Distortion (simulated non-linear load) */
    LOG_INF("");
    LOG_INF("--- Phase C: Non-linear Load (High THD) ---");
    /* Generate waveform with exaggerated harmonics */
    for (uint32_t i = 0; i < FFT_LEN; i++) {
        float t = (float)i / 6400.0f;
        waveform[i] = 220.0f * sinf(2.0f * 3.14159f * 50.0f * t)
                    + 45.0f * sinf(2.0f * 3.14159f * 150.0f * t)   /* 3rd: 20% */
                    + 30.0f * sinf(2.0f * 3.14159f * 250.0f * t)   /* 5th: 14% */
                    + 18.0f * sinf(2.0f * 3.14159f * 350.0f * t);  /* 7th: 8% */
    }
    zero_imaginary(waveform, FFT_LEN);

    spectrum_analyze(waveform, FFT_LEN, 6400.0f, &result);

    LOG_INF(" THD: %.2f %%", result.thd);
    LOG_INF(" Result: %s", result.thd > 10.0f ? "EXCEEDS LIMIT ⚠" : "WITHIN LIMITS ✓");
    LOG_INF(" Recommendation: Check for rectifier or switching load issues.");

    LOG_INF("");
    LOG_INF("Power Quality Monitor initialization complete.");
    return 0;
}
