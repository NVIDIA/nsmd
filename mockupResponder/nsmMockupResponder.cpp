#include "mockupResponder.hpp"

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
    std::cerr << "Usage: nsmMockupResponder [options]\n";
    std::cerr << "Options:\n";
    std::cerr << " [--verbose] - would enable verbosity\n"
              << " [--eid <EID>] - assign EID to mockup responder\n";
}

int main(int argc, char** argv)
{
    bool verbose = false;
    int eid = 0;
    int argflag;
    static struct option long_options[] = {{"verbose", no_argument, 0, 'v'},
                                           {"eid", required_argument, 0, 'e'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    while ((argflag = getopt_long(argc, argv, "hve:", long_options, nullptr)) >=
           0)
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
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (verbose)
    {
        lg2::info("start a Mockup Responder EID={EID}", "EID", eid);
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
        bus.request_name("xyz.openbmc_project.NSM.MockupResponder");

        MockupResponder::MockupResponder mockupResponder(verbose, event);
        mockupResponder.connectMockupEID(eid);

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
