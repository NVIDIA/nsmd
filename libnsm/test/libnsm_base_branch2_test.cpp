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

/**
 * Branch coverage for base.c histogram, GPIO, and event functions.
 * Targets secondary null checks and cc non-success paths not exercised
 * by the existing libnsm_base_branch_coverage_test.cpp.
 */

#include "base.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: minimal 9-byte buffer with cc=0xFF (non-success)
// decode_reason_code_and_cc: reads cc first, then checks msg_len==9 for error
// ---------------------------------------------------------------------------
static std::vector<uint8_t> errCcBuf9()
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code = 0xFF
	return buf;
}

// ===========================================================================
// decode_get_histogram_format_req — secondary null: histogram_id==NULL,
// parameter==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramFormatReq_NullHistogramId)
{
	// Craft a valid-length buffer
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req), 0);
	auto *req = reinterpret_cast<nsm_get_histogram_format_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t param = 0;
	auto rc = decode_get_histogram_format_req(msg, buf.size(),
						  nullptr, // histogram_id==NULL
						  &param);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramFormatReq_NullParameter)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req), 0);
	auto *req = reinterpret_cast<nsm_get_histogram_format_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint32_t hid = 0;
	auto rc = decode_get_histogram_format_req(msg, buf.size(), &hid,
						  nullptr); // parameter==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_histogram_format_resp — secondary null checks:
// data_size==NULL, meta_data==NULL, bucket_offsets==NULL,
// bucket_offsets_size==NULL; cc non-success path
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramFormatResp_NullDataSize)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_histogram_format_metadata meta{};
	uint8_t offsets[4]{};
	uint32_t offsets_sz = 0;

	auto rc =
	    decode_get_histogram_format_resp(msg, buf.size(), &cc, &reason,
					     nullptr, // data_size==NULL
					     &meta, offsets, &offsets_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramFormatResp_NullMetaData)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint8_t offsets[4]{};
	uint32_t offsets_sz = 0;

	auto rc = decode_get_histogram_format_resp(msg, buf.size(), &cc,
						   &reason, &dsz,
						   nullptr, // meta_data==NULL
						   offsets, &offsets_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramFormatResp_NullBucketOffsets)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	nsm_histogram_format_metadata meta{};
	uint32_t offsets_sz = 0;

	auto rc = decode_get_histogram_format_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &meta,
	    nullptr, // bucket_offsets==NULL
	    &offsets_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramFormatResp_NullBucketOffsetsSz)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	nsm_histogram_format_metadata meta{};
	uint8_t offsets[4]{};

	auto rc = decode_get_histogram_format_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &meta, offsets,
	    nullptr); // bucket_offsets_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramFormatResp_CcNonSuccess)
{
	// All out-params non-null; cc=0xFF → decode_reason_code_and_cc returns
	// NSM_SW_SUCCESS with *cc=0xFF, then `*cc != NSM_SUCCESS` TRUE branch
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	nsm_histogram_format_metadata meta{};
	uint8_t offsets[4]{};
	uint32_t offsets_sz = 0;

	auto rc = decode_get_histogram_format_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &meta, offsets, &offsets_sz);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_histogram_data_req — secondary null: histogram_id==NULL,
// parameter==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramDataReq_NullHistogramId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req), 0);
	auto *req = reinterpret_cast<nsm_get_histogram_data_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t param = 0;
	auto rc = decode_get_histogram_data_req(msg, buf.size(),
						nullptr, // histogram_id==NULL
						&param);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataReq_NullParameter)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req), 0);
	auto *req = reinterpret_cast<nsm_get_histogram_data_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint32_t hid = 0;
	auto rc = decode_get_histogram_data_req(msg, buf.size(), &hid,
						nullptr); // parameter==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_histogram_data_resp — secondary null checks + cc non-success
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramDataResp_NullDataSize)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t bdt = 0;
	uint16_t nob = 0;
	uint8_t bdata[4]{};
	uint32_t bdata_sz = 0;

	auto rc = decode_get_histogram_data_resp(msg, buf.size(), &cc, &reason,
						 nullptr, // data_size==NULL
						 &bdt, &nob, bdata, &bdata_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataResp_NullBucketDataType)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint16_t nob = 0;
	uint8_t bdata[4]{};
	uint32_t bdata_sz = 0;

	auto rc =
	    decode_get_histogram_data_resp(msg, buf.size(), &cc, &reason, &dsz,
					   nullptr, // bucket_data_type==NULL
					   &nob, bdata, &bdata_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataResp_NullNumOfBuckets)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint8_t bdt = 0;
	uint8_t bdata[4]{};
	uint32_t bdata_sz = 0;

	auto rc = decode_get_histogram_data_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &bdt,
	    nullptr, // num_of_buckets==NULL
	    bdata, &bdata_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataResp_NullBucketData)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint8_t bdt = 0;
	uint16_t nob = 0;
	uint32_t bdata_sz = 0;

	auto rc = decode_get_histogram_data_resp(msg, buf.size(), &cc, &reason,
						 &dsz, &bdt, &nob,
						 nullptr, // bucket_data==NULL
						 &bdata_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataResp_NullBucketDataSize)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint8_t bdt = 0;
	uint16_t nob = 0;
	uint8_t bdata[4]{};

	auto rc = decode_get_histogram_data_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &bdt, &nob, bdata,
	    nullptr); // bucket_data_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetHistogramDataResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t dsz = 0;
	uint8_t bdt = 0;
	uint16_t nob = 0;
	uint8_t bdata[4]{};
	uint32_t bdata_sz = 0;

	auto rc = decode_get_histogram_data_resp(
	    msg, buf.size(), &cc, &reason, &dsz, &bdt, &nob, bdata, &bdata_sz);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_gpio_state_req — secondary null: offset==NULL, length==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeGetGpioStateReq_NullOffset)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_get_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t length = 0;
	auto rc = decode_get_gpio_state_req(msg, buf.size(),
					    nullptr, // offset==NULL
					    &length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetGpioStateReq_NullLength)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_get_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t offset = 0;
	auto rc = decode_get_gpio_state_req(msg, buf.size(), &offset,
					    nullptr); // length==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_gpio_state_resp — secondary null checks + cc non-success
