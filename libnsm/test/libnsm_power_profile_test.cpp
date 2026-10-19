/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Unit tests for the Type 1 Set Power Profile (0x0C) and Get Power Profile
// (0x0D) codecs.

#include "base.h"
#include "device-configuration.h"
#include "network-ports.h"

#include <endian.h>

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace
{

constexpr size_t entrySize = sizeof(nsm_power_profile_entry);
constexpr size_t fixedDataSize = 2;

size_t getRespLen(uint8_t count)
{
	return sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + fixedDataSize +
	       count * entrySize;
}

nsm_power_profile_entry makeEntry(uint8_t id, const char *name)
{
	nsm_power_profile_entry entry{};
	entry.profile_id = id;
	std::strncpy(entry.profile_name, name, sizeof(entry.profile_name) - 1);
	return entry;
}

std::vector<uint8_t>
encodeGetResp(uint8_t active, const std::vector<nsm_power_profile_entry> &e)
{
	std::vector<uint8_t> buf(getRespLen(static_cast<uint8_t>(e.size())));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	EXPECT_EQ(NSM_SW_SUCCESS, encode_get_power_profile_resp(
				      0, NSM_SUCCESS, ERR_NULL, active,
				      static_cast<uint8_t>(e.size()),
				      e.empty() ? nullptr : e.data(), msg));
	return buf;
}

} // namespace

TEST(setPowerProfile, testGoodEncodeRequest)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_set_power_profile_req));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());

	ASSERT_EQ(NSM_SW_SUCCESS, encode_set_power_profile_req(0, 2, msg));

	const std::vector<uint8_t> expected{0x10, 0xde, 0x80, 0x89,
					    0x01, 0x0c, 0x01, 0x02};
	EXPECT_EQ(expected, buf);
}

TEST(setPowerProfile, testEncodeRequestNull)
{
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  encode_set_power_profile_req(0, 1, nullptr));
}

TEST(setPowerProfile, testGoodDecodeRequest)
{
	std::vector<uint8_t> buf{0x10, 0xde, 0x80, 0x89,
				 0x01, 0x0c, 0x01, 0x05};
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t id = 0;

	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_set_power_profile_req(msg, buf.size(), &id));
	EXPECT_EQ(5, id);
}

TEST(setPowerProfile, testBadDecodeRequest)
{
	std::vector<uint8_t> buf{0x10, 0xde, 0x80, 0x89,
				 0x01, 0x0c, 0x02, 0x05};
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t id = 0;

	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  decode_set_power_profile_req(msg, buf.size(), nullptr));
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_set_power_profile_req(msg, buf.size() - 1, &id));
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_set_power_profile_req(msg, buf.size(), &id));
}

TEST(setPowerProfile, testResponseRoundTrip)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	ASSERT_EQ(NSM_SW_SUCCESS,
		  encode_set_power_profile_resp(0, NSM_SUCCESS, ERR_NULL, msg));

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0xffff;
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_set_power_profile_resp(msg, buf.size(), &cc, &reason));
	EXPECT_EQ(NSM_SUCCESS, cc);
}

TEST(setPowerProfile, testErrorResponse)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	ASSERT_EQ(NSM_SW_SUCCESS, encode_set_power_profile_resp(
				      0, NSM_ERR_INVALID_DATA, 0x1234, msg));

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason = 0;
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_set_power_profile_resp(msg, buf.size(), &cc, &reason));
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0x1234, reason);
}

TEST(getPowerProfile, testGoodEncodeRequest)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());

	ASSERT_EQ(NSM_SW_SUCCESS, encode_get_power_profile_req(0, msg));

	const std::vector<uint8_t> expected{0x10, 0xde, 0x80, 0x89,
					    0x01, 0x0d, 0x00};
	EXPECT_EQ(expected, buf);
	EXPECT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_req(msg, buf.size()));
}

