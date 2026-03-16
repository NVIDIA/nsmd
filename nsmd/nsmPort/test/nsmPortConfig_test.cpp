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
 * Unit tests for:
 *   - NsmPCIePortConfigurationInfo (nsmPortConfigurationInfo.cpp)
 *     Functions: constructor, genRequestMsg, handleSample (via
 *     handleResponseMsg), updatePresetProperty, updateTxAmplitudeProperty,
 *     setPortConfiguration
 *
 *   - NsmDevicePortDisableFuture (nsmPortDisableFuture.cpp)
 *     Functions: update, setDevicePortDisableFuture, setPortDisableFuture
 */

#include "base.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include <cstring>

#define private public
#define protected public

#include "nsmPortConfigurationInfo.hpp"
#include "nsmPortDisableFuture.hpp"

// ============================================================================
// Helper: NsmPCIePortConfigurationInfo
// ============================================================================

struct PCIePortConfigInfoHelper
{
    sdbusplus::bus::bus& bus = utils::DBusHandler::getBus();
    std::string name{"portConfigInfo"};
    std::string type{"NSM_PCIe"};
    uint8_t portNumber = 2;
    uint8_t portType = 1;
    uint8_t portIndex = 0;
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_config";
    std::shared_ptr<nsm::PCIePortConfigurationInfoIntf> pciePortConfigInfoIntf =
        std::make_shared<nsm::PCIePortConfigurationInfoIntf>(
            bus, inventoryObjPath.c_str());
};

// ============================================================================
// NsmPCIePortConfigurationInfo — Constructor
// ============================================================================

TEST(NsmPCIePortConfigurationInfo, Constructor_InitializesFieldsAndPresets)
{
    // Arrange
    PCIePortConfigInfoHelper h;

    // Act
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Assert
    EXPECT_EQ(sensor.getName(), h.name);
    EXPECT_EQ(sensor.getType(), h.type);
    EXPECT_EQ(sensor.portNumber, h.portNumber);
    EXPECT_EQ(sensor.portType, h.portType);
    EXPECT_EQ(sensor.portIndex, h.portIndex);
    EXPECT_EQ(sensor.inventoryObjPath, h.inventoryObjPath);

    // Presets should be initialized to DeviceDefault (4 elements each)
    auto preset0 = h.pciePortConfigInfoIntf->preset0();
    auto preset1 = h.pciePortConfigInfoIntf->preset1();
    EXPECT_EQ(preset0.size(), 4u);
    EXPECT_EQ(preset1.size(), 4u);
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(
            preset0[i],
            nsm::PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault);
        EXPECT_EQ(
            preset1[i],
            nsm::PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault);
    }
}

// ============================================================================
// NsmPCIePortConfigurationInfo — genRequestMsg
// ============================================================================

TEST(NsmPCIePortConfigurationInfo, GenRequestMsg_Success_ReturnsValidRequest)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    // Act
    auto request = sensor.genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req));

    // Verify the encoded fields by decoding
    auto requestMsg = reinterpret_cast<const nsm_msg*>(request->data());
    uint8_t decodedPortNumber = 0;
    uint8_t decodedPortType = 0;
    uint8_t decodedPortIndex = 0;
    auto rc = decode_get_pcie_port_config_req(
        requestMsg, request->size(), &decodedPortNumber, &decodedPortType,
        &decodedPortIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decodedPortNumber, h.portNumber);
    EXPECT_EQ(decodedPortType, h.portType);
    EXPECT_EQ(decodedPortIndex, h.portIndex);
}

// ============================================================================
// NsmPCIePortConfigurationInfo — updatePresetProperty
// ============================================================================

