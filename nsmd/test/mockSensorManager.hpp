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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "requester/handler.hpp"
#include "sensorManager.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#undef private
#undef protected

#include "commonMock.hpp"
using namespace nsm;
using ::testing::NiceMock;

class MockMctpDiscovery
{
  public:
    static MockMctpDiscovery& getInstance(NsmDeviceTable& nsmDevices)
    {
        if (!instance)
        {
            instance = new MockMctpDiscovery(nsmDevices);
        }
        return *instance;
    }

  private:
    MockMctpDiscovery() = delete;
    ~MockMctpDiscovery() = default;
    MockMctpDiscovery(const MockMctpDiscovery&) = delete;
    MockMctpDiscovery& operator=(const MockMctpDiscovery&) = delete;
    MockMctpDiscovery(MockMctpDiscovery&&) = delete;
    MockMctpDiscovery& operator=(MockMctpDiscovery&&) = delete;
    MockMctpDiscovery(NsmDeviceTable& nsmDevices) : nsmDevices(nsmDevices) {};
    std::map<uint8_t, std::map<uint16_t, std::shared_ptr<NsmDevice>>> deviceMap;
    NsmDeviceTable& nsmDevices;

  public:
    static MockMctpDiscovery* instance;
    std::shared_ptr<NsmDevice>
        findOrCreateNsmDevice(uint8_t deviceType, uint8_t deviceRole,
                              uint8_t instanceNumber, std::string remapPropName,
                              std::vector<std::string>& remapPropValue)
    {
        uint16_t staticInstanceAndRole = (deviceRole >> 8) | instanceNumber;

        if (deviceMap.find(deviceType) != deviceMap.end())
        {
            if (deviceMap[deviceType].find(staticInstanceAndRole) !=
                deviceMap[deviceType].end())
            {
                return deviceMap[deviceType][staticInstanceAndRole];
            }
        }

        auto nsmDevice = std::make_shared<MockNsmDeviceBase>(
            deviceType, instanceNumber, remapPropName, remapPropValue[0],
            deviceRole);
        lg2::info(
            "Creating new NsmDevice for deviceType:{TYPE} instanceNumber:{INST} deviceRole:{ROLE} remapPropName:{REMAPPNAME} remapPropValue:{REMAPPVALUE}",
            "TYPE", deviceType, "INST", instanceNumber, "ROLE", deviceRole,
            "REMAPPNAME", remapPropName, "REMAPPVALUE", remapPropValue[0]);
        nsmDevices.emplace_back(nsmDevice);
        deviceMap[deviceType][staticInstanceAndRole] = nsmDevice;
        return deviceMap[deviceType][staticInstanceAndRole];
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
        std::shared_ptr<NsmDevice> ret{};
        uint16_t staticInstanceAndRole = (deviceRole << 8) | instanceNumber;
        if (deviceMap.find(deviceType) != deviceMap.end())
        {
            if (deviceMap[deviceType].find(staticInstanceAndRole) !=
                deviceMap[deviceType].end())
            {
                ret = deviceMap[deviceType][staticInstanceAndRole];
            }
        }
        return ret;
    }
};

MockMctpDiscovery* MockMctpDiscovery::instance = nullptr;

struct MockSensorManager : public SensorManager
{
    MockSensorManager() = delete;
    MockSensorManager(NsmDeviceTable& nsmDevices) : SensorManager(nsmDevices, 0)
    {
        // Ignore nsmDevices parameter for mock
    }
    /*  MOCK_METHOD(requester::Coroutine, SendRecvNsmMsg,
                  (eid_t eid, Request& request,
                   std::shared_ptr<const nsm_msg>& responseMsg,
                   size_t& responseLen, bool bypassCommandCheck),
                  (override));
      MOCK_METHOD(requester::Coroutine, postPatchNsmCommand,
                  (eid_t eid, Request& request,
                   std::shared_ptr<const nsm_msg>& responseMsg,
                   size_t& responseLen),
                  (override));*/
    MOCK_METHOD(eid_t, getEid, (std::shared_ptr<NsmDevice> nsmDevice),
                (override));
    MOCK_METHOD(sdbusplus::asio::object_server&, getObjServer, (), (override));

    std::shared_ptr<NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid) override
    {
        return MockMctpDiscovery::getInstance(nsmDevices)
            .getNsmDeviceFromStaticUUID(uuid);
    };
    std::shared_ptr<NsmDevice> getNsmDevice(uint8_t deviceType,
                                            uint8_t instanceNumber,
                                            uint8_t deviceRole) override
    {
        return MockMctpDiscovery::getInstance(nsmDevices)
            .getNsmDeviceByIdentification(deviceType, instanceNumber,
                                          deviceRole);
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
    NiceMock<MockSensorManager> mockManager;
    Response lastResponse;
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

    SensorManagerTest() = delete;
    SensorManagerTest(NsmDeviceTable& devices) : mockManager(devices)
    {
        SensorManager::instance.reset(&mockManager);
    }
    virtual ~SensorManagerTest()
    {
        SensorManager::instance.release();
    }
};