// ===========================================================================
TEST(BaseBranch2, DecodeGetGpioStateResp_NullOffset)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc = decode_get_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     nullptr, // offset==NULL
					     &length, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetGpioStateResp_NullLength)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc =
	    decode_get_gpio_state_resp(msg, buf.size(), &cc, &reason, &offset,
				       nullptr, // length==NULL
				       vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetGpioStateResp_NullGpioValues)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint32_t vals_sz = 0;

	auto rc = decode_get_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length,
					     nullptr, // gpio_values==NULL
					     &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetGpioStateResp_NullGpioValuesSz)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};

	auto rc = decode_get_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length, vals,
					     nullptr); // gpio_values_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeGetGpioStateResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc = decode_get_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// encode_set_gpio_state_req — compound condition: gpio_values_size>0 &&
// gpio_values==NULL → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(BaseBranch2, EncodeSetGpioStateReq_GpioValuesSizeNonZeroNullPtr)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req) + 4, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	// gpio_values_size=1 > 0 AND gpio_values=NULL → ERROR_DATA
	auto rc = encode_set_gpio_state_req(0, 0, 1,
					    nullptr, // gpio_values==NULL
					    1,	     // gpio_values_size > 0
					    msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_gpio_state_req — secondary null: offset==NULL, length==NULL,
// gpio_values_size==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeSetGpioStateReq_NullOffset)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_set_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t length = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;
	auto rc = decode_set_gpio_state_req(msg, buf.size(),
					    nullptr, // offset==NULL
					    &length, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateReq_NullLength)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_set_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t offset = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;
	auto rc = decode_set_gpio_state_req(msg, buf.size(), &offset,
					    nullptr, // length==NULL
					    vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateReq_NullGpioValuesSz)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_set_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};
	auto rc =
	    decode_set_gpio_state_req(msg, buf.size(), &offset, &length, vals,
				      nullptr); // gpio_values_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_gpio_state_resp — secondary null checks + cc non-success
// ===========================================================================
TEST(BaseBranch2, DecodeSetGpioStateResp_NullOffset)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc = decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     nullptr, // offset==NULL
					     &length, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateResp_NullLength)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc =
	    decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason, &offset,
				       nullptr, // length==NULL
				       vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateResp_NullGpioValues)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint32_t vals_sz = 0;

	auto rc = decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length,
					     nullptr, // gpio_values==NULL
					     &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateResp_NullGpioValuesSz)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};

	auto rc = decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length, vals,
					     nullptr); // gpio_values_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeSetGpioStateResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t vals[4]{};
	uint32_t vals_sz = 0;

	auto rc = decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &offset, &length, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_event_acknowledgement — secondary null:
