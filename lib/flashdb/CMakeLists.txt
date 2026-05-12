# FlashDB CMakeLists.txt

zephyr_library()

zephyr_include_directories(src/inc)
zephyr_include_directories(port)

zephyr_library_sources(
    src/src/fdb.c
    src/src/fdb_kvdb.c
    src/src/fdb_tsdb.c
    src/src/fdb_utils.c
    port/fdb_port.c
)
