# Zephyr 工业级开发实操手册

本手册涵盖了 Zephyr 高级生态系统的三大核心实操：自动化测试 (Twister HIL)、性能剖析 (Tracing) 和灾后重建 (Coredump)。

---

## 1. 方案 A：灾后现场还原 (Coredump)

当设备在现场发生 HardFault 或异常崩溃时，Coredump 能捕获那一刻的所有寄存器和内存快照。

### 实操步骤
1.  **触发崩溃**：在串口终端输入指令 `fdb crash`。系统会立即触发一个 NULL 指针解引用错误。
2.  **导出日志**：串口会输出一段以 `** COREDUMP START **` 开始的十六进制数据。
3.  **解析日志**：
    *   将串口数据块保存为本地文件 `crash.log`。
    *   运行解析脚本：
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
*   `CONFIG_TRACING=y`
*   `CONFIG_SEGGER_SYSTEMVIEW=y`
*   `CONFIG_SEGGER_RTT_MAX_NUM_UP_BUFFERS=3` (防止高频采样丢包)

### 实操步骤
1.  **烧录最新固件**（确保包含上述配置）。
2.  **启动 SystemView**：在电脑端打开 SEGGER SystemView 上位机。
3.  **连接录制**：通过 J-Link 连接板子，点击 "Start Recording"。
4.  **看点**：
    *   实时观察 `app_main` 与 `shell` 线程的优先级切换。
    *   查看计量中断（ADC/Timer）占用了多少微秒的 CPU 时间。
    *   定位导致系统卡顿的长周期中断任务。

---

## 下一步建议
通过这套系统，你已经可以对驱动程序进行 24 小时的“无人值守”压力测试。如果 `v32` 在跑了一万次 FlashDB 写入后依然没有 Crash，你的系统才算真正“工业级”可靠。
