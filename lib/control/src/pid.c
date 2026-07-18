/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <control/pid.h>

void pid_init(struct pid *pid, float kp, float ki, float kd, float limit_max)
{
	pid->kp = kp;
	pid->ki = ki;
	pid->kd = kd;
	pid->integral = 0.0f;
	pid->prev_error = 0.0f;
	pid->limit_max = limit_max;
	pid->limit_min = -limit_max;
}

float pid_update(struct pid *pid, float setpoint, float measurement, float dt)
{
	float error = setpoint - measurement;

	/* Accumulate integral */
	pid->integral += error * dt;

	/* Integral anti-windup clamping */
	if (pid->integral > pid->limit_max) {
		pid->integral = pid->limit_max;
	}
	if (pid->integral < pid->limit_min) {
		pid->integral = pid->limit_min;
	}

	/* Derivative on error */
	float derivative = (error - pid->prev_error) / dt;

	/* PID output */
	float output = (pid->kp * error) +
		       (pid->ki * pid->integral) +
		       (pid->kd * derivative);

	/* Output limiting */
	if (output > pid->limit_max) {
		output = pid->limit_max;
	}
	if (output < pid->limit_min) {
		output = pid->limit_min;
	}

	pid->prev_error = error;
	return output;
}
