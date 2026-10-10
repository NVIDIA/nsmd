// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "base.h"
#include "device-configuration.h"
#include "fpga/fpga-maps.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// One negative test per libnsm function the FPGA discovery path adds: the
// inputs each one must refuse, and that a refusal leaves its outputs alone.

namespace
{
const std::vector<uint8_t> digest(32, 0x5a);

std::vector<uint8_t> readReply()
{
	std::vector<uint8_t> msg(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_get_device_config_v2_resp) - 1 +
				 digest.size());
	auto response = reinterpret_cast<nsm_msg *>(msg.data());
	EXPECT_EQ(encode_get_device_config_v2_resp(
		      0, NSM_SUCCESS, ERR_NULL, digest.data(),
		      static_cast<uint16_t>(digest.size()), nullptr, 0,
		      response),
		  NSM_SW_SUCCESS);
	return msg;
}

std::vector<uint8_t> configsReply(const std::vector<uint32_t> &types,
				  uint32_t total)
{
	std::vector<uint8_t> msg(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_get_supported_device_configs_resp) -
				 sizeof(uint32_t) +
				 types.size() * sizeof(uint32_t));
	auto response = reinterpret_cast<nsm_msg *>(msg.data());
	EXPECT_EQ(encode_get_supported_device_configs_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, total, types.data(),
		      static_cast<uint16_t>(types.size()), response),
		  NSM_SW_SUCCESS);
	return msg;
}
} // namespace

TEST(mmioNegative, encode_get_device_config_mmio_req)
{
	std::vector<uint8_t> untouched(64, 0xaa);
	std::vector<uint8_t> buffer = untouched;
	auto msg = reinterpret_cast<nsm_msg *>(buffer.data());
	EXPECT_EQ(
	    encode_get_device_config_mmio_req(
		0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1080, 8, nullptr),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_get_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1081, 8, msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_get_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1082, 8, msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_get_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1080, 0, msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_get_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1080,
		      NSM_MMIO_ACCESS_MAX_LENGTH_DWORDS + 1, msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(buffer, untouched);
}

TEST(mmioNegative, decode_get_device_config_mmio_resp)
{
	std::vector<uint8_t> reply = readReply();
	auto response = reinterpret_cast<nsm_msg *>(reply.data());
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	const std::vector<uint8_t> untouched(32, 0x55);
	std::vector<uint8_t> data = untouched;
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      nullptr, reply.size(), 8, &cc, &reasonCode, data.data()),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response, reply.size(), 8,
						     nullptr, &reasonCode,
						     data.data()),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response, reply.size(), 8,
						     &cc, nullptr, data.data()),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_device_config_mmio_resp(response, reply.size(), 8,
						     &cc, &reasonCode, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, reply.size(), 0, &cc, &reasonCode, data.data()),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, reply.size(),
		      NSM_MMIO_ACCESS_MAX_LENGTH_DWORDS + 1, &cc, &reasonCode,
		      data.data()),
		  NSM_SW_ERROR_DATA);
	// a reply that carries fewer DWORDs than were asked for
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, reply.size(), 9, &cc, &reasonCode, data.data()),
		  NSM_SW_ERROR_LENGTH);
	for (size_t len = 0; len < reply.size(); ++len) {
		EXPECT_NE(decode_get_device_config_mmio_resp(
			      response, len, 8, &cc, &reasonCode, data.data()),
			  NSM_SW_SUCCESS);
	}
	auto resp = reinterpret_cast<nsm_get_device_config_v2_resp *>(
	    response->payload);
	resp->pending_config_length = htole16(4);
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, reply.size(), 8, &cc, &reasonCode, data.data()),
		  NSM_SW_ERROR_LENGTH);
	resp->pending_config_length = 0;
	resp->hdr.data_size = htole16(8);
	EXPECT_EQ(decode_get_device_config_mmio_resp(
		      response, reply.size(), 8, &cc, &reasonCode, data.data()),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(data, untouched);
}

