# 领域语言 (Vango Zephyr RTOS 项目)

## Language

**wiring**:
项目中所有连接关系的总称。包括编译流水线 wiring（ workflow.sh → cmake → ninja → flash ）、硬件 seam wiring（ DTS binding → overlay → driver init ）、跨核 wiring（ IPC endpoint ↔ RPMsg ↔ shared memory ）、安全 wiring（ MCUboot ↔ TF-M ↔ NS app ↔ cpumeter ）。
_Avoid_: configuration, integration（太泛）

**seam**:
架构中可独立替换而不影响两侧的接口边界。DTS 是硬件 seam（切换硬件不改 C 代码）、Kconfig 是特性 seam（开关功能不碰源码）、TF-M SAU 是安全 seam（ NS 代码不可访问 S 内存 ）。每个 seam 有容限：可以修改 seam 本身的结构（如增加 DTS binding 属性），但不应绕过 seam（如硬编码寄存器地址）。
_Avoid_: abstraction, layer（太重）

**Sysbuild**:
Zephyr 的多镜像构建编排器。本项目管理 4 个镜像： mcuboot (M33 S) → tfm_s (M33 S) → vango_demo (M33 NS) → cpumeter (M0)。通过 `sysbuild.cmake` 设置各镜像的 BOARD/BOARD_ROOT/CONF_FILE/DTC_OVERLAY_FILE。
_Avoid_: multi-image build（不精确）

**workflow.sh**:
项目的核心编译编排器脚本。处理 RAM-disk 同步（ `/root/fast_space` ）、 ZEPHYR_MODULES 路径拼接、 Sysbuild 参数传递。永远不应绕过 workflow.sh 直接运行 `west build`。
_Avoid_: build script, build system（太泛）

**reference design (ref_*)**:
项目 `apps/ref_*` 下的 10 个行业参考应用。每个是独立的 Zephyr 应用程序，有自己的 Kconfig/CMakeLists/main.c/testcase.yaml。从 `vango_demo` 提取的通用中间件通过 `lib/` 链接，通过 `common_core.h` 统一启动横幅。
_Avoid_: demo app, example（暗示不完整）

**IPC**:
M33 与 M0 之间的跨核通信。使用 Zephyr `ipc_service` 框架（基于 RPMsg 和共享内存），双端点设计（ `data_ept` + `ctrl_ept` ），消息格式为 `struct ipc_header {magic + type + len} + payload`。不需要结构化序列化（如 CBOR 或 Protobuf ）。
_Avoid_: TLV（项目中无此术语）、shared memory（特指 WAVEFORM_SHM 物理区域）

**WAVEFORM_SHM**:
64KB 物理共享内存区域（ `0x20100000` ），位于 SRAM 非缓存区，用于 M0→M33 的零拷贝波形数据传输。划分为 Ping-Pong 缓冲区。M0 写入，M33 通过 IPC 消息接收指针后直接读取。
_Avoid_: ring buffer（当前无实现）、shared region（不精确）

**Host-API**:
WASM 沙盒导出的宿主函数契约。沙盒内的 .wasm 字节码通过 NativeSymbols 注册表调用宿主服务。当前合约： `log_to_host` 、 `get_sensor_data` 。目标合约（ Phase 2D ）： `host_read_power` 、 `host_get_harmonics` 、 `host_db_store` 。
_Avoid_: syscall, bridge API（与 Zephyr/OS 术语冲突）

**tinyML**:
在资源受限 MCU 上运行的轻量级机器学习。本项目涵盖：传感器时频域特征提取（ CMSIS-DSP FFT ）、轻量级神经网络推理（ TFLite-Micro 或手动 int8 量化 ）、异常检测（ Autoencoder MSE ）。推理运行在低优先级抢占式线程中。
_Avoid_: AI, edge AI（范围太宽）

**sim_axon**:
模拟的硬件加速器 PM 驱动。用于原型验证电源管理状态机（ DEFAULT 上拉 / SLEEP 下拉 pinctrl 切换）。产品阶段将被真实硬件加速器驱动替换。
_Avoid_: dummy driver（暗示无价值）
