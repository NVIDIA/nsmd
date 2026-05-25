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

#pragma once

#include <algorithm>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "nsmSetAsync/asyncOperationManager.hpp"
#include "requester/handler.hpp"
#include "sensorManager.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#undef private

namespace nsm
{
extern std::unique_ptr<SensorManager> sensorManagerInstance;
} // namespace nsm

namespace mctp
{
extern std::unique_ptr<MctpDiscovery> mctpDiscoveryInstance;
} // namespace mctp

#include "commonMock.hpp"
using namespace nsm;
using ::testing::NiceMock;

class MockMctpDiscovery
{
  public:
    MockMctpDiscovery(NsmDeviceTable& nsmDevices) : nsmDevices(nsmDevices) {};
    ~MockMctpDiscovery() = default;

  private:
    MockMctpDiscovery() = delete;
    MockMctpDiscovery(const MockMctpDiscovery&) = delete;
    MockMctpDiscovery& operator=(const MockMctpDiscovery&) = delete;
    MockMctpDiscovery(MockMctpDiscovery&&) = delete;
    MockMctpDiscovery& operator=(MockMctpDiscovery&&) = delete;
    NsmDeviceTable& nsmDevices;

    auto findNsmDevice(uint8_t deviceType, uint8_t instanceNumber,
                       uint8_t deviceRole)
    {
        return std::find_if(nsmDevices.begin(), nsmDevices.end(),
                            [deviceType, instanceNumber, deviceRole](
                                const std::shared_ptr<NsmDevice>& device) {
            return device->getDeviceType() == deviceType &&
                   device->getInstanceNumber() == instanceNumber &&
                   device->getDeviceRole() == deviceRole;
        });
    }

  public:
    std::shared_ptr<NsmDevice>
        findOrCreateNsmDevice(uint8_t deviceType, uint8_t deviceRole,
                              uint8_t instanceNumber, std::string remapPropName,
                              std::vector<std::string>& remapPropValue)
    {
        if (auto it = findNsmDevice(deviceType, instanceNumber, deviceRole);
            it != nsmDevices.end())
        {
            return *it;
        }

        // Create new device if not found
        std::shared_ptr<NsmDevice> nsmDevice = std::make_shared<MockNsmDevice>(
            deviceType, instanceNumber, remapPropName, remapPropValue[0],
            deviceRole);
        lg2::info(
            "Creating new NsmDevice for deviceType:{TYPE} instanceNumber:{INST} deviceRole:{ROLE} remapPropName:{REMAPPNAME} remapPropValue:{REMAPPVALUE}",
            "TYPE", deviceType, "INST", instanceNumber, "ROLE", deviceRole,
            "REMAPPNAME", remapPropName, "REMAPPVALUE", remapPropValue[0]);
        nsmDevices.push_back(nsmDevice);
        return nsmDevice;
    }
    std::shared_ptr<NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid)
    {
        uint8_t deviceType = 0xff;
        uint8_t instanceNumber = 0xff;
        uint8_t deviceRole = NSM_DEV_ROLE_RESERVED;
        std::string remapPropName;
        std::vector<std::string> remapPropValue;
        if (utils::parseStaticUuid(uuid, deviceType, instanceNumber, deviceRole,
                                   remapPropName, remapPropValue) < 0)
        {
            throw std::runtime_error(
                "MockMctpDiscovery::getNsmDevice: uuid in EM json is not in a valid format(STATIC:d:d:s:s), UUID=" +
                uuid);
        }

        return findOrCreateNsmDevice(deviceType, deviceRole, instanceNumber,
                                     remapPropName, remapPropValue);
    }

    std::shared_ptr<NsmDevice>
        getNsmDeviceByIdentification(uint8_t deviceType, uint8_t instanceNumber,
                                     uint8_t deviceRole)
    {
        // Search for existing device in nsmDevices
        auto it = findNsmDevice(deviceType, instanceNumber, deviceRole);
        return (it != nsmDevices.end()) ? *it : nullptr;
    }
};

struct MockSensorManager : public SensorManager
{
    MockMctpDiscovery mockMctpDiscovery;
    MockSensorManager() = delete;
    MockSensorManager(NsmDeviceTable& nsmDevices) :
        SensorManager(nsmDevices), mockMctpDiscovery(nsmDevices)
    {}
    MOCK_METHOD(eid_t, getEid, (std::shared_ptr<NsmDevice> nsmDevice),
                (override));
    MOCK_METHOD(sdbusplus::asio::object_server&, getObjServer, (), (override));

    std::shared_ptr<NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid) override
    {
        return mockMctpDiscovery.getNsmDeviceFromStaticUUID(uuid);
    };
    std::shared_ptr<NsmDevice> getNsmDevice(uint8_t deviceType,
                                            uint8_t instanceNumber,
                                            uint8_t deviceRole) override
    {
        return mockMctpDiscovery.getNsmDeviceByIdentification(
            deviceType, instanceNumber, deviceRole);
    }
};

// Mock SensorManager that always returns nullptr from
// getNsmDeviceFromStaticUUID. Use this to exercise factory-function !nsmDevice
// error branches.
struct NullReturnMockSensorManager : public MockSensorManager
{
    NullReturnMockSensorManager(NsmDeviceTable& d) : MockSensorManager(d) {}
    std::shared_ptr<NsmDevice>
        getNsmDeviceFromStaticUUID(uuid_t /*uuid*/) override
    {
        return nullptr;
    }
};

