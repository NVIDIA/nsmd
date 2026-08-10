/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

// Coverage tests for NsmPowerControl loop-body branches:
//   maxPowerCapValue(), minPowerCapValue(), defaultPowerCap() with non-empty
//   SensorManager lists (loop body entered), and
//   doClearPowerCap()/setPowerCap() loop bodies.
//
// IMPORTANT: Do NOT include nsmProcessorModulePowerControl.hpp here — it
// defines a conflicting nsm::NsmClearPowerCapIntf with
// nsmClearPowerCapIface.hpp (both pulled in via nsmProcessor.hpp).

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "pci-links.h"
#include "platform-environmental.h"

// nsmProcessor.hpp transitively includes:
//   nsmClearPowerCapIface.hpp  → nsm::NsmClearPowerCapIntf /
//   NsmClearPowerCapAsyncIntf nsmPowerCapIface.hpp       → nsm::NsmPowerCapIntf
//   nsmChassis/nsmPowerControl.hpp → nsm::NsmPowerControl
#include "nsmProcessor/nsmProcessor.hpp"

using namespace nsm;

// Forward-declare free function from nsmPowerControl.cpp
namespace nsm
{
requester::Coroutine
    doClearPowerCap(std::shared_ptr<AsyncStatusIntf> statusInterface);
} // namespace nsm

// =============================================================================
// Response helpers
// =============================================================================

static std::vector<uint8_t> makeSetPwrLimitOkResp()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                reinterpret_cast<nsm_msg*>(buf.data()));
    return buf;
}

static std::vector<uint8_t> makeGetPwrLimitOkResp(uint32_t enforcedMw = 0)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedMw,
                                reinterpret_cast<nsm_msg*>(buf.data()));
    return buf;
}

// =============================================================================
// Test fixture
// =============================================================================

struct NsmPowerControlLoopBodyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "LoopBodyCtrl";
    const std::string type = "NSM_PowerControl";
    const std::string ctrlPath =
        "/xyz/openbmc_project/control/power_cap/loop_body_ctrl";
    const std::string physicalContext = "SystemBoard";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPowerControl> powerControl;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerControlLoopBodyTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0));
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        std::string typeCopy = type;
        powerControl = std::make_shared<NsmPowerControl>(
            bus, name, associations, typeCopy, ctrlPath, physicalContext);
    }

    ~NsmPowerControlLoopBodyTest()
    {
        mockManager.maxPowerCapList.clear();
        mockManager.minPowerCapList.clear();
        mockManager.defaultPowerCapList.clear();
        mockManager.powerCapList.clear();
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// maxPowerCapValue(): loop body entered (non-empty maxPowerCapList)
// Covers nsmPowerControl.cpp L129-130
// =============================================================================

TEST_F(NsmPowerControlLoopBodyTest,
       MaxPowerCapValue_NonEmptyList_ReturnsSumOfValues)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pcName = "MaxLoopPC";
    std::vector<std::string> parents;
    std::string invPath = "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/max1";

    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);
    pcIntf->maxPowerCapValue(750);

    auto limitIntf = std::make_shared<PowerLimitIface>(bus, invPath.c_str());

    std::string sName = "MaxPCLoopSensor";
    std::string sType = "NSM_MaxPowerCap";
    auto maxPcSensor = std::make_shared<NsmMaxPowerCap>(sName, sType, pcIntf,
                                                        limitIntf, invPath);

    mockManager.maxPowerCapList.push_back(maxPcSensor);

    EXPECT_EQ(powerControl->maxPowerCapValue(), 750u);
}

