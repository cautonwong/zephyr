# CMakeLists.txt for Simulated Axon driver
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SIM_AXON sim_axon.c)
