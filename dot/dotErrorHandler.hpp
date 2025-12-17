/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#pragma once

#include "libnsm/firmware-utils.h"

#include <cstdint>
#include <string>
#include <tuple>

namespace nsm
{
namespace dot
{

/**
 * @brief Format DOT device error into human-readable message
 *
 * Converts DOT command completion code and reason code into a tuple
 * containing the reason code and a human-readable error message.
 *
 * @param cc Completion code from DOT command response
 * @param reasonCode Reason code from DOT command response
 * @param operation DOT operation command enum (e.g., NSM_FW_DOT_LOCK)
 * @return Tuple of (reasonCode, errorMessage)
 */
std::tuple<uint16_t, std::string>
    formatDotDeviceError(uint8_t cc, uint16_t reasonCode,
                         nsm_firmware_commands operation);

} // namespace dot
} // namespace nsm
