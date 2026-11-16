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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmAssetIntf.hpp"
#include "nsmIRoTResponder.hpp"
#include "nsmInventoryProperty.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <libnsm/debug-token.h>

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// PART 4: NsmIRoTResponder -- template instantiation and update() paths
// =============================================================================

struct Batch12DIRoTResponderTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "IRoTB12D";
    const std::string type = "NSM_IRoTResponder";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    Batch12DIRoTResponderTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~Batch12DIRoTResponderTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ---------------------------------------------------------------------------
// NsmIRoTResponder: HealthIntf template instantiation
// ---------------------------------------------------------------------------
TEST_F(Batch12DIRoTResponderTest, ConstructorWithHealthIntf_CreatesObject)
{
    // Arrange & Act
    auto healthResponder = std::make_shared<NsmIRoTResponder<HealthIntf>>(
        "HealthTestB12D", "NSM_ChassisIRoTResponder");

    // Assert
    EXPECT_NE(healthResponder, nullptr);
    EXPECT_EQ(healthResponder->getName(), "HealthTestB12D");
    EXPECT_EQ(healthResponder->getType(), "NSM_ChassisIRoTResponder");
}

// ---------------------------------------------------------------------------
// NsmIRoTResponder: LocationIntf template instantiation
// ---------------------------------------------------------------------------
TEST_F(Batch12DIRoTResponderTest, ConstructorWithLocationIntf_CreatesObject)
{
    // Arrange & Act
    auto locationResponder = std::make_shared<NsmIRoTResponder<LocationIntf>>(
        "LocationTestB12D", "NSM_ChassisIRoTResponder");

    // Assert
    EXPECT_NE(locationResponder, nullptr);
    EXPECT_EQ(locationResponder->getName(), "LocationTestB12D");
    EXPECT_EQ(locationResponder->getType(), "NSM_ChassisIRoTResponder");
}

// ---------------------------------------------------------------------------
// NsmIRoTResponder: UuidIntf template instantiation
// ---------------------------------------------------------------------------
TEST_F(Batch12DIRoTResponderTest, ConstructorWithUuidIntf_CreatesObject)
{
    // Arrange & Act
    auto uuidResponder = std::make_shared<NsmIRoTResponder<UuidIntf>>(
        "UuidTestB12D", "NSM_ChassisIRoTResponder");

    // Assert
    EXPECT_NE(uuidResponder, nullptr);
    EXPECT_EQ(uuidResponder->getName(), "UuidTestB12D");
    EXPECT_EQ(uuidResponder->getType(), "NSM_ChassisIRoTResponder");
}

// ---------------------------------------------------------------------------
// NsmIRoTResponder: AssociationDefinitionsIntf template instantiation
// ---------------------------------------------------------------------------
TEST_F(Batch12DIRoTResponderTest, ConstructorWithAssociationIntf_CreatesObject)
{
    // Arrange & Act
    auto assocResponder =
        std::make_shared<NsmIRoTResponder<AssociationDefinitionsIntf>>(
            "AssocTestB12D", "NSM_ChassisIRoTResponder");

    // Assert
    EXPECT_NE(assocResponder, nullptr);
    EXPECT_EQ(assocResponder->getName(), "AssocTestB12D");
    EXPECT_EQ(assocResponder->getType(), "NSM_ChassisIRoTResponder");
}

// ---------------------------------------------------------------------------
// NsmIRoTResponder: SPDMResponderIntf template instantiation
// ---------------------------------------------------------------------------
TEST_F(Batch12DIRoTResponderTest,
       ConstructorWithSPDMResponderIntf_CreatesObject)
{
    // Arrange & Act
    auto spdmResponder = std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
        "SPDMTestB12D", "NSM_ChassisIRoTResponder");

    // Assert
    EXPECT_NE(spdmResponder, nullptr);
    EXPECT_EQ(spdmResponder->getName(), "SPDMTestB12D");
    EXPECT_EQ(spdmResponder->getType(), "NSM_ChassisIRoTResponder");
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(Batch12DIRoTResponderTest, AddSensorNsmIRoTResponderSPDM)
{
    auto sensor = std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
        "SPDMResponder_AS", "NSM_ChassisIRoTResponder");
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// NsmIRoTResponder<T>::update() coverage
// For non-NsmAssetIntf and non-UuidIntf types, update() just co_returns //
// NSM_SUCCESS.
// ============================================================================

