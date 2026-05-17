cmake_minimum_required(VERSION 3.20.0)

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
)

# 3. Capability-based Service Inclusion
# Sources are linked only if the corresponding Kconfig is enabled in prj.conf
target_sources_ifdef(CONFIG_APP_FEATURE_METERING  app PRIVATE src/services/metering.c)
target_sources_ifdef(CONFIG_APP_FEATURE_COMMS     app PRIVATE src/services/comms.c)
target_sources_ifdef(CONFIG_MODBUS                app PRIVATE src/services/modbus_service.c)
target_sources_ifdef(CONFIG_APP_FEATURE_SECURITY  app PRIVATE src/services/security.c)
target_sources_ifdef(CONFIG_APP_FEATURE_LOW_POWER app PRIVATE src/services/low_power.c)

# 4. Global Includes
target_include_directories(app PRIVATE src)
