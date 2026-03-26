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
 * Branch coverage tests for nsmPort.cpp:
 * - NsmPortStatus::update: all portState switch cases, portStatus switch cases
 * - NsmPortStatus::checkPortCharactersticRCAndPopulateRuntimeErr paths
 * - NsmPortCharacteristics::updateLinkDownCode: parameterized over all 36 codes
 * - NsmPortMetrics::getBitErrorRate: value=0 and value!=0 paths
 * - NsmPortMetrics::updateCounterValues: null interface paths
 * - EthPortTelemetryAggregator: handleSample, updateCounterValues, getIntfName
 * - NsmNetworkAddressAggregator: handleResponseMsg linkType branches
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

using namespace nsm;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmPortBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    // Helper: build NsmPortStatus update response with given portState/status
    static std::vector<uint8_t> buildPortStatusResp(uint8_t portState,
                                                    uint8_t portStatus)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_query_port_status_resp));
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                portState, portStatus, msg);
        (void)rc;
        return response;
    }
};

// ===========================================================================
// NsmPortStatus::update - all portState switch cases
// ===========================================================================

struct PortStateParam
{
    uint8_t state;
    const char* name;
};

class NsmPortStatusStateTest :
    public NsmPortBranch2Test,
    public WithParamInterface<PortStateParam>
{};

TEST_P(NsmPortStatusStateTest, PortState_SwitchCase)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_State";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_state";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    auto param = GetParam();
    auto resp = buildPortStatusResp(param.state, NSM_PORTSTATUS_ENABLED);

    if (param.state == NSM_PORTSTATE_DOWN_LOCK)
    {
        // DOWN_LOCK calls checkPortCharactersticRCAndPopulateRuntimeErr
        // which does a second sensorIO
        auto charResp = std::vector<uint8_t>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        charResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
            .WillOnce(mockSensorIO(resp))
            .WillOnce(mockSensorIO(charResp));
    }
    else
    {
        EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    }

    sensor.update(gpu);
}

INSTANTIATE_TEST_SUITE_P(
    AllPortStates, NsmPortStatusStateTest,
    Values(PortStateParam{NSM_PORTSTATE_DOWN, "DOWN"},
           PortStateParam{NSM_PORTSTATE_SLEEP, "SLEEP"},
           PortStateParam{NSM_PORTSTATE_DOWN_LOCK, "DOWN_LOCK"},
           PortStateParam{NSM_PORTSTATE_UP, "UP"},
           PortStateParam{NSM_PORTSTATE_POLLING, "POLLING"},
           PortStateParam{NSM_PORTSTATE_RESERVED, "RESERVED"},
           PortStateParam{NSM_PORTSTATE_TRAINING, "TRAINING"},
           PortStateParam{NSM_PORTSTATE_TRAINING_FAILURE, "TRAINING_FAILURE"},
           PortStateParam{NSM_PORTSTATE_TRAINING_FAILURE_LOCKED,
                          "TRAINING_FAILURE_LOCKED"},
           PortStateParam{NSM_PORTSTATE_PHYSICAL_UP, "PHYSICAL_UP"},
           PortStateParam{0xFF, "DEFAULT"}));

// ===========================================================================
// NsmPortStatus::update - portStatus switch cases
// ===========================================================================

TEST_F(NsmPortBranch2Test, PortStatus_Disabled)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_Disabled";
    uint8_t portNum = 2;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_disabled";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    auto resp = buildPortStatusResp(NSM_PORTSTATE_UP, NSM_PORTSTATUS_DISABLED);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

TEST_F(NsmPortBranch2Test, PortStatus_Default)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_Default";
    uint8_t portNum = 3;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_default";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    auto resp = buildPortStatusResp(NSM_PORTSTATE_UP, 0xFF);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}

// ===========================================================================
// NsmPortStatus::update - sensorIO failure
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortStatus_SensorIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_IOFail";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_iofail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));
    sensor.update(gpu);
}