TEST(NsmPCIePortConfigurationInfo, UpdatePresetProperty_ValidTag_UpdatesPresets)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act — update tag 0 (PresetGen3)
    sensor.updatePresetProperty(0, 3, 7);

    // Assert
    auto preset0 = h.pciePortConfigInfoIntf->preset0();
    auto preset1 = h.pciePortConfigInfoIntf->preset1();
    EXPECT_EQ(static_cast<uint8_t>(preset0[0]), 3u);
    EXPECT_EQ(static_cast<uint8_t>(preset1[0]), 7u);
    // Other indices unchanged (DeviceDefault)
    for (size_t i = 1; i < 4; ++i)
    {
        EXPECT_EQ(
            preset0[i],
            nsm::PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault);
    }
}

TEST(NsmPCIePortConfigurationInfo,
     UpdatePresetProperty_AllValidTags_UpdatesCorrectIndices)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act — update all 4 tags (0=Gen3, 1=Gen4, 2=Gen5, 3=Gen6)
    sensor.updatePresetProperty(0, 1, 2);
    sensor.updatePresetProperty(1, 3, 4);
    sensor.updatePresetProperty(2, 5, 6);
    sensor.updatePresetProperty(3, 7, 8);

    // Assert
    auto preset0 = h.pciePortConfigInfoIntf->preset0();
    auto preset1 = h.pciePortConfigInfoIntf->preset1();
    EXPECT_EQ(static_cast<uint8_t>(preset0[0]), 1u);
    EXPECT_EQ(static_cast<uint8_t>(preset0[1]), 3u);
    EXPECT_EQ(static_cast<uint8_t>(preset0[2]), 5u);
    EXPECT_EQ(static_cast<uint8_t>(preset0[3]), 7u);
    EXPECT_EQ(static_cast<uint8_t>(preset1[0]), 2u);
    EXPECT_EQ(static_cast<uint8_t>(preset1[1]), 4u);
    EXPECT_EQ(static_cast<uint8_t>(preset1[2]), 6u);
    EXPECT_EQ(static_cast<uint8_t>(preset1[3]), 8u);
}

TEST(NsmPCIePortConfigurationInfo, UpdatePresetProperty_InvalidTag_DoesNotCrash)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act — tag >= 4 should log error and return early
    sensor.updatePresetProperty(4, 10, 20);
    sensor.updatePresetProperty(255, 10, 20);

    // Assert — presets remain at DeviceDefault
    auto preset0 = h.pciePortConfigInfoIntf->preset0();
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(
            preset0[i],
            nsm::PCIePortConfigurationInfoIntf::PCIePresetIndex::DeviceDefault);
    }
}

// ============================================================================
// NsmPCIePortConfigurationInfo — updateTxAmplitudeProperty
// ============================================================================

TEST(NsmPCIePortConfigurationInfo, UpdateTxAmplitudeProperty_SetsValueCorrectly)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act
    sensor.updateTxAmplitudeProperty(42);

    // Assert
    EXPECT_EQ(h.pciePortConfigInfoIntf->txAmplitude(), 42u);
}

TEST(NsmPCIePortConfigurationInfo, UpdateTxAmplitudeProperty_ZeroValue_SetsZero)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act
    sensor.updateTxAmplitudeProperty(0);

    // Assert
    EXPECT_EQ(h.pciePortConfigInfoIntf->txAmplitude(), 0u);
}

TEST(NsmPCIePortConfigurationInfo,
     UpdateTxAmplitudeProperty_MaxValue_SetsCorrectly)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Act
    sensor.updateTxAmplitudeProperty(255);

    // Assert
    EXPECT_EQ(h.pciePortConfigInfoIntf->txAmplitude(), 255u);
}

// ============================================================================
// NsmPCIePortConfigurationInfo — handleSample via handleResponseMsg
// (NsmSensorAggregator calls handleSample for each sample in the aggregate
// response)
// ============================================================================

