// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "base.h"
#include "common-tests.hpp"
#include "device-configuration.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace
{
// Read of the identity register: 8 DWORDs at 0x1080; the digest is the
// P0 map's semantic_sha256
const std::vector<uint8_t> readRequestPayload = {
    0x11, 0x00, 0x10, 0x00, 0x00, 0x00, // v2 header: cmd 0x11, size 16
    0x02, 0x00, 0x00, 0x00,		// configuration type 2
    0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // address 0x1080
    0x08, 0x00,					    // 8 DWORDs
    0x00, 0x00					    // reserved
};
const std::vector<uint8_t> mapDigest = {
    0x80, 0x65, 0x3a, 0xc8, 0x5e, 0x22, 0x66, 0x26, 0x5d, 0x66, 0x23,
    0xa7, 0x5a, 0x4b, 0xba, 0xbd, 0xcd, 0x84, 0x49, 0x0d, 0xcb, 0xee,
    0x1c, 0xf2, 0xf8, 0x0d, 0xf3, 0xef, 0xfa, 0xa5, 0xa2, 0xaf};
} // namespace

TEST(getDeviceConfigMmio, requestLayout)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) + sizeof(uint32_t) +
	    sizeof(nsm_mmio_access_query));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_get_device_config_mmio_req(
	    0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1080, 8, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(request->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);
	std::vector<uint8_t> payload(requestMsg.begin() + sizeof(nsm_msg_hdr),
				     requestMsg.end());
	EXPECT_EQ(payload, readRequestPayload);
}

