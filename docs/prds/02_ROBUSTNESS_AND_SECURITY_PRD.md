# 鲁棒性与安全体系愿景 (Robustness & Security Hardening PRD)

## 1. 愿景定义 (The Crown Jewel)
不可侵犯的系统鲁棒性与自愈力，以及绝对安全的执行与存储体系。
设备在断电、死锁或受到网络攻击时，能够保护核心密钥、隔离故障并自动恢复。

## 2. 落地路线与规范

### 2.1 硬件信任根与 TrustZone
*   **规范：** 敏感密钥（如私钥、证书）和核心加密算法绝不能暴露在普通应用空间。
*   **实现：** 
    *   利用 Cortex-M33 的 TrustZone。
    *   集成 TF-M (Trusted Firmware-M)。App 仅通过 NSC (Non-Secure Callable) 接口请求加密运算，无法直接读取密钥内存。

### 2.2 零信任内存隔离 (MPU)
*   **规范：** 限制第三方库或应用层协议栈的破坏力。
*   **实现：** 开启 Zephyr `CONFIG_USERSPACE`。将核心内核服务置于特权模式，将不信任的代码置于用户模式。非法的内存访问将触发 MPU 异常，而不是系统级崩溃。

### 2.3 极高可靠性的 FOTA 与自愈
*   **规范：** 固件更新必须是原子操作，支持断电续传与静默回滚，杜绝“变砖”。
*   **实现：** 
    *   集成 MCUBoot。修改 DTS 定义标准的 Flash Layout (slot0, slot1)。
    *   实现双区启动机制：若新固件启动后未能在 Watchdog 超时前确认自身状态（自检失败），MCUBoot 将在下次重启时自动回滚到旧版本。

### 2.4 系统灾难现场保存 (Core Dump)
*   **实现：** 开启 `CONFIG_DEBUG_COREDUMP`。发生 HardFault 时，将线程堆栈和寄存器状态序列化并转储到外部 Flash 的存储分区，便于后续分析。