// ===========================================================================
// NsmPortStatus::checkPortCharactersticRCAndPopulateRuntimeErr
// covers: encode fail, sensorIO fail, cc!=SUCCESS&&reasonCode!=ERR_NULL,
//         cc==SUCCESS (runtimeError false)
// ===========================================================================
TEST_F(NsmPortBranch2Test, CheckPortChar_SensorIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_CharIO";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_chario";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    // DOWN_LOCK triggers checkPortCharactersticRCAndPopulateRuntimeErr
    auto portResp = buildPortStatusResp(NSM_PORTSTATE_DOWN_LOCK,
                                        NSM_PORTSTATUS_ENABLED);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(portResp))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));
    sensor.update(gpu);
}

TEST_F(NsmPortBranch2Test, CheckPortChar_RuntimeErrorTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_CharRT";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_charrt";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    auto portResp = buildPortStatusResp(NSM_PORTSTATE_DOWN_LOCK,
                                        NSM_PORTSTATUS_ENABLED);

    // Build characteristics response with cc=NSM_ERROR, reasonCode!=ERR_NULL
    std::vector<uint8_t> charResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto charMsg = reinterpret_cast<nsm_msg*>(charResp.data());
    nsm_header_info hdr = {};
    hdr.nsm_msg_type = NSM_RESPONSE;
    hdr.instance_id = 0;
    hdr.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;
    pack_nsm_header(&hdr, &charMsg->hdr);
    encode_reason_code(NSM_ERROR, 0x1234, NSM_QUERY_PORT_CHARACTERISTICS,
                       charMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(portResp))
        .WillOnce(mockSensorIO(charResp));
    sensor.update(gpu);
    EXPECT_TRUE(portMetricsOem3Intf->runtimeError());
}

TEST_F(NsmPortBranch2Test, CheckPortChar_RuntimeErrorFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_CharOK";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_charok";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    auto portResp = buildPortStatusResp(NSM_PORTSTATE_DOWN_LOCK,
                                        NSM_PORTSTATUS_ENABLED);

    // Build successful characteristics response -> cc==SUCCESS -> false
    struct nsm_port_characteristics_data data = {};
    std::vector<uint8_t> charResp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp));
    auto charMsg = reinterpret_cast<nsm_msg*>(charResp.data());
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                           charMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(portResp))
        .WillOnce(mockSensorIO(charResp));
    sensor.update(gpu);
    EXPECT_FALSE(portMetricsOem3Intf->runtimeError());
}

// ===========================================================================
// NsmPortCharacteristics::updateLinkDownCode - parameterized over all codes
// ===========================================================================

struct LinkDownCodeParam
{
    uint32_t code;
    const char* name;
};

class NsmLinkDownCodeTest :
    public NsmPortBranch2Test,
    public WithParamInterface<LinkDownCodeParam>
{};

