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

#pragma once

#include "base.h"
#include "types.hpp"
#include <cstdint>
#include <functional>

/**
 * @brief Tests request encoding function
 *
 * @param[in] function Encode request function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] payloadSize Payload size
 * @param[in] expectedPayload Palyoad used in encode request function
 * @param[out] payload Payload copied from request after nsm_common_req
 */
void testEncodeRequest(
    std::function<int(uint8_t, const uint8_t *, nsm_msg *)> function,
    uint8_t nvidiaMsgType, uint8_t command, uint8_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload);

/**
 * @brief Tests common request encoding function
 *
 * @param[in] function Encode request function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 */
void testEncodeCommonRequest(std::function<int(uint8_t, nsm_msg *)> function,
			     uint8_t nvidiaMsgType, uint8_t command);

/**
 * @brief Tests custom request encoding function
 *
 * @tparam RequestPayload Type of C request structure palyoad after struct
 * nsm_common_req hdr
 * @param[in] function Encode request function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] expectedPayload Palyoad used in encode request function
 * @param[out] payload Encoded payload
 */
template <typename RequestPayload>
void testEncodeRequest(
    std::function<int(uint8_t, const RequestPayload *, nsm_msg *)> function,
    uint8_t nvidiaMsgType, uint8_t command,
    const RequestPayload &expectedPayload, RequestPayload &payload)
{
	testEncodeRequest(
	    [function](uint8_t instanceId, const uint8_t *data, nsm_msg *msg) {
		    return function(
			instanceId,
			reinterpret_cast<const RequestPayload *>(data), msg);
	    },
	    nvidiaMsgType, command, sizeof(RequestPayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}

/**
 * @brief Tests request decoding function
 *
 * @param[in] function Decode request function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] payloadSize Palyoad structure size
 * @param[in] expectedPayload Palyoad used in decode request function
 * @param[out] payload Palyoad decoded from request function
 */
void testDecodeRequest(
    std::function<int(nsm_msg *, uint16_t, uint8_t *)> function,
    uint8_t nvidiaMsgType, uint8_t command, uint8_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload);

/**
 * @brief Tests common request decoding function
 *
 * @param[in] function Decode request function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 */
void testDecodeCommonRequest(std::function<int(nsm_msg *, uint16_t)> function,
			     uint8_t nvidiaMsgType, uint8_t command);

/**
 * @brief Tests custom request decoding function
 *
 * @tparam RequestPayload Type of C request structure palyoad after struct
 * nsm_common_req hdr
 * @param[in] function Decode request function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] expectedPayload Palyoad used in decode request function
 * @param[out] payload Palyoad decoded from request function
 */
template <typename RequestPayload>
void testDecodeRequest(
    std::function<int(nsm_msg *, uint16_t, RequestPayload *)> function,
    uint8_t nvidiaMsgType, uint8_t command,
    const RequestPayload &expectedPayload, RequestPayload &payload)
{
	testDecodeRequest(
	    [function](nsm_msg *msg, uint16_t len, uint8_t *data) {
		    return function(msg, len,
				    reinterpret_cast<RequestPayload *>(data));
	    },
	    nvidiaMsgType, command, sizeof(RequestPayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}

/**
 * @brief Tests custom response encoding function
 *
 * @param[in] function Encode response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 * @param[in] payloadSize Palyoad size used in encode response function
 * @param[in] expectedPayload Palyoad used in encode response function
 * @param[out] payload Payload copied from response after nsm_common_resp
 */
void testEncodeResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, const uint8_t *, nsm_msg *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload);

/**
 * @brief Tests common response encoding function
 *
 * @param[in] function Encode response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 */
void testEncodeCommonResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, nsm_msg *)> function,
    uint8_t nvidiaMsgType, uint8_t command);

/**
 * @brief Tests custom response encoding function
 *
 * @tparam ResponsePayload Type of C response structure palyoad after struct
 * nsm_common_resp hdr
 * @param[in] function Encode response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 * @param[in] expectedPayload Palyoad used in encode response function
 * @param[out] payload Encoded payload
 */
template <typename ResponsePayload>
void testEncodeResponse(std::function<int(uint8_t, uint8_t, uint16_t,
					  const ResponsePayload *, nsm_msg *)>
			    function,
			uint8_t nvidiaMsgType, uint8_t command,
			const ResponsePayload &expectedPayload,
			ResponsePayload &payload)
{
	testEncodeResponse(
	    [function](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
		       const uint8_t *data, nsm_msg *msg) {
		    return function(
			instanceId, cc, reasonCode,
			reinterpret_cast<const ResponsePayload *>(data), msg);
	    },
	    nvidiaMsgType, command, sizeof(ResponsePayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}

/**
 * @brief Tests custom response decoding function
 *
 * @param[in] function Decode response function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] payloadSize Palyoad size decoded from response function
 * @param[in] expectedPayload Palyoad used in decode response function
 * @param[in] payload Palyoad decoded from response function
 */
void testDecodeResponse(std::function<int(const nsm_msg *, size_t, uint8_t *,
					  uint16_t *, uint8_t *)>
			    function,
			uint8_t nvidiaMsgType, uint8_t command,
			size_t payloadSize, const uint8_t *expectedPayload,
			uint8_t *payload);

/**
 * @brief Tests common response decoding function
 *
 * @param[in] function Decode response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 */
void testDecodeCommonResponse(
    std::function<int(const nsm_msg *, size_t, uint8_t *, uint16_t *)> function,
    uint8_t nvidiaMsgType, uint8_t command);

/**
 * @brief Tests custom response decoding function
 *
 * @tparam ResponsePayload Type of C response structure palyoad after struct
 * nsm_common_resp hdr
 * @param[in] function Decode response function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] expectedPayload Palyoad used in decode response function
 * @param[out] payload Decoded payload
 */
template <typename ResponsePayload>
void testDecodeResponse(std::function<int(const nsm_msg *, size_t, uint8_t *,
					  uint16_t *, ResponsePayload *)>
			    function,
			uint8_t nvidiaMsgType, uint8_t command,
			const ResponsePayload &expectedPayload,
			ResponsePayload &payload)
{
	testDecodeResponse(
	    [function](const nsm_msg *msg, size_t len, uint8_t *cc,
		       uint16_t *reasonCode, uint8_t *data) {
		    return function(msg, len, cc, reasonCode,
				    reinterpret_cast<ResponsePayload *>(data));
	    },
	    nvidiaMsgType, command, sizeof(ResponsePayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}

/**
 * @brief Tests custom long running response encoding function
 *
 * @param[in] function Encode long running response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 * @param[in] payloadSize Palyoad size used in encode response function
 * @param[in] expectedPayload Palyoad used in encode response function
 * @param[out] payload Payload copied from response after nsm_common_resp
 */
void testEncodeLongRunningResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, const uint8_t *, nsm_msg *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload);

/**
 * @brief Tests custom long running response encoding function
 *
 * @tparam ResponsePayload Type of C response structure palyoad after struct
 * nsm_long_running_resp hdr
 * @param[in] function Encode long running response function to be tested
 * @param[in] nvidiaMsgType expected nvidia msg type
 * @param[in] command expected command
 * @param[in] expectedPayload Palyoad used in encode response function
 * @param[out] payload Encoded payload
 */
template <typename ResponsePayload>
void testEncodeLongRunningResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, const ResponsePayload *,
		      nsm_msg *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command,
    const ResponsePayload &expectedPayload, ResponsePayload &payload)
{
	testEncodeLongRunningResponse(
	    [function](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
		       const uint8_t *data, nsm_msg *msg) {
		    return function(
			instanceId, cc, reasonCode,
			reinterpret_cast<const ResponsePayload *>(data), msg);
	    },
	    nvidiaMsgType, command, sizeof(ResponsePayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}

/**
 * @brief Tests custom long running response decoding function
 *
 * @param[in] function Decode long running response function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] payloadSize Palyoad size decoded from response function
 * @param[in] expectedPayload Palyoad used in decode response function
 * @param[in] payload Palyoad decoded from response function
 */
void testDecodeLongRunningResponse(
    std::function<int(const nsm_msg *, size_t, uint8_t *, uint16_t *,
		      uint8_t *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload);

/**
 * @brief Tests custom long running response decoding function
 *
 * @tparam ResponsePayload Type of C response structure palyoad after struct
 * nsm_common_resp hdr
 * @param[in] function Decode long running response function to be tested
 * @param[in] nvidiaMsgType Expected nvidia msg type
 * @param[in] command Expected command
 * @param[in] expectedPayload Palyoad used in decode response function
 * @param[out] payload Decoded payload
 */
template <typename ResponsePayload>
void testDecodeLongRunningResponse(
    std::function<int(const nsm_msg *, size_t, uint8_t *, uint16_t *,
		      ResponsePayload *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command,
    const ResponsePayload &expectedPayload, ResponsePayload &payload)
{
	testDecodeLongRunningResponse(
	    [function](const nsm_msg *msg, size_t len, uint8_t *cc,
		       uint16_t *reasonCode, uint8_t *data) {
		    return function(msg, len, cc, reasonCode,
				    reinterpret_cast<ResponsePayload *>(data));
	    },
	    nvidiaMsgType, command, sizeof(ResponsePayload),
	    reinterpret_cast<const uint8_t *>(&expectedPayload),
	    reinterpret_cast<uint8_t *>(&payload));
}