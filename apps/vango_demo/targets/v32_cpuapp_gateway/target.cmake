# v32_cpuapp_gateway Profile Configuration (SOURCE FIX)

set(BOARD v32f20x_board/v32f20x/cpuapp/cpuapp)
# Use the fast_space path because the compiler runs in /root/fast_space
set(BOARD_ROOT /root/fast_space/modules/soc/v32f20x)
set(SOC_ROOT /root/fast_space/modules/soc/v32f20x)
set(DTS_ROOT /root/fast_space/modules/soc/v32f20x)
set(CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/prj.conf)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/app.overlay)
