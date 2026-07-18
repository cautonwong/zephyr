# CMakeLists.txt for ref_drone_fcc
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ref_drone_fcc)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ../../include)
