#!/bin/bash
# Professional Workflow Manager (Production Version)

set -e

# --- 1. Real Path Definition ---
WORKSPACE_ROOT="/workspaces"
FAST_SPACE="/root/fast_space"

echo "--> Syncing applications to fast storage..."
mkdir -p "${FAST_SPACE}/applications"
ln -snf "${FAST_SPACE}/applications" "${FAST_SPACE}/rtos/zephyr/applications"
# Sync critical directories for Zephyr build
for dir in apps boards drivers dts include lib zephyr tests; do
    if [ -d "${WORKSPACE_ROOT}/applications/$dir" ]; then
        rsync -a --delete "${WORKSPACE_ROOT}/applications/$dir" "${FAST_SPACE}/applications/"
    fi
done
# Sync top-level configuration files
cp "${WORKSPACE_ROOT}/applications/Kconfig" "${FAST_SPACE}/applications/" || true
cp "${WORKSPACE_ROOT}/applications/CMakeLists.txt" "${FAST_SPACE}/applications/" || true
cp "${WORKSPACE_ROOT}/applications/west.yml" "${FAST_SPACE}/applications/" || true

export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-1.0.1
export ZEPHYR_BASE="${FAST_SPACE}/rtos/zephyr/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export CCACHE_DIR=/root/.cache/ccache

source "${WORKSPACE_ROOT}/edgeos/EdgeC/.venv/bin/activate"

COMMAND=$1
APP_NAME=$2
TARGET_NAME=$3

usage() {
    echo "Usage: $0 <command> <app_name> [target_profile]"
    echo ""
    echo "Commands:"
    echo "  build <app_name> <target> : Build for physical hardware"
    echo "  sim   <app_name>          : Build and run native Linux simulation"
    echo "  test  <app_name>          : Run Twister tests for the app"
    echo "  clean <app_name>          : Clean build directories"
    exit 1
}

if [ -z "$COMMAND" ] || [ -z "$APP_NAME" ]; then
    usage
fi

APP_PATH="${FAST_SPACE}/applications/apps/${APP_NAME}"
if [ ! -d "$APP_PATH" ]; then
    echo "Error: Application directory not found at $APP_PATH"
    exit 1
fi

CPUS=$(nproc)

case $COMMAND in
    build|target)
        if [ -z "$TARGET_NAME" ]; then
            echo "Error: Missing target_profile for build command."
            usage
        fi
        
        BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${APP_NAME}/${TARGET_NAME}"

        if [[ "$TARGET_NAME" == "v85"* ]]; then
            SOC_TYPE="v85xxp"
            HAL_MODULE="${FAST_SPACE}/modules/hal/V85XXP_Lib_V2.5"
        else
            SOC_TYPE="v32f20x"
            HAL_MODULE="${FAST_SPACE}/modules/hal/V32F20X_StdPeriph_Lib_V1.0.6"
        fi

        SOC_MODULE="${FAST_SPACE}/modules/soc/${SOC_TYPE}"
        CMSIS_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
        SEGGER_MODULE="${FAST_SPACE}/rtos/zephyr/modules/debug/segger"
        APP_MODULE="${FAST_SPACE}/applications"
        export ZEPHYR_MODULES="${HAL_MODULE};${SOC_MODULE};${CMSIS_MODULE};${SEGGER_MODULE};${APP_MODULE}"
        export SOC_ROOT="${SOC_MODULE}"
        export BOARD_ROOT="${SOC_MODULE}"

        mkdir -p "$BUILD_DIR"
        echo "--> [BUILD] App: $APP_NAME, Profile: $TARGET_NAME"
        
        cmake -GNinja -DZEPHYR_MODULES="$ZEPHYR_MODULES" -B"$BUILD_DIR" -S"$APP_PATH" \
              -DZEPHYR_BASE="$ZEPHYR_BASE" \
              -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
              -DSOC_ROOT="$SOC_MODULE" \
              -DBOARD_ROOT="$SOC_MODULE" \
              -DDTS_ROOT="$SOC_MODULE" \
              -DTARGET_NAME="$TARGET_NAME" \
              -DZEPHYR_CCACHE=ON
        
        ninja -C "$BUILD_DIR" -j $CPUS
        ;;
        
    sim)
        echo "--> [SIM] Building Native Linux Simulation for $APP_NAME"
        BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${APP_NAME}/native_sim"
        
        # For native sim, we might not strictly need the hardware HALs, 
        # but we include the APP_MODULE (our workspace) so it finds custom drivers/dts
        CMSIS_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
        APP_MODULE="${FAST_SPACE}/applications"
        export ZEPHYR_MODULES="${CMSIS_MODULE};${APP_MODULE}"
        
        mkdir -p "$BUILD_DIR"
        cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
              -DBOARD="native_sim" \
              -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
              -DZEPHYR_BASE="$ZEPHYR_BASE" \
              -DZEPHYR_CCACHE=ON
              
        ninja -C "$BUILD_DIR" -j $CPUS
        
        echo "--> [SIM] Running Application (Press Ctrl+C to exit)..."
        "$BUILD_DIR/zephyr/zephyr.exe"
        ;;
        
    clean)
        echo "--> [CLEAN] Removing build directories for $APP_NAME"
        rm -rf "${WORKSPACE_ROOT}/applications/build/${APP_NAME}"
        rm -rf "${WORKSPACE_ROOT}/applications/apps/${APP_NAME}/build_native"
        ;;
        
    *)
        usage
        ;;
esac
