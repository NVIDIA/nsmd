#include "deviceManager.hpp"
#include "instance_id.hpp"
#include "invoker.hpp"
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

#include <iostream>

using namespace phosphor::logging;

void optionUsage(void)
{
    std::cerr << "Usage: nsmd [options]\n";
    std::cerr << "Options:\n";
    std::cerr << " [--verbose] - would enable verbosity\n";
}

int main(int argc, char** argv)
{
    bool verbose = false;
    int argflag;
    static struct option long_options[] = {{"verbose", no_argument, 0, 'v'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    while ((argflag =
                getopt_long(argc, argv, "hvre:", long_options, nullptr)) >= 0)
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
        sdbusplus::server::manager::manager objManager(bus, "/");

        bus.request_name("xyz.openbmc_project.NSM");
        nsm::InstanceIdDb instanceIdDb;
        mctp_socket::Manager sockManager;
        responder::Invoker invoker{};
        requester::Handler<requester::Request> reqHandler(event, instanceIdDb,
                                                          sockManager, verbose);
        mctp_socket::Handler sockHandler(event, reqHandler, invoker,
                                         sockManager, verbose);
        std::unique_ptr<nsm::DeviceManager> deviceManager =
            std::make_unique<nsm::DeviceManager>(event, reqHandler,
                                                 instanceIdDb, objServer);
        std::unique_ptr<mctp::MctpDiscovery> mctpDiscoveryHandler =
            std::make_unique<mctp::MctpDiscovery>(
                bus, sockHandler,
                std::initializer_list<mctp::MctpDiscoveryHandlerIntf*>{
                    deviceManager.get()});

        std::unique_ptr<nsm::SensorManager> sensorManager =
            std::make_unique<nsm::SensorManager>(bus, event, reqHandler,
                                                 instanceIdDb, objServer);

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