TEST_P(NsmLinkDownCodeTest, UpdateLinkDownCode_Case)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "LDC_Case";
    uint8_t portNum = 1;
    std::string type = "LDCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ldc_case";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    NsmPortCharacteristics sensor(bus, portName, portNum, type,
                                  portMetricsOem3Intf, iBPortIntf, objPath);

    auto param = GetParam();

    struct nsm_port_characteristics_data data = {};
    data.port_status.port_down_reason_code = param.code;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(
    AllLinkDownCodes, NsmLinkDownCodeTest,
    Values(
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN, "NoLinkDown"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_UNKNOWN, "Unknown"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_HI_SER_BER, "HiSERBER"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS,
                          "BlockLockLoss"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS,
                          "AlignmentLoss"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS,
                          "FECSyncLoss"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PLL_LOCK_LOSS,
                          "PLLLockLoss"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FIFO_OVERFLOW,
                          "FIFOOverflow"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FALSE_SKIP_CONDITION,
                          "FalseSkip"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_MINOR_ERR_THRESHOLD,
                          "MinorErrThreshold"},
        LinkDownCodeParam{
            NSM_PORT_DOWN_REASON_CODE_PHY_LAYER_RETRANSMIT_TIMEOUT,
            "PhyRetransmitTimeout"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_HEARTBEAT_ERRORS,
                          "HeartbeatErrors"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_CREDIT_MON_WD,
                          "CreditMonWD"},
        LinkDownCodeParam{
            NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_INTEGRITY_THRESHOLD,
            "IntegrityThreshold"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_BUFFER_OVERRUN,
                          "BufferOverrun"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HEALTHY,
                          "OOBCmdHealthy"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HI_BER,
                          "OOBCmdHiBER"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HEALTHY,
                          "InbandCmdHealthy"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HI_BER,
                          "InbandCmdHiBER"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_DOWN_BY_VERIFICATION_GW,
                          "VerificationGW"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_RECEIVED_REMOTE_FAULT,
                          "RemoteFault"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_RECEIEVED_TS1,
                          "ReceivedTS1"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_DOWN_BY_MGMT_CMD,
                          "MgmtCmd"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED,
                          "CableUnplugged"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_CABLE_ACCESS_ISSUES,
                          "CableAccessIssues"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN,
                          "ThermalShutdown"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_CURRENT_ISSUE,
                          "CurrentIssue"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_POWER_BUDGET,
                          "PowerBudget"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_RAW_BER,
                          "FastRecoveryRawBER"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_EFFECTIVE_BER,
                          "FastRecoveryEffectiveBER"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_SYMBOL_BER,
                          "FastRecoverySymbolBER"},
        LinkDownCodeParam{
            NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_CREDIT_WATCHDOG,
            "FastRecoveryCreditWD"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_SLEEP, "PeerSleep"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE,
                          "PeerDisable"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE_LOCK,
                          "PeerDisableLock"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_THERMAL_EVENT,
                          "PeerThermalEvent"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_FORCE_EVENT,
                          "PeerForceEvent"},
        LinkDownCodeParam{NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT,
                          "PeerResetEvent"},
        LinkDownCodeParam{0xFFFF, "DefaultCase"}));

// ===========================================================================
// NsmPortMetrics::getBitErrorRate - both branches
// ===========================================================================
TEST_F(NsmPortBranch2Test, GetBitErrorRate_ZeroValue_ReturnsZero)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BER0";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
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

TEST_F(NsmPortBranch2Test, GetBitErrorRate_NonZeroValue_WithCoefFloat)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BERnz";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bernz_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bernz";
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

    // value = magnitude(8 bits) << 8 | coef_float(4 bits) << 4 | coef(4 bits)
    // magnitude=3, coef_float=5, coef=2 -> (2 + 5/10) * 10^-3 = 0.0025
    uint64_t value = (3 << 8) | (5 << 4) | 2;
    double result = sensor.getBitErrorRate(value);
    EXPECT_GT(result, 0.0);
}

TEST_F(NsmPortBranch2Test,
       GetBitErrorRate_NonZeroValue_ZeroCoefFloat_DigitCount1)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BERzf";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_berzf_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_berzf";
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

    // coef_float=0 -> digitCount=1 (special case)
    uint64_t value = (3 << 8) | (0 << 4) | 5;
    double result = sensor.getBitErrorRate(value);
    EXPECT_GT(result, 0.0);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues - null interface branches
