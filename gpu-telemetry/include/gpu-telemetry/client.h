/**
 * @brief C API for GPU telemetry client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque context for GPU telemetry client
 */
struct gpu_telemetry_ctx;

/**
 * @brief Callback function type for async responses
 * @param user_data User data passed to send_message
 * @param response Response message data
 * @param response_len Length of response message
 */
typedef void (*gpu_telemetry_callback)(void* user_data, 
                                     const uint8_t* response, 
                                     size_t response_len);

/**
 * @brief Initialize GPU telemetry context
 * @param[out] ctx Pointer to context pointer
 * @return 0 on success, negative on error
 */
int gpu_telemetry_init(gpu_telemetry_ctx** ctx);

/**
 * @brief Send NSM telemetry message
 * @param ctx Context pointer
 * @param message NSM message data
 * @param message_len Message length
 * @param callback Response callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative on error
 */
int gpu_telemetry_send_message(gpu_telemetry_ctx* ctx,
                              const uint8_t* message,
                              size_t message_len, 
                              gpu_telemetry_callback callback,
                              void* user_data);

/**
 * @brief Free GPU telemetry context
 * @param ctx Context pointer
 */
void gpu_telemetry_free(gpu_telemetry_ctx* ctx);

#ifdef __cplusplus
}
#endif
