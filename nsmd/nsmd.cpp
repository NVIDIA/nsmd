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

#include "config.h"

#include "deviceManager.hpp"
#include "eventManager.hpp"
#include "eventType0Handler.hpp"
#include "eventType3Handler.hpp"
#include "instance_id.hpp"
#include "nsmDbusIfaceOverride/nsmLogDumpOnDemand.hpp"
#include "nsmDevice.hpp"
#include "nsmServiceReadyInterface.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"

#include <err.h>
#include <getopt.h>

#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdeventplus/event.hpp>
#include <tal.hpp>

#include <iostream>

using namespace phosphor::logging;

void optionUsage(void)
{
    std::cerr << "Usage: nsmd [options]\n";
    std::cerr << "Options:\n";
    std::cerr << " [--verbose] - would enable verbosity\n";
    std::cerr << " [--eid <EID>] - local EID\n";
}

int main(int argc, char** argv)
{
    bool verbose = false;
    int argflag;
    int localEid = LOCAL_EID;
    static struct option long_options[] = {{"verbose", no_argument, 0, 'v'},
                                           {"help", no_argument, 0, 'h'},
                                           {"eid", required_argument, 0, 'e'},
                                           {0, 0, 0, 0}};

    while ((argflag = getopt_long(argc, argv, "hvre:", long_options,
                                  nullptr)) >= 0)
    {
        switch (argflag)
        {
            case 'h':
                optionUsage();
                exit(EXIT_FAILURE);
                break;
            case 'v':
                verbose = true;
                break;
            case 'e':
                localEid = std::stoi(optarg);
                if (localEid < 0 || localEid > 255)
                {
                    optionUsage();
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (verbose)
    {
        lg2::info("start nsmd\n");
    }

    try
    {
        boost::asio::io_context io;
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        sdbusplus::asio::object_server objServer(systemBus);

        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();
        bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
        sdbusplus::server::manager::manager rootObjManager(bus, "/");
        sdbusplus::server::manager::manager inventoryObjManager(
            bus, "/xyz/openbmc_project/inventory");

        bus.request_name("xyz.openbmc_project.NSM");
        nsm::InstanceIdDb instanceIdDb;
        mctp_socket::Manager sockManager;
        nsm::EventManager eventManager;
        // corresponding to a UUID there could be multiple occurance of same eid
        // with different medium types.
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>
            eidTable;

        requester::Handler<requester::Request> reqHandler(event, instanceIdDb,
                                                          sockManager, verbose);
        mctp_socket::Handler sockHandler(event, reqHandler, eventManager,
                                         sockManager, verbose);

        nsm::NsmDeviceTable nsmDevices;

        // Initialize the singleton instance
        nsm::NsmServiceReadyIntf::initialize(bus, "/xyz/openbmc_project/NSM",
                                             nsmDevices);

        // Initialize the singleton instance for on demand logging of critical
        // logs
        nsm::NsmLogDumpTracker::initialize(bus, "/xyz/openbmc_project/NSM");

        // Initialize the DeviceManager before getting its instance
        nsm::DeviceManager::initialize(event, reqHandler, instanceIdDb,
                                       objServer, eidTable, nsmDevices);
        nsm::DeviceManager& deviceManager = nsm::DeviceManager::getInstance();
        std::unique_ptr<mctp::MctpDiscovery> mctpDiscoveryHandler =
            std::make_unique<mctp::MctpDiscovery>(
                bus, sockHandler,
                std::initializer_list<mctp::MctpDiscoveryHandlerIntf*>{
                    &deviceManager});

        // Initialize the SensorManager before getting its instance
        nsm::SensorManagerImpl::initialize(bus, event, reqHandler, instanceIdDb,
                                           objServer, eidTable, nsmDevices,
                                           localEid, sockManager, verbose);

        auto eventType0Handler = std::make_unique<nsm::EventType0Handler>();
        eventManager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                     std::move(eventType0Handler));

        auto eventType3Handler = std::make_unique<nsm::EventType3Handler>();
        eventManager.registerHandler(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                     std::move(eventType3Handler));

#ifdef NVIDIA_SHMEM
        // Initialize TAL
        if (tal::TelemetryAggregator::namespaceInit(tal::ProcessType::Producer,
                                                    "nsmd"))
        {
            lg2::info("Initialized tal from nsmd.");
        }
#endif

        return event.loop();
    }
    catch (const std::exception& e)
    {
        lg2::error("Exception: {HANDLER_EXCEPTION}", "HANDLER_EXCEPTION",
                   e.what());
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
