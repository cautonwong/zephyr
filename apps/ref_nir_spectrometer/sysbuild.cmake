# sysbuild.cmake for ref_nir_spectrometer
if(NOT SYSBUILD_REF_NIR_SPECTROMETER_INCLUDED)
    set(SYSBUILD_REF_NIR_SPECTROMETER_INCLUDED TRUE)

    # Prevent vango_demo/sysbuild.cmake from defining duplicate images
    set(SYSBUILD_VANGO_DEMO_INCLUDED TRUE)

    # Image 1: MCUboot Bootloader
    set(SB_CONFIG_BOOTLOADER_MCUBOOT y)
    set(mcuboot_BOARD v32f20x_board/v32f20x/cpuapp/cpuapp CACHE STRING "" FORCE)
    set(mcuboot_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)

    # Image 2+3: TF-M + Non-Secure App
    set(ref_nir_spectrometer_BOARD v32f20x_board/v32f20x/cpuapp/cpuapp_ns CACHE STRING "" FORCE)
    set(ref_nir_spectrometer_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)
    set(ref_nir_spectrometer_CONF_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/prj.conf CACHE STRING "" FORCE)
    set(ref_nir_spectrometer_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpuapp_gateway_ns/app.overlay CACHE STRING "" FORCE)
    # Image 4: cpumeter on M0 Coprocessor (shared metering image)
    ExternalZephyrProject_Add(
        APPLICATION cpumeter
        SOURCE_DIR /workspaces/applications/apps/vango_demo
        BOARD v32f20x_board/v32f20x/cpumeter/cpumeter
    )
    set(cpumeter_BOARD_ROOT /workspaces/modules/soc/v32f20x CACHE STRING "" FORCE)
    set(cpumeter_CONF_FILE /workspaces/applications/apps/vango_demo/targets/v32_cpumeter_metering/prj.conf CACHE STRING "" FORCE)
    set(cpumeter_DTC_OVERLAY_FILE /workspaces/applications/apps/vango_demo/targets/v32_cpumeter_metering/app.overlay CACHE STRING "" FORCE)
    
    sysbuild_add_dependencies(CONFIGURE ref_nir_spectrometer cpumeter)
endif()