// ===========================================================================
TEST_F(NsmPortBranch2Test, UpdateCounterValues_NullIBPortIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullIB";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullib_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nullib";
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

    // Set to null after construction to test the else branch
    sensor.iBPortIntf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

TEST_F(NsmPortBranch2Test, UpdateCounterValues_NullPortMetricsOem2Intf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullOem2";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
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

    // Set to null after construction to test the else branch
    sensor.portMetricsOem2Intf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

TEST_F(NsmPortBranch2Test, UpdateCounterValues_NullPortPacketCountersIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullPkt";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
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

    // Set to null after construction to test the else branch
    sensor.portPacketCountersIntf = nullptr;

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

TEST_F(NsmPortBranch2Test, UpdateCounterValues_NullPortData)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NullData";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
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

    sensor.updateCounterValues(nullptr);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues - individual counter flag FALSE branches
// Each test sets only specific supported_counter bits to test
// ===========================================================================
TEST_F(NsmPortBranch2Test, UpdateCounterValues_NoSupportedCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NoFlags";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_noflags_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_noflags";
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

    // All supported_counter fields 0 -> all if branches FALSE
    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0, sizeof(data.supported_counter));
    sensor.updateCounterValues(&data);
}

// ===========================================================================
// EthPortTelemetryAggregator::handleSample - branches
// ===========================================================================
TEST_F(NsmPortBranch2Test, EthHandleSample_InvalidSample_ReturnSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_InvalidSample";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/eth_invalid_sample";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    // !valid -> early return
    NsmSensorAggregator::TelemetrySample sample(0, 0, nullptr, false);
    auto rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test, EthHandleSample_TagExceedsMax_ReturnSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_BigTag";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_bigtag";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    // tag > max -> early return
    uint8_t dummyData[8] = {};
    NsmSensorAggregator::TelemetrySample sample(
        NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1, sizeof(dummyData),
        dummyData, true);
    auto rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test, EthHandleSample_DecodeFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_DecFail";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_decfail";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    // Wrong data length to trigger decode failure
    uint8_t badData[1] = {0};
    NsmSensorAggregator::TelemetrySample sample(0, 1, badData, true);
    auto rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, 0);
}

// ===========================================================================
// EthPortTelemetryAggregator::updateCounterValues - each tag 0-20
// ===========================================================================
struct EthTagParam
{
    uint8_t tag;
    bool is64bit;
};

class EthTagTest :
    public NsmPortBranch2Test,
    public WithParamInterface<EthTagParam>
{};

TEST_P(EthTagTest, UpdateCounterValues_Tag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_Tag";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_tag";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    auto param = GetParam();
    nsm_ethernet_port_counter_data counterValue = {};
    if (param.is64bit)
    {
        counterValue.ethernet_port_counter_data_64bit = 12345;
    }
    else
    {
        counterValue.ethernet_port_counter_data_32bit = 678;
    }

    sensor.updateCounterValues(param.tag, &counterValue);
}

INSTANTIATE_TEST_SUITE_P(AllEthTags, EthTagTest,
                         Values(EthTagParam{0, true}, EthTagParam{1, true},
                                EthTagParam{2, true}, EthTagParam{3, true},
                                EthTagParam{4, true}, EthTagParam{5, true},
                                EthTagParam{6, true}, EthTagParam{7, true},
                                EthTagParam{8, false}, EthTagParam{9, false},
                                EthTagParam{10, false}, EthTagParam{11, false},
                                EthTagParam{12, false}, EthTagParam{13, false},
                                EthTagParam{14, false}, EthTagParam{15, false},
                                EthTagParam{16, false}, EthTagParam{17, false},
                                EthTagParam{18, false}, EthTagParam{19, false},
                                EthTagParam{20, false}));

// ===========================================================================
// EthPortTelemetryAggregator::updateCounterValues - unknown tag
// ===========================================================================
TEST_F(NsmPortBranch2Test, EthUpdateCounterValues_UnknownTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_UnkTag";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_unktag";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    nsm_ethernet_port_counter_data counterValue = {};
    counterValue.ethernet_port_counter_data_64bit = 999;
    // Tag 100 not in tagToPropertyMap -> no update
    sensor.updateCounterValues(100, &counterValue);
}

// ===========================================================================
// NsmNetworkAddressAggregator::handleResponseMsg - linkType branches
// ===========================================================================
TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_EthernetLinkType_MACAddress)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_EthMAC";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_ethmac";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    // Build aggregate response with:
    // 1. NSM_TAG_LINK_TYPE = ETHERNET
    // 2. NSM_TAG_MAC_ADDRESS
    // 3. NSM_TAG_PERMANENT_MAC_ADDRESS
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample 1: link type = ETHERNET
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Sample 2: MAC address (padded to 8 bytes — power of 2 required)
    uint8_t macData[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_MAC_ADDRESS, true, macData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    // Sample 3: Permanent MAC address (padded to 8 bytes)
    uint8_t permMacData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, 0x00};
    size_t s3Len = 0;
    uint8_t s3Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_PERMANENT_MAC_ADDRESS, true, permMacData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s3Buf), &s3Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len + s3Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 3,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);
    offset += s1Len;
    memcpy(response.data() + offset, s2Buf, s2Len);
    offset += s2Len;
    memcpy(response.data() + offset, s3Buf, s3Len);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_EthernetLinkType_InvalidTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_EthBadTag";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_ethbadtag";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = ETHERNET
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Invalid tag (NSM_TAG_NODE_GUID with Ethernet linkType -> else branch)
    uint8_t guidData[8] = {};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_NODE_GUID, true, guidData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);
    offset += s1Len;
    memcpy(response.data() + offset, s2Buf, s2Len);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_InfiniBandLinkType_NodeAndPortGuid)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_IBGuids";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_ibguids";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = INFINIBAND
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_INFINIBAND};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Node GUID
    uint8_t nodeGuid[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_NODE_GUID, true, nodeGuid, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    // Port GUID
    uint8_t portGuid[8] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};
    size_t s3Len = 0;
    uint8_t s3Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_PORT_GUID, true, portGuid, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s3Buf), &s3Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len + s3Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 3,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);
    offset += s1Len;
    memcpy(response.data() + offset, s2Buf, s2Len);
    offset += s2Len;
    memcpy(response.data() + offset, s3Buf, s3Len);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_DefaultLinkType_UnknownProtocol)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_DefLT";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_deflt";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = unknown value (0xFF -> default case in switch)
    uint8_t linkTypeData[1] = {0xFF};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 1,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);

    // linkType stays UNKNOWN -> returns NSM_SW_ERROR_DATA
    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_NoLinkTypeSample_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_NoLT";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_nolt";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    // Empty response with no link type sample -> linkType remains UNKNOWN
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> response(headerSize);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 0,
                          responseMsg);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortBranch2Test,
       NsmNetworkAddressAggregator_InvalidSample_SkipsBranches)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_InvSamp";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_invsamp";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = ETHERNET
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Invalid sample (!valid) — padded to 8 bytes (power of 2 required)
    uint8_t macData[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_MAC_ADDRESS, false, macData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);
    offset += s1Len;
    memcpy(response.data() + offset, s2Buf, s2Len);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

