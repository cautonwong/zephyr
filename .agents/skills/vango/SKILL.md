---
name: vango
description: Vango 工业级 Zephyr RTOS 项目 — workflow.sh 编译/多镜像 Sysbuild/驱动移植/参考设计开发
disable-model-invocation: true
---

# Vango Project Skill

Two leading words govern everything below:

- **wiring** — 连接比组件更难。workflow.sh 管线、多镜像 Sysbuild、跨核 IPC、安全边界、DTS 绑定 — 每条连接都是潜在的断点。做好 wiring 意味着每次变动都验证完整链路，不假设"另一边已经好了"。
- **seam** — 架构解耦的切入边界。DTS 是硬件 seam、Kconfig 是特性 seam、模块目录是仓库 seam、TF-M SAU 是安全 seam。每个 seam 都是可以独立替换而不影响两端的接口。不违反 seam、不绕过 seam、知道每个 seam 的容限。

---

## Overview

```
Vango RTOS Workspace
├── apps/          # 应用 (10 ref designs + base_app + vango_demo)
├── boards/        # 板级定义
├── drivers/       # 外树驱动 (ad7616, blink, sensor, sim_axon)
├── dts/           # 外树绑定
├── include/       # 公共头文件 (common_core.h, protocol.h)
├── lib/           # 外树库 (custom, flashdb, janpatch, qcbor)
├── tests/         # 测试套件 (algo_dsp, flashdb, ref_*, vango_demo)
├── docs/          # PRD、设计规格、超级能力
├── .agents/       # Claude Code 技能 & 代理
├── workflow.sh    # 核心编译编排器 (RAM-disk + ZEPHYR_MODULES)
└── west.yml       # West manifest (zephyr main + custom modules)
```

**SoC**: V32F20x (Cortex-M33 + Cortex-M0 双核) / V85xxp  
**OS**: Zephyr RTOS (main 分支) + TF-M + MCUboot  
**构建**: Sysbuild 多镜像 (mcuboot + tfm_s + ns_app + cpumeter)  
**IPC**: Zephyr ipc_service (RPMsg), struct header+memcpy, 不需要 CBOR/Protobuf  
**WASM**: WAMR 运行时已集成, 内联字节码加载 + NativeSymbols 桥接  
**janpatch**: 库编译可用, OTA shell 命令集成待完成

---

## Branches

### 1. Build & Flash — wiring the pipeline

Run builds through `./workflow.sh` — never raw `west build`. The script handles RAM-disk syncing to `/root/fast_space` and `ZEPHYR_MODULES` path stitching that bare `west build` misses.

**Targets** (vango_demo):

| Target | SoC | Core | Purpose |
|--------|-----|------|---------|
| `v32_cpuapp_gateway_ns` | V32F20x | M33 NS | 主应用 (网关) |
| `v32_cpuapp_gateway` | V32F20x | M33 S | Secure 完整构建 |
| `v32_cpumeter_metering` | V32F20x | M0 | 计量辅核 |
| `v85_collector` | V85xxp | — | 数据采集器 |
| `native_sim` | — | — | 本地仿真 (ASAN) |

**Workflow commands**:
```
./workflow.sh build vango_demo <target>  # 物理硬件构建
./workflow.sh sim vango_demo             # 本地仿真
./workflow.sh renode vango_demo          # Renode 多核仿真
./workflow.sh clean vango_demo           # 清理构建
```

**Key wiring details**:
- Sysbuild 生成 4 个镜像：`mcuboot.bin` → `tfm_s_signed.bin` → `zephyr_ns_signed.bin` → `cpumeter.bin`
- Flash 布局：MCUboot @0x0 (64K) → TF-M S @0x10000 (256K) → NS App @0x50000 (320K) → Upgrade/Data 区域 → cpumeter @0x1A0000 (128K)
- 启动链：ROM → MCUboot(RSA verify) → TF-M(SAU/MPC) → NS App → release M0 reset
- 在 sysbuild.cmake 中调整 BOARD/BOARD_ROOT/CONF_FILE/DTC_OVERLAY_FILE 来重定向镜像
- `ZEPHYR_BOARD_ROOT` 环境变量指向外树 SoC 模块

