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
#include "sensorManager.hpp"

using namespace nsm;
using ::testing::NiceMock;

struct MockSensorManager : public SensorManager
{
    MockSensorManager() = delete;
    MockSensorManager(NsmDeviceTable& nsmDevices) : SensorManager(nsmDevices, 0)
    {}
    MOCK_METHOD(requester::Coroutine, SendRecvNsmMsg,
                (eid_t eid, Request& request,
                 std::shared_ptr<const nsm_msg>& responseMsg,
                 size_t& responseLen, bool isLongRunning),
                (override));
    MOCK_METHOD(eid_t, getEid, (std::shared_ptr<NsmDevice> nsmDevice),
                (override));
    MOCK_METHOD(void, startPolling, (uuid_t uuid), (override));
    MOCK_METHOD(void, stopPolling, (uuid_t uuid), (override));
    MOCK_METHOD(sdbusplus::asio::object_server&, getObjServer, (), (override));
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
    auto mockSendRecvNsmMsg(const Response& response,
                            nsm_completion_codes code = NSM_SUCCESS)
    {
        lastResponse = response;
        return
            [response, code](
                eid_t, Request&, std::shared_ptr<const nsm_msg>& responseMsg,
                size_t& responseLen,
                [[maybe_unused]] bool isLongRunning) -> requester::Coroutine {
            allocMessage(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return code;
        };
    }
    auto mockSendRecvNsmMsg(const Response& header, const Response& data,
                            nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockSendRecvNsmMsg(joinResponse(header, data), code);
    }
    auto mockSendRecvNsmMsg(nsm_completion_codes code = NSM_SUCCESS)
    {
        return mockSendRecvNsmMsg(Response(), code);
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
