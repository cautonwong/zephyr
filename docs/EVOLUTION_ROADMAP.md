# Vango 项目执行计划 v2.1 (修正版)

基于 2026-07-06 代码审计与 20 轮领域审问。

---

## 状态速览 (修正后)

| 维度 | 真实状态 | 路线图修正 |
|------|---------|-----------|
| Sysbuild 4 镜像 | ✅ 完整编译通过 | 不变 |
| TF-M PSA Attestation | ✅ 真实 psa_initial_attest_get_token 调用 | 不变 |
| IPC (RPMsg) | ✅ 双端点 struct 协议 | 改描述：struct header+memcpy, 非 TLV |
| WASM 沙盒 | ✅ 25 次真实 WAMR API 调用, 内联字节码 | 不变 |
| sim_axon PM 驱动 | ✅ 真实 pinctrl + PM runtime | 不变 |
| 10 个 ref_* 应用 | ✅ 全部有 main.c(34-51行) + Kconfig + CMakeLists | 比宣称好 |
| flash 写入 OTA | ✅ 真实 flash_img_buffered_write | 不变 |
| janpatch 差分 OTA | ❌ `/* Simulate janpatch operation */` | 降级 → P1 |
| 双核 Renode | ❌ 单核 M33 仅 | 降级 → P1 |
| TFLite-Micro | ❌ 零引用 | 降级 → 先调研 |
| CONFIG_USERSPACE | ❌ 未实现 | 降级 → P2 |
| 硬件加密加速 | ❌ v32f20x Crypto 状态未知 | 降级 → 先调研 |

---

## 优先级原则

1. **先真后假** — 把 simulate/fake 的链路先打通，再谈新功能
2. **先库后应用** — 先提取可复用库 (lib/control/), 再填充 ref_* 
3. **先测后移** — 先在 native_sim 上验证算法, 再部署到硬件目标
4. **先单核后双核** — IPC 先在单核 native_sim 上测通, 再到 Renode 双核

---

## Phase 2A: 修补 Phase 1 模拟裂缝 (Week 1-2)

### P2A.1 janpatch 真正集成

**当前**: `ota_service.c:140` — `/* Simulate janpatch operation */` + sleep + print

**要做**:
```
ota_service.c cmd_ota_delta_apply():
  1. 从 slot0_partition 读当前固件到 source buffer (malloc 或静态arena)
  2. 从 LittleFS 读 patch 文件到 patch buffer
  3. 在 target buffer 调用 janpatch(ctx, source, patch, target)
  4. 校验 target CRC
  5. 写入 slot1_partition
  6. 触发 MCUboot swap
```

**依赖**: janpatch 库已编译 (`CONFIG_JANPATCH=y`), 调用 `janpatch()` 即可  
**验收**: `ota delta_apply` shell 命令不再打印进度条, 实际产生目标固件到 slot1

### P2A.2 Kconfig feature gate 补齐

**当前**: 只有 `main.c` 用 `#ifdef CONFIG_APP_FEATURE_*`, 服务 .c 文件没有

**要做**:
```
在每个 services/*.c 文件加:
  #if defined(CONFIG_APP_FEATURE_XXX)
  ... 函数体 ...
  #endif
```

**验收**: 关闭 `CONFIG_APP_FEATURE_WASM_SANDBOX=n` 后 wasm_sandbox.c 不参与编译

### P2A.3 OTA 双区验证自动化

**当前**: MCUboot swap 配置了但从未在 test 中验证

**要做**: 写一个 Twister pytest:
```
1. 编译 vango_demo + good firmware
2. 烧录 slot0
3. 模拟下载 bad firmware → slot1
4. 触发 reboot → MCUboot swap → 运行 bad firmware → watchdog timeout
5. 触发 reboot → MCUboot rollback → 恢复 good firmware
6. 验证 UART 输出 "回滚完成"
```

**验收**: rollback 全链路可在 native_sim 或 Renode 上自动化验证

---

## Phase 2B: 控制算法库化 (Week 2-3)

### P2B.1 lib/control/ 创建

从 `tests/algo_dsp/src/main.c` 提取已验证算法到正式库:

```
lib/control/
├── CMakeLists.txt
├── Kconfig
├── include/
│   └── control/
│       ├── pid.h           # PID 结构体 + init/update/reset
│       ├── leso.h          # LESO 结构体 + init/update (L₁=2ωₒ, L₂=ωₒ²)
│       └── spectrum.h      # FFT 封装: windowing + cfft + peak_detect + thd
└── src/
    ├── pid.c
    ├── leso.c
    └── spectrum.c
```

