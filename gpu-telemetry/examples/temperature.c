/**
 * @brief Example of temperature monitoring using GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpu-telemetry/client.h"
#include "libnsm/base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

static volatile sig_atomic_t running = 1;

// Signal handler for clean shutdown
static void signal_handler(int signum) {
    (void)signum;  // Unused parameter
    running = 0;
}

// Callback for temperature responses
static void temperature_callback(void* user_data, 
                               const uint8_t* response, 
                               size_t response_len) {
    time_t* last_time = (time_t*)user_data;
    time_t current = time(NULL);
    
    // Parse NSM response
    if (response_len >= sizeof(nsm_msg)) {
        const nsm_msg* msg = (const nsm_msg*)response;
        
        // Check completion code
        if (msg->header[0] == NSM_SUCCESS) {
            // Get data size from header
            uint16_t data_size;
            memcpy(&data_size, &msg->header[1], sizeof(data_size));
            
            // Parse temperature if size matches
            if (data_size == sizeof(float)) {
                float temperature;
                memcpy(&temperature, msg->payload, sizeof(temperature));
                
                // Format timestamp
                char timestamp[32];
                struct tm* tm_info = localtime(&current);
                strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
                
                // Print temperature with timestamp
                printf("[%s] GPU Temperature: %.1f°C\n", 
                       timestamp, temperature);
            }
        }
        else {
            // Get reason code from header
            uint16_t reason;
            memcpy(&reason, &msg->header[3], sizeof(reason));
            fprintf(stderr, "Error response: cc=0x%02x reason=0x%04x\n",
                    msg->header[0], reason);
        }
    }
    
    *last_time = current;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int interval_ms = 1000;  // Default 1 second interval
    int opt;
    
    while ((opt = getopt(argc, argv, "i:h")) != -1) {
        switch (opt) {
            case 'i':
                interval_ms = atoi(optarg);
                if (interval_ms < 100) {
                    fprintf(stderr, "Interval must be >= 100ms\n");
                    return 1;
                }
                break;
                
            case 'h':
            default:
                printf("Usage: %s [-i interval_ms]\n", argv[0]);
                printf("Options:\n");
                printf("  -i <ms>    Polling interval in milliseconds (default: 1000)\n");
                printf("  -h         Show this help message\n");
                return opt == 'h' ? 0 : 1;
        }
    }

    // Initialize client
    gpu_telemetry_ctx* ctx;
    if (gpu_telemetry_init(&ctx) < 0) {
        fprintf(stderr, "Failed to initialize client\n");
        return 1;
    }

    // Set up signal handling
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create temperature request message
    nsm_msg request;
    if (encode_common_req(0x01, NSM_TYPE_TEMPERATURE,
                         NSM_GET_TEMPERATURE_READING, &request) < 0) {
        fprintf(stderr, "Failed to create request\n");
        gpu_telemetry_free(ctx);
        return 1;
    }

    printf("Monitoring GPU temperature (Ctrl+C to exit)...\n");
    
    // Main polling loop
    time_t last_time = 0;
    struct timespec sleep_time = {
        .tv_sec = interval_ms / 1000,
        .tv_nsec = (interval_ms % 1000) * 1000000
    };

    while (running) {
        // Send temperature request
        if (gpu_telemetry_send_message(ctx,
                                     (const uint8_t*)&request,
                                     sizeof(request),
                                     temperature_callback,
                                     &last_time) < 0) {
            fprintf(stderr, "Failed to send request\n");
            break;
        }

        // Sleep for interval
        nanosleep(&sleep_time, NULL);
    }

    printf("\nShutting down...\n");
    gpu_telemetry_free(ctx);
    return 0;
}

#ifdef UNIT_TESTING
#include <gtest/gtest.h>

// Mock response data for testing
static uint8_t mock_response[sizeof(nsm_msg)];
static size_t mock_response_len;
static float mock_temperature = 45.5f;

// Test fixture
class TemperatureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock success response
        nsm_msg* msg = (nsm_msg*)mock_response;
        msg->header[0] = NSM_SUCCESS;
        uint16_t data_size = sizeof(float);
        memcpy(&msg->header[1], &data_size, sizeof(data_size));
        memcpy(msg->payload, &mock_temperature, sizeof(float));
        mock_response_len = sizeof(nsm_msg);
    }
};

// Test temperature callback
TEST_F(TemperatureTest, CallbackHandling) {
    time_t last_time = 0;
    temperature_callback(&last_time, mock_response, mock_response_len);
    EXPECT_GT(last_time, 0);
}

// Test error handling
TEST_F(TemperatureTest, ErrorHandling) {
    // Create error response
    nsm_msg* msg = (nsm_msg*)mock_response;
    msg->header[0] = 0xFF;  // Error code
    uint16_t reason = 0x1234;
    memcpy(&msg->header[3], &reason, sizeof(reason));
    
    time_t last_time = 0;
    testing::internal::CaptureStderr();
    temperature_callback(&last_time, mock_response, mock_response_len);
    std::string error_output = testing::internal::GetCapturedStderr();
    
    EXPECT_THAT(error_output, 
                testing::HasSubstr("Error response: cc=0xff reason=0x1234"));
}

#endif // UNIT_TESTING
