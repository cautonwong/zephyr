# Zephyr 工业级开发实操手册

本手册涵盖了 Zephyr 高级生态系统的三大核心实操：自动化测试 (Twister HIL)、性能剖析 (Tracing) 和灾后重建 (Coredump)。

---

## 1. 方案 A：灾后现场还原 (Coredump)

当设备在现场发生 HardFault 或异常崩溃时，Coredump 能捕获那一刻的所有寄存器和内存快照。

### 实操步骤

1.  **触发崩溃**：在串口终端输入指令 `fdb crash`。系统会立即触发一个 NULL 指针解引用错误。
2.  **导出日志**：串口会输出一段以 `** COREDUMP START **` 开始的十六进制数据。
3.  **解析日志**：
    - 将串口数据块保存为本地文件 `crash.log`。
    - 运行解析脚本：
    ```bash
    python3 scripts/coredump/coredump_serial_log_parser.py crash.log coredump.elf
    ```
4.  **调试还原**：使用交叉编译器 GDB 查看现场：
    ```bash
    arm-zephyr-eabi-gdb build/zephyr/zephyr.elf coredump.elf
    (gdb) bt
    ```

---

## 2. 方案 B：Twister 物理硬件 (HIL) 自动化测试

这是实现“持续集成 (CI)”的核心。无需人工干预，脚本自动在真机上跑完所有压力测试。

### 准备工作

1.  **复制模板**：将 `hardware_map.yaml.example` 复制为 `hardware_map.yaml`。
2.  **配置串口**：根据你的 USB 转串口号（如 `/dev/ttyUSB0`）修改 `serial` 字段。

### 运行指令

```bash
# 在 applications 目录下运行
./zephyr/scripts/twister \
  -T tests/flashdb \
  --device-testing \
  --hardware-map hardware_map.yaml \
  -p v32f20x_board/v32f20x/cpuapp_ns \
  -v
```

### 体验

Twister 会全自动完成：编译 -> 烧录 -> 监控串口 -> 自动判定测试结果（Pass/Fail） -> 生成 HTML/XML 报告。

---

## 3. 方案 C：图形化系统性能剖析 (Tracing)

使用 SEGGER SystemView 以“上帝视角”实时观察 CPU 的调度、中断响应和线程抢占。

### 内核配置

已经在 `prj.conf` 中预置了以下调优：

- `CONFIG_TRACING=y`
- `CONFIG_SEGGER_SYSTEMVIEW=y`
- `CONFIG_SEGGER_RTT_MAX_NUM_UP_BUFFERS=3` (防止高频采样丢包)

### 实操步骤

1.  **烧录最新固件**（确保包含上述配置）。
2.  **启动 SystemView**：在电脑端打开 SEGGER SystemView 上位机。
3.  **连接录制**：通过 J-Link 连接板子，点击 "Start Recording"。
4.  **看点**：
    - 实时观察 `app_main` 与 `shell` 线程的优先级切换。
    - 查看计量中断（ADC/Timer）占用了多少微秒的 CPU 时间。
    - 定位导致系统卡顿的长周期中断任务。

---

## 下一步建议

通过这套系统，你已经可以对驱动程序进行 24 小时的“无人值守”压力测试。如果 `v32` 在跑了一万次 FlashDB 写入后依然没有 Crash，你的系统才算真正“工业级”可靠。

基于万高（Vango V32F20x）双核 SoC 平台、Zephyr RTOS 架构以及我们刚刚落地的 sim_axon 闭环电源管理机制，我们需要从**“深度”**与**“广度”**两个维度，为该项目制定一份专业、硬核且具备高度可行性的系统级演进蓝图。  
 ──────

## 架构演进全景图

                           【Vango 工业级双核参考设计生态系统】
                                           |
        +----------------------------------+----------------------------------+
        |                                                                     |
        v (深度 - 算法、控制与系统安全)                                          v (广度 - 多场景覆盖、沙盒化与CI测试)
    +----------------------------------+                              +----------------------------------+
    | 1. 双核 lockless 零拷贝 IPC 链路  |                              | 1. 8大工业参考设计矩阵 (ref_*)   |
    | -> WAVEFORM_SHM 环形队列与 MPU 隔离|                              | -> 提取通用中间件核心 (Common Core)|
    +----------------------------------+                              +----------------------------------+
    | 2. ADRC (LESO) 控制与小波变换    |                              | 2. WASM 运行时沙盒应用生态       |
    | -> 电力暂态谐波/电机振动频域分析    |                              | -> 算法模块热更新，动态 OTA 加载   |
    +----------------------------------+                              +----------------------------------+
    | 3. tinyML 量化推理引擎与 LUT 查表 |                              | 3. 多核异构 Renode 硬件仿真 HIL  |
    | -> TFLite-Micro / CMSIS-NN (int8) |                              | -> Pytest 驱动的多核全链路自动化测试|
    +----------------------------------+                              +----------------------------------+
    | 4. TF-M PSA 零信任安全防御        |                              | 4. 统一 West 拓扑与模块化沉淀    |
    | -> OTA 固件硬件解密与防降级回滚    |                              | -> 将 hal/soc 彻底模块化为 OS 组件|
    +----------------------------------+                              +----------------------------------+
    ──────

## 一、 深度（Depth）维度：算法硬核化与系统安全机制