**Common failures**:
- `<delete-node/>` 遗漏 → DTC 分区冲突 (双份标签)
- `sysbuild.cmake` 移出应用根目录 → Zephyr 回退到 stub board
- `KCONFIG_WERROR` 写在 `prj.conf` 而非 `-D` 参数中 → 构建断裂
- RAM-disk 未同步 → `rsync` 旧代码; 重新运行 `./workflow.sh build`

**Completion criterion**: `ninja -C build/<app>/<target>` 无错误完成，或 Renode 仿真日志显示应用启动。

---

### 2. Architecture Navigation — reading the seams

理解 seam 在哪里，以及如何在不破坏它们的前提下工作。

**Seam 1: DTS-first 硬件 seam**  
硬件细节只存在于 `.overlay` / `.dts` / 绑定中。C 代码引用 `DT_ALIAS()` / `DT_NODE()` — 不出现寄存器地址或引脚号。  
违反 seam 的表现：`#define USART0_BASE 0x40013800UL` — 如果看到，立即拆回 DTS 层。

**Seam 2: Kconfig 特性 seam**  
每个功能组件通过 `CONFIG_APP_FEATURE_*` 开关。`Kconfig` 定义菜单，`CMakeLists.txt` 通过 `add_subdirectory_ifdef` / `zephyr_library_sources_ifdef` 条件编译。  
违反 seam 的表现：在 `main.c` 中直接 `#ifdef` 某个 SoC 型号而不是 feature 宏。

**Seam 3: 模块仓库 seam**  
上游 (`rtos/zephyr/`) 和 HAL (`modules/hal/`) 目录不可修改。所有适配通过外树驱动 (drivers/)、板级 (boards/)、SoC 模块实现。  
违反 seam 的表现：编辑了 `zephyr/` 或 `modules/` 中的文件来修复编译问题。回退，通过外树 shim 或 Kconfig 覆写实现。

**Seam 4: TF-M 安全 seam**  
SAU/SPU 划定的安全/非安全边界。M33 Secure 世界拥有加密密钥、安全存储; M33 Non-Secure 运行业务逻辑; M0 在纯 NS 环境运行实时采样。  
违反 seam 的表现：NS 代码直接访问加密寄存器、尝试读写安全内存、绕过 PSA API。

**Seam 5: IPC 跨核 seam**  
M33 ↔ M0 通过共享内存 + OpenAMP/RPMsg 通信，不共享指针。  
违反 seam 的表现：M33 直接解引用 M0 内存地址（无 MPU 保护）。

**Key architectural documents** (加载这些文件以获得完整上下文):
- `docs/prds/01_ARCH_DECOUPLING_AND_IPC_PRD.md` — 架构解耦规范
- `docs/prds/07_TRUSTZONE_SECURITY_ORGANIZATION_PRD.md` — 安全边界
- `.agents/skills/zephyr/SKILL.md` — Zephyr 开发细节 & DSP 模式
- `ZEPHYR_ECOSYSTEM_GUIDE.md` — Coredump / Tracing / Twister HIL
- `PROJECT_EVOLUTION_ANALYSIS.md` — 整体架构分析

**Completion criterion**: 对于给定的任务，能明确指出涉及哪些 seam、每个 seam 你将在哪一侧工作、以及如何不跨越它。

---

### 3. Driver & Board Porting — DTS-first seam

所有硬件适配都必须在外树目录完成，不接触上游代码。

