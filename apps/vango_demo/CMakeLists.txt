cmake_minimum_required(VERSION 3.20.0)
# --- HWMv2 FORCED PATH (SURGICAL FIX) ---
if(NOT DEFINED BOARD_ROOT)
    set(BOARD_ROOT /root/fast_space/modules/soc/v32f20x /workspaces/modules/soc/v32f20x)
endif()
if(NOT DEFINED SOC_ROOT)
    set(SOC_ROOT /root/fast_space/modules/soc/v32f20x /workspaces/modules/soc/v32f20x)
endif()
if(BOARD STREQUAL "v32f20x_board/v32f20x/cpuapp")
    set(BOARD v32f20x_board/v32f20x/cpuapp/cpuapp CACHE STRING "" FORCE)
endif()
# ----------------------------------------

# 1. Target Profile Loading (Building a repeatable preset system)
if(TARGET_NAME)
    set(TARGET_PATH ${CMAKE_CURRENT_LIST_DIR}/targets/${TARGET_NAME})
    if(EXISTS ${TARGET_PATH}/target.cmake)
        message(STATUS "--> Loading Target Profile: ${TARGET_NAME}")
        include(${TARGET_PATH}/target.cmake)
    else()
        message(FATAL_ERROR "Target profile ${TARGET_NAME} not found in targets/ directory.")
    endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vango_demo)

# 2. Main Orchestrator
target_sources(app PRIVATE 
    src/main.c
    src/services/ipc_service.c
    src/services/power_monitor.c
    src/services/storage_init.c
    src/drivers/watchdog.c
)

# 3. Capability-based Service Inclusion
# Sources are linked only if the corresponding Kconfig is enabled in prj.conf
target_sources_ifdef(CONFIG_APP_FEATURE_METERING  app PRIVATE src/services/metering.c)
target_sources_ifdef(CONFIG_APP_FEATURE_COMMS     app PRIVATE src/services/comms.c)
target_sources_ifdef(CONFIG_MODBUS                app PRIVATE src/services/modbus_service.c)
target_sources_ifdef(CONFIG_NETWORKING            app PRIVATE src/services/network_service.c)
target_sources_ifdef(CONFIG_APP_FEATURE_SECURITY  app PRIVATE src/services/security.c)
target_sources_ifdef(CONFIG_APP_FEATURE_LOW_POWER app PRIVATE src/services/low_power.c)
target_sources_ifdef(CONFIG_APP_FEATURE_OTA       app PRIVATE src/services/ota_service.c)

# 4. Global Includes
target_include_directories(app PRIVATE 
    src
    /workspaces/applications/lib/flashdb/src/inc
    /workspaces/applications/lib/flashdb/port
)
