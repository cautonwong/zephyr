# CMakeLists.txt for ref_motor_pred_maint
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ref_motor_pred_maint)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ../../include)
zephyr_library_link_libraries(control)