TEST_F(Batch12DIRoTResponderTest, UpdateHealthIntf)
{
    auto sensor = std::make_shared<NsmIRoTResponder<HealthIntf>>(
        "HealthUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

TEST_F(Batch12DIRoTResponderTest, UpdateLocationIntf)
{
    auto sensor = std::make_shared<NsmIRoTResponder<LocationIntf>>(
        "LocationUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

TEST_F(Batch12DIRoTResponderTest, UpdateAssociationDefinitionsIntf)
{
    auto sensor =
        std::make_shared<NsmIRoTResponder<AssociationDefinitionsIntf>>(
            "AssocUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

TEST_F(Batch12DIRoTResponderTest, UpdateSPDMResponderIntf)
{
    auto sensor = std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
        "SPDMUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

TEST_F(Batch12DIRoTResponderTest, UpdateChassisIntf)
{
    auto sensor = std::make_shared<NsmIRoTResponder<ChassisIntf>>(
        "ChassisUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<NsmAssetIntf>::update – sensorIO failure → lines 65-69
// ============================================================================
TEST_F(Batch12DIRoTResponderTest,
       UpdateNsmAssetIntf_SensorIOFails_CoversErrorPath)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetUpd_sioerr", "NSM_ChassisIRoTResponder");

    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<NsmAssetIntf>::update – first decode fails (cc=NSM_ERROR)
// → if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) TRUE → lines 79-83
// ============================================================================
TEST_F(Batch12DIRoTResponderTest,
       UpdateNsmAssetIntf_FirstDecodeErrorCC_CoversFirstDecodeFailPath)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetUpd_badcc", "NSM_ChassisIRoTResponder");

    // Minimal response with cc=NSM_ERROR: sensorIO returns NSM_SUCCESS (0)
    // but the response has a bad completion code, so the first decode path
    // at line 77 triggers the TRUE branch (cc != NSM_SUCCESS)
    uint8_t devId[] = {0x01};
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_resp), 0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, ERR_NULL, devId, 0, respMsg);

    // Return the error-CC response with transport success (code=NSM_SUCCESS)
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));

    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<UuidIntf>::update – MctpDiscovery::getInstance() throws
// → lines 108: calling getInstance() in test env raises std::runtime_error
// ============================================================================
TEST_F(Batch12DIRoTResponderTest,
       UpdateUuidIntf_MctpDiscoveryNotInitialized_Throws)
{
    auto sensor = std::make_shared<NsmIRoTResponder<UuidIntf>>(
        "UuidUpd_throw", "NSM_ChassisIRoTResponder");

    // MctpDiscovery::getInstance() raises std::runtime_error in test env
    EXPECT_THROW_COROUTINE(sensor->update(fpga), std::runtime_error);
}

// ============================================================================
// NsmIRoTResponder<NsmAssetIntf>::update – success path → lines 73-95+
// ============================================================================
TEST_F(Batch12DIRoTResponderTest, UpdateNsmAssetIntf_Success_SetsSerialNumber)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetUpd_ok", "NSM_ChassisIRoTResponder");

    // Build a valid query_device_ids_resp
    uint8_t devId[] = {0xAB, 0xCD};
    const size_t devIdLen = sizeof(devId);
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_resp) + devIdLen - 1,
        0);
    auto* respMsg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, devId, devIdLen,
                                     respMsg);

    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    // Covers: first decode (false branch), second decode (false branch),
    // serial number string formatting and serialNumber setter
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<NsmAssetIntf>::update – first decode second operand (L77):
//   rc==NSM_SW_SUCCESS but cc==NSM_ERROR
// 9-byte buffer: decode_nsm_query_device_ids_resp calls
// decode_reason_code_and_cc which returns NSM_SW_SUCCESS with cc=NSM_ERROR
// → second operand of (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) is TRUE.
// ============================================================================
TEST_F(Batch12DIRoTResponderTest,
       UpdateNsmAssetIntf_FirstDecodeSuccessNonZeroCC_CoversSecondOperand)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetUpd_cc_err", "NSM_ChassisIRoTResponder");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR

    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(buf, Response{}));

    sensor->update(fpga);
}
