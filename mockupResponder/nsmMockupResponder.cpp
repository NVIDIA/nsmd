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

#include "mockupResponder.hpp"

#include <err.h>
#include <getopt.h>

#include <boost/algorithm/string.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdeventplus/event.hpp>

#include <iostream>

using namespace phosphor::logging;

void optionUsage(void)
{
    std::cerr << "Usage: nsmMockupResponder [options]\n";
    std::cerr << "Options:\n";
    std::cerr
        << " [--verbose] - would enable verbosity\n"
        << " [--eid <EID>] - assign EID to mockup responder\n"
        << " [--instanceId <InstanceID>] - assign instanceId to mockup responder [default - 0]\n"
        << " [--device <DeviceType>] - assign DeviceType to mockup responder [GPU, Switch, PCIeBridge and Baseboard]\n";
}

int main(int argc, char** argv)
{
    bool verbose = false;
    int eid = 0;
    std::string device = "";
    int deviceType = 0;
    int instanceId = 0;
    int argflag;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"eid", required_argument, 0, 'e'},
        {"device", required_argument, 0, 'd'},
        {"instanceId", required_argument, 0, 'i'},
        {0, 0, 0, 0}};

    while ((argflag = getopt_long(argc, argv, "hve:d:i:", long_options,
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
                eid = std::stoi(optarg);
                if (eid < 0 || eid > 255)
                {
                    optionUsage();
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                device = optarg;
                if (boost::iequals(device, "GPU"))
                {
                    deviceType = NSM_DEV_ID_GPU;
                }
                else if (boost::iequals(device, "Switch"))
                {
                    deviceType = NSM_DEV_ID_SWITCH;
                }
                else if (boost::iequals(device, "PCIeBridge"))
                {
                    deviceType = NSM_DEV_ID_PCIE_BRIDGE;
                }
                else if (boost::iequals(device, "Baseboard"))
                {
                    deviceType = NSM_DEV_ID_BASEBOARD;
                }
                else if (boost::iequals(device, "EROT"))
                {
                    deviceType = NSM_DEV_ID_EROT;
                }
                else
                {
                    optionUsage();
                    exit(EXIT_FAILURE);
                }
                break;
            case 'i':
                instanceId = std::stoi(optarg);
                if (instanceId < 0 || instanceId > 255)
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
        lg2::info(
            "start a Mockup Responder EID={EID} DeviceType={DT} ({DID}) InstanceID={IID}",
            "EID", eid, "DT", device, "DID", deviceType, "IID", instanceId);
    }

    try
    {
        boost::asio::io_context io;
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        sdbusplus::asio::object_server objServer(systemBus);

        auto bus = sdbusplus::bus::new_default();
        auto event = sdeventplus::Event::get_default();
        bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
        sdbusplus::server::manager::manager objManager(bus, "/");

        bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
        std::string serviceName = "xyz.openbmc_project.NSM.eid_" +
                                  std::to_string(eid);
        bus.request_name(serviceName.c_str());

        MockupResponder::MockupResponder mockupResponder(
            verbose, event, objServer, eid, deviceType, instanceId);
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