TEST(mmioNegative, encode_set_device_config_mmio_req)
{
	const std::vector<uint8_t> word(4, 0x11);
	std::vector<uint8_t> untouched(64, 0xaa);
	std::vector<uint8_t> buffer = untouched;
	auto msg = reinterpret_cast<nsm_msg *>(buffer.data());
	EXPECT_EQ(encode_set_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1000, 1,
		      word.data(), nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    encode_set_device_config_mmio_req(
		0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1000, 1, nullptr, msg),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_set_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1003, 1,
		      word.data(), msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_set_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1000, 0,
		      word.data(), msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_set_device_config_mmio_req(
		      0, NSM_DEVICE_CONFIG_TYPE_MMIO_ACCESS, 0x1000,
		      NSM_MMIO_ACCESS_MAX_LENGTH_DWORDS + 1, word.data(), msg),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(buffer, untouched);
}

TEST(mmioNegative, encode_get_supported_device_configs_req)
{
	EXPECT_EQ(encode_get_supported_device_configs_req(0, 0, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_get_supported_device_configs_req(0x1f, 7, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(mmioNegative, decode_get_supported_device_configs_req)
{
	std::vector<uint8_t> buffer(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_configs_req));
	auto msg = reinterpret_cast<nsm_msg *>(buffer.data());
	ASSERT_EQ(encode_get_supported_device_configs_req(0, 5, msg),
		  NSM_SW_SUCCESS);
	uint32_t handle = 0xee;
	EXPECT_EQ(decode_get_supported_device_configs_req(
		      nullptr, buffer.size(), &handle),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_req(msg, buffer.size(),
							  nullptr),
		  NSM_SW_ERROR_NULL);
	for (size_t len = 0; len < buffer.size(); ++len) {
		EXPECT_EQ(
		    decode_get_supported_device_configs_req(msg, len, &handle),
		    NSM_SW_ERROR_LENGTH);
	}
	auto request = reinterpret_cast<nsm_get_supported_device_configs_req *>(
	    msg->payload);
	request->hdr.data_size = htole16(8);
	EXPECT_EQ(decode_get_supported_device_configs_req(msg, buffer.size(),
							  &handle),
		  NSM_SW_ERROR_LENGTH);
	request->hdr.data_size = htole16(4);
	request->hdr.command = NSM_GET_SUPPORTED_DEVICE_CONFIGS + 1;
	EXPECT_EQ(decode_get_supported_device_configs_req(msg, buffer.size(),
							  &handle),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(handle, 0xeeu);
}

TEST(mmioNegative, encode_get_supported_device_configs_resp)
{
	const std::vector<uint32_t> types = {1, 2};
	std::vector<uint8_t> buffer(64);
	auto msg = reinterpret_cast<nsm_msg *>(buffer.data());
	EXPECT_EQ(encode_get_supported_device_configs_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, 2, types.data(), 2, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_get_supported_device_configs_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, 2, nullptr, 2, msg),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_get_supported_device_configs_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, 1, types.data(), 2, msg),
		  NSM_SW_ERROR_DATA);
	auto resp = reinterpret_cast<nsm_get_supported_device_configs_resp *>(
	    msg->payload);
	EXPECT_EQ(le16toh(resp->hdr.data_size), 0u);
}

TEST(mmioNegative, decode_get_supported_device_configs_resp)
{
	std::vector<uint8_t> reply = configsReply({1, 2, 3}, 3);
	auto response = reinterpret_cast<nsm_msg *>(reply.data());
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint32_t nextHandle = 0;
	uint32_t totalCount = 0;
	const std::vector<uint32_t> untouched(4, 0x5555);
	std::vector<uint32_t> out = untouched;
	uint16_t count = 0;
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      nullptr, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), nullptr, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, nullptr, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, nullptr,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      nullptr, out.data(), 4, &count),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, nullptr, 4, &count),
		  NSM_SW_ERROR_NULL);
	for (size_t len = 0; len < reply.size(); ++len) {
		EXPECT_NE(decode_get_supported_device_configs_resp(
			      response, len, &cc, &reasonCode, &nextHandle,
			      &totalCount, out.data(), 4, &count),
			  NSM_SW_SUCCESS);
	}
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 2, &count),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(count, 3u);
	auto resp = reinterpret_cast<nsm_get_supported_device_configs_resp *>(
	    response->payload);
	resp->hdr.data_size = htole16(2 * sizeof(uint32_t) + 5);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_LENGTH);
	resp->hdr.data_size = htole16(sizeof(uint32_t));
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_LENGTH);
	resp->hdr.data_size =
	    htole16(2 * sizeof(uint32_t) + 3 * sizeof(uint32_t));
	resp->total_count = htole32(1);
	EXPECT_EQ(decode_get_supported_device_configs_resp(
		      response, reply.size(), &cc, &reasonCode, &nextHandle,
		      &totalCount, out.data(), 4, &count),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(out, untouched);
}

TEST(mmioNegative, nsm_fpga_map_identity_by_platform)
{
	EXPECT_EQ(nsm_fpga_map_identity_by_platform(nullptr), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform(""), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("lp30"), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("LP30 "), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("LP3"), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("GB300"), nullptr);
}
