/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_LIB_CONTROL_LESO_H_
#define ZEPHYR_LIB_CONTROL_LESO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Linear Extended State Observer (LESO) instance
 *
 * A second-order LESO used for Active Disturbance Rejection Control (ADRC).
 * Estimates the system state (z1) and total disturbance (z2).
 */
struct leso {
	float z1; /**< Estimated state */
	float z2; /**< Estimated total disturbance */
	float l1; /**< Observer gain 1 */
	float l2; /**< Observer gain 2 */
	float b;  /**< Input gain */
};

/**
 * @brief Initialize a LESO observer
 *
 * Sets observer gains from bandwidth: L1 = 2 * omega_o, L2 = omega_o^2.
 * Initial estimated states (z1, z2) are zeroed.
 *
 * @param observer LESO instance to initialize
 * @param omega_o  Observer bandwidth in rad/s
 * @param b        Input gain (plant parameter)
 */
void leso_init(struct leso *observer, float omega_o, float b);

/**
 * @brief Update the LESO observer for one time step
 *
 * State update equations (forward Euler):
 *   error = z1 - y
 *   z1 += (z2 - L1 * error + b * u) * dt
 *   z2 += (-L2 * error) * dt
 *
 * @param observer LESO instance
 * @param y        Measured plant output
 * @param u        Control input applied to plant
 * @param dt       Time step in seconds (must be > 0)
 */
void leso_update(struct leso *observer, float y, float u, float dt);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_LIB_CONTROL_LESO_H_ */
