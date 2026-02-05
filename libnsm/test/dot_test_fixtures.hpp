/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "firmware-utils.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace dot_test
{

constexpr size_t CAK_KEY_SIZE = 148;
constexpr size_t LAK_KEY_SIZE = 148;
constexpr size_t CHALLENGE_SIZE = 32;
constexpr size_t SIGNATURE_SIZE = 1840;

/**
 * Fills buffer with sequential pattern: startValue, startValue+1, ..., wrapping
 * at 256
 */
inline void fillPatternSequential(uint8_t *buffer, size_t size,
				  uint8_t startValue = 0)
{
	for (size_t i = 0; i < size; i++) {
		buffer[i] = static_cast<uint8_t>((startValue + i) % 256);
	}
}

/**
 * Fills buffer with cycling pattern starting at startValue
 */
inline void fillPatternCycling(uint8_t *buffer, size_t size, uint8_t startValue)
{
	for (size_t i = 0; i < size; i++) {
		buffer[i] = static_cast<uint8_t>((startValue + i) % 256);
	}
}

/**
 * Fills buffer with constant value
 */
inline void fillPatternConstant(uint8_t *buffer, size_t size, uint8_t value)
{
	std::memset(buffer, value, size);
}

/**
 * Fills buffer with pattern: buffer[i] = (i * multiplier) % 256
 */
inline void fillPatternMultiplied(uint8_t *buffer, size_t size,
				  uint8_t multiplier)
{
	for (size_t i = 0; i < size; i++) {
		buffer[i] = static_cast<uint8_t>((i * multiplier) % 256);
	}
}

/**
 * Creates DOT CAK Install request with test patterns
 * @param cakPattern Start value for CAK pattern
 * @param lakPattern Start value for LAK pattern
 */
inline void createDotCAKInstallRequest(nsm_dot_cak_install_req &req,
				       uint8_t cakPattern = 0,
				       uint8_t lakPattern = 0xA0,
				       uint8_t lockDisable = 0,
				       uint32_t vendorMinSvn = 0x78,
				       uint32_t ownerMinSvn = 0x56)
{
	fillPatternSequential(req.cak_pub, CAK_KEY_SIZE, cakPattern);
	fillPatternCycling(req.lak_pub, LAK_KEY_SIZE, lakPattern);
	req.lock_disable = lockDisable;
	uint32_t minSvnBitmap =
	    (ownerMinSvn & 0xFF) | ((vendorMinSvn & 0xFF) << 8);
	req.min_svn = minSvnBitmap;
}

/**
 * Creates DOT Lock request with test patterns
 * @param signatureMultiplier Multiplier for signature pattern generation
 */
inline void createDotLockRequest(nsm_dot_lock_req &req, uint8_t cakPattern = 0,
				 uint8_t lakPattern = 0xA0,
				 uint8_t unlockMethod = 2,
				 uint8_t challengePattern = 0,
				 uint8_t signatureMultiplier = 7)
{
	fillPatternSequential(req.cak_pub, CAK_KEY_SIZE, cakPattern);
	fillPatternCycling(req.lak_pub, LAK_KEY_SIZE, lakPattern);
	req.unlock_method = unlockMethod;
	fillPatternSequential(req.s_challenge, CHALLENGE_SIZE,
			      challengePattern);
	fillPatternMultiplied(req.signature, SIGNATURE_SIZE,
			      signatureMultiplier);
}

/**
 * Creates DOT Lock request with custom multipliers for challenge and signature
 */
inline void createDotLockRequestCustom(nsm_dot_lock_req &req,
				       uint8_t cakPattern, uint8_t lakPattern,
				       uint8_t unlockMethod,
				       uint8_t challengeMultiplier,
				       uint8_t signatureMultiplier)
{
	fillPatternSequential(req.cak_pub, CAK_KEY_SIZE, cakPattern);
	fillPatternSequential(req.lak_pub, LAK_KEY_SIZE, lakPattern);
	req.unlock_method = unlockMethod;
	fillPatternMultiplied(req.s_challenge, CHALLENGE_SIZE,
			      challengeMultiplier);
	fillPatternMultiplied(req.signature, SIGNATURE_SIZE,
			      signatureMultiplier);
}

/**
 * Creates DOT CAK Rotate request with test patterns
 * Uses DOT_KEY_AUTH_DATA_SIZE and DOT_SIGNATURE_SIZE from firmware-utils.h
 */
inline void createDotCAKRotateRequest(nsm_dot_cak_rotate_req &req,
				      uint8_t cakPattern = 0,
				      uint8_t signatureMultiplier = 3)
{
	for (size_t i = 0; i < DOT_KEY_AUTH_DATA_SIZE; i++) {
		req.new_cak[i] = static_cast<uint8_t>((cakPattern + i) % 256);
	}
	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		req.signature[i] =
		    static_cast<uint8_t>((i * signatureMultiplier) % 256);
	}
}

/**
 * Validates NSM request header fields
 * @return true if header is valid request with expected message type and
 * version
 */
inline bool
validateNSMRequestHeader(const nsm_msg *msg,
			 uint8_t expectedMsgType = NSM_TYPE_FIRMWARE,
			 uint8_t expectedOcpVersion = OCP_VERSION_V2)
{
	return (msg->hdr.request == 1 && msg->hdr.datagram == 0 &&
		msg->hdr.nvidia_msg_type == expectedMsgType &&
		msg->hdr.ocp_version == expectedOcpVersion);
}

/**
 * Validates NSM response header fields
 * @return true if header is valid response with expected message type and
 * version
 */
inline bool
validateNSMResponseHeader(const nsm_msg *msg,
			  uint8_t expectedMsgType = NSM_TYPE_FIRMWARE,
			  uint8_t expectedOcpVersion = OCP_VERSION_V2)
{
	return (msg->hdr.request == 0 &&
		msg->hdr.nvidia_msg_type == expectedMsgType &&
		msg->hdr.ocp_version == expectedOcpVersion);
}

struct TestPattern {
	const char *name;
	uint8_t cakPatternStart;
	uint8_t lakPatternStart;
	uint8_t lockDisable;
	uint32_t minSvn;
};

inline const TestPattern *getCommonTestPatterns()
{
	static const TestPattern patterns[] = {
	    {"all_zeros", 0x00, 0x00, 0, 0x00000000},
	    {"all_ones", 0xFF, 0xFF, 1, 0xFFFFFFFF},
	    {"alternating", 0xAA, 0x55, 1, 0xAAAAAAAA},
	    {"sequential", 0x00, 0x00, 0, 0x12345678},
	    {"random", 0x42, 0xDE, 1, 0xDEADBEEF},
	};
	return patterns;
}

inline size_t getCommonTestPatternsCount() { return 5; }

} // namespace dot_test