// instance_id==NULL, nsm_type==NULL, event_id==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeNsmEventAck_NullInstanceId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_event_ack),
				 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t nsm_type = 0;
	uint8_t event_id = 0;

	auto rc = decode_nsm_event_acknowledgement(msg, buf.size(),
						   nullptr, // instance_id==NULL
						   &nsm_type, &event_id);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(BaseBranch2, DecodeNsmEventAck_NullNsmType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_event_ack),
				 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t inst_id = 0;
	uint8_t event_id = 0;

	auto rc = decode_nsm_event_acknowledgement(msg, buf.size(), &inst_id,
						   nullptr, // nsm_type==NULL
						   &event_id);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(BaseBranch2, DecodeNsmEventAck_NullEventId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_event_ack),
				 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t inst_id = 0;
	uint8_t nsm_type = 0;

	auto rc = decode_nsm_event_acknowledgement(msg, buf.size(), &inst_id,
						   &nsm_type,
						   nullptr); // event_id==NULL
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// ===========================================================================
// decode_nsm_event — secondary null: event_state==NULL, data_size==NULL
// ===========================================================================
TEST(BaseBranch2, DecodeNsmEvent_NullEventState)
{
	// Need a buffer at least sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN = 4+6
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t data_size = 0;

	auto rc = decode_nsm_event(msg, buf.size(), 0, 0,
				   nullptr, // event_state==NULL
				   &data_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(BaseBranch2, DecodeNsmEvent_NullDataSize)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t event_state = 0;

	auto rc = decode_nsm_event(msg, buf.size(), 0, 0, &event_state,
				   nullptr); // data_size==NULL
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_event_with_data — L843 FALSE (data_size==0, skip memcpy),
//                               L844 TRUE  (data_size>0, data==NULL)
// ===========================================================================
TEST(BaseBranch2, DecodeNsmEventWithData_ZeroDataSize_SkipsMemcpy)
{
	// Build event with data_size=0: version/ackr byte, event_id=1,
	// event_class=0, event_state=0, data_size=0 → L843 FALSE branch
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	// event->data_size is byte at offset 5 in payload (0-based):
	// byte0: version/ackr/resvd, byte1: event_id, byte2: event_class,
	// byte3-4: event_state LE, byte5: data_size — leave at 0
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t event_state = 0;
	uint8_t data_size_out = 0xFF;
	uint8_t data_buf[4] = {0};

	auto rc = decode_nsm_event_with_data(
	    msg, buf.size(), 0, 0, &event_state, &data_size_out, data_buf);
	// data_size == 0 → FALSE branch at L843 → skip memcpy → NSM_SW_SUCCESS
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_size_out, 0);
}

TEST(BaseBranch2, DecodeNsmEventWithData_NonZeroDataSizeNullData)
{
	// Build event with data_size=1: triggers L843 TRUE, then L844 TRUE
	// because we pass data==NULL
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 1,
				 0);
	// Set data_size field (byte 5 in payload)
	buf[sizeof(nsm_msg_hdr) + 5] = 1;
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t event_state = 0;
	uint8_t data_size_out = 0;

	auto rc = decode_nsm_event_with_data(msg, buf.size(), 0, 0,
					     &event_state, &data_size_out,
					     nullptr); // data==NULL → L844 TRUE
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_histogram_format_req — L1250 TRUE (data_size too small)
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramFormatReq_DataSizeTooSmall)
{
	// Build buffer large enough to pass length check, but hdr.data_size=0
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req), 0);
	// Leave req->hdr.data_size = 0 → < sizeof(histogram_id)+sizeof(param)=6
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t hid = 0;
	uint16_t param = 0;
	auto rc =
	    decode_get_histogram_format_req(msg, buf.size(), &hid, &param);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_histogram_format_resp — L1306 TRUE (bucket_offsets==NULL)
