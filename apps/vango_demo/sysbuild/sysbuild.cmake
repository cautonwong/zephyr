if(NOT SYSBUILD_VANGO_DEMO_INCLUDED)
    set(SYSBUILD_VANGO_DEMO_INCLUDED TRUE)

    # Primary Image (vango_demo) Configuration
    set(vango_demo_BOARD v32f20x_board/v32f20x/cpuapp/cpuapp CACHE STRING "" FORCE)
    set(vango_demo_CONF_FILE ${APP_DIR}/targets/v32_cpuapp_gateway/prj.conf CACHE STRING "" FORCE)
    set(vango_demo_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpuapp_gateway/app.overlay CACHE STRING "" FORCE)

    # Secondary Image (cpumeter) Orchestration
    ExternalZephyrProject_Add(
        APPLICATION cpumeter
        SOURCE_DIR ${APP_DIR}
        BOARD v32f20x_board/v32f20x/cpumeter/cpumeter
    )

    set(cpumeter_CONF_FILE ${APP_DIR}/targets/v32_cpumeter_metering/prj.conf CACHE STRING "" FORCE)
    set(cpumeter_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpumeter_metering/app.overlay CACHE STRING "" FORCE)

    # Coordinate build order
    sysbuild_add_dependencies(CONFIGURE vango_demo cpumeter)
endif()