**关键设计**:
- PID: 并行结构, 积分钳位 anti-windup, 输出限幅 — 从 tests 直接移植
- LESO: 2 阶, `leso_init(omega_o, b)` + `leso_update(y, u, dt)` — 从 tests 直接移植
- Spectrum: CMSIS-DSP 封装 — `fft_analyze(samples, len, &result)` 输出 peak_freq + magnitude + THD

**依赖**: CMSIS-DSP 已通过 ZEPHYR_MODULES 链接  
**验收**: `west twister -T tests/lib/control -p native_sim` 返回 PASS

---

## Phase 2C: ref_motor_pred_maint MVP (Week 3-4)

### P2C.1 当前状态

```
apps/ref_motor_pred_maint/src/main.c: 43 行
- common_core_init() ✅
- run_autoencoder_anomaly_detection() — 模拟乘法, 不是真的 Autoencoder
- 4 个硬编码 float 作为"特征"
```

### P2C.2 要做

```
ref_motor_pred_maint MVP:
1. 链接 lib/control (PID + LESO + Spectrum)
2. 替换模拟 Autoencoder → 真实 Autoencoder 结构:
   - 输入层: 64 维 (32 频谱 + 32 时域)
   - Encoder: 64→16→8
   - Decoder: 8→16→64
   - MSE 输出作为异常分数
3. ADC 波形采样任务 (M0 模拟 → M33 通过 IPC 接收)
   → MVP 阶段: 用 native_sim 上预生成 .c 数组代替
4. CMSIS-DSP FFT → 频谱特征 (peak, variance, kurtosis)
5. testcase.yaml:
   - build_only: true (编译验证)
   - pytest: 注入波形 → 验证 UART 输出异常报警
```

**依赖**: lib/control 库化完成 (P2B.1)  
**验收**: `./workflow.sh build ref_motor_pred_maint v32_cpuapp_gateway_ns` 编译通过  
`west twister -T tests/ref_motor_pred_maint -p native_sim` 输出异常分数

---

## Phase 2D: ref_nilm_meter MVP (Week 4-5)

### P2D.1 当前状态

```
apps/ref_nilm_meter/src/main.c: 35 行
- common_core_init() ✅
- process_nilm_classification() — 硬编码阈值 (active_delta > 1500 → "heater")
- 不是真正的 NILM 模型
```

### P2D.2 要做

```
ref_nilm_meter MVP:
1. 定义 WASM Host-API 契约:
   - host_read_power(float *active, float *reactive) → int
   - host_get_harmonics(int8_t *buffer, size_t size) → int
   - host_db_store(const char *appliance, float energy) → int
2. 在 wasm_sandbox.c 中实现这三个 NativeSymbols
3. 小型 CNN 模型 (int8):
   - 3 层: Conv1D(3→8, k=3) → Conv1D(8→16, k=3) → Dense(16→5)
   - 5 类: heater, refrigerator, microwave, tv, unknown
   - 权重编译为 C 数组 (参考 TFLM 的量化工具链)
4. WASM 编译:
   - 将 CNN 推理代码编译为 .wasm 字节码
   - 通过 OTA 热更新: `ota wasm_load` 替换模型
5. testcase.yaml: pytest 注入模拟暂态事件

**关键问题**: TFLite-Micro 是否可用仍需确认。如果 v32f20x 没有 TFLM 支持:
   - 备选方案: 手写 int8 量化推理 (3 层卷积, 纯 CMSIS-DSP 矩阵乘)
   - 或 WAMR 内置的 TensorFlow Lite for WASM
```

**依赖**: WAMR 已在 wasm_sandbox.c 中集成  
**验收**: native_sim 上 `ota wasm_load` 替换模型后推理结果变化; 准确率 > 85%

---

## Phase 2E: 双核 Renode 仿真 (Week 5-6)

### P2E.1 当前

```
tests/algo_dsp/renode/v32.repl:
  cpu: Cortex-M33 @ sysbus (单核)
  无 M0, 无 SHM, 无 MBOX
```

### P2E.2 要做

```
双核 v32.repl:
  cpu0: Cortex-M0 @ sysbus  (cpumeter)
  cpu1: Cortex-M33 @ sysbus (cpuapp)
  
  sram: Memory.MappedMemory @ sysbus 0x20000000
    size: 0x00114000  (1104 KB)
    
  shm: Memory.MappedMemory @ sysbus 0x20100000
    size: 0x00010000   (64 KB WAVEFORM_SHM)
    
  mbox: Misc.MultiTimer @ sysbus 0x40020000
    → nvic@X  (MBOX 中断)
    
  uart0 → cpu1@60  (M33 UART)
  uart1 → cpu0@Y   (M0 UART, 可选)
```

