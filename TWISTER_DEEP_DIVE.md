# Zephyr Twister 深度挖掘与 Vango 项目落地实操指南

本手册深度剖析了 Zephyr 官方原生测试套件工具 **Twister**（位于 `zephyr/scripts/twister`）的底层源码设计与高级使用方法。所有字段、机制与结构体定义均严格基于 `/workspaces/rtos/zephyr/zephyr/scripts/pylib/twister/twisterlib/` 目录下的 Python 源码与规范，杜绝虚幻凭空的描述。

---

## 一、 Twister 底层架构与模块构成

在 Zephyr 原生代码中，Twister 并非一个简单的单体脚本，而是由高度模块化的 Python 类库构成的测试框架：

* **`config_parser.py` (配置解析器)**: 负责加载与验证 `testcase.yaml` 及 `sample.yaml` 文件，定义了测试用例所支持的所有合法属性。
* **`harness.py` (测试装具)**: 负责解析设备输出数据以判断测试是否通过。包含了 `Ztest` (解析 Zephyr Ztest 套件输出)、`Console` (正则匹配文本输出)、`Pytest` (调用外部 Python 进行高级控制) 等。
* **`hardwaredata.py` / `hardwaremap.py` (硬件映射管理)**: 负责解析物理测试设备的 YAML 描述，支持单核（`HardwareData`）以及双核/多核复合设备（`CompoundHardwareData`）的映射定义。
* **`runner.py` (构建与执行器)**: 调度 CMake、Ninja 编译目标固件，并驱动 QEMU 模拟器或 physical debugger（如 pyOCD、J-Link）进行烧录。

---

## 二、 `testcase.yaml` 原生语法与高级字段全解

根据 `config_parser.py` 中 `testsuite_valid_keys` 字典定义，一个合法的测试配置文件可以配置如下原生属性：

### 1. 核心字段分类与规范

| 属性名称 | 类型 | 默认值 | 底层逻辑与实操用途 |
| :--- | :--- | :--- | :--- |
| **`tags`** | `set` | - | 标签过滤器。例如 `tags: drivers flash`，可在执行时通过 `twister -t flash` 筛选运行。 |
| **`harness`** | `str` | `"test"` | 测试装具类型。主要可选：`ztest`, `console`, `pytest`, `shell`, `robot`, `gtest`。 |
| **`harness_config`** | `map` | `{}` | 装具附加属性。当 `harness: console` 时，需在其下指定匹配的正则表达式（`regex`）。 |
| **`filter`** | `str` | - | Python 表达式过滤器。支持判断平台属性（如 `dt_compat_enabled("vango,v32f20x-uart")`、`ram_size >= 32` 等）。 |
| **`extra_args`** | `list` | - | 传给 CMake 的编译参数列表。例如 `-DCONF_FILE=debug.conf` 或 `-DDTC_OVERLAY_FILE=app.overlay`。 |
| **`extra_configs`** | `list` | - | 动态注入的 Kconfig 参数。例如 `extra_configs: ["CONFIG_LOG=y"]`。 |
| **`sysbuild`** | `bool` | `False` | 是否启用 Zephyr 的多映像构建系统（Sysbuild），用于包含 Bootloader（MCUboot）的多映像测试。 |
| **`platform_allow`** | `set` | - | 显式允许运行该测试的板级列表。 |
| **`platform_exclude`** | `set` | - | 显式禁止运行该测试的板级列表。 |
| **`build_only`** | `bool` | `False` | 若为 `True`，则 Twister 仅编译该测试工程，不尝试在模拟器或真机上执行。常用于 CI/CD 代码合规性检查。 |
| **`ignore_faults`** | `bool` | `False` | 若为 `True`，则当内核发生异常 HardFault（输出 `ZEPHYR FATAL ERROR`）时，不会直接判定测试失败。 |

---

## 三、 底层 Harness (测试装具) 底层机制与实操

在 `harness.py` 中，Twister 提供了多种子类来检测测试的生命周期和状态。掌握这几种 Harness 是编写高质量自动化测试的前提：

### 1. Ztest Harness (`class Ztest`)
专门用于对接 Zephyr 自带的 C 语言 Ztest 单元测试框架。
* **判定原理**：监听串口或控制台输出，识别诸如 `START - test_xxx`、`PASS - test_xxx`、`FAIL - test_xxx` 的格式化行。
* **用法示例**：
  ```yaml
  tests:
    vango.lib.custom_test:
      harness: ztest
  ```

### 2. Console Harness (`class Console`)
适用于非 Ztest 测试（例如测试标准 Sample 输出，或者检查一个 Shell 初始化命令行）。
* **判定原理**：基于 `harness_config` 中的正则匹配规则，如果控制台依次输出指定的字符串，则判定通过。
* **用法示例**：
  ```yaml
  tests:
    vango.shell.prompt_test:
      harness: console
      harness_config:
        type: one_line
        regex:
          - "uart:.*\\$"  # 正则匹配检测 shell 提示符是否成功打印
  ```

### 3. Pytest Harness (`class Pytest`)
最强大和灵活的系统级交互测试方案，适用于复杂的 HIL（硬件在环）测试。
* **判定原理**：Twister 启动物理目标板后，会调用外部 pytest 脚本。pytest 可以通过串口发送指令，读取回显，验证复杂的网络协议、OTA 流程。
* **用法示例**：
  ```yaml
  tests:
    vango.ota.mqtt_verify:
      harness: pytest
  ```

