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
 * Branch coverage tests for nsmLeakDetectionCreateSensors factory coroutine
 * in nsmd/nsmChassis/nsmLeakDetection.cpp.
 *
 * Targets the FALSE branches of each property count() check:
 *   Name, Type, UUID, SensorIdMap, SensorNameMap, ChassisPath,
 *   MinThresholdsmV, CriticalThresholdsmV, MaxThresholdsmV,
 *   MaxValue, MinValue
 *
 * Also tests:
 *   - thresholdsGiven = false (no threshold properties)
 *   - sensorNameMap/sensorIdMap size mismatch (early error return)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "nsmLeakDetection.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine nsmLeakDetectionCreateSensors(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmLeakDetectionBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_LeakDetection";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Null device lookup (UUID not found) -> co_return NSM_ERROR
// ============================================================================
struct NsmLeakDetectionBranch2NullDevTest : public Test, public utils::DBusTest
{
    const std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_LeakDetection";
    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    NsmLeakDetectionBranch2NullDevTest()
    {
        sensorManagerInstance.reset(&nullManager);
    }

    ~NsmLeakDetectionBranch2NullDevTest()
    {
        sensorManagerInstance.release();
        devices.clear();
    }
};

TEST_F(NsmLeakDetectionBranch2NullDevTest, Factory_NullDevice)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_nulldev";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakNullDev");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = uuid_t("STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:99");
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_null"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");

    // Device lookup returns nullptr -> co_return NSM_ERROR
    nsmLeakDetectionCreateSensors(nullManager, baseInterface, objPath);
}

// ============================================================================
// Only Name and UUID present, all others absent
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_OnlyNameAndUuid)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_2";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakBranch2");
    propMap["UUID"] = fpgaUuid;
    // Type, SensorIdMap, SensorNameMap, ChassisPath, thresholds, MaxValue,
    // MinValue all absent

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    // Sensor is created with empty maps but added to device
    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Thresholds absent (thresholdsGiven = false branch)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_NoThresholds)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_3";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakNoThresh");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1, 2};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_a", "sensor_b"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    // MinThresholdsmV, CriticalThresholdsmV, MaxThresholdsmV absent
    // -> thresholdsGiven = false -> no NsmSetLeakDetectionThresholds created

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    // NsmLeakDetection sensor created, but no static threshold sensor
    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// MaxValue and MinValue absent (FALSE branches for those)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_NoMaxMinValue)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_4";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakNoMaxMin");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_c"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    // MaxValue and MinValue absent -> default infinity values used

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// SensorNameMap/SensorIdMap size mismatch -> co_return NSM_ERROR //

// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_SizeMismatch)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_5";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakMismatch");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1, 2, 3};
    propMap["SensorNameMap"] = std::vector<std::string>{"only_one"};
    // Size mismatch: 3 vs 1

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    // Should return early without creating any sensors
}

// ============================================================================
// Partial threshold properties (only MinThresholdsmV present)
// thresholdsGiven = false because not all three are present
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_PartialThresholds)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_6";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakPartialThresh");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_d"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MinThresholdsmV"] = std::vector<uint64_t>{100};
    // CriticalThresholdsmV and MaxThresholdsmV absent
    // -> thresholdsGiven = false

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// With thresholds present but size mismatch -> error
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_ThresholdsSizeMismatch)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_7";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakThreshMismatch");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1, 2};
    propMap["SensorNameMap"] = std::vector<std::string>{"sa", "sb"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MinThresholdsmV"] = std::vector<uint64_t>{100};
    propMap["CriticalThresholdsmV"] = std::vector<uint64_t>{200};
    propMap["MaxThresholdsmV"] = std::vector<uint64_t>{300};
    // Threshold arrays have 1 element each but sensorIdMap has 2

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    // Should return error without creating sensors
}