// ===========================================================================
// Forward declarations for testing getTopologyObjPath
// ===========================================================================
namespace nsm
{
std::string getTopologyObjPath(const std::string& deviceName,
                               uint8_t deviceType);
} // namespace nsm

// ===========================================================================
// getTopologyObjPath - all device type switch cases
// ===========================================================================
TEST_F(NsmPortBranch2Test, GetTopologyObjPath_GPU)
{
    auto path = getTopologyObjPath("dev0", NSM_DEV_ID_GPU);
    EXPECT_EQ(path,
              "/xyz/openbmc_project/inventory/system/linktopology/GPU/dev0");
}

TEST_F(NsmPortBranch2Test, GetTopologyObjPath_SWITCH)
{
    auto path = getTopologyObjPath("sw0", NSM_DEV_ID_SWITCH);
    EXPECT_EQ(path,
              "/xyz/openbmc_project/inventory/system/linktopology/SWITCH/sw0");
}

TEST_F(NsmPortBranch2Test, GetTopologyObjPath_PCIE_BRIDGE)
{
    auto path = getTopologyObjPath("br0", NSM_DEV_ID_PCIE_BRIDGE);
    EXPECT_EQ(
        path,
        "/xyz/openbmc_project/inventory/system/linktopology/PCIE_BRIDGE/br0");
}

