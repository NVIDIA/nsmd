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
 * @brief Sensor manager abstraction class
 *
 */
class SensorManager
{
  public:
    SensorManager(NsmDeviceTable& nsmDevices, eid_t localEid) :
        nsmDevices(nsmDevices), localEid(localEid)
    {}
    virtual ~SensorManager() = default;

    /** @brief Send request NSM message to eid by blocking socket API directly.
     *         The function will return when received the response message from
     *         NSM device. Unlike SendRecvNsmMsg, there is no retry of sending
     *         request.
     *
     *  @param[in] eid endpoint ID
     *  @param[in] request request NSM message
     *  @param[out] responseMsg response NSM message
     *  @param[out] responseLen length of response NSM message
     *  @return return_value - nsm_requester_error_codes
     */
    virtual requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                                const nsm_msg** responseMsg,
                                                size_t* responseLen) = 0;

    /** @brief Send request NSM message to eid by blocking socket API directly.
     *         The function will return when received the response message from
     *         NSM device. Unlike SendRecvNsmMsg, there is no retry of sending
     *         request.
     *
     *  @param[in] eid endpoint ID
     *  @param[in] request request NSM message
     *  @param[out] responseMsg response NSM message
     *  @param[out] responseLen length of response NSM message
     *  @return return_value - nsm_requester_error_codes
     */
    virtual uint8_t SendRecvNsmMsgSync(eid_t eid, Request& request,
                                       const nsm_msg** responseMsg,
                                       size_t* responseLen) = 0;
    virtual eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice) = 0;
    eid_t getLocalEid()
    {
        return localEid;
    }
    std::shared_ptr<NsmDevice> getNsmDevice(uuid_t uuid);
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

  protected:
    static std::unique_ptr<SensorManager> instance;
    NsmDeviceTable& nsmDevices;
    const eid_t localEid;
};

/**
 * @brief SensorManagerImpl
 *
 * This class manages the NSM sensors defined by EM configuration PDI.
 * SensorManagerImpl register callback function to create sensor when there is
 * new NSM device inventory added to D-Bus and provides function calls for other
 * classes to start/stop sensor monitoring.
 *
 */
class SensorManagerImpl : public SensorManager
{
  public:
    // Delete copy constructor and copy assignment operator
    SensorManagerImpl(const SensorManagerImpl&) = delete;
    SensorManagerImpl& operator=(const SensorManagerImpl&) = delete;

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
        static SensorManagerImpl inst(bus, event, handler, instanceIdDb,
                                      objServer, eidTable, nsmDevices, localEid,
                                      sockManager);
        instance.reset(&inst);
    }

  private:
    // Regular methods as before
    void startPolling();
    void stopPolling();
    void doPolling(std::shared_ptr<NsmDevice> nsmDevice);
    void interfaceAddedhandler(sdbusplus::message::message& msg);
    void _startPolling(sdeventplus::source::EventBase& /* source */);
    requester::Coroutine doPollingTask(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen) override;
    uint8_t SendRecvNsmMsgSync(eid_t eid, Request& request,
                               const nsm_msg** responseMsg,
                               size_t* responseLen) override;
    void scanInventory();
    requester::Coroutine pollEvents(eid_t eid);
    eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice) override;
    // Private constructor
    SensorManagerImpl(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid,
        mctp_socket::Manager& sockManager);

    sdbusplus::bus::bus& bus;
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;

    std::unique_ptr<sdbusplus::bus::match_t> inventoryAddedSignal;
    std::unique_ptr<sdeventplus::source::Defer> deferScanInventory;
    std::unique_ptr<sdeventplus::source::Defer> newSensorEvent;

    mctp_socket::Manager& sockManager;
};
} // namespace nsm
