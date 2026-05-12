#!/bin/bash
# Professional Workflow Manager (Production Version)

set -e

# --- 1. Real Path Definition ---
WORKSPACE_ROOT="/workspaces"
FAST_SPACE="/root/fast_space"

echo "--> Syncing applications to fast storage..."
mkdir -p "${FAST_SPACE}/applications"
# Sync critical directories for Zephyr build
for dir in apps boards drivers dts include lib zephyr; do
    if [ -d "${WORKSPACE_ROOT}/applications/$dir" ]; then
        rsync -a --delete "${WORKSPACE_ROOT}/applications/$dir" "${FAST_SPACE}/applications/"
    fi
done
# Sync top-level configuration files
cp "${WORKSPACE_ROOT}/applications/Kconfig" "${FAST_SPACE}/applications/" || true
cp "${WORKSPACE_ROOT}/applications/CMakeLists.txt" "${FAST_SPACE}/applications/" || true

export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-1.0.1
export ZEPHYR_BASE="${FAST_SPACE}/rtos/zephyr/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export CCACHE_DIR=/root/.cache/ccache

source "${WORKSPACE_ROOT}/edgeos/EdgeC/.venv/bin/activate"

COMMAND=$1
TARGET_NAME=$2

if [ "$COMMAND" != "target" ]; then
    echo "Usage: $0 target <profile_name>"
    exit 1
fi

APP_PATH="${FAST_SPACE}/applications/apps/vango_demo"
BUILD_DIR="/tmp/vango_build/vango_demo/${TARGET_NAME}"

if [[ "$TARGET_NAME" == "v85"* ]]; then
    SOC_TYPE="v85xxp"
    HAL_MODULE="${FAST_SPACE}/modules/hal/V85XXP_Lib_V2.5"
else
    SOC_TYPE="v32f20x"
    HAL_MODULE="${FAST_SPACE}/modules/hal/V32F20X_StdPeriph_Lib_V1.0.6"
fi

SOC_MODULE="${FAST_SPACE}/modules/soc/${SOC_TYPE}"
CMSIS_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
APP_MODULE="${FAST_SPACE}/applications"
export ZEPHYR_MODULES="${HAL_MODULE};${SOC_MODULE};${CMSIS_MODULE};${APP_MODULE}"

mkdir -p "$BUILD_DIR"
CPUS=$(nproc)

case $COMMAND in
    target)
        echo "--> [TARGET] Profile: $TARGET_NAME"
        # Force DTS_ROOT and SOC_ROOT to ensure discovery
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
              -DZEPHYR_BASE="$ZEPHYR_BASE" \
              -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
              -DSOC_ROOT="$SOC_MODULE" \
              -DBOARD_ROOT="$SOC_MODULE" \
              -DDTS_ROOT="$SOC_MODULE" \
              -DTARGET_NAME="$TARGET_NAME" \
              -DZEPHYR_CCACHE=ON
        
        ninja -C "$BUILD_DIR" -j $CPUS
        ;;
    *)
        usage
        ;;
esac
