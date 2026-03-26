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
 * Additional branch coverage tests for nsmPort.cpp:
 * - getTopologyObjPath: all switch cases + default
 * - NsmPortMetrics::updateCounterValues: null iBPortIntf, null
 *   portMetricsOem2Intf, null portPacketCountersIntf, null portData
 * - NsmPortMetrics::updateCounterValues: counters NOT supported (FALSE
 * branches)
 * - NsmPortMetrics::getBitErrorRate: symbol_ber_coef_float == 0 path
 * - NsmPortMetrics::handleResponseMsg: cc != NSM_SUCCESS (cc ? cc : rc branch)
 * - NsmPortStatus::update: encode failure path
 * - NsmPortStatus::checkPortCharactersticRCAndPopulateRuntimeErr: success with
 *   cc=NSM_SUCCESS and reasonCode=ERR_NULL (else branch: runtimeError=false)
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"
#include "test/commonMock.hpp"

namespace nsm
{
std::string getTopologyObjPath(const std::string& deviceName,
                               const uint8_t deviceType);
} // namespace nsm

using namespace nsm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmPortBranch5Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:95";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortBranch5Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortBranch5Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ===========================================================================
// getTopologyObjPath: all device type switch cases
// ===========================================================================
TEST_F(NsmPortBranch5Test, GetTopologyObjPath_GPU)
{
    auto result = getTopologyObjPath("dev0", NSM_DEV_ID_GPU);
    EXPECT_NE(result.find("GPU"), std::string::npos);
    EXPECT_NE(result.find("dev0"), std::string::npos);
}

TEST_F(NsmPortBranch5Test, GetTopologyObjPath_SWITCH)
{
    auto result = getTopologyObjPath("dev1", NSM_DEV_ID_SWITCH);
    EXPECT_NE(result.find("SWITCH"), std::string::npos);
}

TEST_F(NsmPortBranch5Test, GetTopologyObjPath_PCIE_BRIDGE)
{
    auto result = getTopologyObjPath("dev2", NSM_DEV_ID_PCIE_BRIDGE);
    EXPECT_NE(result.find("PCIE_BRIDGE"), std::string::npos);
}

TEST_F(NsmPortBranch5Test, GetTopologyObjPath_EROT)
{
    auto result = getTopologyObjPath("dev3", NSM_DEV_ID_EROT);
    EXPECT_NE(result.find("EROT"), std::string::npos);
}

TEST_F(NsmPortBranch5Test, GetTopologyObjPath_CPU)
{
    auto result = getTopologyObjPath("dev4", NSM_DEV_ID_CPU);
    EXPECT_NE(result.find("CPU"), std::string::npos);
}

TEST_F(NsmPortBranch5Test, GetTopologyObjPath_UnknownDeviceType)
{
    auto result = getTopologyObjPath("devX", 0xFF);
    EXPECT_EQ(result, "");
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues: null portData
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_UpdateCounterValues_NullPortData)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullData";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nulldata_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nulldata";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    // nullptr -> else branch at L1079
    sensor.updateCounterValues(nullptr);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues: null iBPortIntf
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_UpdateCounterValues_NullIBPortIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullIB";
    uint8_t portNum = 2;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullib_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullib";
    std::vector<utils::Association> associations;

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    // Create with valid iBPortIntf but then null it out
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    // Null out iBPortIntf to trigger else branch
    sensor.iBPortIntf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues: null portMetricsOem2Intf
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_UpdateCounterValues_NullOem2Intf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullOem2";
    uint8_t portNum = 3;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nulloem2_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nulloem2";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    sensor.portMetricsOem2Intf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues: null portPacketCountersIntf
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_UpdateCounterValues_NullPktCountersIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullPkt";
    uint8_t portNum = 4;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullpkt_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullpkt";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    sensor.portPacketCountersIntf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues: counters NOT supported (all zero)
// Covers all FALSE branches of supported_counter.X conditionals
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_UpdateCounterValues_NoCountersSupported)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NoCounters";
    uint8_t portNum = 5;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nocount_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nocount";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    struct nsm_port_counter_data data = {};
    // supported_counter all zeros -> all FALSE branches taken
    memset(&data.supported_counter, 0, sizeof(data.supported_counter));
    data.port_rcv_pkts = 999; // should not be set since not supported
    sensor.updateCounterValues(&data);

    // Values should remain at 0 (initial)
    EXPECT_EQ(iBPortIntf->rxPkts(), 0u);
}

// ===========================================================================
// NsmPortMetrics::getBitErrorRate: value=0 path
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_GetBitErrorRate_ZeroValue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BER_Zero";
    uint8_t portNum = 6;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_ber0_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_ber0";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    EXPECT_DOUBLE_EQ(sensor.getBitErrorRate(0), 0.0);
}