TEST_F(NsmPortBranch2Test, GetTopologyObjPath_EROT)
{
    auto path = getTopologyObjPath("erot0", NSM_DEV_ID_EROT);
    EXPECT_EQ(path,
              "/xyz/openbmc_project/inventory/system/linktopology/EROT/erot0");
}

TEST_F(NsmPortBranch2Test, GetTopologyObjPath_CPU)
{
    auto path = getTopologyObjPath("cpu0", NSM_DEV_ID_CPU);
    EXPECT_EQ(path,
              "/xyz/openbmc_project/inventory/system/linktopology/CPU/cpu0");
}

TEST_F(NsmPortBranch2Test, GetTopologyObjPath_Default_ReturnsEmpty)
{
    auto path = getTopologyObjPath("unknown", 0xFF);
    EXPECT_EQ(path, "");
}

// ===========================================================================
// NsmNetworkAddressAggregator::handleResponseMsg -
// InfiniBand link type with invalid tag (else branch at L1525-1529)
// ===========================================================================
TEST_F(NsmPortBranch2Test, NsmNetworkAddressAggregator_InfiniBand_InvalidTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_IBBadTag";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_ibbadtag";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = INFINIBAND
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_INFINIBAND};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Invalid tag for InfiniBand (MAC_ADDRESS -> else branch)
    uint8_t macData2[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_MAC_ADDRESS, true, macData2, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);

    size_t off = headerSize;
    memcpy(response.data() + off, s1Buf, s1Len);
    off += s1Len;
    memcpy(response.data() + off, s2Buf, s2Len);

    auto ibResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(ibResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmNetworkAddressAggregator::handleResponseMsg - cc/rc error
// ===========================================================================
TEST_F(NsmPortBranch2Test, NsmNetworkAddressAggregator_DecodeAggregateRespFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_DecFail";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_decfail";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    // Too-short response -> decode_aggregate_resp fails
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto decResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(decResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmNetworkAddressAggregator::handleResponseMsg - sample tag > max
// ===========================================================================
TEST_F(NsmPortBranch2Test, NsmNetworkAddressAggregator_SampleTagExceedsMax)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_BigTag";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_bigtag";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = ETHERNET
    uint8_t linkTypeData[1] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t s1Len = 0;
    uint8_t s1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Sample with tag > max
    uint8_t bigTagData[8] = {};
    size_t s2Len = 0;
    uint8_t s2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1, true, bigTagData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);

    size_t off = headerSize;
    memcpy(response.data() + off, s1Buf, s1Len);
    off += s1Len;
    memcpy(response.data() + off, s2Buf, s2Len);

    auto tagResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(tagResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmPortMetrics::handleResponseMsg - decode fail (cc!=SUCCESS)
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortMetrics_HandleResponseMsg_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_DecFail";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_decfail_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_decfail";
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

    // Short buffer -> decode fail
    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    shortResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<nsm_msg*>(shortResp.data());
    auto cc = sensor.handleResponseMsg(msg, shortResp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPortCharacteristics::handleResponseMsg - decode fail
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortCharacteristics_HandleResponseMsg_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PC_DecFail";
    uint8_t portNum = 1;
    std::string type = "PCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/pc_decfail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    NsmPortCharacteristics sensor(bus, portName, portNum, type,
                                  portMetricsOem3Intf, iBPortIntf, objPath);

    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    shortResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<nsm_msg*>(shortResp.data());
    auto cc = sensor.handleResponseMsg(msg, shortResp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - all ECC tag cases
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortECCCounters_HandleResponseMsg_AllTags)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_AllTags";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_all_tags";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Build 6 samples for tags 0-5 (all ECC tags)
    struct SampleBuf
    {
        uint8_t buf[24];
        size_t len;
    };
    SampleBuf eccSamples[6];

    for (uint8_t tag = 0; tag < 6; tag++)
    {
        uint64_t counterVal = 100 + tag;
        eccSamples[tag].len = 0;
        encode_aggregate_resp_sample(
            tag, true, reinterpret_cast<uint8_t*>(&counterVal),
            sizeof(counterVal),
            reinterpret_cast<nsm_aggregate_resp_sample*>(eccSamples[tag].buf),
            &eccSamples[tag].len);
    }

    size_t totalSampleLen = 0;
    for (int i = 0; i < 6; i++)
        totalSampleLen += eccSamples[i].len;

    std::vector<uint8_t> response(headerSize + totalSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 6,
                          responseMsg);

    size_t off = headerSize;
    for (int i = 0; i < 6; i++)
    {
        memcpy(response.data() + off, eccSamples[i].buf, eccSamples[i].len);
        off += eccSamples[i].len;
    }

    auto eccResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - default tag case
// ===========================================================================
TEST_F(NsmPortBranch2Test,
       DISABLED_PortECCCounters_HandleResponseMsg_DefaultTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_DefTag";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_deftag";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint64_t counterVal = 42;
    size_t sLen = 0;
    uint8_t sBuf[24] = {};
    encode_aggregate_resp_sample(
        10, true, reinterpret_cast<uint8_t*>(&counterVal), sizeof(counterVal),
        reinterpret_cast<nsm_aggregate_resp_sample*>(sBuf), &sLen);

    std::vector<uint8_t> response(headerSize + sLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 1,
                          responseMsg);

    memcpy(response.data() + headerSize, sBuf, sLen);

    auto eccResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - cc != SUCCESS
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortECCCounters_HandleResponseMsg_CcError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_CcErr";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_ccerr";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    std::vector<uint8_t> shortResp(sizeof(nsm_msg_hdr), 0);
    auto msg = reinterpret_cast<nsm_msg*>(shortResp.data());
    auto eccResult = sensor.handleResponseMsg(msg, shortResp.size());
    EXPECT_NE(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - !valid sample (continue)
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortECCCounters_HandleResponseMsg_InvalidSample)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_InvSamp";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_invsamp";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint64_t counterVal = 0;
    size_t sLen = 0;
    uint8_t sBuf[24] = {};
    encode_aggregate_resp_sample(
        0, false, reinterpret_cast<uint8_t*>(&counterVal), sizeof(counterVal),
        reinterpret_cast<nsm_aggregate_resp_sample*>(sBuf), &sLen);

    std::vector<uint8_t> response(headerSize + sLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 1,
                          responseMsg);

    memcpy(response.data() + headerSize, sBuf, sLen);

    auto eccResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - tag > max (continue)
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortECCCounters_HandleResponseMsg_TagExceedsMax)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_BigTag";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_bigtag";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint64_t counterVal = 0;
    size_t sLen = 0;
    uint8_t sBuf[24] = {};
    encode_aggregate_resp_sample(
        NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1, true,
        reinterpret_cast<uint8_t*>(&counterVal), sizeof(counterVal),
        reinterpret_cast<nsm_aggregate_resp_sample*>(sBuf), &sLen);

    std::vector<uint8_t> response(headerSize + sLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 1,
                          responseMsg);

    memcpy(response.data() + headerSize, sBuf, sLen);

    auto eccResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters::handleResponseMsg - no lane tags (FALSE branch)
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortECCCounters_HandleResponseMsg_NoLaneTags)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_NoLanes";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_nolanes";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    struct
    {
        uint8_t buf[24];
        size_t len;
    } eccSamples[2];

    for (uint8_t tag = 0; tag < 2; tag++)
    {
        uint64_t val = 50 + tag;
        eccSamples[tag].len = 0;
        encode_aggregate_resp_sample(
            tag, true, reinterpret_cast<uint8_t*>(&val), sizeof(val),
            reinterpret_cast<nsm_aggregate_resp_sample*>(eccSamples[tag].buf),
            &eccSamples[tag].len);
    }

    size_t totalSampleLen = eccSamples[0].len + eccSamples[1].len;
    std::vector<uint8_t> response(headerSize + totalSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 2,
                          responseMsg);

    size_t off = headerSize;
    for (int i = 0; i < 2; i++)
    {
        memcpy(response.data() + off, eccSamples[i].buf, eccSamples[i].len);
        off += eccSamples[i].len;
    }

    auto eccResult = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(eccResult, NSM_SW_SUCCESS);
}

// ===========================================================================
// genRequestMsg encode fail tests (nullopt paths)
// ===========================================================================
TEST_F(NsmPortBranch2Test, NsmNetworkAddressAggregator_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_EncFail";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_encfail";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    auto genResult = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(genResult.has_value());
}

TEST_F(NsmPortBranch2Test, NsmGetPortECCCounters_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_EncFail";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_encfail";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    auto genResult = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(genResult.has_value());
}

TEST_F(NsmPortBranch2Test, PortMetrics_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_EncFail";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_encfail_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_encfail";
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

    auto genResult = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(genResult.has_value());
}

TEST_F(NsmPortBranch2Test, PortCharacteristics_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PC_EncFail";
    uint8_t portNum = 1;
    std::string type = "PCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/pc_encfail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    NsmPortCharacteristics sensor(bus, portName, portNum, type,
                                  portMetricsOem3Intf, iBPortIntf, objPath);

    auto genResult = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(genResult.has_value());
}

