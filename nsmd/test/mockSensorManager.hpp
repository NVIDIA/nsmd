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
#include "sensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace nsm;

struct MockSensorManager : public SensorManager
{
    MockSensorManager() = delete;
    MockSensorManager(NsmDeviceTable& nsmDevices) : SensorManager(nsmDevices, 0)
    {}
    MOCK_METHOD(requester::Coroutine, SendRecvNsmMsg,
                (eid_t eid, Request& request, const nsm_msg** responseMsg,
                 size_t* responseLen),
                (override));
    MOCK_METHOD(uint8_t, SendRecvNsmMsgSync,
                (eid_t eid, Request& request, const nsm_msg** responseMsg,
                 size_t* responseLen),
                (override));
    MOCK_METHOD(eid_t, getEid, (std::shared_ptr<NsmDevice> nsmDevice),
                (override));
    MOCK_METHOD(void, startPolling, (uuid_t uuid), (override));
    MOCK_METHOD(void, stopPolling, (uuid_t uuid), (override));
};