class SensorManagerTest
{
    static void allocMessage(const Response& response,
                             std::shared_ptr<const nsm_msg>& responseMsg,
                             size_t& responseLen)
    {
        responseLen = response.size();
        if (responseLen > 0)
        {
            responseMsg = std::shared_ptr<const nsm_msg>(
                reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
                [](const nsm_msg* ptr) { free((void*)ptr); });
            memcpy((uint8_t*)responseMsg.get(), response.data(), responseLen);
        }
    }
    Response joinResponse(const Response& header, const Response& data)
    {
        Response response;
        response.insert(response.end(), header.begin(), header.end());
        response.insert(response.end(), data.begin(), data.end());
        return response;
    }

  protected:
    // Default asio plumbing so MockSensorManager::getObjServer() has a
    // valid return reference for fixtures that don't override it.
    // Producers that construct sensors needing Sensor.Type publication
    // (NsmNumericSensorDbusValue, NsmNumericSensorComposite, ...) call
    // SensorManager::getInstance().getObjServer() unconditionally, so
    // every SensorManagerTest-derived fixture needs a default.
    // Declared BEFORE mockManager so they outlive it during destruction:
    // ~SensorManager destroys the objects map whose NsmObjects hold
    // sdbusplus interfaces backed by this io_context; if io_context died
    // first, the interface destructors would touch freed asio descriptors
    // (heap-use-after-free under ASan).
    boost::asio::io_context defaultIoCtx;
    std::shared_ptr<sdbusplus::asio::connection> defaultSystemBus;
    std::shared_ptr<sdbusplus::asio::object_server> defaultObjServer;
    Response lastResponse;
    NiceMock<MockSensorManager> mockManager;
    template <typename DataType>
    DataType& data(size_t lastResponseOffset)
    {
        EXPECT_GE(lastResponse.size(), lastResponseOffset + sizeof(DataType));
        return *reinterpret_cast<DataType*>(lastResponse.data() +
                                            lastResponseOffset);
    }
    auto mockSensorIO(const Response& response,
                      nsm_completion_codes code = NSM_SUCCESS)
    {
        lastResponse = response;
        return [response, code](
                   eid_t, Request&, std::shared_ptr<const nsm_msg>& responseMsg,
                   size_t& responseLen, bool) -> requester::Coroutine {
            allocMessage(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return code;
        };
    }
    auto mockSensorIO(const Response& header, const Response& data,
                      nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockSensorIO(joinResponse(header, data), code);
    }
    auto mockSensorIO(nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockSensorIO(Response(), code);
    }

    auto mockPostPatchIO(const Response& response,
                         nsm_completion_codes code = NSM_SUCCESS)
    {
        lastResponse = response;
        return [response, code](eid_t, Request&,
                                std::shared_ptr<const nsm_msg>& responseMsg,
                                size_t& responseLen) -> requester::Coroutine {
            allocMessage(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return code;
        };
    }
    auto mockPostPatchIO(const Response& header, const Response& data,
                         nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockPostPatchIO(joinResponse(header, data), code);
    }
    auto mockPostPatchIO(nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockPostPatchIO(Response(), code);
    }

    // Helper method to clean up sensors from devices
    // Call this from derived class destructor before devices is destroyed
    static void cleanupDeviceSensors(NsmDeviceTable& devices)
    {
        // Clear sensors before devices is destroyed
        // This prevents use-after-free and ensures test isolation
        for (auto& device : devices)
        {
            if (device)
            {
                device->deviceSensors.clear();
                device->staticSensors.clear();
                device->longRunningSensors.clear();
                device->prioritySensors.clear();
                device->roundRobinSensors.clear();
                device->dumpCollectionSensors.clear();
                device->gpmSensors.clear();
                device->capabilityRefreshSensors.clear();
                device->standByToDcRefreshSensors.clear();
                device->deviceEvents.clear();
                device->setSensors.clear();
                device->sensorAggregators.clear();
                device->gpuDriverSensor.reset();
                device->msgTypesSensor.reset();
                device->eventDispatcher.eventsMap.clear();
                // Detach coroutine tasks to break circular references:
                // deviceTask captures shared_ptr<NsmDevice> in its frame.
                device->task.detach();
                device->longRunningTask.detach();
                // Clear shared_ptr to avoid circular references
                device->nsmMsgHandler.reset();
                device->objServer.reset();
            }
        }
        // Clear devices to ensure fresh state for each test
        devices.clear();
        // Clear AsyncOperationManager dispatchers to release sensor references.
        // Dispatchers hold shared_ptr<NsmObject> via AsyncSetOperationInfo,
        // which prevents sensor/D-Bus interface destruction between tests.
        // Access via #define private public in this header.
        AsyncOperationManager::getInstance()->dispatchers.clear();
        AsyncOperationManager::getInstance()->statusInterfaces.clear();
        AsyncOperationManager::getInstance()->valueInterfaces.clear();
        AsyncOperationManager::getInstance()->objectPathTimers.clear();
    }

    SensorManagerTest() = delete;
    SensorManagerTest(NsmDeviceTable& devices) :
        defaultSystemBus(
            std::make_shared<sdbusplus::asio::connection>(defaultIoCtx)),
        defaultObjServer(
            std::make_shared<sdbusplus::asio::object_server>(defaultSystemBus)),
        mockManager(devices)
    {
        sensorManagerInstance.reset(&mockManager);
        // Provide a real object_server reference by default; derived
        // fixtures may override with their own ON_CALL.
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(::testing::ReturnRef(*defaultObjServer));
    }
    virtual ~SensorManagerTest()
    {
        // Drop SensorManager-owned shared_ptrs so LSan doesn't flag them.
        mockManager.processorModuleToDeviceMap.clear();
        mockManager.deviceToPortMap.clear();
        mockManager.defaultPowerCapList.clear();
        mockManager.maxPowerCapList.clear();
        mockManager.minPowerCapList.clear();
        mockManager.debugTokenList.clear();
        sensorManagerInstance.release();
    }
};
