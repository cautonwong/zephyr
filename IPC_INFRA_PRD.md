# PRD: V32F20x Inter-Core Communication (IPM/RPMsg) Infrastructure

## Problem Statement
The V32F20x SoC features a dual-core architecture (Cortex-M0 and Cortex-M33). Currently, these cores operate in isolation without a formal mechanism for high-performance data exchange or synchronization. This limits the ability to build integrated gateway applications where one core handles real-time metering and the other manages connectivity and storage.

## Solution
Implement a robust Inter-Processor Communication (IPC) layer based on Zephyr's IPM (Inter-Processor Messaging) and OpenAMP (RPMsg) framework. This will provide a standardized, asynchronous communication channel between the M0 and M33 cores, supporting both small message notifications and large-scale data sharing via Shared Memory (SHM).

## User Stories
1. As a system architect, I want a standardized IPC mechanism so that I can decouple real-time tasks from connectivity logic.
2. As a firmware developer, I want to use RPMsg APIs to send and receive structured data between cores without manually managing hardware registers.
3. As a metering core (CPU0), I want to push high-frequency accumulation data to the gateway core (CPU1) with minimal latency.
4. As a gateway core (CPU1), I want to receive metering events and persist them to FlashDB asynchronously.

## Implementation Decisions
- **Hardware Abstraction**: Implement a dedicated Mailbox (MBOX) driver in `modules/soc/v32f20x/drivers` that abstracts the Vango hardware IPC registers.
- **Shared Memory (SHM)**: Define specific memory regions in the DeviceTree (`zephyr,ipc_shm`) for RPMsg ring buffers.
- **Protocol Stack**: Use Zephyr's OpenAMP library to provide the RPMsg transport layer.
- **Thread Safety**: Use Zephyr's `k_mbox` or `k_msgq` to buffer incoming inter-core messages on each side.
- **Lifecycle Management**: Implement a "Remote Processor Service" to handle core startup and synchronization.

## Testing Decisions
- **Ping-Pong Test**: A unit test where CPU0 sends a "Ping" and verifies a "Pong" response from CPU1 within a 10ms deadline.
- **Throughput Benchmark**: Measure the maximum data rate sustainable over RPMsg without dropping packets.
- **Stress Test**: Run continuous communication for 24 hours while performing FlashDB operations on CPU1 to check for concurrency issues.

## Out of Scope
- TrustZone-based secure memory partitioning (will be handled in the Security PRD).
- Dynamic core power-down (will be handled in the Power Management PRD).
