# Copyright (c) 2024 EdgeC
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(include)

zephyr_library_sources(src/pid.c)
zephyr_library_sources(src/leso.c)

zephyr_library_sources_ifdef(CONFIG_CONTROL_SPECTRUM src/spectrum.c)

if(CONFIG_CONTROL_SPECTRUM)
  zephyr_library_link_libraries(cmsis_dsp)
endif()
