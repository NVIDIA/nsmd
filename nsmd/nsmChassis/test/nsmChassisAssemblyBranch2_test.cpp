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
 * Branch coverage for nsmChassisAssembly.cpp
 *
 * Covers:
 * - NsmInventoryProperty<NsmAssetIntf> genRequestMsg (encode fail, success)
 * - NsmInventoryProperty<NsmAssetIntf> handleResponseMsg
 *   (success, errorCC, decode fail)
 * - createChassisAssemblyFeature: non-string attribute rejected
 * - AssemblyType branches: Device, Board, FRU, invalid default
 * - createChassisAssemblyAsset: Name present/absent in current props
 * - createChassisAssemblyAsset: AssemblyType present/absent in base props
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

#undef private
#undef protected

namespace nsm
{
void createChassisAssemblyArea(std::shared_ptr<NsmDevice> device,
                               const std::string& chassisName,
                               const std::string& name,
                               dbus::PropertyMap& allCurrentIfaceProperties);
void createChassisAssemblyAsset(std::shared_ptr<NsmDevice> device,
                                const std::string& chassisName,
                                const std::string& name,
                                dbus::PropertyMap& allCurrentIfaceProperties,
                                dbus::PropertyMap& allBaseIfaceProperties);
void createChassisAssemblyHealth(std::shared_ptr<NsmDevice> device,
                                 const std::string& chassisName,
                                 const std::string& name);
void createChassisAssemblyLocation(
    std::shared_ptr<NsmDevice> device, const std::string& chassisName,
    const std::string& name, dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

using namespace nsm;

static const uuid_t kGpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

struct NsmChassisAssemblyBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_GPU_SXM_B2";
    const std::string name = "AssemblyB2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kGpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Direct helper tests
// ============================================================================

// createChassisAssemblyHealth: direct call
TEST_F(NsmChassisAssemblyBranch2Test, CreateChassisAssemblyHealth_DirectCall)
{
    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyHealth(gpu, chassisName, name);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// createChassisAssemblyArea: direct call with valid PhysicalContext
TEST_F(NsmChassisAssemblyBranch2Test, CreateChassisAssemblyArea_ValidContext)
{
    dbus::PropertyMap props;
    props["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyArea(gpu, chassisName, name, props);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// createChassisAssemblyLocation: direct call with valid LocationType
TEST_F(NsmChassisAssemblyBranch2Test,
       CreateChassisAssemblyLocation_ValidLocation)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyLocation(gpu, chassisName, name, props);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// ============================================================================
// createChassisAssemblyAsset: AssemblyType branches
// ============================================================================

// AssemblyType == "Device" -> partNumberId = DEVICE_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Device)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("DeviceAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    // partNumber + serialNumber + model + buildDate = 4
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "Board" -> partNumberId = BOARD_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Board)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("BoardAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Board");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "FRU" -> partNumberId = FRU_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_FRU)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("FRUAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("FRU");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "InvalidType" -> else branch -> default BOARD_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Invalid)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("InvalidAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("InvalidType");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType absent -> assemblyType="" -> else branch
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Missing)
{
    dbus::PropertyMap currProps;
    dbus::PropertyMap baseProps;
    // No "AssemblyType" key -> FALSE branch at L65 -> assemblyType=""

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// Name present in current props -> assetsName set (TRUE branch at L58)
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_NamePresent)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("NamedAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// Name absent in current props -> assetsName="" (FALSE branch at L58)
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_NameAbsent)
{
    dbus::PropertyMap currProps;
    // No "Name" key -> FALSE branch
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// ============================================================================
// NsmInventoryProperty genRequestMsg / handleResponseMsg
// via sensors created by createChassisAssemblyAsset
// ============================================================================

TEST_F(NsmChassisAssemblyBranch2Test, InventoryProperty_GenRequestMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("SensorTest");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_GenRequestMsg_EncodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("EncFail");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespOK");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    const std::string partNum = "900-00000-0000-000";
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNum.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, partNum.size(),
        reinterpret_cast<const uint8_t*>(partNum.data()), responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_ErrorCC)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespCC");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Board");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Build error CC response (min 7 bytes for valgrind safety)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_DecodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespDec");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("FRU");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Short buffer -> decode fail
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, sizeof(nsm_msg_hdr));
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// createChassisAssemblyFeature: type checks before any object is created
// ============================================================================

TEST_F(NsmChassisAssemblyBranch2Test, Feature_NonStringAssemblyType_Rejected)
{
    nsm::features::DeviceContext dev;
    dev.name = chassisName;
    dev.kind = "gpu";
    dev.staticUuid = kGpuUuid;
    dev.platformPrefix = "HGX_";
    dev.device = gpu;
    dbus::PropertyMap params{{"Name", std::string("GPU_Assembly")},
                             {"AssemblyType", uint64_t{1}}};

    const size_t before = gpu->staticSensors.size();
    auto rc = nsm::features::createChassisAssemblyFeature(mockManager, dev,
                                                          params);
    EXPECT_EQ(rc.data(), NSM_SW_ERROR_DATA);
    EXPECT_EQ(gpu->staticSensors.size(), before);
    // The rejected assembly did not take an index
    EXPECT_EQ(dev.nextIndex("Assembly"), 0u);
}
