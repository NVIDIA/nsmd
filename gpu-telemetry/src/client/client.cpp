/**
 * @brief C API implementation for GPU telemetry client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpu-telemetry/client.h"
#include "gpu-telemetry/error.h"
#include "ipc.hpp"
#include <memory>

using namespace gpu::telemetry;

struct gpu_telemetry_ctx {
    std::unique_ptr<IpcClient> client;
    gpu_telemetry_callback callback{nullptr};
    void* user_data{nullptr};
};

int gpu_telemetry_init(gpu_telemetry_ctx** ctx) {
    if (!ctx) {
        return static_cast<int>(Error::InvalidArgument);
    }

    try {
        auto newCtx = std::make_unique<gpu_telemetry_ctx>();
        newCtx->client = std::make_unique<IpcClient>(
            "/tmp/gpu-telemetry.sock");
        *ctx = newCtx.release();
        return static_cast<int>(Error::Success);
    } 
    catch (const std::system_error& e) {
        return static_cast<int>(e.code().value());
    }
    catch (...) {
        return static_cast<int>(Error::ConnectionFailed);
    }
}

int gpu_telemetry_send_message(gpu_telemetry_ctx* ctx,
                              const uint8_t* message,
                              size_t message_len,
                              gpu_telemetry_callback callback,
                              void* user_data) {
    if (!ctx || !message || !callback || message_len == 0) {
        return static_cast<int>(Error::InvalidArgument);
    }

    try {
        ctx->callback = callback;
        ctx->user_data = user_data;

        stdexec::start_detached(
            ctx->client->sendMessage({message, message_len}) |
            stdexec::then([ctx](std::vector<uint8_t>&& response) {
                if (ctx->callback) {
                    ctx->callback(ctx->user_data, 
                                response.data(), 
                                response.size());
                }
            }));

        return static_cast<int>(Error::Success);
    }
    catch (const std::system_error& e) {
        return static_cast<int>(e.code().value());
    }
    catch (...) {
        return static_cast<int>(Error::SendFailed);
    }
}

void gpu_telemetry_free(gpu_telemetry_ctx* ctx) {
    delete ctx;
}
