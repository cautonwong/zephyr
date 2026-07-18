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
cp "${WORKSPACE_ROOT}/applications/Kconfig" "${FAST_SPACE}/applications/" || true
cp "${WORKSPACE_ROOT}/applications/CMakeLists.txt" "${FAST_SPACE}/applications/" || true
cp "${WORKSPACE_ROOT}/applications/west.yml" "${FAST_SPACE}/applications/" || true
# CRITICAL: Sync modules to RAM-disk to ensure logic patches are visible
if [ -d "${WORKSPACE_ROOT}/modules" ]; then
    rsync -a --delete "${WORKSPACE_ROOT}/modules/" "${FAST_SPACE}/modules/"
fi

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
    echo "  renode <app_name>         : Build ARM ELF and run Renode simulation"
    echo "  clean <app_name>          : Clean build directories"
    exit 1
}

if [ -z "$COMMAND" ] || [ -z "$APP_NAME" ]; then
    usage
fi

APP_PATH="${FAST_SPACE}/applications/apps/${APP_NAME}"
if [ ! -d "$APP_PATH" ]; then
    APP_PATH="${FAST_SPACE}/applications/tests/${APP_NAME}"
    if [ ! -d "$APP_PATH" ]; then
        echo "Error: Application directory not found in apps/ or tests/ for $APP_NAME"
        exit 1
    fi
fi

CPUS=$(nproc)

