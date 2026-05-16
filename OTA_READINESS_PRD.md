# PRD: Production Readiness (MCUboot and Secure OTA)

## Problem Statement
The current firmware lacks a secure bootloader and field upgrade capabilities. Any bug or required feature enhancement after deployment would require physical access to the device, which is impractical for industrial/metering scale.

## Solution
Integrate **MCUboot** as the primary bootloader and implement a robust Over-The-Air (OTA) update mechanism. This will allow the device to download, verify, and apply firmware updates remotely and securely using a dual-slot bank approach.

## User Stories
1. As a product owner, I want the device to be upgradeable in the field so that I can fix security vulnerabilities remotely.
2. As a security officer, I want firmware updates to be cryptographically signed so that unauthorized code cannot be executed.
3. As a developer, I want a failsafe update mechanism so that the device doesn't "brick" if an update is interrupted by power loss.
4. As a user, I want the update process to be automatic and transparent.

## Implementation Decisions
- **Bootloader**: Port and integrate `MCUboot` for the Vango architecture.
- **Partition Layout**: Define `mcuboot`, `slot0_partition` (active), and `slot1_partition` (secondary) in the DTS.
- **Image Signing**: Configure a build-time signing process using ECDSA-P256 keys via the `west sign` utility.
- **OTA Backend**: Use the MQTT/TCP stack to download firmware chunks and store them in the `slot1` partition via the Zephyr `flash_img` API.
- **Swap Strategy**: Use the "Swap" mode of MCUboot for failsafe rollback capability.

## Testing Decisions
- **Power-Loss Recovery**: Intentionally cut power during the image swap process and verify that MCUboot can either resume the swap or rollback safely on the next boot.
- **Signature Verification**: Attempt to flash an unsigned or incorrectly signed image and verify that the bootloader refuses to boot it.
- **A/B Swap Test**: Perform 50 successful full-cycle OTA updates and verify the system is stable.

## Out of Scope
- Differential updates (initial version will use full image swaps).
- Multi-core simultaneous update (initial version will update the primary CPU1 core).