TEST_F(NsmPowerControlLoopBodyTest, MaxPowerCapValue_TwoItems_ReturnsSum)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<std::string> parents;

    std::string path1 = "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/max2a";
    std::string name1 = "MaxPC2a";
    auto pc1 = std::make_shared<NsmPowerCapIntf>(bus, path1.c_str(), name1,
                                                 parents, gpu);
    pc1->maxPowerCapValue(300);
    auto lim1 = std::make_shared<PowerLimitIface>(bus, path1.c_str());
    std::string sn1 = "MaxPCSensor2a";
    std::string st1 = "NSM_MaxPowerCap";
    auto s1 = std::make_shared<NsmMaxPowerCap>(sn1, st1, pc1, lim1, path1);

    std::string path2 = "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/max2b";
    std::string name2 = "MaxPC2b";
    auto pc2 = std::make_shared<NsmPowerCapIntf>(bus, path2.c_str(), name2,
                                                 parents, gpu);
    pc2->maxPowerCapValue(450);
    auto lim2 = std::make_shared<PowerLimitIface>(bus, path2.c_str());
    std::string sn2 = "MaxPCSensor2b";
    std::string st2 = "NSM_MaxPowerCap";
    auto s2 = std::make_shared<NsmMaxPowerCap>(sn2, st2, pc2, lim2, path2);

    mockManager.maxPowerCapList.push_back(s1);
    mockManager.maxPowerCapList.push_back(s2);

    EXPECT_EQ(powerControl->maxPowerCapValue(), 750u);
}

// =============================================================================
// minPowerCapValue(): loop body entered
// Covers nsmPowerControl.cpp L142-143
// =============================================================================

TEST_F(NsmPowerControlLoopBodyTest,
       MinPowerCapValue_NonEmptyList_ReturnsSumOfValues)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pcName = "MinLoopPC";
    std::vector<std::string> parents;
    std::string invPath = "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/min1";

    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);
    pcIntf->minPowerCapValue(100);

    auto limitIntf = std::make_shared<PowerLimitIface>(bus, invPath.c_str());

    std::string sName = "MinPCLoopSensor";
    std::string sType = "NSM_MinPowerCap";
    auto minPcSensor = std::make_shared<NsmMinPowerCap>(sName, sType, pcIntf,
                                                        limitIntf, invPath);

    mockManager.minPowerCapList.push_back(minPcSensor);

    EXPECT_EQ(powerControl->minPowerCapValue(), 100u);
}

// =============================================================================
// defaultPowerCap(): loop body entered
// Covers nsmPowerControl.cpp L155-156
// =============================================================================

TEST_F(NsmPowerControlLoopBodyTest,
       DefaultPowerCap_NonEmptyList_ReturnsSumOfValues)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string clearName = "DefaultPCLoop";
    std::string clearType = "NSM_DefaultPowerCap";
    std::string invPath = "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/def1";

    std::string pcName = "DefaultPCLoopCap";
    std::vector<std::string> parents;
    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);
    pcIntf->PowerCapIntf::defaultPowerCap(500);

    // clearPowerCapAsyncIntf not needed for the getter — pass nullptr
    auto defaultPcSensor = std::make_shared<NsmDefaultPowerCap>(
        clearName, clearType, pcIntf, nullptr);

    mockManager.defaultPowerCapList.push_back(defaultPcSensor);

    EXPECT_EQ(powerControl->defaultPowerCap(), 500u);
}

// =============================================================================
// doClearPowerCap(): loop body entered (defaultPowerCapList non-empty)
// Covers nsmPowerControl.cpp L169-179
// =============================================================================

