/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ## Logging Macro Levels:
 *
 * 1. **LG2_<LEVEL>_RAW**
 *    - Logs the message without checking for flooding prevention.
 *    - Includes file name, line number
 *    - Includes object name, and device identifier if current object is an
 *      NsmObject.
 *    - Use when object and device details are needed without rate limiting
 *      or when shouldLog() arguments are different that logged ones.
 *
 * 2. **LG2_<LEVEL>**
 *    - Logs the message with flooding prevention.
 *    - Includes file name, line number
 *    - Includes object name, and device identifier if current object is an
 *      NsmObject.
 *    - Use when detailed logging is required, but flooding prevention is
 *      necessary.
 *
 * @note Use the appropriate macro depending on whether flooding prevention is
 * needed.
 */

#pragma once

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <string>

/* Logging macros helpers */
#define __FILENAME__ (strrchr(__FILE__, '/') + 1)

#define TO_UPPER_emergency "EMERGENCY"
#define TO_UPPER_alert "ALERT"
#define TO_UPPER_critical "CRITICAL"
#define TO_UPPER_error "ERROR"
#define TO_UPPER_warning "WARNING"
#define TO_UPPER_notice "NOTICE"
#define TO_UPPER_info "INFO"
#define TO_UPPER_debug "DEBUG"

/* Macro to check for odd-numbered arguments to prevent flooding. */
#define SHOULD_LOG(msg, ...)                                                   \
    std::apply(                                                                \
        [this](auto&&... args) { return this->shouldLog(msg, args...); },      \
        StateChangeLogger::extractOddArgs(__VA_ARGS__))

/* Logging macros which will store log level, fileName and line */
#define LG2_LEVEL_FILE(level, msg, ...)                                        \
    lg2::level("[{LEVEL} {FILENAME}:{LINE}] " msg, "LEVEL",                    \
               std::string(TO_UPPER_##level), "FILENAME",                      \
               std::string(__FILENAME__), "LINE", __LINE__, ##__VA_ARGS__)

#define LG2_LEVEL(level, msg, ...)                                             \
    if constexpr (std::is_base_of_v<nsm::NsmObject,                            \
                                    std::decay_t<decltype(*this)>>)            \
    {                                                                          \
        auto& nsmObject = *dynamic_cast<nsm::NsmObject*>(this);                \
        LG2_LEVEL_FILE(level, msg ", name: {NAME}, devId: {DEVID}",            \
                       ##__VA_ARGS__, "NAME", nsmObject.getName(), "DEVID",    \
                       nsmObject.getDeviceIdentifier());                       \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        LG2_LEVEL_FILE(level, msg, ##__VA_ARGS__);                             \
    }

#define LG2_EMERGENCY(msg, ...) LG2_LEVEL(emergency, msg, ##__VA_ARGS__)
#define LG2_ALERT(msg, ...) LG2_LEVEL(alert, msg, ##__VA_ARGS__)
#define LG2_CRITICAL(msg, ...) LG2_LEVEL(critical, msg, ##__VA_ARGS__)
#define LG2_ERROR(msg, ...) LG2_LEVEL(error, msg, ##__VA_ARGS__)
#define LG2_WARNING(msg, ...) LG2_LEVEL(warning, msg, ##__VA_ARGS__)
#define LG2_NOTICE(msg, ...) LG2_LEVEL(notice, msg, ##__VA_ARGS__)
#define LG2_INFO(msg, ...) LG2_LEVEL(info, msg, ##__VA_ARGS__)
#define LG2_DEBUG(msg, ...) LG2_LEVEL(debug, msg, ##__VA_ARGS__)

/* Macros including argument checks for flooding prevention */
#define LG2_LEVEL_FLT(level, msg, ...)                                         \
    if (SHOULD_LOG(msg, ##__VA_ARGS__))                                        \
    {                                                                          \
        LG2_LEVEL(level, msg, ##__VA_ARGS__);                                  \
    }

#define LG2_EMERGENCY_FLT(msg, ...) LG2_LEVEL_FLT(emergency, msg, ##__VA_ARGS__)
#define LG2_ALERT_FLT(msg, ...) LG2_LEVEL_FLT(alert, msg, ##__VA_ARGS__)
#define LG2_CRITICAL_FLT(msg, ...) LG2_LEVEL_FLT(critical, msg, ##__VA_ARGS__)
#define LG2_ERROR_FLT(msg, ...) LG2_LEVEL_FLT(error, msg, ##__VA_ARGS__)
#define LG2_WARNING_FLT(msg, ...) LG2_LEVEL_FLT(warning, msg, ##__VA_ARGS__)
#define LG2_NOTICE_FLT(msg, ...) LG2_LEVEL_FLT(notice, msg, ##__VA_ARGS__)
#define LG2_INFO_FLT(msg, ...) LG2_LEVEL_FLT(info, msg, ##__VA_ARGS__)
#define LG2_DEBUG_FLT(msg, ...) LG2_LEVEL_FLT(debug, msg, ##__VA_ARGS__)