**Driver 目录布局**:
- `drivers/<driver_name>/` — 驱动源码 + Kconfig + CMakeLists.txt
- `dts/bindings/<vendor>,<device>.yaml` — DTS binding（必须在外树 `dts/bindings/` 中）
- `include/app/drivers/<driver_name>.h` — 驱动头文件（非必要，但推荐）
- 在 `drivers/Kconfig` 中 `rsource` 新驱动 Kconfig
- 在 `drivers/CMakeLists.txt` 中 `add_subdirectory_ifdef(CONFIG_*)`

**Driver 开发步骤**:
1. 在 `dts/bindings/` 创建 YAML binding（`compatible`, `reg`, `pinctrl-device.yaml`, 属性）
2. 在 `boards/` 对应的 `.overlay` 中添加 DTS node，或为应用创建 `targets/<target>/app.overlay`
3. 编写驱动 `.c` 实现：`DEVICE_DT_INST_DEFINE`、`#define DT_DRV_COMPAT`、pm_action callback
4. 添加 `Kconfig`：`config <DRIVER_NAME>` 和 `if <DRIVER_NAME>` 的依赖属性
5. 注册到 `drivers/Kconfig` 和 `drivers/CMakeLists.txt`
6. 在目标应用的 `.conf` / `prj.conf` 中启用 `CONFIG_<DRIVER_NAME>=y`

**Board overlay 模式**:
- 应用级 overlay 在 `apps/<app>/targets/<target>/app.overlay`
- Sysbuild 需要同时提供 mcuboot.overlay（在 `sysbuild/mcuboot.overlay`）
- 分区定义必须包含完整的 flash layout，不可重叠
- 如果 overlay 覆盖已有分区标签，先 `/delete-node/ &<existing>;`

**Example reference**: `drivers/sim_axon/` — 一个完整的 PM-aware 外树驱动实现，包含 binding、pinctrl 状态、PM runtime 回调。

**Completion criterion**: 新的硬件节点在 DTC 编译中无错误通过，device init 在启动日志中打印初始化成功。

---

### 4. Test & Verify — tight loop

**Test frameworks**:
- **Ztest** — Zephyr 原生单元测试（`ZTEST_SUITE`, `ZTEST`, harness: ztest）
- **pytest** — 串口交互测试（`harness: pytest`，驱动 `native_sim` 的 UART shell）
- **Console** — 正则匹配输出（`harness: console`，`harness_config.type: one_line`）
- **Renode** — 多核仿真测试（`tests/<app>/renode/` 中的 `.repl` + `.resc` 脚本）

**Test wiring**:
1. 测试在 `tests/<app>/` 目录下，包含 `testcase.yaml` 定义测试场景
2. Twister 通过 `west twister -T tests/<app>` 运行
3. 物理硬件测试需要 `hardware_map.yaml`（从 `hardware_map.yaml.example` 复制）
4. FFF 框架用于硬件隔离：`DEFINE_FFF_GLOBALS` + `FAKE_VALUE_FUNC` 模拟外设

**Renode 仿真**:
- `tests/<app>/renode/v32.repl` — 虚拟 SoC 定义（CPU、内存、UART、外设）
- `tests/<app>/renode/v32_run.resc` — 启动脚本（加载 ELF、绑定 UART 日志、启动 CPU）
- `./workflow.sh renode <app>` — 一键启动 / 运行 5 秒 / 收集日志

**DSP verification** (`tests/algo_dsp`):
- CMSIS-DSP FFT (1024 点 CFFT, 50Hz 信号检测, <1Hz 容差)
- PID with anti-windup (并行结构, 积分钳位)
- LESO (2 阶, 带宽调优: L₁=2ωₒ, L₂=ωₒ²)

**Tight loop**:
```
修改 → workflow.sh build/renode/sim → 观察输出 → 断言通过? 完成 : 退回修改
```

**Shorthands**:
- `./workflow.sh sim vango_demo` — 最快的本地反馈（native_sim + ASAN）
- `./workflow.sh renode vango_demo` — 最真实的预硬件测试（ARM 指令级仿真）
- `west twister -T tests/algo_dsp -p native_sim --integration` — 纯测试场景

