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
 * Branch coverage tests for libnsm/firmware-utils.c
 * Covers "if (rc != NSM_SW_SUCCESS)" TRUE branches after pack_nsm_header.
 *
 * pack_nsm_header returns NSM_SW_ERROR_DATA when instance_id > NSM_INSTANCE_MAX
 * (31). Functions that assign header.instance_id = instance_id without masking
 * will fail when instance_id = 32. This file tests those error-path branches.
 */

#include "firmware-utils.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t BAD_IIDX = 32; // > NSM_INSTANCE_MAX (31)

// Convenience buffer for nsm_msg output
static std::vector<uint8_t> msgBuf(1024, 0);
static nsm_msg *msgPtr() { return reinterpret_cast<nsm_msg *>(msgBuf.data()); }

// ---------------------------------------------------------------------------
// encode_nsm_query_get_erot_state_parameters_resp — L352
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeGetErotStateParamResp_PackFail)
{
	auto rc = encode_nsm_query_get_erot_state_parameters_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, nullptr, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_req — L886
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeIrreversibleConfigReq_PackFail)
{
	struct nsm_firmware_irreversible_config_req fw_req = {};
	auto rc = encode_nsm_firmware_irreversible_config_req(BAD_IIDX, &fw_req,
							      msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_0_resp — L940
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeIrreversibleConfigReq0Resp_PackFail)
{
	struct nsm_firmware_irreversible_config_request_0_resp cfg_resp = {};
	auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, &cfg_resp, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_1_resp — L983
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeIrreversibleConfigReq1Resp_PackFail)
{
	auto rc = encode_nsm_firmware_irreversible_config_request_1_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_2_resp — L1021
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeIrreversibleConfigReq2Resp_PackFail)
{
	struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
	auto rc = encode_nsm_firmware_irreversible_config_request_2_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, &cfg_resp, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_query_resp — L1279
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeCodeAuthKeyPermQueryResp_PackFail)
{
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, 0, 0, 0, nullptr, nullptr, nullptr,
	    nullptr, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_update_req — L1392
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeCodeAuthKeyPermUpdateReq_PackFail)
{
	auto rc = encode_nsm_code_auth_key_perm_update_req(
	    BAD_IIDX,
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE, 0, 0, 0,
	    0, 0, nullptr, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_update_resp — L1463
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeCodeAuthKeyPermUpdateResp_PackFail)
{
	auto rc = encode_nsm_code_auth_key_perm_update_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, 0, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_query_firmware_security_version_number_resp — L1556
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeQueryFwSecVerNumResp_PackFail)
{
	struct nsm_firmware_security_version_number_resp sec_info = {};
	auto rc = encode_nsm_query_firmware_security_version_number_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, &sec_info, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_update_sec_ver_req — L1629
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeFwUpdateSecVerReq_PackFail)
{
	struct nsm_firmware_update_min_sec_ver_req fw_req = {};
	auto rc =
	    encode_nsm_firmware_update_sec_ver_req(BAD_IIDX, &fw_req, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_update_sec_ver_resp — L1682
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeFwUpdateSecVerResp_PackFail)
{
	struct nsm_firmware_update_min_sec_ver_resp sec_resp = {};
	auto rc = encode_nsm_firmware_update_sec_ver_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, &sec_resp, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_set_rot_property_req — L1750
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeFwSetRotPropertyReq_PackFail)
{
	struct nsm_firmware_set_rot_property_req fw_req = {};
	auto rc = encode_nsm_firmware_set_rot_property_req(BAD_IIDX, &fw_req,
							   msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_set_rot_property_resp — L1821
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeFwSetRotPropertyResp_PackFail)
{
	auto rc = encode_nsm_firmware_set_rot_property_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_image_copy_control_req — L2055
// ---------------------------------------------------------------------------
TEST(FwUtilsPackFailBranch, EncodeFwImageCopyControlReq_PackFail)
{
	struct nsm_firmware_image_copy_control_req fw_req = {};
	auto rc = encode_nsm_firmware_image_copy_control_req(BAD_IIDX, &fw_req,
							     nullptr, msgPtr());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}
