/*
 * WebAssembly (WAMR) Business Isolation Sandbox Service Header
 * Inspired by AkiraOS's lightweight security model.
 */

#ifndef WASM_SANDBOX_H
#define WASM_SANDBOX_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the lightweight WebAssembly sandbox thread.
 * 
 * @return int 0 on success, negative error code on failure.
 */
int wasm_sandbox_init(void);

#ifdef __cplusplus
}
#endif

#endif /* WASM_SANDBOX_H */
