/**
 * @brief GPU telemetry server executable
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server/server.hpp"
#include "server/mock_device.hpp"
#include <stdexec/execution.hpp>
#include <iostream>
#include <csignal>
#include <cstdlib>

using namespace gpu::telemetry;

namespace {
    volatile sig_atomic_t running = 1;

    void signal_handler(int) {
        running = 0;
    }

    void print_usage(const char* program) {
        std::cerr << "Usage: " << program << " [options] <socket-path>\n"
                  << "Options:\n"
                  << "  -m <clients>    Maximum number of clients (default: 10)\n"
                  << "  -p <perms>      Socket permissions in octal (default: 666)\n"
                  << "  -d <delay_ms>   Mock device response delay (default: 0)\n"
                  << "  -t <min:max>    Temperature range in Celsius (default: 30:80)\n"
                  << "  -h              Show this help message\n";
    }
}

int main(int argc, char* argv[]) {
    // Parse command line options
    ServerConfig serverConfig;
    MockDeviceConfig deviceConfig;
    
    int opt;
    while ((opt = getopt(argc, argv, "m:p:d:t:h")) != -1) {
        switch (opt) {
            case 'm':
                serverConfig.maxClients = std::atoi(optarg);
                break;
            case 'p':
                serverConfig.socketPerms = std::stoul(optarg, nullptr, 8);
                break;
            case 'd':
                deviceConfig.responseDelay = 
                    std::chrono::milliseconds(std::atoi(optarg));
                break;
            case 't': {
                char* colon = strchr(optarg, ':');
                if (colon) {
                    *colon = '\0';
                    deviceConfig.temperature.min = std::atof(optarg);
                    deviceConfig.temperature.max = std::atof(colon + 1);
                }
                break;
            }
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check for socket path argument
    if (optind >= argc) {
        std::cerr << "Error: Socket path required\n";
        print_usage(argv[0]);
        return 1;
    }
    serverConfig.socketPath = argv[optind];

    try {
        // Create mock device
        auto device = std::make_unique<MockDevice>(deviceConfig);

        // Create and start server
        Server server(std::move(device), serverConfig);

        // Set up signal handling
        struct sigaction sa = {};
        sa.sa_handler = signal_handler;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        // Start server
        stdexec::sync_wait(server.start());
        std::cout << "Server started on " << serverConfig.socketPath << std::endl;

        // Run until signaled
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop server
        stdexec::sync_wait(server.stop());
        std::cout << "\nServer stopped" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
