# ADR-0001: IPC 序列化保持 struct header + memcpy，不引入 CBOR/Protobuf

**决定**: IPC 消息序列化使用 `struct ipc_header {magic + type + len}` + memcpy payload，不引入 QCBOR 或其他结构化序列化库。

**背景**: 路线图 v2.0 曾计划用 QCBOR 替代手写 TLV，声称 "QCBOR 已入 lib/"。经代码审计发现 QCBOR 只在 `lib/qcbor/` 作为 TF-M 编译依赖存在，从未被应用层使用，也未在 `lib/CMakeLists.txt` 中链接。当前 IPC 使用 Zephyr `ipc_service` (RPMsg) 框架，消息格式为固定结构体头部 + 字节 payload。

**理由**:
1. 项目当前的 IPC 场景只有两类消息——计量数据和波形指针——类型可枚举，无需 schema 演进
2. struct header + memcpy 在 Cortex-M33 上产生零解析开销，CBOR 解码每次要遍历字节流
3. 不增加 ROM 占用（QCBOR 链接进来保守估计 +8-12KB）
4. 如果未来需要 schema 演化，在 struct header 中加 version 字段即可，无需引入序列化框架

**后果**: IPC 消息收发双方必须编译同样的 struct 定义。这是一种紧凑耦合，但对于单一厂商的双核 SoC 项目是可接受的。

**状态**: accepted