TEST(getPowerProfile, testRoundTripZeroProfiles)
{
	auto buf = encodeGetResp(0, {});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint8_t active = 0xff;
	uint8_t count = 0xff;
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, nullptr, 0));
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, active);
	EXPECT_EQ(0, count);
}

TEST(getPowerProfile, testRoundTripOneProfile)
{
	auto buf = encodeGetResp(1, {makeEntry(1, "compute")});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	auto resp =
	    reinterpret_cast<const nsm_get_power_profile_resp *>(msg->payload);
	EXPECT_EQ(fixedDataSize + entrySize, le16toh(resp->hdr.data_size));

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint8_t active = 0;
	uint8_t count = 0;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries,
						NSM_POWER_PROFILE_MAX_COUNT));
	EXPECT_EQ(1, active);
	ASSERT_EQ(1, count);
	EXPECT_EQ(1, entries[0].profile_id);
	EXPECT_STREQ("compute", entries[0].profile_name);
}

TEST(getPowerProfile, testRoundTripMaxProfiles)
{
	std::vector<nsm_power_profile_entry> in;
	for (uint8_t i = 1; i <= NSM_POWER_PROFILE_MAX_COUNT; ++i) {
		in.push_back(
		    makeEntry(i, ("profile" + std::to_string(i)).c_str()));
	}
	auto buf = encodeGetResp(16, in);
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint8_t active = 0;
	uint8_t count = 0;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries,
						NSM_POWER_PROFILE_MAX_COUNT));
	EXPECT_EQ(16, active);
	ASSERT_EQ(NSM_POWER_PROFILE_MAX_COUNT, count);
	EXPECT_EQ(16, entries[15].profile_id);
	EXPECT_STREQ("profile16", entries[15].profile_name);
}

// Reply captured from a QM5 that has no profiles provisioned: active ID 0 and
// a single placeholder entry with ID 0 and an empty name.
TEST(getPowerProfile, testDecodeDevicePlaceholderEntry)
{
	std::vector<uint8_t> buf{0x10, 0xde, 0x00, 0x89, 0x01, 0x0d, 0x00,
				 0x00, 0x00, 0x17, 0x00, 0x00, 0x01, 0x00};
	buf.resize(buf.size() + NSM_POWER_PROFILE_NAME_LEN, 0x00);
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint8_t active = 0xff;
	uint8_t count = 0;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries,
						NSM_POWER_PROFILE_MAX_COUNT));
	EXPECT_EQ(NSM_POWER_PROFILE_ID_HW_DEFAULT, active);
	ASSERT_EQ(1, count);
	EXPECT_EQ(0, entries[0].profile_id);
	EXPECT_STREQ("", entries[0].profile_name);
}

TEST(getPowerProfile, testEncodeTooManyProfiles)
{
	std::vector<nsm_power_profile_entry> in(NSM_POWER_PROFILE_MAX_COUNT +
						1);
	std::vector<uint8_t> buf(getRespLen(NSM_POWER_PROFILE_MAX_COUNT + 1));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  encode_get_power_profile_resp(0, NSM_SUCCESS, ERR_NULL, 0,
						NSM_POWER_PROFILE_MAX_COUNT + 1,
						in.data(), msg));
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  encode_get_power_profile_resp(0, NSM_SUCCESS, ERR_NULL, 0, 1,
						nullptr, msg));
}

TEST(getPowerProfile, testDecodeTooManyProfiles)
{
	auto buf = encodeGetResp(0, {makeEntry(1, "a")});
	auto resp = reinterpret_cast<nsm_get_power_profile_resp *>(
	    reinterpret_cast<nsm_msg *>(buf.data())->payload);
	resp->profile_count = NSM_POWER_PROFILE_MAX_COUNT + 1;

	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_get_power_profile_resp(
		      reinterpret_cast<nsm_msg *>(buf.data()), buf.size(), &cc,
		      &reason, &active, &count, entries,
		      NSM_POWER_PROFILE_MAX_COUNT));
}