TEST(getDeviceConfigMmio, requestRejectsBadAddressOrLength)
{
	std::vector<uint8_t> requestMsg(64);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	EXPECT_EQ(encode_get_device_config_mmio_req(0, 2, 0x1041, 8, request),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_get_device_config_mmio_req(0, 2, 0x1080, 0, request),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(
	    encode_get_device_config_mmio_req(0, 2, 0x1080, 1024, request),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_get_device_config_mmio_req(0, 2, 0x1080, 8, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(getDeviceConfigMmio, responseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_device_config_v2_resp) -
					 1 + mapDigest.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc = encode_get_device_config_v2_resp(
	    0, NSM_SUCCESS, ERR_NULL, mapDigest.data(),
	    static_cast<uint16_t>(mapDigest.size()), nullptr, 0, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> data(32);
	rc = decode_get_device_config_mmio_resp(response, responseMsg.size(), 8,
						&cc, &reasonCode, data.data());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data, mapDigest);

	// asking for a different length than the reply carries is an error
	std::vector<uint8_t> big(64);
	rc = decode_get_device_config_mmio_resp(
	    response, responseMsg.size(), 16, &cc, &reasonCode, big.data());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getDeviceConfigMmio, responseRejectsPendingData)
{
	const std::vector<uint8_t> pending(4, 0xaa);
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_device_config_v2_resp) -
					 1 + mapDigest.size() + pending.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc = encode_get_device_config_v2_resp(
	    0, NSM_SUCCESS, ERR_NULL, mapDigest.data(),
	    static_cast<uint16_t>(mapDigest.size()), pending.data(),
	    static_cast<uint16_t>(pending.size()), response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> data(32);
	rc = decode_get_device_config_mmio_resp(response, responseMsg.size(), 8,
						&cc, &reasonCode, data.data());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getDeviceConfigMmio, errorCompletionIsReported)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc =
	    encode_get_device_config_v2_resp(0, NSM_ERR_INVALID_DATA, ERR_NULL,
					     nullptr, 0, nullptr, 0, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> data(32);
	rc = decode_get_device_config_mmio_resp(response, responseMsg.size(), 8,
						&cc, &reasonCode, data.data());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
}

TEST(setDeviceConfigMmio, requestCarriesQueryAndData)
{
	const std::vector<uint8_t> data = {0xde, 0xad, 0xbe, 0xef};
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) + sizeof(uint32_t) +
	    sizeof(nsm_mmio_access_query) + data.size());
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_set_device_config_mmio_req(0, 2, 0x1000, 1, data.data(),
						   request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	const std::vector<uint8_t> expected = {
	    0x10, 0x00, 0x14, 0x00, 0x00, 0x00,		    // cmd 0x10, size 20
	    0x02, 0x00, 0x00, 0x00,			    // type 2
	    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // address 0x1000
	    0x01, 0x00, 0x00, 0x00,			    // 1 DWORD, reserved
	    0xde, 0xad, 0xbe, 0xef};
	std::vector<uint8_t> payload(requestMsg.begin() + sizeof(nsm_msg_hdr),
				     requestMsg.end());
	EXPECT_EQ(payload, expected);
	EXPECT_EQ(encode_set_device_config_mmio_req(0, 2, 0x1001, 1,
						    data.data(), request),
		  NSM_SW_ERROR_DATA);
}

TEST(getSupportedDeviceConfigs, requestLayout)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_configs_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_get_supported_device_configs_req(0, 0, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	const std::vector<uint8_t> expected = {0x12, 0x00, 0x04, 0x00, 0x00,
					       0x00, 0x00, 0x00, 0x00, 0x00};
	std::vector<uint8_t> payload(requestMsg.begin() + sizeof(nsm_msg_hdr),
				     requestMsg.end());
	EXPECT_EQ(payload, expected);

	uint32_t handle = 7;
	rc = decode_get_supported_device_configs_req(request, requestMsg.size(),
						     &handle);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(handle, 0u);
}

TEST(getSupportedDeviceConfigs, responseRoundTrip)
{
	const std::vector<uint32_t> types = {1, 2};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_supported_device_configs_resp) - sizeof(uint32_t) +
	    types.size() * sizeof(uint32_t));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc = encode_get_supported_device_configs_resp(
	    0, NSM_SUCCESS, ERR_NULL, 0, 2, types.data(),
	    static_cast<uint16_t>(types.size()), response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t nextHandle = 9;
	uint32_t totalCount = 0;
	std::vector<uint32_t> out(4);
	uint16_t count = 0;
	rc = decode_get_supported_device_configs_resp(
	    response, responseMsg.size(), &cc, &reasonCode, &nextHandle,
	    &totalCount, out.data(), 4, &count);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(nextHandle, 0u);
	EXPECT_EQ(totalCount, 2u);
	EXPECT_EQ(count, 2u);
	EXPECT_EQ(out[0], 1u);
	EXPECT_EQ(out[1], 2u);

	// too small a buffer reports the needed count
	rc = decode_get_supported_device_configs_resp(
	    response, responseMsg.size(), &cc, &reasonCode, &nextHandle,
	    &totalCount, out.data(), 1, &count);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(count, 2u);
}

TEST(getSupportedDeviceConfigs, decodesDeviceReply)
{
	// Reply bytes: 10 de 00 89 05 12 00 00 00 10 00 | next 0 | total 2 |
	// 1 | 2
	const std::vector<uint8_t> reply = {
	    0x10, 0xde, 0x00, 0x89, 0x05, 0x12, 0x00, 0x00, 0x00,
	    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	    0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
	auto response = reinterpret_cast<const nsm_msg *>(reply.data());
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t nextHandle = 9;
	uint32_t totalCount = 0;
	std::vector<uint32_t> out(4);
	uint16_t count = 0;
	int rc = decode_get_supported_device_configs_resp(
	    response, reply.size(), &cc, &reasonCode, &nextHandle, &totalCount,
	    out.data(), 4, &count);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(nextHandle, 0u);
	EXPECT_EQ(totalCount, 2u);
	EXPECT_EQ(count, 2u);
	EXPECT_EQ(out[0], 1u);
	EXPECT_EQ(out[1], 2u);
}

// Replies the FPGA could corrupt: every truncation and every length field
// that disagrees with the frame is refused before any byte is copied
TEST(getDeviceConfigMmio, truncatedAndInconsistentRepliesAreRejected)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_device_config_v2_resp) -
					 1 + mapDigest.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	ASSERT_EQ(encode_get_device_config_v2_resp(
		      0, NSM_SUCCESS, ERR_NULL, mapDigest.data(),
		      static_cast<uint16_t>(mapDigest.size()), nullptr, 0,
		      response),
		  NSM_SW_SUCCESS);
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> data(32, 0x55);
	for (size_t len = 0; len < responseMsg.size(); ++len) {
		EXPECT_NE(decode_get_device_config_mmio_resp(
			      response, len, 8, &cc, &reasonCode, data.data()),
			  NSM_SW_SUCCESS)
		    << "accepted a reply cut to " << len << " bytes";
	}
	// the data was never touched by a rejected reply
	EXPECT_EQ(data, std::vector<uint8_t>(32, 0x55));
	// a length field that disagrees with the frame
	auto resp = reinterpret_cast<nsm_get_device_config_v2_resp *>(
	    response->payload);
	resp->current_config_length = htole16(36);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response,
						     responseMsg.size(), 8, &cc,
						     &reasonCode, data.data()),
		  NSM_SW_ERROR_LENGTH);
	resp->current_config_length = htole16(32);
	resp->hdr.data_size = htole16(4);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response,
						     responseMsg.size(), 8, &cc,
						     &reasonCode, data.data()),
		  NSM_SW_ERROR_LENGTH);
	// the length asked for is checked before the frame is read
	EXPECT_EQ(decode_get_device_config_mmio_resp(response,
						     responseMsg.size(), 0, &cc,
						     &reasonCode, data.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, responseMsg.size(), 1024, &cc, &reasonCode,
		      data.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(decode_get_device_config_mmio_resp(nullptr,
						     responseMsg.size(), 8, &cc,
						     &reasonCode, data.data()),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response,
						     responseMsg.size(), 8, &cc,
						     &reasonCode, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(setDeviceConfigMmio, requestRejectsNullAndBadLength)
{
	const std::vector<uint8_t> data(8, 0x11);
	std::vector<uint8_t> requestMsg(64);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	EXPECT_EQ(encode_set_device_config_mmio_req(0, 2, 0x1000, 2,
						    data.data(), nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_set_device_config_mmio_req(0, 2, 0x1000, 2, nullptr,
						    request),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_set_device_config_mmio_req(0, 2, 0x1000, 0,
						    data.data(), request),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_set_device_config_mmio_req(0, 2, 0x1000, 1024,
						    data.data(), request),
		  NSM_SW_ERROR_DATA);
}

TEST(getSupportedDeviceConfigs, truncatedAndInconsistentRepliesAreRejected)
{
	const std::vector<uint32_t> types = {1, 2, 3};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_supported_device_configs_resp) - sizeof(uint32_t) +
	    types.size() * sizeof(uint32_t));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	ASSERT_EQ(encode_get_supported_device_configs_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, 3, types.data(),
		      static_cast<uint16_t>(types.size()), response),
		  NSM_SW_SUCCESS);
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t nextHandle = 0;
	uint32_t totalCount = 0;
	std::vector<uint32_t> out(4, 0x5555);
	uint16_t count = 0;
	for (size_t len = 0; len < responseMsg.size(); ++len) {
		EXPECT_NE(decode_get_supported_device_configs_resp(
			      response, len, &cc, &reasonCode, &nextHandle,
			      &totalCount, out.data(), 4, &count),
			  NSM_SW_SUCCESS)
		    << "accepted a reply cut to " << len << " bytes";
	}
	EXPECT_EQ(out, std::vector<uint32_t>(4, 0x5555));
	auto resp = reinterpret_cast<nsm_get_supported_device_configs_resp *>(
	    response->payload);
	// a data size that is not whole entries
	resp->hdr.data_size = htole16(2 * sizeof(uint32_t) + 6);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, responseMsg.size(), &cc, &reasonCode,
		      &nextHandle, &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_LENGTH);
	// more entries announced than the frame carries
	resp->hdr.data_size =
	    htole16(2 * sizeof(uint32_t) + 4 * sizeof(uint32_t));
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, responseMsg.size(), &cc, &reasonCode,
		      &nextHandle, &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_LENGTH);
	// more entries in the chunk than the total the device claims
	resp->hdr.data_size =
	    htole16(2 * sizeof(uint32_t) + 3 * sizeof(uint32_t));
	resp->total_count = htole32(2);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, responseMsg.size(), &cc, &reasonCode,
		      &nextHandle, &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      nullptr, responseMsg.size(), &cc, &reasonCode,
		      &nextHandle, &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, responseMsg.size(), &cc, &reasonCode,
		      &nextHandle, &totalCount, out.data(), 4, nullptr),
		  NSM_SW_ERROR_NULL);
	// a request cut short or carrying the wrong size is refused too
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_configs_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	ASSERT_EQ(encode_get_supported_device_configs_req(0, 5, request),
		  NSM_SW_SUCCESS);
	uint32_t handle = 0;
	EXPECT_EQ(decode_get_supported_device_configs_req(
		      request, requestMsg.size() - 1, &handle),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(encode_get_supported_device_configs_req(0, 5, nullptr),
		  NSM_SW_ERROR_NULL);
}
