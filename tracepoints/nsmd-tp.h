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
 */

#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER nsmd

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "./nsmd-tp.h"

#if !defined(_NSM_TP_H) || defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define _NSM_TP_H

// clang-format off

#include <lttng/tracepoint.h>
#include "libnsm/base.h"

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    sensor_polling_request_generated,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid,
        const uint8_t*, requestMsg,
        const char*, name
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
        lttng_ust_field_integer(uint8_t, instance_id, requestMsg[2] & 0x1F)
        lttng_ust_field_integer(uint8_t, message_type, requestMsg[4])
        lttng_ust_field_integer(uint8_t, opcode, requestMsg[5])
        lttng_ust_field_string(sensor_name, name)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    priority_polling_started,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    priority_polling_ended,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    socket_send,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid,
        const uint8_t*, nsm_req_msg,
        size_t, msg_size
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
        lttng_ust_field_integer(uint8_t, instance_id, (*(nsm_req_msg + 2)) & 0x1F)
        lttng_ust_field_integer(uint8_t, message_type, *( nsm_req_msg + 4))
        lttng_ust_field_integer(uint8_t, opcode, *( nsm_req_msg + 5))
        lttng_ust_field_integer(size_t, message_size, msg_size)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    socket_recv,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid,
        const struct nsm_msg*, response,
        size_t, msg_size
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
        lttng_ust_field_integer(uint8_t, instance_id, response->hdr.instance_id)
        lttng_ust_field_integer(uint8_t, message_type, response->hdr.nvidia_msg_type)
        lttng_ust_field_integer(uint8_t, opcode, response->payload[0])
        lttng_ust_field_integer(size_t, message_size, msg_size)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    nsmd,
    dbus_sensor_reading_updated,
    LTTNG_UST_TP_ARGS(
        uint8_t, eid,
        const struct nsm_msg*, requestMsg,
        const char*, name
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer(uint8_t, eid, eid)
        lttng_ust_field_integer(uint8_t, instance_id, requestMsg->hdr.instance_id)
        lttng_ust_field_integer(uint8_t, message_type, requestMsg->hdr.nvidia_msg_type)
        lttng_ust_field_integer(uint8_t, opcode, requestMsg->payload[0])
        lttng_ust_field_string(sensor_name, name)
    )
)

#endif /* _NSM_TP_H */

#include <lttng/tracepoint-event.h>

// clang-format on
