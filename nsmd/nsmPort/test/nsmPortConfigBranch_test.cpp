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

/*
 * Branch coverage tests for nsmPortConfigurationInfo.cpp:
 * - handleSample: tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
 * - handleSample: unknown tag (e.g. 5, 10)
 * - handleSample: decode_preset_PCIe_data failure (bad data length)
 * - handleSample: decode_PCIe_TxAmplitude_data failure (bad data length)
 * - genRequestMsg: encode failure (invalid instanceId)
 * - setPortConfiguration: empty request (no matching properties)
 * - setPortConfiguration: nullptr value -> InvalidArgument
 * - setPortConfiguration: postPatchIO failure
 * - setPortConfiguration: decode failure
 */

#include "base.h"
#include "pci-links.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPortConfigurationInfo.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmPortConfigBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:70";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    sdbusplus::bus::bus& bus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_cfg_br";
    std::shared_ptr<PCIePortConfigurationInfoIntf> configIntf;
    std::unique_ptr<NsmPCIePortConfigurationInfo> sensor;

    NsmPortConfigBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        configIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
            bus, invPath.c_str());
        sensor = std::make_unique<NsmPCIePortConfigurationInfo>(
            "cfgBranch", "NSM_PCIe", configIntf, uint8_t(2), uint8_t(1),
            uint8_t(0), invPath);
    }

    ~NsmPortConfigBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ===========================================================================
// handleSample: tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (0xEF)
// Covers the TRUE branch of the reserved-tag early return
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_ReservedTag_EarlyReturn)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 0xFF; // > 0xEF
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// handleSample: unknown tag (e.g. tag=5) not in tagToPropertyMap
// Covers the TRUE branch of (it == tagToPropertyMap.end())
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_UnknownTag_ReturnsSuccess)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 5; // not in map (0-4 are valid)
    uint8_t data[4] = {0};
    sample.data = data;
    sample.data_len = sizeof(data);

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// handleSample: decode_preset_PCIe_data failure (wrong data length)
// Covers the TRUE branch of decode_preset_PCIe_data != NSM_SW_SUCCESS
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_PresetDecodeFailure)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 0; // PresetGen3 -> calls decode_preset_PCIe_data
    // Provide zero-length data to trigger decode failure
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmPortConfigBranchTest, HandleSample_PresetDecodeFailure_Tag1)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 1; // PresetGen4
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmPortConfigBranchTest, HandleSample_PresetDecodeFailure_Tag2)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 2; // PresetGen5
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmPortConfigBranchTest, HandleSample_PresetDecodeFailure_Tag3)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 3; // PresetGen6
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// handleSample: decode_PCIe_TxAmplitude_data failure (wrong data length)
// Covers the TRUE branch of decode_PCIe_TxAmplitude_data != NSM_SW_SUCCESS
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_TxAmplitudeDecodeFailure)
{
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 4; // TxAmplitude -> calls decode_PCIe_TxAmplitude_data
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// genRequestMsg: encode failure (invalid instanceId)
// Covers the TRUE branch of (rc != NSM_SW_SUCCESS)
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, GenRequestMsg_EncodeFailure)
{
    auto request = sensor->genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// setPortConfiguration: nullptr value -> InvalidArgument exception
// Covers the TRUE branch of (pciePortConfigData == nullptr)
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, SetPortConfig_NullptrValue_Throws)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    // Pass a string instead of vector<tuple<string,uint32_t>>
    AsyncSetOperationValueType value = std::string("invalid");

    EXPECT_THROW_COROUTINE(
        sensor->setPortConfiguration(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ===========================================================================
// setPortConfiguration: empty request (no matching properties in the vector)
// Covers the TRUE branch of (request.size() == 0)
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, SetPortConfig_EmptyRequest_InvalidArgument)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    // Empty vector -> no samples -> request.size() == 0
    std::vector<std::tuple<std::string, uint32_t>> configData;
    AsyncSetOperationValueType value = configData;

    sensor->setPortConfiguration(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ===========================================================================
// setPortConfiguration: postPatchIO failure
// Covers the TRUE branch of (rc_ != NSM_SW_SUCCESS) after postPatchIO
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, SetPortConfig_PostPatchIOFail)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::vector<std::tuple<std::string, uint32_t>> configData;
    configData.emplace_back("TxAmplitude", uint32_t(3));
    AsyncSetOperationValueType value = configData;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor->setPortConfiguration(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// setPortConfiguration: decode failure (cc != NSM_SUCCESS)
// Covers the else branch of (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, SetPortConfig_DecodeFail)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::vector<std::tuple<std::string, uint32_t>> configData;
    configData.emplace_back("PresetGen3", uint32_t(1));
    AsyncSetOperationValueType value = configData;

    // Build an error response with cc=NSM_ERROR
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    sensor->setPortConfiguration(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ===========================================================================
// setPortConfiguration: success path
// Covers the TRUE branch of (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, SetPortConfig_Success)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::vector<std::tuple<std::string, uint32_t>> configData;
    configData.emplace_back("TxAmplitude", uint32_t(5));
    configData.emplace_back("PresetGen3", uint32_t(2));
    configData.emplace_back("PresetGen4", uint32_t(3));
    configData.emplace_back("PresetGen5", uint32_t(4));
    configData.emplace_back("PresetGen6", uint32_t(5));
    AsyncSetOperationValueType value = configData;

    // Build a proper success response for decode_set_port_config_aggregate_resp
    std::vector<uint8_t> successResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* successMsg = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, ERR_NULL, successMsg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    sensor->setPortConfiguration(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ===========================================================================
// handleSample: valid preset data (tags 0-3) - success path
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_ValidPresetTag0_Success)
{
    uint8_t reading[64] = {};
    size_t readingLen = 0;
    encode_preset_PCIe_data(5, reading, &readingLen);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 0;
    sample.data = reading;
    sample.data_len = readingLen;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// handleSample: valid TxAmplitude data (tag 4) - success path
// ===========================================================================
TEST_F(NsmPortConfigBranchTest, HandleSample_ValidTxAmplitudeTag4_Success)
{
    uint8_t reading[64] = {};
    size_t readingLen = 0;
    encode_PCIe_TxAmplitude_data(7, reading, &readingLen);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 4;
    sample.data = reading;
    sample.data_len = readingLen;

    auto rc = sensor->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(configIntf->txAmplitude(), 7u);
}
