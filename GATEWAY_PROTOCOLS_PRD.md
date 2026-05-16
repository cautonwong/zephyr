# PRD: Industrial Gateway Protocol Stack Integration (Modbus/Modem)

## Problem Statement
The current system lacks connectivity with industrial peripherals and cloud services. To function as an industrial gateway, the Vango platform must support standard protocols like Modbus for reading sub-meters and a cellular modem interface (4G Cat.1) for upstream data transmission.

## Solution
Integrate Zephyr's native Modbus subsystem and Modem framework. This will allow the application to act as both a Modbus Master (reading external devices) and an MQTT client over a cellular PPP connection, leveraging standard Socket APIs.

## User Stories
1. As a field operator, I want the gateway to read data from external Modbus-RTU sensors so that I can aggregate all site data.
2. As a cloud developer, I want the device to publish data via MQTT so that I can monitor the site remotely.
3. As a network engineer, I want the 4G modem to be treated as a standard network interface so that I can use standard networking libraries.
4. As a firmware developer, I want to use standard `modbus_read_holding_regs` APIs without handling low-level UART framing manually.

## Implementation Decisions
- **Modbus Architecture**: Use the `subsys/modbus` component of Zephyr. Configure UART interfaces in the SoC layer to support RS-485 transceiver signals (DE/RE).
- **Modem Framework**: Use Zephyr's `drivers/modem` and `subsys/net/lib/ppp` to abstract AT command interactions with the 4G module.
- **Protocol Bridge**: Implement a service that translates Modbus registers into JSON payloads for MQTT.
- **Asynchronous Connectivity**: Use a dedicated thread for network connection management (reconnect logic, signal strength monitoring).

## Testing Decisions
- **Modbus Loopback**: Connect a Modbus simulator to the RS-485 port and verify 100% successful register reads over 10,000 iterations.
- **Network Stress Test**: Perform continuous MQTT publishing while cycling the modem power to ensure robust recovery and no memory leaks in the PPP stack.

## Out of Scope
- Support for proprietary non-standard protocols.
- Wi-Fi or Ethernet connectivity (initial focus is 4G).
