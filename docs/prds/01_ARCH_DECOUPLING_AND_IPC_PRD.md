# 架构解耦与协同愿景 (Architecture Decoupling & Orchestration PRD)

## 1. 愿景定义 (The Crown Jewel)
高度正交的抽象与解耦 (Orthogonal Abstraction & Decoupling)。
将底层硬件、RTOS 调度、通信协议、业务逻辑完全分离。实现“一套业务代码，多平台无缝编译运行”。在多核（如 Cortex-M0 + M33）环境下，实现高效、低延迟、内存隔离的数据交换。

## 2. 落地路线与规范

### 2.1 彻底的 Devicetree (DTS) 化
*   **规范：** 绝不允许在应用 C 代码中出现硬编码的引脚号（如 `GPIOA`, `PIN_5`）。
*   **实现：** 所有硬件资源（按键、LED、传感器接口、共享内存区）全部在 `boards/` 下的 DTS 和 Overlay 中定义别名 (aliases)。
*   **目标：** `apps/base_app` 可以直接通过切换 `-b` 参数在不同芯片板级上构建。

### 2.2 标准化 IPC (跨核/跨进程通信)
*   **规范：** 严禁通过裸指针直接读写共享内存 (Shared Memory) 进行多核通信。
*   **实现：** 
    *   在 `ipc_service.c` 中集成 Zephyr OpenAMP (RPMsg) 或原生 IPC Service API。
    *   定义一套跨核结构化消息协议（如 Protobuf-c, CBOR）。
*   **多核协同划分：** M0 核负责硬实时 IO 与高频中断响应；M33 核负责复杂的协议栈、文件系统与加密运算。两者通过无锁消息队列交互。

### 2.3 基于 Kconfig 的功能解耦
*   **规范：** 所有的业务组件必须是“可插拔”的。
*   **实现：** 完善 `applications/Kconfig` 菜单树，配合 `CMakeLists.txt` 中的 `target_sources_ifdef`，实现极细粒度的固件尺寸与功能裁剪。
