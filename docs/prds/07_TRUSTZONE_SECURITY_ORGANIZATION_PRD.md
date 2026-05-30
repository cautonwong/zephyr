# V32F20x TrustZone 安全架构与组织规范 (TrustZone & Security Organization PRD)

## 1. 愿景与对标 (Vision & Benchmarking)
本规范深度对标 **Nordic nRF5340** 的双核安全架构。利用 V32F20X 的 ARMv8-M TrustZone 技术，构建硬件级的隔离体系。将芯片从单一运行环境演进为“安全世界 (Secure)”与“非安全世界 (Non-Secure)”并存的工业级高安全底座。

> [!WARNING]
> **架构纠偏与明确**：参照 nRF5340 的标准设计（`cpuapp` 作为主控核心拥有 TrustZone，`cpunet` 作为从核），V32F20X 中**拥有 TrustZone 的 Cortex-M33 必须作为系统的 Primary Core (CPU0)**，而 Cortex-M0 作为 Secondary Core (CPU1)。
> 由 CPU0 (M33) 承担安全启动与 Root of Trust 角色，并负责在非安全状态下引导 CPU1 (M0)。以往文档中关于 CPU1 是 M33 的定义属于严重误导，已在此规范中全面修正。

## 2. 核心架构模型 (Architectural Model)

### 2.1 角色职责划分 (Role Definitions)
| 核心/实体 | 运行模式 | 角色定义 | 核心功能 |
| :--- | :--- | :--- | :--- |
| **cpuapp (CM33) - S** | Secure | **根信任 (Root of Trust)** | 运行 MCUBoot (安全引导)、TF-M (Trusted Firmware-M)、密钥管理 (PSA Crypto)、硬件隔离配置 (SAU/SPU)。 |
| **cpuapp (CM33) - NS** | Non-Secure | **业务大脑 (Application)** | 运行 Zephyr OS、网络栈、文件系统、北向通信，**并负责加载固件与启动 cpumeter**。 |
| **cpumeter (CM0) - NS** | Non-Secure | **实时工头 (Coprocessor)** | **纯非安全环境**运行极速 ADC 采样、FOC 闭环、高频 IO 响应。 |

### 2.2 存储空间阴阳分割 (Memory Partitioning)
参考 nRF5340 的 SPU/SAU 配置进行静态划分：

*   **Flash 划分：**
    *   `Secure Sector` (低地址): 存放 Bootloader (MCUBoot) 与 Secure Image (TF-M)。
    *   `Non-Secure Sector`: 存放 cpuapp-NS 业务固件、cpumeter-NS 固件镜像、DTS 资源。
*   **RAM 划分：**
    *   `SRAM_S` (256KB): 仅限 Secure 世界访问，存放 TF-M 运行栈与加密中间变量。**硬件级拦截 NS 侧访问**。
    *   `SRAM_NS` (768KB): 供 cpuapp-NS 运行主要业务逻辑。
    *   `SRAM_SHARED` (64KB): 顶端区域，作为 cpuapp-NS 与 cpumeter-NS 的 OpenAMP/RPMsg 交换缓冲区。

## 3. 启动流程与安全交互规范 (Boot & Secure Interaction)

### 3.1 信任链与启动流程 (Boot Sequence)
1. **Reset (ROM Boot)**：系统上电，**cpuapp (M33)** 默认在 Secure 模式下启动。
2. **Secure Bootloader**：cpuapp 执行 MCUBoot，验证 TF-M 与 cpuapp-NS 业务固件的数字签名。
3. **TF-M 初始化**：跳转至 TF-M，配置系统级保护单元 (SPU/SAU)，将 Flash、RAM 和外设严格划分为 S 和 NS 区域。
4. **NS OS 启动**：TF-M 释放控制权，跳转至 cpuapp 的 Non-Secure 环境，启动 Zephyr OS。
5. **从核加载**：cpuapp-NS 将 cpumeter(M0) 的固件加载到对应内存，释放 cpumeter 的硬件复位信号，cpumeter 在纯 Non-Secure 模式下启动并运行。

### 3.2 非安全调用 (NSC - Non-Secure Callable)
*   **原则**：NS 模式下的业务代码（无论 cpuapp 还是 cpumeter）**严禁**直接操作加密寄存器。
*   **实现**：cpuapp-NS 需通过 `psa_framework` 定义的 NSC 陷阱入口，陷入 Secure 世界调用加密/签名服务（如 PSA Crypto API）。
*   **对标**：完全遵循 ARM PSA (Platform Security Architecture) 规范。

### 3.3 硬件资源所有权 (Peripheral Ownership)
*   **Secure-Only**：TRNG (随机数)、内部安全时钟、特定密钥槽位、SAU/SPU 控制寄存器（仅 cpuapp-S 可访问）。
*   **Non-Secure 可见**：UART, SPI, AD7616, PWM 等业务外设（分配给 cpuapp-NS 或 cpumeter-NS）。
*   **权限隔离**：若 NS 代码尝试访问 Secure 区域，硬件必须立即触发 `SecureFault`。

## 4. 跨核通信集成 (Inter-Processor Communication)

### 4.1 借鉴 nRF5340 的 Mailbox 机制
*   cpuapp (M33) NS 拥有对 cpumeter (M0) NS 的生命周期管理权限。
*   两者通过标准的 **RPMsg over OpenAMP** 进行非对称多处理通信。
*   **实时性保证**：高频采样数据通过 `SRAM_SHARED` 直接内存交换，信令同步通过硬件 Mailbox/IPC 寄存器触发跨核中断。

## 5. 软件组织结构 (Software Organization)

### 5.1 目录层级映射 (对标 Zephyr 标准)
为了适配标准 Zephyr 架构，目标板配置应反映主从及安全关系，完全对标 nRF5340DK：
*   `zephyr/soc/arm/vango/v32f20x/secure/`：存放 TF-M 适配层代码。
*   `boards/vango/v32f20x_board/`：作为统一个板级目录，其下通过 qualifier 区分。
*   `boards/vango/v32f20x_board/v32f20x_board_v32f20x_cpuapp.dts`：定义 **cpuapp Secure 目标**。
*   `boards/vango/v32f20x_board/v32f20x_board_v32f20x_cpuapp_ns.dts`：定义 **cpuapp Non-Secure 目标**。
*   `boards/vango/v32f20x_board/v32f20x_board_v32f20x_cpumeter.dts`：定义 **cpumeter 纯非安全目标**。

### 5.2 编译流水线 (Build Workflow)
*   构建 cpuapp 的 `_ns` 目标时，Zephyr 构建系统自动作为子镜像触发 Secure 侧 (TF-M) 的编译。
*   构建系统自动执行 **Merge & Sign**，生成 `tfm_s.bin` + `app_ns.bin` 的复合签名固件。
*   cpumeter 的固件单独编译，并通过 `CONFIG_CORE_IMAGE` 机制内嵌至 cpuapp-NS 固件中，或单独烧写。

---
**本规范即日起生效。后续所有 Issue #8 (IPC 标准化) 和 #9 (MCUBoot 安全启动) 的实现必须严格遵守本 TrustZone 主从隔离准则。**
