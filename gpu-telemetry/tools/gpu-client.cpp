/**
 * @brief GPU telemetry client command line tool
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpu-telemetry/client.h"
#include "libnsm/base.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <chrono>
#include <thread>

namespace {
    volatile sig_atomic_t running = 1;

    void signal_handler(int) {
        running = 0;
    }

    void print_usage(const char* program) {
        std::cerr << "Usage: " << program << " [options] <hex-message>\n"
                  << "Options:\n"
                  << "  -s <socket>     Socket path (default: /tmp/gpu-telemetry.sock)\n"
                  << "  -i <interval>   Repeat interval in ms (default: 0 = once)\n"
                  << "  -t              Temperature request mode\n"
                  << "  -x              Print responses in hex\n"
                  << "  -h              Show this help message\n"
                  << "\nExamples:\n"
                  << "  " << program << " -t                    # Get temperature\n"
                  << "  " << program << " -t -i 1000           # Monitor temperature\n"
                  << "  " << program << " -x 0102030405        # Send custom message\n";
    }

    // Callback to handle responses
    void message_callback(void* user_data, 
                         const uint8_t* response, 
                         size_t response_len) {
        bool hex_mode = *static_cast<bool*>(user_data);

        if (hex_mode) {
            // Print raw hex response
            std::cout << "Response: ";
            for (size_t i = 0; i < response_len; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(response[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }
        else {
            // Try to parse as NSM response
            const nsm_msg* msg = reinterpret_cast<const nsm_msg*>(response);
            if (response_len >= sizeof(nsm_msg)) {
                uint8_t cc = msg->header[0];
                uint16_t data_size = *reinterpret_cast<const uint16_t*>(&msg->header[1]);
                uint16_t reason = *reinterpret_cast<const uint16_t*>(&msg->header[3]);

                if (cc == NSM_SUCCESS && data_size == sizeof(float)) {
                    // Parse temperature response
                    float temp = *reinterpret_cast<const float*>(msg->payload);
                    std::cout << "Temperature: " << temp << "°C" << std::endl;
                }
                else {
                    std::cout << "NSM Response: cc=0x" << std::hex << (int)cc
                             << " reason=0x" << reason << std::dec << std::endl;
                }
            }
        }
    }

    // Convert hex string to bytes
    std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            bytes.push_back(std::stoul(hex.substr(i, 2), nullptr, 16));
        }
        return bytes;
    }
}

int main(int argc, char* argv[]) {
    std::string socket_path = "/tmp/gpu-telemetry.sock";
    int interval_ms = 0;
    bool temp_mode = false;
    bool hex_mode = false;

    // Parse command line options
    int opt;
    while ((opt = getopt(argc, argv, "s:i:txh")) != -1) {
        switch (opt) {
            case 's':
                socket_path = optarg;
                break;
            case 'i':
                interval_ms = std::atoi(optarg);
                break;
            case 't':
                temp_mode = true;
                break;
            case 'x':
                hex_mode = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check arguments
    if (!temp_mode && optind >= argc) {
        std::cerr << "Error: Message required in hex mode\n";
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Initialize client
        gpu_telemetry_ctx* ctx;
        if (gpu_telemetry_init(&ctx) < 0) {
            throw std::runtime_error("Failed to initialize client");
        }

        // Set up signal handling
        struct sigaction sa = {};
        sa.sa_handler = signal_handler;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        // Prepare message
        std::vector<uint8_t> message;
        if (temp_mode) {
            // Create temperature request
            nsm_msg req = {};
            encode_common_req(0x01, NSM_TYPE_TEMPERATURE,
                            NSM_GET_TEMPERATURE_READING, &req);
            message.assign(reinterpret_cast<uint8_t*>(&req),
                         reinterpret_cast<uint8_t*>(&req) + sizeof(req));
        }
        else {
            // Parse hex message
            message = hex_to_bytes(argv[optind]);
        }

        // Send message(s)
        do {
            if (gpu_telemetry_send_message(ctx, message.data(), message.size(),
                                         message_callback, &hex_mode) < 0) {
                throw std::runtime_error("Failed to send message");
            }

            if (interval_ms > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(interval_ms));
            }
        } while (running && interval_ms > 0);

        gpu_telemetry_free(ctx);
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
