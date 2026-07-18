/*
 * PSA Security Services Implementation
 * Phase 5 Dimension 3: Trust & Attestation
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_APP_FEATURE_SECURITY)
#include <psa/initial_attestation.h>
#include <psa/crypto.h>

LOG_MODULE_REGISTER(security_svc, LOG_LEVEL_INF);

#define ATTEST_CHALLENGE_SIZE 32
#define ATTEST_TOKEN_MAX_SIZE 1024

static uint8_t attest_token[ATTEST_TOKEN_MAX_SIZE];
static uint8_t challenge[ATTEST_CHALLENGE_SIZE] = "VANGO-PHASE5-CHALLENGE-2026";

static int cmd_security_attest(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    psa_status_t status;
    size_t token_size;

    shell_print(sh, "老板，正在为您启动 PSA 实体证明 (Remote Attestation)...");

    /* 1. Request Attestation Token */
    status = psa_initial_attest_get_token(challenge, ATTEST_CHALLENGE_SIZE,
                                         attest_token, ATTEST_TOKEN_MAX_SIZE,
                                         &token_size);

    if (status != PSA_SUCCESS) {
        shell_error(sh, "证明令牌获取失败 (Error %d)", (int)status);
        return -EIO;
    }

    shell_print(sh, "2. 证明令牌获取成功！(大小: %d 字节)", (int)token_size);
    shell_print(sh, "3. 令牌指纹 (前 16 字节):");
    shell_hexdump(sh, attest_token, 16);
    
    shell_print(sh, "4. 提示：请运行 cwt_verify.py 脚本校验该 CBOR 令牌的合法性。");

    return 0;
}

int security_service_init(void)
{
    LOG_INF("PSA Security Service Initialized (Attestation Support Enabled)");
    return 0;
}

/* --- Shell Commands Registration --- */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_security,
    SHELL_CMD_ARG(attest, NULL, "Trigger PSA Remote Attestation.", cmd_security_attest, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(security, &sub_security, "PSA Security commands", NULL);

#endif /* CONFIG_APP_FEATURE_SECURITY */