case $COMMAND in
    build|target|renode)
        if [ "$COMMAND" == "renode" ]; then
            TARGET_NAME="v32_cpuapp_gateway"
        fi

        if [ -z "$TARGET_NAME" ]; then
            echo "Error: Missing target_profile."
            usage
        fi
        
        if [ "$COMMAND" == "renode" ]; then
            BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${APP_NAME}/renode_v32"
        else
            BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${APP_NAME}/${TARGET_NAME}"
        fi

        # Standard Zephyr environment variable to pass board roots to all sysbuild images
        export ZEPHYR_BOARD_ROOT="/workspaces/modules/soc/v32f20x;${FAST_SPACE}/modules/soc/v32f20x"

        if [[ "$TARGET_NAME" == "v85"* ]]; then
            SOC_TYPE="v85xxp"
            HAL_MODULE="${FAST_SPACE}/modules/hal/V85XXP_Lib_V2.5"
            BOARD="v85xxp_board"
        else
            SOC_TYPE="v32f20x"
            HAL_MODULE="${FAST_SPACE}/modules/hal/V32F20X_StdPeriph_Lib_V1.0.6"
            if [[ "$TARGET_NAME" == *"cpumeter"* ]]; then
                BOARD="v32f20x_board/v32f20x/cpumeter"
            elif [[ "$TARGET_NAME" == *"ns"* ]]; then
                BOARD="v32f20x_board/v32f20x/cpuapp/cpuapp_ns"
            else
                BOARD="v32f20x_board/v32f20x/cpuapp"
            fi
        fi

        SOC_MODULE="${FAST_SPACE}/modules/soc/${SOC_TYPE}"
        CMSIS_6_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
        CMSIS_DSP_MODULE="${FAST_SPACE}/rtos/zephyr/modules/lib/cmsis-dsp"
        SEGGER_MODULE="${FAST_SPACE}/rtos/zephyr/modules/debug/segger"
        APP_MODULE="${FAST_SPACE}/applications"
        
        TFM_MODULES="${FAST_SPACE}/rtos/zephyr/modules/tee/tf-m/trusted-firmware-m;${FAST_SPACE}/rtos/zephyr/modules/tee/tf-m/tf-m-tests;${FAST_SPACE}/rtos/zephyr/modules/tee/tf-m/psa-arch-tests;${FAST_SPACE}/rtos/zephyr/modules/crypto/mbedtls;${FAST_SPACE}/rtos/zephyr/modules/crypto/mbedtls-3.6;${FAST_SPACE}/rtos/zephyr/modules/crypto/tf-psa-crypto;${FAST_SPACE}/rtos/zephyr/bootloader/mcuboot;${FAST_SPACE}/rtos/zephyr/modules/lib/zcbor;${FAST_SPACE}/rtos/zephyr/modules/hal/nordic;${FAST_SPACE}/rtos/zephyr/modules/lib/open-amp"
        export ZEPHYR_MODULES="${HAL_MODULE};${SOC_MODULE};${CMSIS_6_MODULE};${CMSIS_DSP_MODULE};${SEGGER_MODULE};${APP_MODULE};${TFM_MODULES}"

        if [ -d "${ZEPHYR_BASE}/boards/vango/v32f20x_board" ]; then
            rm -rf "${ZEPHYR_BASE}/boards/vango/v32f20x_board"
        fi

        mkdir -p "$BUILD_DIR"
        echo "--> [$COMMAND] App: $APP_NAME, Target: $BOARD"
        
        # Determine if we should use Sysbuild (nRF5340 emulation)
        if [[ "$TARGET_NAME" == *"cpuapp"* ]]; then
            echo "--> [SYSBUILD] Activating Zephyr Sysbuild Orchestrator (Multi-Image)"
            cmake -GNinja -B"$BUILD_DIR" -S"${ZEPHYR_BASE}/share/sysbuild" \
                  -DAPP_DIR="$APP_PATH" \
                  -DBOARD="$BOARD" \
                  -DZEPHYR_BASE="$ZEPHYR_BASE" \
                  -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
                  -DBOARD_ROOT="/workspaces/modules/soc/v32f20x;${FAST_SPACE}/modules/soc/v32f20x" \
                  -DSOC_ROOT="$SOC_MODULE" \
                  -DTARGET_NAME="$TARGET_NAME" \
                  -DKCONFIG_WERROR=OFF \
                  -DZEPHYR_CCACHE=ON
        else
            # Standard single image build
            cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
                  -DBOARD="$BOARD" \
                  -DZEPHYR_BASE="$ZEPHYR_BASE" \
                  -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
                  -DBOARD_ROOT="/workspaces/modules/soc/v32f20x;${FAST_SPACE}/modules/soc/v32f20x" \
                  -DSOC_ROOT="$SOC_MODULE" \
                  -DTARGET_NAME="$TARGET_NAME" \
                  -DKCONFIG_WERROR=OFF \
                  -DZEPHYR_CCACHE=ON
        fi
        
        ninja -C "$BUILD_DIR" -j $CPUS

        if [ "$COMMAND" == "renode" ]; then
            RESC_FILE="${WORKSPACE_ROOT}/applications/tests/${APP_NAME}/renode/v32_run.resc"
            echo "--> [RENODE] Starting Headless Simulation (Real-time Logs)..."
            # Start Renode in background
            /opt/renode/renode --disable-xwt "$RESC_FILE" > "${WORKSPACE_ROOT}/applications/renode_output.log" 2>&1 &
            RENODE_PID=$!
            # Wait for 5 seconds in real time
            sleep 5
            # Kill the Renode process safely
            kill $RENODE_PID >/dev/null 2>&1 || true
            sleep 0.5
            kill -9 $RENODE_PID >/dev/null 2>&1 || true
            echo "--> [RENODE] Output Log:"
            cat "${WORKSPACE_ROOT}/applications/renode_output.log" || true
            echo "--> [RENODE] Internal System Log:"
            cat "${WORKSPACE_ROOT}/applications/renode_internal.log" || true
        fi
        ;;
        
    sim)
        echo "--> [SIM] Building Native Linux Simulation for $APP_NAME"
        BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${APP_NAME}/native_sim"
        CMSIS_DSP_MODULE="${FAST_SPACE}/rtos/zephyr/modules/lib/cmsis-dsp"
        CMSIS_6_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
        APP_MODULE="${FAST_SPACE}/applications"
        export ZEPHYR_MODULES="${CMSIS_DSP_MODULE};${CMSIS_6_MODULE};${APP_MODULE}"
        
        mkdir -p "$BUILD_DIR"
        cmake -GNinja -DBOARD=v32f20x_board/v32f20x/cpuapp/cpuapp -DBOARD_ROOT=/workspaces/modules/soc/v32f20x -B"$BUILD_DIR" -S"$APP_PATH" -DBOARD=v32f20x_board/v32f20x/cpuapp/cpuapp -DBOARD_ROOT=/workspaces/modules/soc/v32f20x -DBOARD=v32f20x_board/v32f20x/cpuapp/cpuapp -DBOARD_ROOT=/workspaces/modules/soc/v32f20x \
              -DBOARD="native_sim" \
              -DZEPHYR_MODULES="$ZEPHYR_MODULES" -DBOARD_ROOT=/workspaces/modules/soc/v32f20x -DSOC_ROOT="$SOC_MODULE" \
              -DZEPHYR_BASE="$ZEPHYR_BASE" \
              -DCONFIG_ASAN=y \
              -DZEPHYR_CCACHE=ON
              
        ninja -C "$BUILD_DIR" -j $CPUS
        
        echo "--> [SIM] Running Application (Press Ctrl+C to exit)..."
        "$BUILD_DIR/zephyr/zephyr.exe"
        ;;
        
    clean)
        echo "--> [CLEAN] Removing build directories for $APP_NAME"
        rm -rf "${WORKSPACE_ROOT}/applications/build/${APP_NAME}"
        ;;

    test)
        echo "--> [TEST] Building all reference designs on native_sim..."
        REF_APPS=$(ls -d "${WORKSPACE_ROOT}/applications/apps/ref_"* 2>/dev/null | xargs -n1 basename)
        FAILED=0
        PASSED=0
        for app in $REF_APPS; do
            echo ""
            echo "=============================================="
            echo " Building $app for native_sim..."
            echo "=============================================="
            BUILD_DIR="${WORKSPACE_ROOT}/applications/build/${app}/native_sim"
            APP_PATH="${FAST_SPACE}/applications/apps/${app}"
            CMSIS_DSP_MODULE="${FAST_SPACE}/rtos/zephyr/modules/lib/cmsis-dsp"
            CMSIS_6_MODULE="${FAST_SPACE}/rtos/zephyr/modules/hal/cmsis_6"
            APP_MODULE="${FAST_SPACE}/applications"
            export ZEPHYR_MODULES="${CMSIS_DSP_MODULE};${CMSIS_6_MODULE};${APP_MODULE}"

            mkdir -p "$BUILD_DIR"
            if cmake -GNinja -B"$BUILD_DIR" -S"$APP_PATH" \
                -DBOARD="native_sim" \
                -DZEPHYR_MODULES="$ZEPHYR_MODULES" \
                -DZEPHYR_BASE="$ZEPHYR_BASE" \
                -DZEPHYR_CCACHE=ON 2>&1 | tail -5 && \
                ninja -C "$BUILD_DIR" -j $CPUS 2>&1 | tail -5; then
                echo "  ✓ $app BUILD PASSED"
                PASSED=$((PASSED + 1))
            else
                echo "  ✗ $app BUILD FAILED"
                FAILED=$((FAILED + 1))
            fi
        done
        echo ""
        echo "=============================================="
        echo " Build Results: $PASSED passed, $FAILED failed"
        echo "=============================================="
        [ $FAILED -eq 0 ] || exit 1
        ;;

    *)
        usage
        ;;
esac
