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

#include "common/types.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/timer.hpp>

#include <memory>

namespace nsm
{
using RequesterHandler = requester::Handler<requester::Request>;

/**
 * @brief SensorManager
 *
 * This class manages the NSM sensors defined by EM configuration PDI.
 * SensorManager register callback function to create sensor when there is new
 * NSM device inventory added to D-Bus and provides function calls for other
 * classes to start/stop sensor monitoring.
 *
 */
class SensorManager
{
  public:
    // Delete copy constructor and copy assignment operator
    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    // Static method to access the instance of the class
    static SensorManager& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "SensorManager instance not initialized yet");
        }
        return *instance;
    }

    // Static method to initialize the instance
    static void initialize(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid,
        mctp_socket::Manager& sockManager)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized SensorManager");
        }
        static SensorManager inst(bus, event, handler, instanceIdDb, objServer,
                                  eidTable, nsmDevices, localEid, sockManager);
        instance = &inst;
    }

    // Regular methods as before
    void startPolling();
    void stopPolling();
    void doPolling(std::shared_ptr<NsmDevice> nsmDevice);
    void interfaceAddedhandler(sdbusplus::message::message& msg);
    void _startPolling(sdeventplus::source::EventBase& /* source */);
    requester::Coroutine doPollingTask(std::shared_ptr<NsmDevice> nsmDevice);

    /** @brief Send request NSM message to eid. The function will
     *         return when received the response message from NSM device.
     *
     *  @param[in] eid - eid
     *  @param[in] request - request NSM message
     *  @param[out] responseMsg - response NSM message
     *  @param[out] responseLen - length of response NSM message
     *  @return coroutine return_value - NSM completion code
     */
    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);

    /** @brief Send request NSM message to eid by blocking socket API directly.
     *         The function will return when received the response message from
     *         NSM device. Unlike SendRecvNsmMsg, there is no retry of sending
     *         request.
     *
     *  @param[in] eid - eid
     *  @param[in] request - request NSM message
     *  @param[out] responseMsg - response NSM message
     *  @param[out] responseLen - length of response NSM message
     *  @return return_value - nsm_requester_error_codes
     */
    uint8_t SendRecvNsmMsgSync(eid_t eid, Request& request,
                               const nsm_msg** responseMsg,
                               size_t* responseLen);
    void scanInventory();

    requester::Coroutine pollEvents(eid_t eid);
    eid_t getLocalEid()
    {
        return localEid;
    }
    eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice);
    std::shared_ptr<NsmDevice> getNsmDevice(uuid_t uuid);

  private:
    // Private constructor
    SensorManager(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid,
        mctp_socket::Manager& sockManager);

    // Instance variables as before
    static SensorManager* instance;
    sdbusplus::bus::bus& bus;
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;

    std::unique_ptr<sdbusplus::bus::match_t> inventoryAddedSignal;
    std::unique_ptr<sdeventplus::source::Defer> deferScanInventory;
    std::unique_ptr<sdeventplus::source::Defer> newSensorEvent;

    NsmDeviceTable& nsmDevices;
    eid_t localEid;

    mctp_socket::Manager& sockManager;
};
} // namespace nsm
