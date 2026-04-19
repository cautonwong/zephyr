#!/bin/bash
# Professional Workflow Manager for V32F20X Workspace

# 1. Environment Setup
export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-1.0.1
export ZEPHYR_BASE=/workspaces/rtos/zephyr/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_MODULES="/workspaces/modules/hal/V32F20X_StdPeriph_Lib_V1.0.6;/workspaces/modules/soc/v32f20x;/workspaces/modules/drivers/ad7616;/workspaces/rtos/zephyr/modules/hal/cmsis_6"
source /workspaces/edgeos/EdgeC/.venv/bin/activate

PROJECT_ROOT=$(pwd)
APP_NAME=$2
BOARD=$3

# Usage helper
usage() {
    echo "Usage: $0 <command> <app> [board]"
    echo "Commands:"
    echo "  build   : Incremental build (default board: v32f20x_board/v32f20x/cpu0)"
    echo "  clean   : Wipe build directory for the app"
    echo "  debug   : Build with debug symbols"
    echo "  release : Build with release optimizations"
    echo "  test    : Run twister/unit tests"
    echo "Example: ./workflow.sh build vango_demo v32f20x_board/v32f20x/cpu0"
    exit 1
}

if [ -z "$1" ] || [ -z "$APP_NAME" ]; then usage; fi

# Default board if not specified
if [ -z "$BOARD" ]; then
    BOARD="v32f20x_board/v32f20x/cpu0"
fi

# Normalize build path
BOARD_SLUG=$(echo $BOARD | tr '/' '_')
BUILD_DIR="${PROJECT_ROOT}/build/${APP_NAME}/${BOARD_SLUG}"
APP_PATH="${PROJECT_ROOT}/apps/${APP_NAME}"

mkdir -p "$BUILD_DIR"

case $1 in
    build)
        echo "--> [BUILD] $APP_NAME for $BOARD"
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" -DBOARD="$BOARD"
        ninja -C "$BUILD_DIR"
        ;;
    clean)
        echo "--> [CLEAN] $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        ;;
    debug)
        echo "--> [DEBUG] $APP_NAME"
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" -DBOARD="$BOARD" -DCMAKE_BUILD_TYPE=Debug
        ninja -C "$BUILD_DIR"
        ;;
    release)
        echo "--> [RELEASE] $APP_NAME"
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" -DBOARD="$BOARD" -DCMAKE_BUILD_TYPE=Release
        ninja -C "$BUILD_DIR"
        ;;
    test)
        echo "--> [TEST] $APP_NAME"
        west twister -p "$BOARD" -T "$APP_PATH"
        ;;
    *)
        usage
        ;;
esac