// ===========================================================================
TEST(BaseBranch2, EncodeGetHistogramFormatResp_NullBucketOffsets)
{
	// cc=NSM_SUCCESS, bucket_offsets=NULL → should return NSM_SW_ERROR_NULL
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_histogram_format_metadata meta{};
	meta.num_of_buckets = 0;
	meta.bucket_data_type = 0;

	auto rc = encode_get_histogram_format_resp(0, NSM_SUCCESS, 0, &meta,
						   nullptr, // bucket_offsets
						   0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_histogram_data_req — L1411 TRUE (data_size too small)
// ===========================================================================
TEST(BaseBranch2, DecodeGetHistogramDataReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req), 0);
	// Leave hdr.data_size = 0 → < sizeof(histogram_id)+sizeof(param)=6
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t hid = 0;
	uint16_t param = 0;
	auto rc = decode_get_histogram_data_req(msg, buf.size(), &hid, &param);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_histogram_data_resp — L1458 TRUE (bucket_data==NULL)
// ===========================================================================
TEST(BaseBranch2, EncodeGetHistogramDataResp_NullBucketData)
{
	// cc=NSM_SUCCESS, bucket_data=NULL → NSM_SW_ERROR_NULL
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_get_histogram_data_resp(0, NSM_SUCCESS, 0,
						 0,	  // bucket_data_type
						 0,	  // num_of_buckets
						 nullptr, // bucket_data == NULL
						 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_gpio_state_req — L1552 TRUE (data_size too small)
// ===========================================================================
TEST(BaseBranch2, DecodeGetGpioStateReq_DataSizeTooSmall)
{
	// Build large enough buffer, but leave hdr.data_size = 0 < 4
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req), 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t off = 0, len = 0;
	auto rc = decode_get_gpio_state_req(msg, buf.size(), &off, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_gpio_state_resp — L1598 TRUE (gpio_values==NULL when cc==SUCCESS)
// ===========================================================================
TEST(BaseBranch2, EncodeGetGpioStateResp_NullGpioValues)
{
	// cc=NSM_SUCCESS, gpio_values=NULL → NSM_SW_ERROR_NULL
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_get_gpio_state_resp(0, NSM_SUCCESS, 0, 0, 0,
					     nullptr, // gpio_values == NULL
					     0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_gpio_state_req — L1674 FALSE (gpio_values_size==0, skip memcpy)
// ===========================================================================
TEST(BaseBranch2, EncodeSetGpioStateReq_ZeroSize_SkipsMemcpy)
{
	// gpio_values_size=0, gpio_values=non-null → condition FALSE, skip
	// memcpy
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req) + 4, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t vals[4] = {0};

	auto rc =
	    encode_set_gpio_state_req(0, 0, 0, vals,
				      0, // gpio_values_size == 0 → L1674 FALSE
				      msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_gpio_state_req — L1698 TRUE (data_size too small)
// ===========================================================================
TEST(BaseBranch2, DecodeSetGpioStateReq_DataSizeTooSmall)
{
	// Build large buffer, hdr.data_size=0 < sizeof(offset)+sizeof(length)=4
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req), 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t off = 0, len = 0;
	uint8_t vals[4] = {0};
	uint32_t vals_sz = 0;
	auto rc = decode_set_gpio_state_req(msg, buf.size(), &off, &len, vals,
					    &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_gpio_state_req — L1710 FALSE (gpio_values==NULL skips memcpy)
// ===========================================================================
TEST(BaseBranch2, DecodeSetGpioStateReq_NullGpioValues_SkipsMemcpy)
{
	// Build buffer with data_size = 4 (offset+length) →
	// expected_gpio_size=0 Then gpio_values=NULL, expected_gpio_size=0 →
	// L1710 FALSE → succeeds
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_gpio_state_req), 0);
	auto *req = reinterpret_cast<nsm_set_gpio_state_req *>(
	    buf.data() + sizeof(nsm_msg_hdr));
	// data_size = sizeof(offset)+sizeof(length) = 4 → expected_gpio_size=0
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t off = 0, len = 0;
	uint32_t vals_sz = 0;
	auto rc = decode_set_gpio_state_req(msg, buf.size(), &off, &len,
					    nullptr, // gpio_values==NULL
					    &vals_sz);
	// expected_gpio_size=0 → L1710: (NULL != NULL && 0 > 0) → FALSE →
	// succeed
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_set_gpio_state_resp — L1752 TRUE (gpio_values==NULL when cc==SUCCESS)
// ===========================================================================
TEST(BaseBranch2, EncodeSetGpioStateResp_NullGpioValues)
{
	// cc=NSM_SUCCESS, gpio_values=NULL → NSM_SW_ERROR_NULL
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_set_gpio_state_resp(0, NSM_SUCCESS, 0, 0, 0,
					     nullptr, // gpio_values == NULL
					     0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_gpio_state_resp — L1776 TRUE (msg_len too small after cc check)
// ===========================================================================
TEST(BaseBranch2, DecodeSetGpioStateResp_MsgLenTooSmall)
{
	// Buffer: 9 bytes (valgrind safe), cc=0=NSM_SUCCESS
	// → decode_reason_code_and_cc returns NSM_SW_SUCCESS, *cc=0
	// → msg_len < sizeof(nsm_set_gpio_state_resp)+sizeof(nsm_msg_hdr)
	// → NSM_SW_ERROR_LENGTH
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	// cc byte = 0 = NSM_SUCCESS (default zero fill)
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t off = 0, len = 0;
	uint8_t vals[4] = {0};
	uint32_t vals_sz = 0;
	auto rc = decode_set_gpio_state_resp(msg, buf.size(), &cc, &reason,
					     &off, &len, vals, &vals_sz);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_raw_cmd_req_v2 — L1192 TRUE (encode_common_req_v2 fails, iid=32)
// encode_common_req_v2 calls pack_nsm_header_v2 which fails when iid > 31
// ===========================================================================
TEST(BaseBranch2, EncodeRawCmdReqV2_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t payload[4] = {};
	// instance_id=32 causes encode_common_req_v2 → pack_nsm_header_v2 to
	// fail
	auto rc =
	    encode_raw_cmd_req_v2(32, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				  NSM_PING, payload, sizeof(payload), msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
