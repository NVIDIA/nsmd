/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage for NVLinkManagementNICSWInventory.cpp:
 * - NsmSWInventoryDriverVersionAndStatus: genReq, handleResp, updateValue
 * - createNsmNVLinkManagerDriverSensor factory: missing props, invalid UUID
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "NVLinkManagementNICSWInventory.hpp"
#include "test/commonMock.hpp"

namespace nsm
{
requester::Coroutine
    createNsmNVLinkManagerDriverSensor(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NVLinkMgmtNICSWBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/nvlink_br";
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkManagementSWInventory";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NVLinkMgmtNICSWBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NVLinkMgmtNICSWBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// genRequestMsg - success and failure
// ===========================================================================
TEST_F(NVLinkMgmtNICSWBranchTest, GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Gen", assocs,
                                                "NSM_SWInv", "NVIDIA");
    auto req = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(req.has_value());
}

TEST_F(NVLinkMgmtNICSWBranchTest, GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_GenFail", assocs,
                                                "NSM_SWInv", "NVIDIA");
    auto req = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// ===========================================================================
// handleResponseMsg - success, errorCC, decodeFail
// ===========================================================================
TEST_F(NVLinkMgmtNICSWBranchTest, HandleResponseMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_HRS", assocs,
                                                "NSM_SWInv", "NVIDIA");

    // Build response: driver_info_data = [driver_state(1 byte)] +
    // version_string
    uint8_t driverInfoData[1 + 6] = {2, '1', '.', '2', '.', '3', '\0'};
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
    EXPECT_EQ(sensor.softwareVer_->version(), "1.2.3");
}

TEST_F(NVLinkMgmtNICSWBranchTest, HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_HRE", assocs,
                                                "NSM_SWInv", "NVIDIA");
    auto resp = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST_F(NVLinkMgmtNICSWBranchTest, HandleResponseMsg_DecodeSuccessNonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_HRNZ", assocs,
                                                "NSM_SWInv", "NVIDIA");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_header_info hdr = {};
    hdr.nsm_msg_type = NSM_RESPONSE;
    hdr.instance_id = 0;
    hdr.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    pack_nsm_header(&hdr, &msg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_GET_DRIVER_INFO, msg);

    auto cc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// updateValue - driverState switch cases
// ===========================================================================
TEST_F(NVLinkMgmtNICSWBranchTest, UpdateValue_Functional)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Func", assocs,
                                                "NSM_SWInv", "NVIDIA");
    sensor.updateValue(2, "5.0.0");
    EXPECT_TRUE(sensor.operationalStatus_->functional());
}

TEST_F(NVLinkMgmtNICSWBranchTest, UpdateValue_NotFunctional)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_NoFunc", assocs,
                                                "NSM_SWInv", "NVIDIA");
    sensor.updateValue(0, "none");
    EXPECT_FALSE(sensor.operationalStatus_->functional());
}

// ===========================================================================
// Constructor - with associations
// ===========================================================================
TEST_F(NVLinkMgmtNICSWBranchTest, Constructor_WithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assocs;
    assocs.push_back({"parent", "child", "/xyz/openbmc_project/inv/gpu0"});
    assocs.push_back({"device", "sw", "/xyz/openbmc_project/inv/dev0"});

    NsmSWInventoryDriverVersionAndStatus sensor(bus, "NVLink_Assoc", assocs,
                                                "NSM_SWInv", "TestMfg");
    EXPECT_EQ(sensor.getName(), "NVLink_Assoc");
}

// ===========================================================================
// Factory - missing UUID, missing optional props
// ===========================================================================
TEST_F(NVLinkMgmtNICSWBranchTest, Factory_MissingProps)
{
    const std::string path = "/test/nvlink_br/missing";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("NVLink_Missing");
    pm["UUID"] = gpuUuid;
    // Priority, Manufacturer omitted -> FALSE branches

    createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path);
}

TEST_F(NVLinkMgmtNICSWBranchTest, Factory_InvalidUUID)
{
    const std::string path = "/test/nvlink_br/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("NVLink_BadUUID");
    pm["UUID"] = std::string("STATIC:99:99:BAD:0");

    createNsmNVLinkManagerDriverSensor(mockManager, baseIntf, path);
}
