# 恢复完整 4 镜像 Sysbuild 编译 — 任务跟踪

## 目标

恢复 `/workspaces/applications` 项目的 vango_demo 完整 4 镜像 sysbuild 编译：

| # | 镜像 | 运行核心 | 职责 |
|---|------|---------|------|
| 1 | **MCUboot** | M33 Secure | 安全启动 + OTA 固件升级管理 |
| 2 | **TF-M** | M33 Secure | PSA 安全服务（加密、安全存储、认证、初始认证） |
| 3 | **vango_demo** | M33 Non-Secure | 4G 网关主应用（Modbus、PPP、网络协议栈、IPC、电源管理） |
| 4 | **cpumeter** | M0 | 辅核计量（ADC 采样、HFM 电量计量） |

---

## 进度总览

| 阶段 | 状态 | 说明 |
|------|------|------|
| 修复 Gemini 引入的编译错误 | ✅ 完成 | 3 个问题已定位并修复 |
| 2 镜像编译 (vango_demo + cpumeter) | ✅ 通过 | exit 0 |
| 4 镜像编译 (MCUboot + TF-M + vango_demo_ns + cpumeter) | 🔧 进行中 | BL2 链接错误：`__StackTop` 符号缺失 |
| TF-M 平台端口缺陷修复 | 🔧 进行中 | flash_layout.h / tfm_peripherals_def.h / 启动代码等 |

---

## Gemini 引入的编译错误（已修复）

### 问题 1：sysbuild.cmake 被移到错误位置 🔴→🟢

- **根因**：Zephyr sysbuild 在 `sysbuild_extensions.cmake:936` 通过 `include(${SOURCE_DIR}/sysbuild.cmake)` 加载配置，找的是 **app 根目录**
- **Gemini 改动**：删了 `apps/vango_demo/sysbuild.cmake`，新建 `sysbuild/sysbuild.cmake`（Zephyr 不加载此处）
- **后果**：`vango_demo_BOARD` 丢失 → 板级 DTS 退化为 stub.dts → systick 节点缺失 → Kconfig 报错
- **修复**：重建 `apps/vango_demo/sysbuild.cmake`，恢复完整 board qualifier 和 cpumeter 配置
- **验证**：cmake configure 阶段 BOARD.dts 指向正确板级文件

### 问题 2：prj.conf 加入无效 Kconfig 符号 `CONFIG_KCONFIG_WERROR=n` 🔴→🟢

- **根因**：`KCONFIG_WERROR` 不是 Zephyr 4.4 的 Kconfig 选项，是 cmake 变量
- **Gemini 改动**：在 gateway 和 cpumeter 两个 prj.conf 加了这行
- **后果**：Kconfig 警告 "attempt to assign the value 'n' to the undefined symbol KCONFIG_WERROR"
- **修复**：从两个 prj.conf 删除 `CONFIG_KCONFIG_WERROR=n`；同时注释 cpumeter 中依赖不满足的 `SEGGER_RTT_MAX_NUM_UP_BUFFERS=3`
- **验证**：Kconfig 配置阶段无 "undefined symbol" 警告

### 问题 3：app.overlay 删除了 `/delete-node/` 分区清理指令 🔴→🟢

- **根因**：Board DTS 已定义 7 个分区（含 `_ns` 安全分区），app.overlay 重定义布局不同的 6 个分区，不先删旧分区则标签冲突
- **后果**：DTC 报错 `Label 'slot1_partition' appears on ... partition@a0000 and ... partition@b0000`
- **修复**：在 `v32_cpuapp_gateway/app.overlay` 分区定义前恢复 `/delete-node/` 指令
- **验证**：DTC 设备树编译阶段无 "duplicate label" 错误

---

## TF-M 平台端口缺陷修复（进行中）

> V32F20X TF-M 平台端口位于 `modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/`
> 启用 MCUboot + TF-M + _ns 配置后，TF-M 构建暴露大量未完成项。

### 已完成的 TF-M 修复

