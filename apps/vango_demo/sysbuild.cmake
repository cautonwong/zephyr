if(NOT SYSBUILD_VANGO_DEMO_INCLUDED)
    set(SYSBUILD_VANGO_DEMO_INCLUDED TRUE)

    # Enable MCUboot
    set(SB_CONFIG_BOOTLOADER_MCUBOOT y)

    # Force the main app (vango_demo) to use its target-specific config
    set(vango_demo_CONF_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/prj.conf CACHE STRING "" FORCE)
    set(vango_demo_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/app.overlay CACHE STRING "" FORCE)

    # Add cpumeter as an additional image to sysbuild
    ExternalZephyrProject_Add(
        APPLICATION cpumeter
        SOURCE_DIR ${APP_DIR}
        BOARD v32f20x_board/v32f20x/cpumeter/cpumeter
    )

    # Force cpumeter to use its target-specific config (prevent loading root prj.conf)
    set(cpumeter_CONF_FILE ${APP_DIR}/targets/v32_cpumeter_metering/prj.conf CACHE STRING "" FORCE)
    set(cpumeter_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpumeter_metering/app.overlay CACHE STRING "" FORCE)

    # Make cpumeter build after the main app (to ensure dependencies if needed)
    sysbuild_add_dependencies(CONFIGURE vango_demo cpumeter)
endif()
