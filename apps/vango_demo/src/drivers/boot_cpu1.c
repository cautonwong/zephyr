#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(boot_cpu1, LOG_LEVEL_INF);

/* V32F20X Multi-core Boot Logic (Surgical Implementation) */
#define V32_SYSCFG0_BASE    0x40200000
#define V32_SYSCFGLP_BASE   0x40102000

#define REG_M0_IVT_BADDR    (V32_SYSCFG0_BASE + 0x64)
#define REG_M0_SEC_CTRL     (V32_SYSCFG0_BASE + 0x78)
#define REG_CM0_CTRL        (V32_SYSCFGLP_BASE + 0x50)

/* Aligned with Phase 5 Dimension 1.1 Flash Topology */
#define M0_FLASH_ENTRY      0x08150000

void boot_cpu1(void)
{
    volatile uint32_t *m0_ivt = (uint32_t *)REG_M0_IVT_BADDR;
    volatile uint32_t *m0_sec = (uint32_t *)REG_M0_SEC_CTRL;
    volatile uint32_t *cm0_ctrl = (uint32_t *)REG_CM0_CTRL;

    LOG_INF("Preparing to boot Cortex-M0 (cpumeter)...");
    
    /* 1. Set Vector Table Base */
    *m0_ivt = M0_FLASH_ENTRY;
    LOG_INF("M0 Vector Table set to 0x%08x", *m0_ivt);

    /* 2. Enable Non-secure access for M0 if running from NS slot */
    *m0_sec = 0x1;

    /* 3. Release M0 from Reset (Bit 0) */
    *cm0_ctrl |= 0x1;
    
    LOG_INF("Cortex-M0 released from reset. IPC handshake pending.");
}

void boot_cpu1_reload(void)
{
    volatile uint32_t *cm0_ctrl = (uint32_t *)REG_CM0_CTRL;
    volatile uint32_t *m0_ivt = (uint32_t *)REG_M0_IVT_BADDR;

    LOG_INF("Initiating Cortex-M0 Hot Reload...");

    /* 1. Hold M0 in Reset (Bit 0 = 0) */
    *cm0_ctrl &= ~0x1;
    LOG_INF("M0 core held in reset.");

    /* 2. Brief wait for core stabilization */
    k_busy_wait(100);

    /* 3. Ensure IVT BADDR is still correct (aligned with cpumeter_partition) */
    *m0_ivt = M0_FLASH_ENTRY;

    /* 4. Release M0 from Reset to boot new firmware */
    *cm0_ctrl |= 0x1;
    LOG_INF("M0 core released from reset. Hot update sequence complete.");
}
