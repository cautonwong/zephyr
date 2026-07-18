/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <control/spectrum.h>
#include <arm_math.h>
#include <math.h>

int spectrum_analyze(float *samples, uint32_t len, float sample_freq,
		     struct spectrum_result *result)
{
	uint32_t i;

	if (samples == NULL || result == NULL) {
		return -1;
	}
	if (len < 2 || (len & (len - 1)) != 0) {
		return -1;
	}

	/* ---------------------------------------------------------------
	 * Convert real input samples into interleaved complex format.
	 * Work backwards to avoid overwriting unread real data:
	 *   samples[2*i]   = samples[i]   (real)
	 *   samples[2*i+1] = 0            (imag)
	 * --------------------------------------------------------------- */
	for (i = len; i > 0; i--) {
		samples[2 * i - 1] = 0.0f;
		samples[2 * i - 2] = samples[i - 1];
	}

	/* ---------------------------------------------------------------
	 * Compute FFT in-place on the complex buffer (len complex samples,
	 * i.e. 2 * len floats).
	 * --------------------------------------------------------------- */
	arm_cfft_instance_f32 S;
	arm_status status = arm_cfft_init_f32(&S, len);
	if (status != ARM_MATH_SUCCESS) {
		return -1;
	}
	arm_cfft_f32(&S, samples, 0, 1);

	/* ---------------------------------------------------------------
	 * Compute magnitude spectrum in-place.
	 * arm_cmplx_mag_f32 reads len complex pairs (buffer size 2*len)
	 * and writes len magnitudes starting at the beginning of the buffer.
	 * --------------------------------------------------------------- */
	arm_cmplx_mag_f32(samples, samples, len);

	/* ---------------------------------------------------------------
	 * Find peak frequency (skip DC bin 0).
	 * Search bins 1 .. len/2 - 1  (positive frequencies up to Nyquist).
	 * --------------------------------------------------------------- */
	float32_t max_val;
	uint32_t max_idx;

	arm_max_f32(&samples[1], (len / 2) - 1, &max_val, &max_idx);
	max_idx += 1; /* compensate for skipping DC */

	result->peak_freq = (float)max_idx * sample_freq / (float)len;
	result->peak_magnitude = (float)max_val;

	/* ---------------------------------------------------------------
	 * Compute Total Harmonic Distortion (THD).
	 * THD = ( sqrt(sum_{k=2..N} |H_k|^2) / |H_1| ) * 100 %
	 * where H_1 is the fundamental and H_k are harmonic magnitudes.
	 * --------------------------------------------------------------- */
	if (max_val > 0.0f) {
		float harmonic_power = 0.0f;

		for (i = 2; max_idx * i < len / 2; i++) {
			float mag = samples[max_idx * i];
			harmonic_power += mag * mag;
		}

		result->thd = sqrtf(harmonic_power) / max_val * 100.0f;
	} else {
		result->thd = 0.0f;
	}

	return 0;
}
