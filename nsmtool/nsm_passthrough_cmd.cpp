/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsm_passthrough_cmd.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <sdbusplus/bus.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace nsmtool
{
namespace passthrough
{

std::vector<std::unique_ptr<SendNSMCommand>> sendNSMCommandObjects;
std::vector<std::unique_ptr<GetCommandStatus>> getCommandStatusObjects;
std::vector<std::unique_ptr<WaitCommandStatusComplete>>
    waitCommandStatusCompleteObjects;
std::vector<std::unique_ptr<GetNSMResponse>> getNSMResponseObjects;
SendNSMCommand::SendNSMCommand(CLI::App* app)
{
    std::string targetType;
    int targetInstanceId; // Use int here to avoid issues with small integer
                          // types
    int messageType;      // Use int and cast later
    int commandCode;      // Use int and cast later
    std::string filePath;

    // Define options using int types for the problematic fields
    auto subcmd = app->add_subcommand("sendNSMCommand",
                                      "Send NSM Passthrough Command");
    subcmd
        ->add_option("-t,--targetType", targetType,
                     "Target Type (e.g., GPU, Switch, Baseboard)")
        ->required();
    subcmd
        ->add_option("-i,--targetInstanceId", targetInstanceId,
                     "Target Instance ID")
        ->required();
    subcmd->add_option("-m,--messageType", messageType, "Message Type")
        ->required();
    subcmd->add_option("-c,--commandCode", commandCode, "Command Code")
        ->required();
    subcmd->add_option("-f,--filePath", filePath, "File path for data")
        ->required();

    // In the callback, cast the integers to uint8_t when necessary
    subcmd->callback([this, &targetType, &targetInstanceId, &messageType,
                      &commandCode, &filePath]() {
        uint8_t castedTargetInstanceId = static_cast<uint8_t>(targetInstanceId);
        uint8_t castedMessageType = static_cast<uint8_t>(messageType);
        uint8_t castedCommandCode = static_cast<uint8_t>(commandCode);

        this->execute(targetType, castedTargetInstanceId, castedMessageType,
                      castedCommandCode, filePath);
    });
}

void SendNSMCommand::getMatchingFruDeviceObjectPath(
    const std::string& targetType, uint8_t targetInstanceId,
    std::function<void(const std::string&)> callback)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    std::map<std::string, std::map<std::string, std::vector<std::string>>>
        subtree;

    auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                      "/xyz/openbmc_project/object_mapper",
                                      "xyz.openbmc_project.ObjectMapper",
                                      "GetSubTree");
    method.append("/xyz/openbmc_project/FruDevice/", 1,
                  std::vector<std::string>({}));

    try
    {
        auto reply = bus.call(method);
        reply.read(subtree);

        for (const auto& [objectPath, interfaces] : subtree)
        {
            getDeviceType(
                objectPath, targetType, targetInstanceId,
                [this, callback](const std::string& matchedObjectPath) {
                if (!matchedObjectPath.empty())
                {
                    callback(matchedObjectPath); // Pass the matched object path
                    return; // Exit early to avoid checking other devices
                }
            });
        }

        callback(""); // No match found
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        callback(""); // Error encountered
    }
}