// ===========================================================================
// NsmPortMetrics::getBitErrorRate: non-zero value with symbol_ber_coef_float=0
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_GetBitErrorRate_CoefFloatZero)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BER_CF0";
    uint8_t portNum = 7;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bercf0_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bercf0";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    // Value with symbol_ber_coef_float = 0 (bits 7-4 = 0)
    // magnitude=10 (bits 15-8), coef_float=0 (bits 7-4), coef=5 (bits 3-0)
    uint64_t val = (10 << 8) | (0 << 4) | 5;
    double result = sensor.getBitErrorRate(val);
    EXPECT_GT(result, 0.0);
}

// ===========================================================================
// NsmPortMetrics::getBitErrorRate: non-zero value with symbol_ber_coef_float!=0
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_GetBitErrorRate_NonZeroCoefFloat)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BER_CF1";
    uint8_t portNum = 8;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bercf1_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bercf1";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    // 0x0A21: magnitude=10, coef_float=2, coef=1
    double result = sensor.getBitErrorRate(0x0A21);
    EXPECT_GT(result, 0.0);
}

// ===========================================================================
// NsmPortStatus::update: encode failure (invalid portNumber encoding can't
// be easily forced, so test sensorIO failure instead)
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortStatus_Update_SensorIOFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_IOFail5";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_iofail5";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor.update(gpu);
}

// ===========================================================================
// NsmPortStatus: DOWN_LOCK with successful characteristics query
// (cc=NSM_SUCCESS && reasonCode=ERR_NULL -> runtimeError=false)
// ===========================================================================
TEST_F(NsmPortBranch5Test,
       PortStatus_DownLock_CharacteristicsSuccess_RuntimeErrorFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_DL_CharOK";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_dl_charok";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    // First response: port status with DOWN_LOCK
    std::vector<uint8_t> statusResp(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_status_resp));
    auto statusMsg = reinterpret_cast<nsm_msg*>(statusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, statusMsg);

    // Second response: port characteristics with cc=NSM_SUCCESS
    struct nsm_port_characteristics_data charData = {};
    std::vector<uint8_t> charResp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp));
    auto charMsg = reinterpret_cast<nsm_msg*>(charResp.data());
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL, &charData,
                                           charMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(statusResp))
        .WillOnce(mockSensorIO(charResp));

    sensor.update(gpu);

    // runtimeError should be false (cc==SUCCESS, reasonCode==ERR_NULL)
    EXPECT_FALSE(portMetricsOem3Intf->runtimeError());
}

// ===========================================================================
// NsmPortStatus: DOWN_LOCK with characteristics returning cc!=NSM_SUCCESS
// (runtimeError=true branch)
// ===========================================================================
TEST_F(NsmPortBranch5Test,
       PortStatus_DownLock_CharacteristicsFail_RuntimeErrorTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_DL_CharFail";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/ps_dl_charfail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    // First response: DOWN_LOCK
    std::vector<uint8_t> statusResp(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_status_resp));
    auto statusMsg = reinterpret_cast<nsm_msg*>(statusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, statusMsg);

    // Second response: characteristics with cc=NSM_ERROR + non-null reasonCode
    // nsm_common_non_success_resp layout: command(1), completion_code(1),
    // reason_code(2)
    std::vector<uint8_t> charResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto charMsg = reinterpret_cast<nsm_msg*>(charResp.data());
    auto* nonSuccResp =
        reinterpret_cast<nsm_common_non_success_resp*>(charMsg->payload);
    nonSuccResp->completion_code = NSM_ERROR;
    nonSuccResp->reason_code = htole16(0x0001); // non-null reason code

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(statusResp))
        .WillOnce(mockSensorIO(charResp));

    sensor.update(gpu);

    // runtimeError should be true (cc != NSM_SUCCESS && reasonCode != ERR_NULL)
    EXPECT_TRUE(portMetricsOem3Intf->runtimeError());
}

// ===========================================================================
// NsmPortStatus: DOWN_LOCK with characteristics sensorIO failure
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortStatus_DownLock_CharacteristicsSensorIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_DL_CharIOFail";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/ps_dl_chariofail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    // First response: DOWN_LOCK
    std::vector<uint8_t> statusResp(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_status_resp));
    auto statusMsg = reinterpret_cast<nsm_msg*>(statusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, statusMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(statusResp))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    sensor.update(gpu);
}

// ===========================================================================
// NsmPortMetrics::handleResponseMsg: cc=NSM_ERROR (TRUE branch of cc ? cc : rc)
// ===========================================================================
TEST_F(NsmPortBranch5Test, PortMetrics_HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_ErrCC";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_errcc_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_errcc";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