**双核 v32_run.resc**:
```
# 加载两个 ELF
sysbus LoadELF @ "/path/to/cpumeter/zephyr.elf"  (M0)
sysbus LoadELF @ "/path/to/cpuapp/zephyr.elf"     (M33)

# 绑定 UART
uart0 RecordToLog true

# 启动 M0, 然后 M33
cpu0 IsStarted true
cpu1 IsStarted true
```

**验收**: `./workflow.sh renode vango_demo` 输出 M0 采样 → M33 IPC 接收 → 分类完成的日志

---

## Phase 2F: 参考设计扩展 (Week 6-8)

### 优先级矩阵 (修正后)

| 参考设计 | 优先级 | 理由 |
|---------|--------|------|
| ref_motor_pred_maint | P0 | 算法复用度最高, PRD 产品明确 |
| ref_nilm_meter | P0 | WASM 差异化, 商业价值高 |
| ref_power_quality | P1 | 50次谐波 FFT 可复用频谱库 |
| ref_radar_ranging | P1 | 2D FFT 算法成熟 |
| ref_bms_eis | P2 | 需 EIS 专业知识 |
| ref_drone_fcc | P2 | 需飞控领域知识 |
| ref_v2g_charger | P2 | 需 ISO 15118 协议 |
| ref_6dof_sensor | P3 | 需传感器驱动 |
| ref_medical_wearable | P3 | 需生物信号处理 |
| ref_nir_spectrometer | P3 | 需光谱仪驱动 |

### 通用填充模式 (所有 ref_*)

```
每个 ref_<domain> MVP 的通用步骤:
1. Kconfig + CMakeLists (已有 ✅)
2. main.c + common_core_init (已有 ✅)
3. 链接 lib/control/ (库化后)
4. 领域特定算法 (hardcoded stub → 真实算法)
5. testcase.yaml (至少 build_only)
6. Renode 仿真验证
```

---

## Phase 2G: 中间件提取 (Ongoing)

### 提取路线

| 库 | 来源文件 | 当前大小 | 提取时机 |
|---|---------|---------|---------|
| `lib/control/` | tests/algo_dsp/main.c | ~150 行 | 立即 (Week 2) |
| `lib/ipc/` | ipc_service.c | 114 行 | 等等, 需求稳定后 |
| `lib/storage/` | storage_init.c + flashdb | ~50 行 + 三方 | 低优先级 |
| `lib/pm/` | low_power.c + sim_axon | ~180 行 + 驱动 | 等第二个 PM 用户出现 |
| `lib/wasm/` | wasm_sandbox.c | 220 行 | 等 Host-API 稳定后 (Week 4) |

**原则**: 不要提前提取。等到至少第三个 ref_* 需要同样的服务才动手。

---

## 执行路径总图

```
Week 1   Week 2    Week 3    Week 4    Week 5    Week 6    Week 7    Week 8
│        │         │         │         │         │         │         │
P2A.1────P2A.2─────┤         │         │         │         │         │
(janpatch)         │         │         │         │         │         │
       P2B.1───────P2C.1─────P2C.2─────┤         │         │         │
       (lib/control)  (motor PID) (motor FFT+AE) │         │         │
                             P2D.1─────P2D.2─────P2D.3─────┤         │
                             (NILM H-API)  (CNN)   (WASM)  │         │
                                        P2E.1─────P2E.2─────P2E.3────┤
                                        (REPL)    (RESC)    (pytest) │
                                                            P2F.1─────P2F.2
                                                            (pq)     (radar)
                                                                        P2F.3
                                                                        (bms)

Dependencies:
  P2B.1 ← P2C (lib/control 是 motor 的前提)
  P2B.1 ← P2F (所有算法类 ref_* 依赖 control 库)
  P2D.1 ← P2D.2 (先定义 Host-API, 再实现 CNN)
  P2E.1 ← P2E.2 (先 REPL 定义, 再 RESC 脚本)
  P2E.2 ← P2E.3 (先双核仿真运行, 再 pytest 断言)
```

---

## 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| v32f20x TFLM 支持不存在 | 高 | 高 | 备选: 手写 int8 量化推理 + CMSIS-DSP 矩阵乘 |
| v32f20x 硬件 AES 无 Zephyr 驱动 | 中 | 高 | 先调研再投入, 不行就软件 AES (慢但 PSA 仍可认证 Level 1) |
| Renode M0 模型精度不够 | 中 | 中 | 先用 native_sim 跑 M33 逻辑, IPC 用环回模式 |
| WAMR 在 Zephyr 上 ROM/RAM 超限 | 中 | 中 | 已有 wasm_sandbox 编译通过, 但需测 4 镜像下的 ROM 占用 |
| janpatch 在海量 flash 上的性能 | 低 | 中 | 256KB delta 在 96MHz M33 上预计 < 2s, 可接受 |
