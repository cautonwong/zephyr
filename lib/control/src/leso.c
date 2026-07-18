/*
 * Copyright (c) 2024 EdgeC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <control/leso.h>

void leso_init(struct leso *observer, float omega_o, float b)
{
	observer->z1 = 0.0f;
	observer->z2 = 0.0f;
	observer->l1 = 2.0f * omega_o;
	observer->l2 = omega_o * omega_o;
	observer->b = b;
}

void leso_update(struct leso *observer, float y, float u, float dt)
{
	float error = observer->z1 - y;

	/* Forward Euler integration */
	observer->z1 += (observer->z2 - observer->l1 * error + observer->b * u) * dt;
	observer->z2 += (-observer->l2 * error) * dt;
}