TEST_F(NsmPowerControlLoopBodyTest,
       DoClearPowerCap_NonEmptyList_LoopBodyEntered_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string clearName = "DoClearLoop";
    std::string clearType = "NSM_DefaultPowerCap";
    std::string invPath =
        "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/doclear1";

    std::string pcName = "DoClearPC";
    std::vector<std::string> parents;
    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);

    pcIntf->PowerCapIntf::defaultPowerCap(300);

    auto clearPCIntf = std::make_shared<NsmClearPowerCapIntf>(bus,
                                                              invPath.c_str());
    auto clearAsyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, invPath.c_str(), gpu, pcIntf, clearPCIntf);

    auto defaultPcSensor = std::make_shared<NsmDefaultPowerCap>(
        clearName, clearType, pcIntf, clearAsyncIntf);

    mockManager.defaultPowerCapList.push_back(defaultPcSensor);

    // clearPowerCapOnDevice: set_power_limit then getPowerCapFromDevice
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPwrLimitOkResp()))
        .WillOnce(mockPostPatchIO(makeGetPwrLimitOkResp(300000)));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/doclear_loop1");

    EXPECT_NO_THROW(nsm::doClearPowerCap(statusIntf));
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// doClearPowerCap loop body: deviceStatus failure → status propagated
TEST_F(NsmPowerControlLoopBodyTest,
       DoClearPowerCap_NonEmptyList_DeviceFailure_StatusWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string clearName = "DoClearLoopFail";
    std::string clearType = "NSM_DefaultPowerCap";
    std::string invPath =
        "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/doclear2";

    std::string pcName = "DoClearPCFail";
    std::vector<std::string> parents;
    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);

    pcIntf->PowerCapIntf::defaultPowerCap(300);

    auto clearPCIntf = std::make_shared<NsmClearPowerCapIntf>(bus,
                                                              invPath.c_str());
    auto clearAsyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, invPath.c_str(), gpu, pcIntf, clearPCIntf);

    auto defaultPcSensor = std::make_shared<NsmDefaultPowerCap>(
        clearName, clearType, pcIntf, clearAsyncIntf);

    mockManager.defaultPowerCapList.push_back(defaultPcSensor);

    // postPatchIO fails → deviceStatus = WriteFailure
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/doclear_loop2");

    EXPECT_NO_THROW(nsm::doClearPowerCap(statusIntf));
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// setPowerCap(): loop body entered (powerCapList non-empty)
// Covers nsmPowerControl.cpp L90-100
// =============================================================================

TEST_F(NsmPowerControlLoopBodyTest,
       SetPowerCap_NonEmptyPowerCapList_LoopBodyEntered_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pcName = "SetCapLoopPC";
    std::vector<std::string> parents;
    std::string invPath =
        "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/setcap1";

    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);

    // PowerPersistencyIntf at a sub-path to avoid D-Bus interface collision
    std::string persistPath = invPath + "/persist";
    auto persistIntf =
        std::make_shared<PowerPersistencyIntf>(bus, persistPath.c_str());

    std::string sName = "SetCapLoopSensor";
    std::string sType = "NSM_PowerCap";
    auto powerCapSensor = std::make_shared<NsmPowerCap>(
        sName, sType, pcIntf, parents, persistIntf, invPath);

    mockManager.powerCapList.push_back(powerCapSensor);

    // powerLimit=0: maxPowerCapList empty → maxPowerCapValue()=0 → 0<=0 passes
    // setPowerCapOnDevice(0): encode, postPatchIO(set),
    // postPatchIO(getPowerCap)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPwrLimitOkResp()))
        .WillOnce(mockPostPatchIO(makeGetPwrLimitOkResp(0)));

    uint32_t validValue = 0;
    AsyncSetOperationValueType value = validValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_NO_THROW(powerControl->setPowerCap(value, &status, gpu));
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setPowerCap loop body: deviceStatus != Success → *status updated
TEST_F(NsmPowerControlLoopBodyTest,
       SetPowerCap_NonEmptyPowerCapList_DeviceFailure_StatusPropagated)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pcName = "SetCapLoopPCFail";
    std::vector<std::string> parents;
    std::string invPath =
        "/xyz/openbmc_project/inventory/nsmPwrCtrlLoop/setcap2";

    auto pcIntf = std::make_shared<NsmPowerCapIntf>(bus, invPath.c_str(),
                                                    pcName, parents, gpu);

    std::string persistPath = invPath + "/persist";
    auto persistIntf =
        std::make_shared<PowerPersistencyIntf>(bus, persistPath.c_str());

    std::string sName = "SetCapLoopSensorFail";
    std::string sType = "NSM_PowerCap";
    auto powerCapSensor = std::make_shared<NsmPowerCap>(
        sName, sType, pcIntf, parents, persistIntf, invPath);

    mockManager.powerCapList.push_back(powerCapSensor);

    // postPatchIO fails → deviceStatus = WriteFailure → propagated to *status
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    uint32_t validValue = 0;
    AsyncSetOperationValueType value = validValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_NO_THROW(powerControl->setPowerCap(value, &status, gpu));
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