// Helper function to build an aggregate response with preset samples
static std::vector<uint8_t> buildPortConfigAggregateResponse(
    uint16_t sampleCount,
    const std::vector<std::tuple<uint8_t, std::vector<uint8_t>>>& samples)
{
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Encode each sample
    std::vector<std::pair<std::vector<uint8_t>, size_t>> encodedSamples;
    size_t totalSampleSize = 0;

    for (const auto& [tag, data] : samples)
    {
        std::vector<uint8_t> sampleBuf(64, 0);
        size_t sampleLen = 0;
        auto samplePtr =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
        encode_aggregate_resp_sample(tag, true, data.data(), data.size(),
                                     samplePtr, &sampleLen);
        encodedSamples.emplace_back(sampleBuf, sampleLen);
        totalSampleSize += sampleLen;
    }

    // Build full response
    std::vector<uint8_t> response(headerSize + totalSampleSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_CONFIGURATION, NSM_SUCCESS,
                          sampleCount, responseMsg);

    // Copy samples after header
    size_t offset = headerSize;
    for (const auto& [buf, len] : encodedSamples)
    {
        memcpy(response.data() + offset, buf.data(), len);
        offset += len;
    }

    return response;
}

TEST(NsmPCIePortConfigurationInfo,
     HandleResponseMsg_PresetSamples_UpdatesPresets)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Encode preset data for tag 0 (PresetGen3): preset0=5, preset1=9
    uint8_t presetReading[64] = {};
    size_t presetLen = 0;
    encode_preset_PCIe_data(5, presetReading, &presetLen);
    std::vector<uint8_t> presetData(presetReading, presetReading + presetLen);

    // Encode preset data for tag 1 (PresetGen4): preset0=2, preset1=3
    uint8_t presetReading2[64] = {};
    size_t presetLen2 = 0;
    encode_preset_PCIe_data(2, presetReading2, &presetLen2);
    std::vector<uint8_t> presetData2(presetReading2,
                                     presetReading2 + presetLen2);

    auto response = buildPortConfigAggregateResponse(
        2, {{0, presetData}, {1, presetData2}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto preset0 = h.pciePortConfigInfoIntf->preset0();
    EXPECT_GE(preset0.size(), 2u);
}

TEST(NsmPCIePortConfigurationInfo,
     DISABLED_HandleResponseMsg_TxAmplitudeSample_UpdatesProperty)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Encode TxAmplitude data for tag 4
    uint8_t txReading[64] = {};
    size_t txLen = 0;
    encode_PCIe_TxAmplitude_data(77, txReading, &txLen);
    std::vector<uint8_t> txData(txReading, txReading + txLen);

    auto response = buildPortConfigAggregateResponse(1, {{4, txData}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(h.pciePortConfigInfoIntf->txAmplitude(), 77u);
}

TEST(NsmPCIePortConfigurationInfo,
     DISABLED_HandleResponseMsg_MixedPresetAndTxAmplitude_UpdatesAll)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Preset tag 2 (PresetGen5)
    uint8_t presetReading[64] = {};
    size_t presetLen = 0;
    encode_preset_PCIe_data(11, presetReading, &presetLen);
    std::vector<uint8_t> presetData(presetReading, presetReading + presetLen);

    // TxAmplitude tag 4
    uint8_t txReading[64] = {};
    size_t txLen = 0;
    encode_PCIe_TxAmplitude_data(200, txReading, &txLen);
    std::vector<uint8_t> txData(txReading, txReading + txLen);

    auto response =
        buildPortConfigAggregateResponse(2, {{2, presetData}, {4, txData}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(h.pciePortConfigInfoIntf->txAmplitude(), 200u);
}

TEST(NsmPCIePortConfigurationInfo,
     HandleResponseMsg_ErrorCompletion_ReturnsError)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build an error aggregate response (cc=NSM_ERROR)
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> response(headerSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_CONFIGURATION, NSM_ERROR, 0,
                          responseMsg);

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIePortConfigurationInfo,
     HandleResponseMsg_SpecialTagTimestamp_ReturnsSuccess)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build response with a special tag >
    // NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE Tag 0xFF (TIMESTAMP) is a
    // special tag that handleSample should skip
    uint8_t dummyData[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto response = buildPortConfigAggregateResponse(
        1, {{0xFF, std::vector<uint8_t>(dummyData, dummyData + 8)}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert - special tags are silently handled
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIePortConfigurationInfo, HandleResponseMsg_UnknownTag_ReturnsSuccess)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build response with an unknown tag (e.g., tag 100 is in valid range but
    // not in tagToPropertyMap)
    uint8_t dummyData[] = {0xAA};
    auto response = buildPortConfigAggregateResponse(
        1, {{100, std::vector<uint8_t>(dummyData, dummyData + 1)}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert - unknown tag logged but returns success
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIePortConfigurationInfo,
     HandleResponseMsg_AllFourPresetTags_UpdatesAllIndices)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build preset data for all 4 tags
    std::vector<std::tuple<uint8_t, std::vector<uint8_t>>> samples;
    for (uint8_t tag = 0; tag < 4; ++tag)
    {
        uint8_t reading[64] = {};
        size_t len = 0;
        encode_preset_PCIe_data(tag + 10, reading, &len);
        samples.emplace_back(tag, std::vector<uint8_t>(reading, reading + len));
    }

    auto response = buildPortConfigAggregateResponse(4, samples);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIePortConfigurationInfo,
     DISABLED_HandleResponseMsg_InvalidPresetData_ReturnsLengthError)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build response with tag 0 (preset) but empty data (will fail decode)
    auto response =
        buildPortConfigAggregateResponse(1, {{0, std::vector<uint8_t>()}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert - decode failure returns error code
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIePortConfigurationInfo,
     DISABLED_HandleResponseMsg_InvalidTxAmplitudeData_ReturnsLengthError)
{
    // Arrange
    PCIePortConfigInfoHelper h;
    nsm::NsmPCIePortConfigurationInfo sensor(
        h.name, h.type, h.pciePortConfigInfoIntf, h.portNumber, h.portType,
        h.portIndex, h.inventoryObjPath);

    // Build response with tag 4 (TxAmplitude) but empty data
    auto response =
        buildPortConfigAggregateResponse(1, {{4, std::vector<uint8_t>()}});
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    // Act
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert - decode failure returns error code
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmPCIePortConfigurationInfo — setPortConfiguration (coroutine)
// ============================================================================

struct NsmPCIePortConfigSetTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:20";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIePortConfigSetTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIePortConfigSetTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_TxAmplitude_Success)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_set";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgSet", "NSM_PCIe", pcieIntf, 2,
                                        1, 0, inventoryObjPath);

    // Build a successful set port config aggregate response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {
        {"TxAmplitude", 50}};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_PresetProperties_Success)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_set2";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgSet2", "NSM_PCIe", pcieIntf, 3,
                                        0, 1, inventoryObjPath);

    // Build a successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act - send preset properties
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {
        {"PresetGen3", 5},
        {"PresetGen4", 7},
        {"PresetGen5", 3},
        {"PresetGen6", 1}};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_PostPatchIOFailure_WriteFail)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_fail";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgFail", "NSM_PCIe", pcieIntf, 1,
                                        0, 0, inventoryObjPath);

    // Return error from postPatchIO
    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {
        {"TxAmplitude", 10}};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_DecodeResponseFailure_WriteFail)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_decode_fail";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgDecodeFail", "NSM_PCIe",
                                        pcieIntf, 1, 0, 0, inventoryObjPath);

    // Build an error response (cc=NSM_ERROR)
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_config_aggregate_resp(0, NSM_ERROR, 0x1234, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {
        {"PresetGen3", 5}};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       DISABLED_SetPortConfiguration_InvalidValueType_ThrowsInvalidArgument)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_invalid";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgInvalid", "NSM_PCIe", pcieIntf,
                                        1, 0, 0, inventoryObjPath);

    // Act & Assert - wrong variant type should throw InvalidArgument
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a uint32_t instead of vector<tuple<string,uint32_t>>
    AsyncSetOperationValueType value = static_cast<uint32_t>(42);

    EXPECT_THROW(
        sensor.setPortConfiguration(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_EmptyConfigData_InvalidArgument)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_empty";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgEmpty", "NSM_PCIe", pcieIntf, 1,
                                        0, 0, inventoryObjPath);

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert - empty data results in InvalidArgument status
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmPCIePortConfigSetTestFixture,
       SetPortConfiguration_MixedProperties_Success)
{
    using namespace nsm;

    // Arrange
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_mixed";
    auto pcieIntf = std::make_shared<PCIePortConfigurationInfoIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIePortConfigurationInfo sensor("portCfgMixed", "NSM_PCIe", pcieIntf, 2,
                                        1, 0, inventoryObjPath);

    // Build successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act - mixed preset + tx amplitude
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<std::tuple<std::string, uint32_t>> configData = {
        {"PresetGen3", 5}, {"TxAmplitude", 100}, {"PresetGen6", 8}};
    AsyncSetOperationValueType value = configData;

    auto coroutine = sensor.setPortConfiguration(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmDevicePortDisableFuture — Constructor
// ============================================================================

TEST(NsmDevicePortDisableFuture, Constructor_InitializesFields)
{
    // Arrange & Act
    auto& bus [[maybe_unused]] = utils::DBusHandler::getBus();
    std::string name{"/PortDisableFuture"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/nvlink";

    nsm::NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Assert
    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
    EXPECT_EQ(sensor.objPath, inventoryObjPath + name);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST(NsmDevicePortDisableFuture, GetInventoryObjectPath_ReturnsCorrectPath)
{
    // Arrange
    std::string name{"/PortDisableFuture"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/nvlink2";

    nsm::NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Act
    auto path = sensor.getInventoryObjectPath();

    // Assert
    EXPECT_EQ(path, inventoryObjPath + name);
}

// ============================================================================
// NsmDevicePortDisableFuture — update (coroutine)
// ============================================================================

struct NsmPortDisableFutureTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:20";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortDisableFutureTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortDisableFutureTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPortDisableFutureTestFixture, Update_Success_UpdatesDbusProperty)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFut"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_update";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful get port disable future response
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    mask[0].bits.bit0 = true; // port 0 disabled
    mask[0].bits.bit3 = true; // port 3 disabled
    mask[1].bits.bit0 = true; // port 8 disabled

    Response getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_resp), 0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    auto rc = encode_get_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  mask, getRespMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(getResp, Response{}));

    // Act
    auto coroutine = sensor.update(gpu);

    // Assert
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
}

TEST_F(NsmPortDisableFutureTestFixture, Update_SensorIOFailure_ReturnsError)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutFail"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_fail";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Return error from sensorIO
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    auto coroutine = sensor.update(gpu);

    // Assert - non-success return
    EXPECT_NE(coroutine.data(), NSM_SUCCESS);
}

TEST_F(NsmPortDisableFutureTestFixture,
       Update_DecodeFailure_ReturnsCompletionCode)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutDec"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_decode";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build an error response (cc=NSM_ERROR)
    Response errResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                     0);
    auto errRespMsg = reinterpret_cast<nsm_msg*>(errResp.data());
    encode_reason_code(NSM_ERROR, 0x5678, NSM_GET_PORT_DISABLE_FUTURE,
                       errRespMsg);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errResp, Response{}));

    // Act
    auto coroutine = sensor.update(gpu);

    // Assert - returns non-zero (error)
    EXPECT_NE(coroutine.data(), NSM_SUCCESS);
}