| # | 文件 | 缺陷 | 修复 |
|---|------|------|------|
| 1 | `CMakeLists.txt` | BL2 找不到 HAL `v32f20x.h` | 添加 HAL include 路径 |
| 2 | `CMakeLists.txt` | BL2 找不到 CMSIS `system_target_cm33_core0.h` | 添加 CMSIS core0 include 路径 |
| 3 | `flash_layout.h` | 缺少 `TFM_HAL_FLASH_PROGRAM_UNIT` | 添加 `(0x4)` |
| 4 | `flash_layout.h` | 缺少 `FLASH_AREA_SCRATCH_ID` | 添加 scratch area 定义 |
| 5 | `flash_layout.h` | 缺少 `TFM_HAL_ITS_FLASH_DRIVER` 等4个宏 | 添加 ITS/PS flash driver 定义 |
| 6 | `flash_layout.h` | 缺少 `TFM_HAL_PS_FLASH_AREA_ADDR` 等6个宏 | 添加 PS 区域地址/大小/扇区定义 |
| 7 | `flash_layout.h` | 缺少 `TFM_OTP_NV_COUNTERS_*` 宏族 | 添加 OTP NV counters TFM_* 前缀宏 |
| 8 | `tfm_peripherals_def.h` | 文件不存在，TF-M 自动生成代码引用 | 新建，声明外设 IRQ 号 |
| 9 | `device_cfg.h` | 文件不存在，`uart_stdout.c` 引用 | 新建，声明 UART 波特率 |
| 10 | `target_cfg.h` | 文件不存在，`target_cfg.c` 引用 | 新建，声明 `TFM_DRIVER_STDIO` 和平台函数 |
| 11 | `tfm_hal_platform.c` | 使用旧版 TF-M API `struct memory_region_limits` | 移除该 struct，直接用 `NS_CODE_START` |
| 12 | `CMakeLists.txt` | `startup_v32f20x.c` 与 TF-M 标准启动代码冲突 | 移除启动文件，改用 TF-M 标准启动 |
| 13 | `vectors_v32f20x.c` | TF-M linker 需要 `__Vectors` 向量表符号 | 新建最小向量表文件 |

### 当前阻塞点

| # | 错误 | 状态 |
|---|------|------|
| 14 | BL2 linker: `undefined reference to '__StackTop'` | 🔧 待验证：改用 `Image$$ARM_LIB_STACK$$ZI$$Limit` |

**堆栈符号映射**：
- TF-M BL2 linker script (`tfm_common_bl2.ld`) 不定义 `__StackTop`
- TF-M 的堆栈顶是 `Image$$ARM_LIB_STACK$$ZI$$Limit`
- 已更新 `vectors_v32f20x.c` 使用 `Image$$ARM_LIB_STACK$$ZI$$Limit`

---

## 涉及修改的文件清单

```
# Gemini 问题修复
apps/vango_demo/sysbuild.cmake                    # 重建（从错误位置 sysbuild/ 移回）
apps/vango_demo/targets/v32_cpumeter_metering/prj.conf  # 删除无效 CONFIG_KCONFIG_WERROR
apps/vango_demo/targets/v32_cpuapp_gateway/prj.conf     # 删除无效 CONFIG_KCONFIG_WERROR
apps/vango_demo/targets/v32_cpuapp_gateway/app.overlay  # 恢复 /delete-node/ 指令
apps/vango_demo/targets/v32_cpumeter_metering/prj.conf  # 注释 SEGGER_RTT_MAX_NUM_UP_BUFFERS

# DTS 修复
modules/soc/v32f20x/boards/.../v32f20x_board_v32f20x_cpuapp_cpuapp_ns.dts  # /delete-property/ pinctrl-1

# TF-M 平台端口修复
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/CMakeLists.txt   # HAL 路径 + 向量表
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/partition/flash_layout.h  # 补全宏定义
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/tfm_peripherals_def.h    # 新建
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/device_cfg.h             # 新建
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/target_cfg.h             # 新建
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/tfm_hal_platform.c       # 移除旧 API
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/zephyr/toolchain.h       # 新建 shim
modules/soc/v32f20x/tfm/platform/ext/target/vango/v32f20x/vectors_v32f20x.c        # 新建
```

---

## 验证方法

```bash
# 1. 清空构建缓存
rm -rf /workspaces/applications/build/vango_demo/v32_cpuapp_gateway

# 2. 全量编译
./workflow.sh build vango_demo v32_cpuapp_gateway

# 3. 验证项：
#    a) cmake configure 无 Kconfig warning/error
#    b) DTC 设备树编译无 label 冲突
#    c) BL2 (MCUboot) 链接成功，输出 bl2.elf/bl2.bin
#    d) TF-M S 安全固件链接成功，输出 tfm_s.elf/tfm_s.bin
#    e) vango_demo (non-secure) 链接成功，输出 zephyr.elf
#    f) cpumeter (M0) 链接成功

# 4. sysbuild 总步数应为 16 步（4 镜像 × 4 步 CMake/Build）
#    最终输出应包含 4 个 Completed 行
```