TEST(getPowerProfile, testDecodeShortAndLongBuffer)
{
	auto buf = encodeGetResp(1, {makeEntry(1, "a"), makeEntry(2, "b")});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_power_profile_resp(
		      msg, buf.size() - 1, &cc, &reason, &active, &count,
		      entries, NSM_POWER_PROFILE_MAX_COUNT));

	buf.push_back(0);
	msg = reinterpret_cast<const nsm_msg *>(buf.data());
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries,
						NSM_POWER_PROFILE_MAX_COUNT));

	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_power_profile_resp(
		      msg, sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + 1,
		      &cc, &reason, &active, &count, entries,
		      NSM_POWER_PROFILE_MAX_COUNT));
}

TEST(getPowerProfile, testDecodeDataSizeMismatch)
{
	auto buf = encodeGetResp(1, {makeEntry(1, "a"), makeEntry(2, "b")});
	auto resp = reinterpret_cast<nsm_get_power_profile_resp *>(
	    reinterpret_cast<nsm_msg *>(buf.data())->payload);
	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};

	// The spec text describes data_size as n * 21, omitting the two leading
	// bytes; devices send 2 + n * 21, so the shorter form is rejected.
	resp->hdr.data_size = htole16(2 * entrySize);
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_get_power_profile_resp(
		      reinterpret_cast<nsm_msg *>(buf.data()), buf.size(), &cc,
		      &reason, &active, &count, entries,
		      NSM_POWER_PROFILE_MAX_COUNT));

	resp->hdr.data_size = htole16(fixedDataSize + 2 * entrySize);
	resp->profile_count = 1;
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_get_power_profile_resp(
		      reinterpret_cast<nsm_msg *>(buf.data()), buf.size(), &cc,
		      &reason, &active, &count, entries,
		      NSM_POWER_PROFILE_MAX_COUNT));
}

TEST(getPowerProfile, testDecodeEntriesBufferTooSmall)
{
	auto buf = encodeGetResp(1, {makeEntry(1, "a"), makeEntry(2, "b")});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[1]{};
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries, 1));
}

TEST(getPowerProfile, testDecodeDuplicateIds)
{
	auto buf = encodeGetResp(1, {makeEntry(3, "a"), makeEntry(3, "b")});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, entries,
						NSM_POWER_PROFILE_MAX_COUNT));
}

TEST(getPowerProfile, testDecodeUnterminatedName)
{
	auto buf = encodeGetResp(1, {makeEntry(1, "")});
	auto resp = reinterpret_cast<nsm_get_power_profile_resp *>(
	    reinterpret_cast<nsm_msg *>(buf.data())->payload);
	std::memset(resp->entries[0].profile_name, 'x',
		    NSM_POWER_PROFILE_NAME_LEN);

	uint8_t cc, active, count;
	uint16_t reason;
	nsm_power_profile_entry entries[NSM_POWER_PROFILE_MAX_COUNT]{};
	ASSERT_EQ(NSM_SW_SUCCESS, decode_get_power_profile_resp(
				      reinterpret_cast<nsm_msg *>(buf.data()),
				      buf.size(), &cc, &reason, &active, &count,
				      entries, NSM_POWER_PROFILE_MAX_COUNT));
	EXPECT_EQ(NSM_POWER_PROFILE_NAME_LEN - 1,
		  std::strlen(entries[0].profile_name));
}

TEST(getPowerProfile, testDecodeErrorResponse)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	ASSERT_EQ(NSM_SW_SUCCESS,
		  encode_get_power_profile_resp(0, NSM_ERR_NOT_READY, 0x0042, 0,
						0, nullptr, msg));

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason = 0;
	uint8_t active = 0, count = 0;
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, nullptr, 0));
	EXPECT_EQ(NSM_ERR_NOT_READY, cc);
	EXPECT_EQ(0x0042, reason);
}