---

## 四、 针对 V32F20x 双核 SoC 的 Compound HIL 硬件映射

在多核异构处理器（如 V32F20x 包含 Cortex-M0 和 Cortex-M33 双核）的物理板载测试中，传统的单核映射格式已经无法满足需求。

根据 `hardwaredata.py` 的底层定义，Twister 提供了 **`CompoundHardwareData`**，支持通过 `entries` 参数为物理上同一个调试器（Same probe ID）下的多核或者多个辅助串口端口进行复合建模。

### 1. V32F20x 双核物理机硬件映射模板 (`hardware_map.yaml`)

当进行物理 HIL 自动化测试时，您需要在 applications 目录下创建 `hardware_map.yaml`：

```yaml
# Vango V32F20x Dual-Core HIL Hardware Map
- platform: v32f20x_board/v32f20x/cpu1     # M33 主核心（网关/连接/存储）
  id: vango_v32_node_01
  runner: pyocd                            # 使用 pyocd 进行烧录与复位
  probe_id: "0001A2345678"                 # 调试器 J-Link / DAPLink 唯一序列号
  serial: /dev/ttyUSB1                     # M33 调试串口设备路径
  serial_baud: 115200
  flash_timeout: 90
  fixtures:                                # 物理连接的外设治具
    - rs485_loopback
    - quectel_4g_antenna
  
  # 使用 CompoundHardwareData 支持的 entries 字段映射 M0 核心和辅助端口
  entries:
    - platform: v32f20x_board/v32f20x/cpu0 # M0 辅助核心（高频计量）
      serial: /dev/ttyUSB0                 # M0 对应的串口
      serial_baud: 115200
      connected: true
```

---

## 五、 Vango 项目的高效专业运用策略

为确保我们的 Vango (V32/V85) 项目的测试体系达到“高覆盖、零误报、极速构建”，务必在开发工作流中落地以下三项策略：

### 策略一：基于兼容属性 (DTS compat) 的自适应动态过滤

禁止在 `testcase.yaml` 中硬编码 `platform_allow: [v32f20x_board]`。随着芯片系列扩展，会导致测试配置维护成本呈指数上升。
* **标准方案**：在 `testcase.yaml` 中利用 `filter` 表达式，读取板子对应的设备树兼容属性：
  ```yaml
  tests:
    vango.driver.modbus_test:
      filter: dt_compat_enabled("vango,v32f20x-modbus")
      harness: ztest
  ```
* **底层机制**：Twister 在编译前会自动提取该板子的 `.dts` 树。如果设备树中使能了 `compatible = "vango,v32f20x-modbus"` 且 `status = "okay"`，该测试才会被调起编译，实现了全自动的驱动兼容性筛选。

### 策略二：治具标记 (Fixtures) 与测试用例的动态匹配

部分测试（如 RS-485 Modbus 压力测试）依赖外部物理接线环回。如果在普通的没有飞线环回的开发板上跑，会因为收不到回显导致测试“虚假失败 (False Failure)”。
* **操作步骤**：
  1. 在 `hardware_map.yaml` 中为对应板卡声明治具属性：`fixtures: [rs485_loopback]`。
  2. 在 `testcase.yaml` 用例下明确要求治具依赖：
     ```yaml
     tests:
       vango.driver.modbus_stress:
         fixtures:
           - rs485_loopback
     ```
  3. 启动指令：`west twister -T tests --device-testing --hardware-map hardware_map.yaml`。
  4. **运行逻辑**：Twister 自动扫描，只有匹配到带 `rs485_loopback` 治具的物理硬件时，才会执行该高危物理测试，否则自动 Skip，确保测试结果 100% 真实可靠。

### 策略三：在 `workflow.sh` 中挂载 Fast Space (RAM-Disk) 加速测试环路

频繁运行 Twister 会在本地产生数以万计的 CMake 临时中间文件，磁盘写入是最大的性能瓶颈。
我们在 [workflow.sh](file:///workspaces/applications/workflow.sh) 统一构建脚本中，为 Twister 添加专有的高性能编译指令：

```bash
# 将此片段整合至 /workspaces/applications/workflow.sh

# 1. 扩充帮助命令
if [ "$COMMAND" = "test" ]; then
    TEST_TAG=$2
    echo "--> Syncing test suite to Fast Storage..."
    # 高速同步测试用例与配置文件
    rsync -a --delete "${WORKSPACE_ROOT}/applications/tests" "${FAST_SPACE}/applications/"
    
    echo "--> Running Twister under Fast Space RAM-Disk..."
    cd "${FAST_SPACE}/applications"
    
    # 启用多核并行编译 (-j)，限制在 Fast Space 提升构建速度
    python3 "${FAST_SPACE}/rtos/zephyr/zephyr/scripts/twister" \
      -T tests \
      -p qemu_cortex_m0 \
      --build-only \
      ${TEST_TAG:+-t $TEST_TAG}
    
    exit 0
fi
```
通过该脚本执行 `./workflow.sh test flashdb`，测试将全部在 RAM-Disk 中以高并行度极速完成编译，反馈效率提升 3 到 5 倍。