TEST_F(NsmPortBranch2Test, EthPortTelemetry_GenRequestMsg_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_EncFail";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_encfail";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    auto genResult = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(genResult.has_value());
}

// ===========================================================================
// EthPortTelemetryAggregator::getInterfaceName - all branches
// ===========================================================================
TEST_F(NsmPortBranch2Test, EthGetInterfaceName_AllBranches)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_IntfName";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_intfname";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    std::string ifaceName;

    sensor.getInterfaceName("RXBytes", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("TXBytes", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("RXUnicastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("RXMulticastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("RXBroadcastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("TXUnicastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("TXMulticastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    sensor.getInterfaceName("TXBroadcastPkts", ifaceName);
    EXPECT_FALSE(ifaceName.empty());

    // Unknown prop -> ifaceName unchanged
    std::string origName = "original";
    ifaceName = origName;
    sensor.getInterfaceName("SomethingElse", ifaceName);
    EXPECT_EQ(ifaceName, origName);
}

// ===========================================================================
// NsmPortMetrics::updateCounterValues - all counter flags TRUE
// ===========================================================================
TEST_F(NsmPortBranch2Test, UpdateCounterValues_AllFlagsTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_AllFlags";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_allflags_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_allflags";
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
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.symbol_ber = (3 << 8) | (5 << 4) | 2;
    data.effective_ber = (2 << 8) | (0 << 4) | 7;
    data.total_raw_ber = (4 << 8) | (9 << 4) | 1;
    data.port_rcv_pkts = 100;
    data.port_xmit_pkts = 200;
    data.port_rcv_data = 300;
    data.port_xmit_data = 400;

    sensor.updateCounterValues(&data);

    EXPECT_EQ(iBPortIntf->rxPkts(), 100u);
    EXPECT_EQ(iBPortIntf->txPkts(), 200u);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 300u);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(), 400u);
}

// ===========================================================================
// NsmPortMetrics::handleResponseMsg - valid response
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortMetrics_HandleResponseMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_Success";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_success_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_success";
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
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.port_rcv_pkts = 42;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp) +
                                      PORT_COUNTER_TELEMETRY_MAX_DATA_SIZE,
                                  0);
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPortStatus::update - decode fail (non-success cc)
// ===========================================================================
TEST_F(NsmPortBranch2Test, PortStatus_Update_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "PS_DecFail";
    uint8_t portNum = 1;
    std::string type = "PSType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ps_decfail";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

    NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                         objPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);
}