**Completion criterion**: Twister 返回 PASS（所有断言通过），或 Renode 日志包含预期的启动/功能输出。

---

### 5. Reference Design — fleshing out ref_* apps

目前 `apps/ref_*` 目录是占位骨架。以下是从占位符到功能实现的标准路径。

**当前参考设计列表**:

| 应用 | 领域 | 关键算法/驱动 |
|------|------|---------------|
| `ref_6dof_sensor` | 6 轴 IMU 姿态融合 | 传感器驱动, Mahony/Madgwick |
| `ref_bms_eis` | 电池管理 EIS | ADC 采样, 阻抗谱拟合 |
| `ref_drone_fcc` | 飞控计算机 | PID 控制, PWM, 传感器融合 |
| `ref_medical_wearable` | 医疗可穿戴 | PPG/ECG 信号, 低功耗 |
| `ref_motor_pred_maint` | 电机预测性维护 | FFT, LESO, 振动分析 |
| `ref_nilm_meter` | 非侵入负荷监测 | tinyML, FFT, 负荷分解 |
| `ref_nir_spectrometer` | 近红外光谱 | 光谱处理, 校准曲线 |
| `ref_power_quality` | 电能质量分析 | FFT 谐波分析, 暂态检测 |
| `ref_radar_ranging` | 雷达测距 | FMCW 处理, CFAR 检测 |
| `ref_v2g_charger` | V2G 充电桩 | PLC 通信, 充电协议栈 |

**Fleshing out steps**:
1. 添加 `Kconfig`（继承 `Kconfig.zephyr`, 定义领域特定 CONFIG_*）
2. 添加 `CMakeLists.txt`（链接 `common_core`、必要的内核库）
3. 编写入口 `src/main.c`（调用 `common_core_init("ref_xxx")`，注册初始化）
4. 导入已验证算法（从 `tests/algo_dsp` 移植 PID/LESO/FFT 模式）
5. 添加 `sysbuild.conf`（开启多镜像支持，如果需要 TF-M）
6. 创建目标配置 `targets/<target>/prj.conf` + `app.overlay`
7. 创建 `testcase.yaml`（至少 `build_only: true` 确保构建验证）

**Common Core** (`include/common_core.h`):
```c
common_core_init("ref_motor_pred_maint");  // 标准启动横幅
struct power_metrics { float voltage, current, active_power, reactive_power, frequency; };
```
参考设计应从 `common_core_init` 开始，复用 `struct power_metrics` 用于能源类产品。

**Completion criterion**: 参考设计在 `./workflow.sh build ref_<domain> v32_cpuapp_gateway_ns` 下编译通过，并能输出正确的启动横幅和功能日志。

---

## Failure modes specific to this project

- **RAM-disk staleness** — 在 Docker/fast_space 环境中，更新了文件但 `workflow.sh` 没有重新 rsync。症状：构建用了旧代码，行为不匹配。修复：`./workflow.sh build` 自动触发 rsync。
- **Unclean upstream** — 编辑了 `rtos/zephyr/` 或 `modules/` 下的文件以绕过外树限制。症状：`git status` 显示上游仓库有修改。修复：撤销更改，改在外树实现。
- **Overlay partition collision** — 应用 overlay 重新定义了 flash 分区但未 `/delete-node/`。症状：DTC 报 "label redefined" 错误。修复：在 overlay 顶部添加 `/delete-node/ &<existing_partition>;`。
- **Bare west build** — 在 workflow.sh 之外直接运行 `west build`。症状：找不到外树驱动或 SoC 模块。修复：始终通过 `./workflow.sh build` 执行。
- **sysbuild.cmake relocation** — 将 `sysbuild.cmake` 移入子目录。症状：sysbuild 回退到 stub board。修复：保持 `sysbuild.cmake` 在应用根目录。
