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
 * Additional branch coverage for NVLinkManagementNICSWInventory.cpp:
 *
 * Targets uncovered code paths:
 * - NsmSWInventoryDriverVersionAndStatus constructor: verify interface
 *   creation and asset manufacturer setting
 * - updateValue: driverState == 2 (Loaded) vs other values
 * - handleResponseMsg: decode success with driver loaded (state 2)
 * - handleResponseMsg: decode success with non-loaded driver
 * - createNsmNVLinkManagerDriverSensor factory: all properties present
 *   with Priority=true (exercises Priority TRUE branch)
 * - createNsmNVLinkManagerDriverSensor factory: all properties present
 *   with Priority=false
 * - createNsmNVLinkManagerDriverSensor factory: with associations
 * - genRequestMsg: success with valid instanceId
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "NVLinkManagementNICSWInventory.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine
    createNsmNVLinkManagerDriverSensor(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NVLinkMgmtNICSWBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkManagementSWInventory";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NVLinkMgmtNICSWBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NVLinkMgmtNICSWBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Constructor: verify interface creation and manufacturer
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Constructor_VerifyInterfacesAndMfr)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;

    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_Ctor", assocs,
                                                "NSM_SWInv", "TestMfr");

    EXPECT_EQ(sensor.getName(), "NVLink_Br2_Ctor");
    EXPECT_NE(sensor.softwareVer_, nullptr);
    EXPECT_NE(sensor.operationalStatus_, nullptr);
    EXPECT_NE(sensor.associationDef_, nullptr);
    EXPECT_NE(sensor.asset_, nullptr);
    EXPECT_EQ(sensor.asset_->manufacturer(), "TestMfr");
}

// ============================================================================
// Constructor: with multiple associations (for-loop body)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Constructor_MultipleAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    assocs.push_back({"parent", "child", "/xyz/openbmc_project/inv/a0"});
    assocs.push_back({"device", "sw", "/xyz/openbmc_project/inv/a1"});
    assocs.push_back({"fabric", "driver", "/xyz/openbmc_project/inv/a2"});

    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_MultiAssoc",
                                                assocs, "NSM_SWInv", "NVIDIA");

    EXPECT_EQ(sensor.getName(), "NVLink_Br2_MultiAssoc");
}

// ============================================================================
// updateValue: driverState == 2 -> functional(true)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, UpdateValue_State2_FunctionalTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_S2", assocs,
                                                "NSM_SWInv", "NVIDIA");
    sensor.updateValue(2, "v1.2.3");
    EXPECT_TRUE(sensor.operationalStatus_->functional());
    EXPECT_EQ(sensor.softwareVer_->version(), "v1.2.3");
    EXPECT_EQ(sensor.driverState_, 2);
    EXPECT_EQ(sensor.driverVersion_, "v1.2.3");
}

// ============================================================================
// updateValue: driverState == 0 -> default -> functional(false)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, UpdateValue_State0_FunctionalFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_S0", assocs,
                                                "NSM_SWInv", "NVIDIA");
    sensor.updateValue(0, "");
    EXPECT_FALSE(sensor.operationalStatus_->functional());
    EXPECT_EQ(sensor.driverState_, 0);
}

// ============================================================================
// updateValue: driverState == 1 -> default -> functional(false)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, UpdateValue_State1_FunctionalFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_S1", assocs,
                                                "NSM_SWInv", "NVIDIA");
    sensor.updateValue(1, "5.0.0");
    EXPECT_FALSE(sensor.operationalStatus_->functional());
}

// ============================================================================
// handleResponseMsg: success with loaded driver (state 2)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, HandleResp_DriverLoaded)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_Loaded",
                                                assocs, "NSM_SWInv", "NVIDIA");

    uint8_t driverInfoData[] = {2, '5', '.', '0', '.', '1', '\0'};
    uint16_t dataSize = sizeof(driverInfoData);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              dataSize);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          driverInfoData, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_TRUE(sensor.operationalStatus_->functional());
    EXPECT_EQ(sensor.softwareVer_->version(), "5.0.1");
}

// ============================================================================
// handleResponseMsg: success with NOT loaded driver (state 0)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, HandleResp_DriverNotLoaded)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_NotLoaded",
                                                assocs, "NSM_SWInv", "NVIDIA");

    uint8_t driverInfoData[] = {0, '\0'};
    uint16_t dataSize = sizeof(driverInfoData);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              dataSize);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          driverInfoData, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_FALSE(sensor.operationalStatus_->functional());
}

// ============================================================================
// Factory: all properties with Priority=true -> sensor created, added with
// priority
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Factory_WithPriorityTrue_SensorCreated)
{
    const std::string path = "/test/nvlink_br2/with_priority";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("NVLink_Br2_Pri");
    pm["UUID"] = gpuUuid;
    pm["Priority"] = true;
    pm["Manufacturer"] = std::string("NVIDIA");

    const size_t before = gpu->deviceSensors.size();
    createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: all properties with Priority=false -> sensor created
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Factory_WithPriorityFalse_SensorCreated)
{
    const std::string path = "/test/nvlink_br2/pri_false";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("NVLink_Br2_PriF");
    pm["UUID"] = gpuUuid;
    pm["Priority"] = false;
    pm["Manufacturer"] = std::string("TestMfr");

    const size_t before = gpu->deviceSensors.size();
    createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: with associations from D-Bus
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Factory_WithAssociations_SensorCreated)
{
    const std::string path = "/test/nvlink_br2/with_assoc";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("NVLink_Br2_Assoc");
    pm["UUID"] = gpuUuid;
    pm["Priority"] = true;
    pm["Manufacturer"] = std::string("NVIDIA");

    // Set up association service map
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {{
            "xyz.openbmc_project.NSM",
            {std::string(baseIntf) + ".Associations"},
        }},
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "parent_device"},
        {"Backward", "sw_inventory"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/nic0"},
    };
    auto& assocPm = utils::MockDbusAsync::propertyMap(
        path, std::string(baseIntf) + ".Associations");
    assocPm = assocProperties;

    const size_t before = gpu->deviceSensors.size();
    createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: missing Name -> name="" -> sdbusplus may throw on empty path
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, Factory_MissingName_Throws)
{
    const std::string path = "/test/nvlink_br2/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // Name intentionally omitted -> name=""
    pm["UUID"] = gpuUuid;
    pm["Priority"] = true;
    pm["Manufacturer"] = std::string("NVIDIA");

    EXPECT_THROW_COROUTINE(
        createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path),
        sdbusplus::exception::SdBusError);
}

// ============================================================================
// genRequestMsg: success with instanceId 0
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, GenRequestMsg_InstanceId0_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_Gen0", assocs,
                                                "NSM_SWInv", "NVIDIA");

    auto req = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(req.has_value());
    EXPECT_GT(req->size(), 0u);
}

// ============================================================================
// handleResponseMsg: decode failure (short buffer)
// ============================================================================

TEST_F(NVLinkMgmtNICSWBranch2Test, HandleResp_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Br2_DecFail",
                                                assocs, "NSM_SWInv", "NVIDIA");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto cc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}
