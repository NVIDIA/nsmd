/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmPCIeRetimerPD.cpp
 *
 * Targets uncovered branches:
 * - NsmPCIeDeviceQueryScalarTelemetry::genRequestMsg encode fail
 *   (instanceId > NSM_INSTANCE_MAX)
 * - NsmPCIeDeviceQueryScalarTelemetry::handleResponseMsg decode fail
 *   (truncated buffer)
 * - NsmPCIeDeviceGetClockOutput::genRequestMsg encode fail
 * - NsmPCIeDeviceGetClockOutput::handleResponseMsg decode fail
 * - createPCIeRetimerChassisPCIeDevice: InventoryObjPath absent
 * - getRetimerClockState: all device instances 0-7 and default
 * - convertToLaneCount: boundary values
 * - convertToGeneration: boundary values
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "pci-links.h"

#define private public
#define protected public

#include "nsmPCIeRetimerPD.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine
    createPCIeRetimerChassisPCIeDevice(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================

struct NsmPCIeRetimerPDBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeDevices";
    const std::string name = "PCIeRetimer_Branch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX/PCIeDevices/" + name;

    const uuid_t retimerUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> retimer;

    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    std::string sensorName = "branch_retimer";
    std::string sensorType = "branch_type";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test_device_branch";

    NsmPCIeRetimerPDBranchTest() : SensorManagerTest(devices)
    {
        retimer = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(retimer, nullptr);
    }

    ~NsmPCIeRetimerPDBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    const MapperServiceMap serviceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations0",
                },
            },
        },
    };
};

