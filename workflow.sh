#!/bin/bash
# Professional Workflow Manager for Vango Multi-SoC Workspace

# 1. Environment Configuration
export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-1.0.1
export ZEPHYR_BASE=/workspaces/rtos/zephyr/zephyr
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

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
BUILD_DIR="${PROJECT_ROOT}/build/${APP_NAME}/${BOARD_SLUG}"
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

        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
              -DBOARD="$BOARD" \
              -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
              $OVERLAY_ARG
        
        ninja -C "$BUILD_DIR"
        ;;
    clean)
        echo "--> [CLEAN] $BUILD_DIR"
        rm -rf "$BUILD_DIR"
        ;;
    *)
        usage
        ;;
esac
