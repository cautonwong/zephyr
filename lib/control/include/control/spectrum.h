/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_LIB_CONTROL_SPECTRUM_H_
#define ZEPHYR_LIB_CONTROL_SPECTRUM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spectrum analysis result
 */
struct spectrum_result {
	float peak_freq;      /**< Peak frequency in Hz */
	float peak_magnitude; /**< Magnitude at the peak frequency */
	float thd;            /**< Total Harmonic Distortion (%), 0..inf */
};

/**
 * @brief Perform FFT-based spectrum analysis on a real-valued signal
 *
 * Uses CMSIS-DSP to compute the FFT, magnitude spectrum, and extract:
 * - Peak frequency (fundamental)
 * - Peak magnitude
 * - Total Harmonic Distortion (THD) as a percentage
 *
 * @note The @p samples buffer is used as scratch space and will be modified.
 *       It must have at least @p len * 2 elements (for complex FFT workspace).
 * @note @p len must be a power of two.
 *
 * @param samples     Input real-valued samples (len elements);
 *                    caller must allocate at least 2 * len floats
 * @param len         Number of input samples (power of two)
 * @param sample_freq Sampling frequency in Hz
 * @param result      Output structure populated with analysis results
 *
 * @return 0 on success, -1 on invalid parameters
 */
int spectrum_analyze(float *samples, uint32_t len, float sample_freq,
		     struct spectrum_result *result);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_LIB_CONTROL_SPECTRUM_H_ */
