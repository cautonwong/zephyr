# Copyright (c) 2024 Vango Technologies
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ADC_AD7616 ad7616.c)
