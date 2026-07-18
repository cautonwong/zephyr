/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_LIB_CONTROL_PID_H_
#define ZEPHYR_LIB_CONTROL_PID_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PID controller instance
 *
 * Tracks integral term and previous error for derivative calculation.
 * Integral anti-windup and output limiting are applied on every update.
 */
struct pid {
	float kp;         /**< Proportional gain */
	float ki;         /**< Integral gain */
	float kd;         /**< Derivative gain */
	float integral;   /**< Accumulated integral term */
	float prev_error; /**< Previous error for derivative calculation */
	float limit_min;  /**< Minimum output / integral limit */
	float limit_max;  /**< Maximum output / integral limit */
};

/**
 * @brief Initialize a PID controller
 *
 * Resets integral and previous error to zero. Sets symmetric limits:
 * @p limit_min = -@p limit_max.
 *
 * @param pid       PID controller instance to initialize
 * @param kp        Proportional gain
 * @param ki        Integral gain
 * @param kd        Derivative gain
 * @param limit_max Positive output / integral limit (limit_min = -limit_max)
 */
void pid_init(struct pid *pid, float kp, float ki, float kd, float limit_max);

/**
 * @brief Update the PID controller and compute the output
 *
 * Performs one time step of the PID algorithm:
 * - error = setpoint - measurement
 * - integral accumulation with anti-windup clamping
 * - derivative on error
 * - output = Kp * error + Ki * integral + Kd * derivative
 * - output clamped to [limit_min, limit_max]
 *
 * @param pid         PID controller instance
 * @param setpoint    Desired reference value
 * @param measurement Current process variable (plant output)
 * @param dt          Time step in seconds (must be > 0)
 *
 * @return Controller output clamped to [limit_min, limit_max]
 */
float pid_update(struct pid *pid, float setpoint, float measurement, float dt);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_LIB_CONTROL_PID_H_ */