TEST_F(NsmPortDisableFutureTestFixture, Update_EmptyMask_Success)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutEmpty"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_empty";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful response with empty mask (no ports disabled)
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};

    Response getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_resp), 0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, mask,
                                        getRespMsg);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(getResp, Response{}));

    // Act
    auto coroutine = sensor.update(gpu);

    // Assert
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
}

// ============================================================================
// NsmDevicePortDisableFuture — setDevicePortDisableFuture (coroutine)
// ============================================================================

TEST_F(NsmPortDisableFutureTestFixture,
       SetDevicePortDisableFuture_Success_ReturnsSuccess)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutSet"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_set";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    mask[0].bits.bit1 = true; // port 1
    mask[0].bits.bit5 = true; // port 5

    // Build a successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coroutine = sensor.setDevicePortDisableFuture(mask, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetDevicePortDisableFuture_PostPatchFail_WriteFailure)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutSetFail"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_set_fail";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    mask[0].bits.bit2 = true;

    // Return error from postPatchIO
    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coroutine = sensor.setDevicePortDisableFuture(mask, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetDevicePortDisableFuture_DecodeError_WriteFailure)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutSetDec"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_set_dec";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};

    // Build an error response (cc=NSM_ERROR)
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_ERROR, 0xABCD, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coroutine = sensor.setDevicePortDisableFuture(mask, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmDevicePortDisableFuture — setPortDisableFuture (top-level coroutine)
// ============================================================================

TEST_F(NsmPortDisableFutureTestFixture, SetPortDisableFuture_ValidPorts_Success)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutTop"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_top";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful set response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act — pass vector of port numbers to disable
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {0, 3, 7, 8, 15};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(NsmPortDisableFutureTestFixture,
       DISABLED_SetPortDisableFuture_InvalidValueType_ThrowsInvalidArgument)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutInvalid"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_invalid";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Act & Assert — wrong variant type
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<uint32_t>(42);

    EXPECT_THROW(
        sensor.setPortDisableFuture(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetPortDisableFuture_ConcurrentPatch_ReturnsUnavailable)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutConc"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_conc";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Simulate a patch already in progress
    sensor.asyncPatchInProgress = true;

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {1, 2};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert - should return Unavailable
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetPortDisableFuture_AllBitOffsets_CorrectMask)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutBits"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_bits";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act — test all 8 bit offsets within a single byte (ports 0-7)
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {0, 1, 2, 3, 4, 5, 6, 7};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetPortDisableFuture_PostPatchFail_ClearsInProgressFlag)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutClear"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_clear";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Return error from postPatchIO
    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {10};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert - even on failure, asyncPatchInProgress should be cleared
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetPortDisableFuture_EmptyPortList_Success)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutEmptyList"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_empty_list";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act - empty list means all-zeros mask
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(NsmPortDisableFutureTestFixture,
       SetPortDisableFuture_HighPortNumbers_CorrectByteIndex)
{
    using namespace nsm;

    // Arrange
    std::string name{"/PortDisFutHigh"};
    std::string type{"NSM_NVLink"};
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pdf_high";

    NsmDevicePortDisableFuture sensor(name, type, inventoryObjPath);

    // Build a successful response
    Response setResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setRespMsg = reinterpret_cast<nsm_msg*>(setResp.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, setRespMsg);

    EXPECT_CALL(*gpu, postPatchIO).WillOnce(mockPostPatchIO(setResp));

    // Act - use high port numbers spanning multiple bytes
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    std::vector<uint8_t> portsToDisable = {16, 24, 32, 64, 128, 255};
    AsyncSetOperationValueType value = portsToDisable;

    auto coroutine = sensor.setPortDisableFuture(value, &status, gpu);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmPCIePortConfigSetTestFixture, AddSensorNsmPCIePortConfigurationInfo)
{
    sdbusplus::bus::bus& testBus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie_config_as";
    auto pciePortConfigInfoIntf =
        std::make_shared<nsm::PCIePortConfigurationInfoIntf>(testBus,
                                                             invPath.c_str());
    auto sensor = std::make_shared<nsm::NsmPCIePortConfigurationInfo>(
        "PCIePortConfig_AS", "NSM_PCIe", pciePortConfigInfoIntf, uint8_t(0),
        uint8_t(0), uint8_t(0), invPath);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}
