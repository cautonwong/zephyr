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
        BOARD v32f20x_board/v32f20x/cpumeter
    )

    set(cpumeter_CONF_FILE ${APP_DIR}/targets/v32_cpumeter_metering/prj.conf CACHE STRING "" FORCE)
    set(cpumeter_DTC_OVERLAY_FILE ${APP_DIR}/targets/v32_cpumeter_metering/app.overlay CACHE STRING "" FORCE)

    # --- Industrial Grade Firmware Stitching (M33 + M0 + MCUboot) ---
    # This logic ensures that the M0 core image is included in the final production binary

    set(final_bin ${CMAKE_BINARY_DIR}/factory_v32_full.bin)
    set(mcuboot_bin ${CMAKE_BINARY_DIR}/mcuboot/zephyr/zephyr.bin)
    # TF-M generates the signed binary in the main app's zephyr folder
    set(vango_demo_signed_bin ${CMAKE_BINARY_DIR}/vango_demo/zephyr/zephyr.signed.bin)
    set(cpumeter_bin ${CMAKE_BINARY_DIR}/cpumeter/zephyr/zephyr.bin)

    # Offset definitions (Aligned with flash_layout.h and app.overlay)
    # MCUboot: 0x000000
    # M33 App: 0x010000 (slot0)
    # M0 App:  0x1A0000 (cpumeter_partition)

    add_custom_target(stitching_factory_bin ALL
        COMMAND ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/build/merged_bin.py
                --output ${final_bin}
                ${mcuboot_bin} 0x0
                ${vango_demo_signed_bin} 0x10000
                ${cpumeter_bin} 0x1A0000
        DEPENDS mcuboot vango_demo cpumeter
        COMMENT "老板，正在为您缝合多核全家桶固件: factory_v32_full.bin"
    )

    # --- Cross-Core Debug Artifacts Collection ---
    # Centralizes ELF files for easier debugging with GDB/J-Link
    set(debug_dir ${CMAKE_BINARY_DIR}/debug_artifacts)
    add_custom_target(collect_debug_artifacts ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${debug_dir}
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/vango_demo/zephyr/zephyr.elf ${debug_dir}/main_m33.elf
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/cpumeter/zephyr/zephyr.elf ${debug_dir}/meter_m0.elf
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/mcuboot/zephyr/zephyr.elf ${debug_dir}/bootloader.elf
        DEPENDS vango_demo cpumeter mcuboot
        COMMENT "老板，所有核心的 ELF 已汇聚至: build/.../debug_artifacts/"
    )

    # Coordinate build order
    sysbuild_add_dependencies(CONFIGURE vango_demo cpumeter)
    endif()
