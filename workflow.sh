#!/bin/bash
# Professional Workflow Manager for Vango Multi-SoC Workspace

# 1. Environment Configuration
export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-1.0.1
export ZEPHYR_BASE=/workspaces/rtos/zephyr/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

# [PERFORMANCE FIX] Boost ccache hit rate and performance
# Redirecting cache to container's native filesystem and allowing some sloppiness 
# for time-macros which are common in firmware.
export CCACHE_DIR=/root/.cache/ccache
export CCACHE_COMPRESS=1
export CCACHE_SLOPPINESS=include_file_mtime,time_macros
export CCACHE_BASEDIR=$PWD

# Activate Python Virtual Environment
source /workspaces/edgeos/EdgeC/.venv/bin/activate

PROJECT_ROOT=$(pwd)
COMMAND=$1
APP_NAME=$2
BOARD=$3

# Usage helper
usage() {
    echo "Usage: $0 <command> <app> [board]"
    echo ""
    echo "Example: ./workflow.sh build vango_demo v32f20x_board/v32f20x/cpu0"
    exit 1
}

if [ -z "$COMMAND" ] || [ -z "$APP_NAME" ]; then usage; fi

# 2. Extract Board and SOC Info
# Default board if not specified
if [ -z "$BOARD" ]; then
    BOARD="v32f20x_board/v32f20x/cpu0"
fi

# Clean board name for file matching (e.g., v32f20x_board)
BOARD_NAME=$(echo $BOARD | cut -d'/' -f1)

if [[ "$BOARD" == *"v85xxp"* ]]; then
    SOC_TYPE="v85xxp"
    HAL_MODULE="/workspaces/modules/hal/V85XXP_Lib_V2.5"
elif [[ "$BOARD" == *"v32f20x"* ]]; then
    SOC_TYPE="v32f20x"
    HAL_MODULE="/workspaces/modules/hal/V32F20X_StdPeriph_Lib_V1.0.6"
else
    echo "Error: Unknown SoC series in board name '$BOARD'"
    exit 1
fi

export ZEPHYR_MODULES="${HAL_MODULE};/workspaces/modules/soc/${SOC_TYPE};/workspaces/rtos/zephyr/modules/hal/cmsis_6"

# 3. Path Normalization
BOARD_SLUG=$(echo $BOARD | tr '/' '_')

# [PERFORMANCE FIX] Option C: Redirect build directory to container's native filesystem.
# Windows mounted directories (/workspaces) have severe I/O performance penalties for 
# large numbers of small files (like .obj files). Building in /tmp is ~10x faster.
BUILD_DIR="/tmp/vango_build/${APP_NAME}/${BOARD_SLUG}"
APP_PATH="${PROJECT_ROOT}/apps/${APP_NAME}"

# 4. Multi-level Overlay Logic (Board-Specific -> SoC-Specific)
OVERLAY_FILE=""
if [ -f "${APP_PATH}/${BOARD_NAME}.overlay" ]; then
    OVERLAY_FILE="${APP_PATH}/${BOARD_NAME}.overlay"
    echo "--> [INFO] Using Board-Specific Overlay: ${BOARD_NAME}.overlay"
elif [ -f "${APP_PATH}/${SOC_TYPE}.overlay" ]; then
    OVERLAY_FILE="${APP_PATH}/${SOC_TYPE}.overlay"
    echo "--> [INFO] Using SoC-Specific Overlay: ${SOC_TYPE}.overlay"
fi

OVERLAY_ARG=""
if [ -n "$OVERLAY_FILE" ]; then
    OVERLAY_ARG="-DDTC_OVERLAY_FILE=${OVERLAY_FILE}"
fi

mkdir -p "$BUILD_DIR"

case $COMMAND in
    build|debug|release)
        BUILD_TYPE="MinSizeRel"
        [[ "$COMMAND" == "debug" ]] && BUILD_TYPE="Debug"
        [[ "$COMMAND" == "release" ]] && BUILD_TYPE="Release"

        # [PERFORMANCE FIX] Explicitly enable ccache in CMake and optimize Ninja parallel jobs
        CPUS=$(nproc)
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
              -DBOARD="$BOARD" \
              -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
              -DZEPHYR_CCACHE=ON \
              $OVERLAY_ARG
        
        # Use all available cores for compilation
        ninja -C "$BUILD_DIR" -j $CPUS
        ;;
    clean)
        echo "--> [CLEAN] $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        ;;
    *)
        usage
        ;;
esac