void SendNSMCommand::getDeviceType(
    const std::string& objectPath, const std::string& targetType,
    uint8_t targetInstanceId, std::function<void(const std::string&)> callback)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    auto deviceTypeMethod =
        bus.new_method_call("xyz.openbmc_project.NSM", objectPath.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
    deviceTypeMethod.append("xyz.openbmc_project.FruDevice", "DEVICE_TYPE");

    try
    {
        auto deviceReply = bus.call(deviceTypeMethod);
        std::variant<uint8_t> deviceType;
        deviceReply.read(deviceType);

        NSMDevID targetTypeId;
        if (targetType == "GPU")
            targetTypeId = NSM_DEV_ID_GPU;
        else if (targetType == "Switch")
            targetTypeId = NSM_DEV_ID_SWITCH;
        else if (targetType == "PCIeBridge")
            targetTypeId = NSM_DEV_ID_PCIE_BRIDGE;
        else if (targetType == "Baseboard")
            targetTypeId = NSM_DEV_ID_BASEBOARD;
        else
        {
            return;
        }

        if (std::get<uint8_t>(deviceType) == targetTypeId)
        {
            getInstanceNumber(objectPath, targetInstanceId, callback);
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {}
}

void SendNSMCommand::getInstanceNumber(
    const std::string& objectPath, uint8_t targetInstanceId,
    std::function<void(const std::string&)> callback)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    auto instanceNumberMethod =
        bus.new_method_call("xyz.openbmc_project.NSM", objectPath.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
    instanceNumberMethod.append("xyz.openbmc_project.FruDevice",
                                "INSTANCE_NUMBER");

    try
    {
        auto instanceReply = bus.call(instanceNumberMethod);
        std::variant<uint8_t> instanceNumber;
        instanceReply.read(instanceNumber);

        if (std::get<uint8_t>(instanceNumber) == targetInstanceId)
        {
            callback(objectPath); // Found matching object path, stop processing
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {}
}

void SendNSMCommand::execute(const std::string& targetType,
                             uint8_t targetInstanceId, uint8_t messageType,
                             uint8_t commandCode, const std::string& filePath)
{
    getMatchingFruDeviceObjectPath(targetType, targetInstanceId,
                                   [this, messageType, commandCode,
                                    filePath](const std::string& objectPath) {
        if (objectPath.empty())
        {
            return;
        }

        sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd == -1)
        {
            return;
        }

        sdbusplus::message::unix_fd unixFd(fd);

        try
        {
            auto method = bus.new_method_call(
                "xyz.openbmc_project.NSM", objectPath.c_str(),
                "xyz.openbmc_project.NSM.NSMRawCommand", "SendNSMRawCommand");
            method.append(messageType, commandCode, unixFd);

            auto reply = bus.call(method);
            sdbusplus::message::object_path returnedObjectPath;
            uint8_t completionCode;
            reply.read(returnedObjectPath, completionCode);

            std::cout << "ObjectPath = " << std::string(returnedObjectPath)
                      << std::endl;

            close(fd);
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {}
    });
}

GetCommandStatus::GetCommandStatus(CLI::App* app)
{
    std::string objectPath;

    auto subcmd = app->add_subcommand("getCommandStatus",
                                      "Get NSM Command Status");
    subcmd->add_option("--object_path", objectPath, "D-Bus Object Path")
        ->required();

    subcmd->callback([this, &objectPath]() { this->execute(objectPath); });
}

void GetCommandStatus::execute(const std::string& objectPath)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto method =
            bus.new_method_call("xyz.openbmc_project.NSM", objectPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Get");
        method.append("xyz.openbmc_project.NSM.NSMRawCommandStatus", "Status");

        auto reply = bus.call(method);
        std::variant<std::string> status;
        reply.read(status);

        std::cout << std::get<std::string>(status) << std::endl;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {}
}

WaitCommandStatusComplete::WaitCommandStatusComplete(CLI::App* app)
{
    std::string objectPath;

    auto subcmd = app->add_subcommand("waitCommandStatusComplete",
                                      "Wait for NSM Command Completion");
    subcmd->add_option("--object_path", objectPath, "D-Bus Object Path")
        ->required();

    subcmd->callback([this, &objectPath]() { this->execute(objectPath); });
}

void WaitCommandStatusComplete::execute(const std::string& objectPath)
{
    std::string status;
    do
    {
        sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
        try
        {
            auto method = bus.new_method_call(
                "xyz.openbmc_project.NSM", objectPath.c_str(),
                "org.freedesktop.DBus.Properties", "Get");
            method.append("xyz.openbmc_project.NSM.NSMRawCommandStatus",
                          "Status");

            auto reply = bus.call(method);
            std::variant<std::string> statusVariant;
            reply.read(statusVariant);
            status = std::get<std::string>(statusVariant);

            std::cout << status << std::endl;

            if (status ==
                "xyz.openbmc_project.NSM.NSMRawCommandStatus.SetOperationStatus.CommandInProgress")
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            return;
        }
    } while (
        status ==
        "xyz.openbmc_project.NSM.NSMRawCommandStatus.SetOperationStatus.CommandInProgress");
}

GetNSMResponse::GetNSMResponse(CLI::App* app)
{
    std::string objectPath;

    auto subcmd = app->add_subcommand("getNSMResponse",
                                      "Get NSM Command Response");
    subcmd->add_option("--object_path", objectPath, "D-Bus Object Path")
        ->required();

    subcmd->callback([this, &objectPath]() { this->execute(objectPath); });
}

void GetNSMResponse::execute(const std::string& objectPath)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto method = bus.new_method_call(
            "xyz.openbmc_project.NSM", objectPath.c_str(),
            "xyz.openbmc_project.NSM.NSMRawCommand", "GetNSMCommandResponse");

        auto reply = bus.call(method);
        uint8_t completionCode;
        uint16_t reasonCode;
        sdbusplus::message::unix_fd responseFd;
        reply.read(completionCode, reasonCode, responseFd);

        std::cout << "Completion Code: " << static_cast<int>(completionCode)
                  << std::endl;
        std::cout << "Reason Code: " << static_cast<int>(reasonCode)
                  << std::endl;

        int fd = static_cast<int>(responseFd);
        char buffer[4096];
        ssize_t bytesRead;
        std::vector<uint8_t> responseData;

        // Read the response data from the file descriptor
        while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        {
            responseData.insert(responseData.end(), buffer, buffer + bytesRead);
        }

        // Print the response data in hex format
        std::cout << "Response Data (Hex): ";
        for (const auto& byte : responseData)
        {
            printf("%02X ", byte); // Print each byte in hex
        }
        std::cout << std::endl;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {}
}

void registerCommand(CLI::App& app)
{
    auto* passthroughApp = app.add_subcommand(
        "passthrough", "Passthrough command support for dbus API testing");

    // Using unique_ptr to manage the dynamically allocated commands
    auto sendNSMCommand = std::make_unique<SendNSMCommand>(passthroughApp);
    auto getCommandStatus = std::make_unique<GetCommandStatus>(passthroughApp);
    auto waitCommandStatusComplete =
        std::make_unique<WaitCommandStatusComplete>(passthroughApp);
    auto getNSMResponse = std::make_unique<GetNSMResponse>(passthroughApp);

    // Push the unique_ptrs to the global vector to keep them alive
    sendNSMCommandObjects.push_back(std::move(sendNSMCommand));
    getCommandStatusObjects.push_back(std::move(getCommandStatus));
    waitCommandStatusCompleteObjects.push_back(
        std::move(waitCommandStatusComplete));
    getNSMResponseObjects.push_back(std::move(getNSMResponse));
}

} // namespace passthrough
} // namespace nsmtool