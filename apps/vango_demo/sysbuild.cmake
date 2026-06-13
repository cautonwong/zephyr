if(NOT SYSBUILD_VANGO_DEMO_INCLUDED)
    set(SYSBUILD_VANGO_DEMO_INCLUDED TRUE)

    # Image 1: MCUboot Bootloader (Secure Boot + OTA Firmware Management)
    set(SB_CONFIG_BOOTLOADER_MCUBOOT y)
    set(mcuboot_BOARD v32f20x_board/v32f20x/cpuapp/cpuapp CACHE STRING "" FORCE)
    set(mcuboot_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)

    # Image 2+3: TF-M (auto-added by CONFIG_BUILD_WITH_TFM=y) + vango_demo Non-Secure App
    # M33 Application Core - 4G Gateway, Modbus, Networking, IPC
    set(vango_demo_BOARD v32f20x_board/v32f20x/cpuapp/cpuapp_ns CACHE STRING "" FORCE)
    set(vango_demo_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)
    set(vango_demo_CONF_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/prj.conf CACHE STRING "" FORCE)
    set(vango_demo_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/app.overlay CACHE STRING "" FORCE)

    # Image 4: cpumeter on M0 Coprocessor - ADC Sampling, HFM Energy Metering
    ExternalZephyrProject_Add(
        APPLICATION cpumeter
        SOURCE_DIR ${APP_DIR}
        BOARD v32f20x_board/v32f20x/cpumeter/cpumeter
    )

    set(cpumeter_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)
    set(cpumeter_CONF_FILE ${APP_DIR}/targets/v32_cpumeter_metering/prj.conf CACHE STRING "" FORCE)
    set(cpumeter_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpumeter_metering/app.overlay CACHE STRING "" FORCE)

    sysbuild_add_dependencies(CONFIGURE vango_demo cpumeter)
endif()