### 1. 双核 Lockless 零拷贝 IPC 链路与 MPU 隔离

• 当前状态：使用基本的 Zephyr icmsg 共享内存。  
 • 深度演进：  
 • Lockless Ring Buffer：在非缓存（Non-cacheable）物理内存区 app.overlay waveform_shm 中，设计无锁循环队列。M0 采样中断直接以 DMA 搬运波形，M33 以指针偏移直接读取，实现 CPU 零拷贝消耗。  
 • MPU 防区隔离：配置 ARM MPU，强制将 M0 专属的计量数据区设为 M33 只读防区。即使 M33 上的应用程序或 WASM 沙盒发生堆栈溢出，也绝无法篡改 M0 的物理计量数据，保证计量数据的法定高可靠性。

### 2. ADRC（主动扰动抑制控制）与小波变换在信号处理中的应用

• 当前状态：仅在 tests/algo_dsp 中进行了基础 PID/LESO 模拟。  
 • 深度演进：  
 • LESO（线性扩张状态观测器）实时去噪：将 LESO 部署在 M33 端，实时观测电力网格中的未知负荷扰动与电磁噪声。通过观测状态 z₂（总扰动估算值）在控制律中进行前馈补偿：

           u₀(t) - z₂(t)
    u(t) = ─────────────
                 b

    *   **一维小波变换（DWT）**：对于暂态负荷（如变频空调启动），FFT 的时域局限性明显。引入小波分析进行多尺度时频局部化分解，准确提取电流暂态突变点的奇异性特征（Singularity Detection）。


### 3. tinyML 量化推理引擎（CMSIS-NN / TFLM）与 LUT 极限优化

• 当前状态：仅有虚拟 sim_axon 驱动。  
 • 深度演进：  
 • int8 量化网络部署：引入 TensorFlow Lite for Microcontrollers，将 NILM 分类模型量化为有符号对称 8 位整型（INT8）。  
 • 全链路 LUT 优化：不仅是后处理，在前处理的加窗、归一化阶段，全面借鉴 sdk-edge-ai 的 lut_init 机制，将浮点转换全部在初始化阶段映射为静态数组，在线推理只执行整型乘加（MAC）与内存查表，单次推理时延压低至 10ms 以内。

### 4. TF-M PSA 零信任安全防御

• 当前状态：开启了默认的 TF-M Medium 配置文件。
• 深度演进：
• 硬件加解密集成：使用 M33 核心的 Crypto 硬件外设（ rng0 与硬件 AES 加速），固件升级包（OTA Bin）在 slot1 中直接进行硬件在线流解密（On-the-fly Decryption），防止固件代码在 Flash 中被物理抄板读取。
• 防降级硬件回滚锁：在 TF-M 中绑定物理 OTP（一次性可编程）或 NV Counters 计数器。每次固件更新验证通过后递增安全版本号，引导区（BL2）一旦检测到升级固件版本低于 NV Counter，拒绝引导，抵御降级重放攻击。

──────

## 二、 广度（Breadth）维度：参考设计沉淀与生态构建

### 1. 8大工业参考设计矩阵 ( ref\_\* ) 的中间件沉淀

• 当前状态： apps/ref\_\* 下均为空目录占位符。
• 广度演进：
• 通用中间件核心（Common Middleware Core）：将 vango_demo 中沉淀成熟的通信（PPP / 4G Modem）、工业协议（Modbus RTU）、存储（FlashDB / KVDB）和电源管理（low_power.c）提取出来，作为独立的子模块库（Library）。
• 参考设计快速填充：
• ref_nilm_meter ：导入通用的波形接收服务与 tinyML 模块，作为电力负荷参考设计。
• ref_motor_pred_maint ：导入 CMSIS-DSP FFT 特征提取器与振动异常检测模型，作为工业电机维护设计。

### 2. WASM 运行时沙盒应用生态（WASM App Store）

• 当前状态：仅能通过 wasm_sandbox.c 加载静态字节码。
• 广度演进：
• 定义 Host-API 硬件导出契约：在 M33 侧定义一组规范的 Host-API（类似于 WebAPI），将 uart_write 、 flashdb_set 、 ipc_send 导出给 WASM 沙盒。
• 动态应用分发（Dynamic App Loading）：主固件作为底层 OS 一次性烧录，上层算法和业务逻辑全部编译为 .wasm 字节码。用户可以通过无线网络（BLE/4G）热更新应用，沙盒自动校验、加载并执行，实现“设备即平台（Device as a Platform）”的先进生态。

### 3. 多核异构 Renode 仿真平台与 Pytest 全链路自动化 HIL

• 当前状态：Renode 脚本 v32.repl 仅支持单核 M33 仿真。
• 广度演进：
• 构建双核虚拟主板：修改 .repl 和 .resc ，在 Renode 中实例化两颗 CPU： cpu0 （Cortex-M0）和 cpu1 （Cortex-M33），并映射一块共同的虚拟 SRAM 作为共享内存，连接虚拟 MBOX 中断控制器。
• Pytest HIL 测试矩阵：结合 testcase.yaml ，利用 Python 自动化脚本向虚拟串口注入模拟的电器暂态波形电流数据，通过断言（Assert）自动判断 M0 采样判定、M33 接收通知、tinyML 唤醒分类、串口 Shell 输出的完整闭环时序，打造无人值守的嵌入式 CI 链路。