TEST(getPowerProfile, testDecodeNullArgs)
{
	auto buf = encodeGetResp(0, {});
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc, active, count;
	uint16_t reason;
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						nullptr, &count, nullptr, 0));
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  decode_get_power_profile_resp(msg, buf.size(), &cc, &reason,
						&active, &count, nullptr, 1));
}

TEST(getSupportedDeviceModesPaged, testGoodEncodeRequest)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_supported_device_modes_paged_req));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());

	ASSERT_EQ(NSM_SW_SUCCESS,
		  encode_get_supported_device_modes_paged_req(0, 0, msg));

	const std::vector<uint8_t> expected{0x10, 0xde, 0x80, 0x89, 0x05, 0x84,
					    0x04, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(expected, buf);

	uint32_t handle = 0xffffffff;
	ASSERT_EQ(NSM_SW_SUCCESS, decode_get_supported_device_modes_paged_req(
				      msg, buf.size(), &handle));
	EXPECT_EQ(0u, handle);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_supported_device_modes_paged_req(
		      msg, buf.size() - 1, &handle));
}

// Reply captured from a QM5: one page listing mode indices 0 and 1.
TEST(getSupportedDeviceModesPaged, testDecodeDeviceReply)
{
	std::vector<uint8_t> buf{0x10, 0xde, 0x00, 0x89, 0x05, 0x84, 0x00,
				 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
	auto msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint32_t next = 0xffffffff;
	uint32_t count = 0;
	uint32_t modes[8]{};
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_supported_device_modes_paged_resp(
		      msg, buf.size(), &cc, &reason, &next, &count, modes, 8));
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0u, next);
	ASSERT_EQ(2u, count);
	EXPECT_EQ(0u, modes[0]);
	EXPECT_EQ(1u, modes[1]);

	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_get_supported_device_modes_paged_resp(
		      msg, buf.size(), &cc, &reason, &next, &count, modes, 1));
	EXPECT_EQ(
	    NSM_SW_ERROR_LENGTH,
	    decode_get_supported_device_modes_paged_resp(
		msg, buf.size() - 1, &cc, &reason, &next, &count, modes, 8));
}

TEST(getSupportedDeviceModesPaged, testResponseRoundTrip)
{
	const uint32_t in[] = {0, 1, 27};
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
				 8 + sizeof(in));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	ASSERT_EQ(NSM_SW_SUCCESS, encode_get_supported_device_modes_paged_resp(
				      0, NSM_SUCCESS, ERR_NULL, 7, 3, in, msg));

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint32_t next = 0;
	uint32_t count = 0;
	uint32_t modes[8]{};
	ASSERT_EQ(NSM_SW_SUCCESS,
		  decode_get_supported_device_modes_paged_resp(
		      msg, buf.size(), &cc, &reason, &next, &count, modes, 8));
	EXPECT_EQ(7u, next);
	ASSERT_EQ(3u, count);
	EXPECT_EQ(27u, modes[2]);

	auto resp =
	    reinterpret_cast<nsm_get_supported_device_modes_paged_resp *>(
		msg->payload);
	resp->hdr.data_size = htole16(8 + 2 * sizeof(uint32_t));
	EXPECT_EQ(NSM_SW_ERROR_DATA,
		  decode_get_supported_device_modes_paged_resp(
		      msg, buf.size(), &cc, &reason, &next, &count, modes, 8));
}

TEST(getSupportedDeviceModesPaged, testErrorResponse)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				 sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	ASSERT_EQ(NSM_SW_SUCCESS, encode_get_supported_device_modes_paged_resp(
				      0, NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL,
				      0, 0, nullptr, msg));

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason = 0;
	uint32_t next = 0;
	uint32_t count = 0;
	ASSERT_EQ(NSM_SW_SUCCESS, decode_get_supported_device_modes_paged_resp(
				      msg, buf.size(), &cc, &reason, &next,
				      &count, nullptr, 0));
	EXPECT_EQ(NSM_ERR_INVALID_DATA_LENGTH, cc);
}
