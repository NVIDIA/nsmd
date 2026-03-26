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

// Additional branch coverage for nsmProcessorModulePowerControl.cpp factory
// Targets:
//   - CreateProcessorModulePowerControl: missing props, empty chassisPath,
//     index != 0, device map already has entry
//   - NsmClearPowerCapIntf::clearPowerCap() returns 0
//   - updatePowerLimitOnModule: multiple devices in map, second device fails

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmProcessorModulePowerControl.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine
    CreateProcessorModulePowerControl(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================
struct NsmProcModPowerBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "ProcModPower_Br2";
    const std::string type = "NSM_ProcessorModulePowerControl";
    const std::string path =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU_br2";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcModPowerBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmProcModPowerBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmClearPowerCapIntf::clearPowerCap returns 0
// ============================================================================
TEST_F(NsmProcModPowerBranch2Test, ClearPowerCapIntf_Returns0)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmClearPowerCapIntf clearIntf(bus, path);
    EXPECT_EQ(clearIntf.clearPowerCap(), 0);
}

// ============================================================================
// updatePowerLimitOnModule: multiple devices, second postPatchIO fails
// ============================================================================
TEST_F(NsmProcModPowerBranch2Test,
       UpdatePowerLimitOnModule_MultipleDevices_SecondFails)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    associations_list.push_back(
        {"chassis", "power_controls",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_br2"});

    auto powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

    auto powerControl = std::make_shared<NsmProcessorModulePowerControl>(
        bus, name, type, powerCapIntf, clearPowerCapIntf, path,
        associations_list);

    // Create a second mock device
    const uuid_t gpuUuid2 = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto gpu2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(gpuUuid2));
    ASSERT_NE(gpu2, nullptr);

    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu, gpu2};

    // First device succeeds, second fails
    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));
    EXPECT_CALL(*gpu2, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    manager.processorModuleToDeviceMap.erase(path);
}

// ============================================================================
// updatePowerLimitOnModule: multiple devices, all succeed
// ============================================================================
TEST_F(NsmProcModPowerBranch2Test,
       UpdatePowerLimitOnModule_MultipleDevices_AllSucceed)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    associations_list.push_back(
        {"chassis", "power_controls",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_br2b"});

    auto powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

    auto powerControl = std::make_shared<NsmProcessorModulePowerControl>(
        bus, name, type, powerCapIntf, clearPowerCapIntf, path,
        associations_list);

    const uuid_t gpuUuid2 = "STATIC:2:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto gpu2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(gpuUuid2));
    ASSERT_NE(gpu2, nullptr);

    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu, gpu2};

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));
    EXPECT_CALL(*gpu2, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    manager.processorModuleToDeviceMap.erase(path);
}

// ============================================================================
// setModulePowerCap: valid value within range goes to updatePowerLimitOnModule
// ============================================================================
TEST_F(NsmProcModPowerBranch2Test,
       SetModulePowerCap_ValidRange_CallsUpdatePowerLimit)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    associations_list.push_back(
        {"chassis", "power_controls",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_br2c"});

    auto powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

    auto powerControl = std::make_shared<NsmProcessorModulePowerControl>(
        bus, name, type, powerCapIntf, clearPowerCapIntf, path,
        associations_list);

    powerCapIntf->minPowerCapValue(100);
    powerCapIntf->maxPowerCapValue(500);

    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncSetOperationValueType value = uint32_t{300};
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->setModulePowerCap(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    manager.processorModuleToDeviceMap.erase(path);
}

// Factory tests removed: require utils::DBusHandler::getInstance() which
// doesn't exist in the test mock infrastructure.