// ============================================================================
// Full factory with all thresholds (happy path)
// Exercises: thresholdsGiven = true, NsmSetLeakDetectionThresholds creation,
// NsmLeakDetectionThresholdsPatch creation, async set registrations
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_FullWithThresholds)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_8";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakFullThresh");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1, 2};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_ft1",
                                                        "sensor_ft2"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MinThresholdsmV"] = std::vector<uint64_t>{100, 150};
    propMap["CriticalThresholdsmV"] = std::vector<uint64_t>{200, 250};
    propMap["MaxThresholdsmV"] = std::vector<uint64_t>{300, 350};
    propMap["MaxValue"] = double(5.0);
    propMap["MinValue"] = double(0.0);

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    // NsmLeakDetection sensor + NsmSetLeakDetectionThresholds static sensor
    // + 2 NsmLeakDetectionThresholdsPatch static sensors
    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
    EXPECT_GE(fpga->deviceSensors.size(), 3u);
}

// ============================================================================
// Factory with MaxValue present but MinValue absent
// Exercises the MaxValue TRUE + MinValue FALSE count() branch combination
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_MaxValueOnly)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_9";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakMaxOnly");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_mx"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MaxValue"] = double(10.0);
    // MinValue absent -> default -infinity used

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Factory with MinValue present but MaxValue absent
// Exercises the MinValue TRUE + MaxValue FALSE count() branch combination
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_MinValueOnly)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_10";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakMinOnly");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_mn"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MinValue"] = double(-1.0);
    // MaxValue absent -> default +infinity used

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Factory with only two threshold properties present (missing MaxThresholdsmV)
// thresholdsGiven = false because all three must be present
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, Factory_TwoOfThreeThresholds)
{
    const std::string objPath = "/xyz/openbmc_project/config/leak_fb2_11";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, baseInterface);
    propMap["Name"] = std::string("LeakTwoThresh");
    propMap["Type"] = std::string("NSM_LeakDetection");
    propMap["UUID"] = fpgaUuid;
    propMap["SensorIdMap"] = std::vector<uint64_t>{1};
    propMap["SensorNameMap"] = std::vector<std::string>{"sensor_2t"};
    propMap["ChassisPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/HGX_BR");
    propMap["MinThresholdsmV"] = std::vector<uint64_t>{100};
    propMap["CriticalThresholdsmV"] = std::vector<uint64_t>{200};
    // MaxThresholdsmV absent -> thresholdsGiven = false

    nsmLeakDetectionCreateSensors(mockManager, baseInterface, objPath);

    EXPECT_GE(fpga->roundRobinSensors.size(), 1u);
}

// ============================================================================
// handleResponseMsg - cc != 0 path (cc ? cc : rc returns cc)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, HandleResponseMsg_CcNonZero_ReturnsCc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakResp_cc";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{1},
        std::vector<std::string>{"sensor_cc"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    // Build response with cc = NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto hdr = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    hdr->completion_code = NSM_ERROR;
    hdr->reason_code = htole16(ERR_NULL);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // ensure valgrind-safe

    auto rc = ld->handleResponseMsg(msg, buf.size());
    // cc != 0 -> returns cc
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// handleResponseMsg - rc != 0, cc == 0 path (cc ? cc : rc returns rc)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, HandleResponseMsg_RcNonZero_ReturnsRc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakResp_rc";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{1},
        std::vector<std::string>{"sensor_rc"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    // Tiny buffer -> decode fails with rc != 0, cc stays 0
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    // completion_code stays 0, but buffer too small for valid decode

    auto rc = ld->handleResponseMsg(msg, buf.size());
    // rc != 0, cc == 0 -> returns rc (the false branch of cc?cc:rc)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// getSensorInterfaces - sensorId found (returns tuple)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, GetSensorInterfaces_Found)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakIntf";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{5},
        std::vector<std::string>{"sensor_intf"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    auto result = ld->getSensorInterfaces(5);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// getSensorInterfaces - sensorId not found (returns nullopt)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, GetSensorInterfaces_NotFound)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakIntf2";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{5},
        std::vector<std::string>{"sensor_intf2"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    auto result = ld->getSensorInterfaces(99);
    EXPECT_FALSE(result.has_value());
}