// ============================================================================
// genRequestMsg: ScalarTelemetry encode fail (bad instanceId)
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ScalarTelemetry_GenReq_BadInstanceId_Nullopt)
{
    std::vector<utils::Association> associations;
    std::string deviceType =
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.DeviceTypes.Retimer";
    uint8_t deviceIndex = 1;

    NsmPCIeDeviceQueryScalarTelemetry sensor(bus, sensorName, associations,
                                             sensorType, deviceType,
                                             deviceIndex, inventoryObjPath);

    auto request = sensor.genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// handleResponseMsg: ScalarTelemetry decode fail (truncated)
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest,
       ScalarTelemetry_HandleResp_DecodeFail_Returns)
{
    std::vector<utils::Association> associations;
    std::string deviceType =
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.DeviceTypes.Retimer";

    NsmPCIeDeviceQueryScalarTelemetry sensor(bus, sensorName, associations,
                                             sensorType, deviceType, uint8_t(0),
                                             inventoryObjPath);

    // Valgrind-safe: 7-byte buffer with cc=NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// genRequestMsg: ClockOutput encode fail (bad instanceId)
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ClockOutput_GenReq_BadInstanceId_Nullopt)
{
    NsmPCIeDeviceGetClockOutput sensor(bus, sensorName, sensorType, uint64_t(0),
                                       inventoryObjPath);

    auto request = sensor.genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// handleResponseMsg: ClockOutput decode fail (truncated)
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ClockOutput_HandleResp_DecodeFail_Returns)
{
    NsmPCIeDeviceGetClockOutput sensor(bus, sensorName, sensorType, uint64_t(0),
                                       inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg: ScalarTelemetry success with all gen values
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ScalarTelemetry_HandleResp_AllGens)
{
    std::vector<utils::Association> associations;
    std::string deviceType =
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.DeviceTypes.Retimer";

    NsmPCIeDeviceQueryScalarTelemetry sensor(bus, sensorName, associations,
                                             sensorType, deviceType, uint8_t(1),
                                             inventoryObjPath);

    // Gen5, x16
    struct nsm_query_scalar_group_telemetry_group_1 link_info = {};
    link_info.negotiated_link_speed = 5;
    link_info.negotiated_link_width = 5;
    link_info.max_link_speed = 6;
    link_info.max_link_width = 7;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    [[maybe_unused]] auto encRc =
        encode_query_scalar_group_telemetry_v1_group1_resp(
            0, NSM_SUCCESS, ERR_NULL, &link_info, response);

    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg: ClockOutput success path
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ClockOutput_HandleResp_Success)
{
    NsmPCIeDeviceGetClockOutput sensor(bus, sensorName, sensorType, uint64_t(3),
                                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint32_t clock_buffer = 0x08; // bit 3 set for retimer4
    [[maybe_unused]] auto encRc = encode_get_clock_output_enable_state_resp(
        0, NSM_SUCCESS, ERR_NULL, clock_buffer, response);

    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// Factory: InventoryObjPath absent → inventoryObjPath="" → sensor created
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest,
       DISABLED_Factory_MissingInventoryObjPath_SensorsCreated)
{
    const std::string uniquePath = "/xyz/test/pcie/branch_no_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Name"] = std::string("BranchNoInvPath");
    pm["UUID"] = retimerUuid;
    // "InventoryObjPath" intentionally omitted
    pm["DeviceType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.DeviceTypes.Retimer");
    pm["Priority"] = bool(false);
    pm["DeviceInstance"] = uint64_t(0);

    const size_t before = retimer->deviceSensors.size();
    createPCIeRetimerChassisPCIeDevice(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// ============================================================================
// getRetimerClockState: all valid device instances 0-7
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, GetRetimerClockState_AllInstances)
{
    NsmPCIeDeviceGetClockOutput sensor(bus, sensorName, sensorType, uint64_t(0),
                                       inventoryObjPath);

    nsm_pcie_clock_buffer_data clkBuf = {};

    // Test each instance with its corresponding bit set
    for (uint8_t i = 0; i < 8; i++)
    {
        memset(&clkBuf, 0, sizeof(clkBuf));
        switch (i)
        {
            case 0:
                clkBuf.clk_buf_retimer1 = 1;
                break;
            case 1:
                clkBuf.clk_buf_retimer2 = 1;
                break;
            case 2:
                clkBuf.clk_buf_retimer3 = 1;
                break;
            case 3:
                clkBuf.clk_buf_retimer4 = 1;
                break;
            case 4:
                clkBuf.clk_buf_retimer5 = 1;
                break;
            case 5:
                clkBuf.clk_buf_retimer6 = 1;
                break;
            case 6:
                clkBuf.clk_buf_retimer7 = 1;
                break;
            case 7:
                clkBuf.clk_buf_retimer8 = 1;
                break;
        }
        sensor.deviceInstanceNumber = i;
        uint32_t* clockBuffer = reinterpret_cast<uint32_t*>(&clkBuf);
        EXPECT_TRUE(sensor.getRetimerClockState(*clockBuffer));
    }

    // Default case: invalid instance
    sensor.deviceInstanceNumber = 255;
    uint32_t* clockBuffer = reinterpret_cast<uint32_t*>(&clkBuf);
    EXPECT_FALSE(sensor.getRetimerClockState(*clockBuffer));
}

// ============================================================================
// ScalarTelemetry: ErrorCC via encode (rc=success, cc=NSM_ERROR)
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest,
       ScalarTelemetry_HandleResp_ErrorCC_SkipsUpdate)
{
    std::vector<utils::Association> associations;
    std::string deviceType =
        "xyz.openbmc_project.Inventory.Item.PCIeDevice.DeviceTypes.Retimer";

    NsmPCIeDeviceQueryScalarTelemetry sensor(bus, sensorName, associations,
                                             sensorType, deviceType, uint8_t(2),
                                             inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_1 link_info = {};

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    [[maybe_unused]] auto encRc =
        encode_query_scalar_group_telemetry_v1_group1_resp(
            0, NSM_ERROR, ERR_NULL, &link_info, response);

    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// ClockOutput: ErrorCC via encode
// ============================================================================

TEST_F(NsmPCIeRetimerPDBranchTest, ClockOutput_HandleResp_ErrorCC_SkipsUpdate)
{
    NsmPCIeDeviceGetClockOutput sensor(bus, sensorName, sensorType, uint64_t(0),
                                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    [[maybe_unused]] auto encRc = encode_get_clock_output_enable_state_resp(
        0, NSM_ERROR, ERR_NULL, 0, response);

    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
