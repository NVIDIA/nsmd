#include "nsm_discovery_cmd.hpp"

#include "base.h"

#include "cmd_helper.hpp"

#include <CLI/CLI.hpp>

namespace nsmtool
{

namespace discovery
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

class Ping : public CommandInterface
{
  public:
    ~Ping() = default;
    Ping() = delete;
    Ping(const Ping&) = delete;
    Ping(Ping&&) = default;
    Ping& operator=(const Ping&) = delete;
    Ping& operator=(Ping&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_ping_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        auto rc =
            decode_ping_resp(responsePtr, payloadLength, &cc, &reason_code);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetSupportedMessageTypes : public CommandInterface
{
  public:
    ~GetSupportedMessageTypes() = default;
    GetSupportedMessageTypes() = delete;
    GetSupportedMessageTypes(const GetSupportedMessageTypes&) = delete;
    GetSupportedMessageTypes(GetSupportedMessageTypes&&) = default;
    GetSupportedMessageTypes&
        operator=(const GetSupportedMessageTypes&) = delete;
    GetSupportedMessageTypes& operator=(GetSupportedMessageTypes&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_supported_nvidia_message_types_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];

        auto rc = decode_get_supported_nvidia_message_types_resp(
            responsePtr, payloadLength, &cc, &reason_code, &supportedTypes[0]);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        std::string key("Supported Nvidia Message Types");

        parseBitfieldVar(result, key, &supportedTypes[0],
                         SUPPORTED_MSG_TYPE_DATA_SIZE);

        nsmtool::helper::DisplayInJson(result);
    }
};

class GetSupportedCommandCodes : public CommandInterface
{
  public:
    ~GetSupportedCommandCodes() = default;
    GetSupportedCommandCodes() = delete;
    GetSupportedCommandCodes(const GetSupportedCommandCodes&) = delete;
    GetSupportedCommandCodes(GetSupportedCommandCodes&&) = default;
    GetSupportedCommandCodes&
        operator=(const GetSupportedCommandCodes&) = delete;
    GetSupportedCommandCodes& operator=(GetSupportedCommandCodes&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetSupportedCommandCodes(const char* type, const char* name,
                                      CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto ccOptionGroup = app->add_option_group(
            "Required",
            "Retrieve supported command codes for the requested Nvidia message type");
        ccOptionGroup->add_option(
            "-t,--type", nvidiaMsgType,
            "retrieve supported command codes for the message type specified.");
        ccOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_supported_command_codes_req(
            instanceId, nvidiaMsgType, request);
        return std::make_pair(rc, requestMsg);
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t supportedCommandCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];

        auto rc = decode_get_supported_command_codes_resp(
            responsePtr, payloadLength, &cc, &reason_code,
            &supportedCommandCodes[0]);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Nvidia Message Type"] = nvidiaMsgType;
        std::string key("Supported Command codes");

        parseBitfieldVar(result, key, &supportedCommandCodes[0],
                         SUPPORTED_COMMAND_CODE_DATA_SIZE);

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t nvidiaMsgType;
};

class QueryDeviceIdentification : public CommandInterface
{
  public:
    ~QueryDeviceIdentification() = default;
    QueryDeviceIdentification() = delete;
    QueryDeviceIdentification(const QueryDeviceIdentification&) = delete;
    QueryDeviceIdentification(QueryDeviceIdentification&&) = default;
    QueryDeviceIdentification&
        operator=(const QueryDeviceIdentification&) = delete;
    QueryDeviceIdentification& operator=(QueryDeviceIdentification&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_nsm_query_device_identification_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint8_t device_identification;
        uint8_t device_instanceID;
        auto rc = decode_query_device_identification_resp(
            responsePtr, payloadLength, &cc, &reason_code,
            &device_identification, &device_instanceID);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        ordered_json result;
        result["Compeletion Code"] = cc;
        switch (device_identification)
        {
            case NSM_DEV_ID_GPU:
                result["Device Identification"] = "GPU";
                break;
            case NSM_DEV_ID_SWITCH:
                result["Device Identification"] = "Switch";
                break;
            case NSM_DEV_ID_PCIE_BRIDGE:
                result["Device Identification"] = "PCIe Bridge";
                break;
            case NSM_DEV_ID_BASEBOARD:
                result["Device Identification"] = "Baseboard";
                break;
            case NSM_DEV_ID_UNKNOWN:
                result["Device Identification"] = "Unknown";
                break;
            default:
                std::cerr << "Invalid device identification received: "
                          << (int)device_identification << "\n";
                return;
        }
        result["Device Instance ID"] = device_instanceID;

        nsmtool::helper::DisplayInJson(result);
    }
};

void registerCommand(CLI::App& app)
{
    auto discovery = app.add_subcommand(
        "discovery", "Device capability discovery type command");
    discovery->require_subcommand(1);

    auto ping = discovery->add_subcommand(
        "Ping", "get the status of responder, if alive or not");
    commands.push_back(std::make_unique<Ping>("discovery", "Ping", ping));

    auto getSupportedMessageTypes = discovery->add_subcommand(
        "GetSupportedMessageTypes",
        "get supported nvidia message types by the device");
    commands.push_back(std::make_unique<GetSupportedMessageTypes>(
        "discovery", "GetSupportedMessageTypes", getSupportedMessageTypes));

    auto getSupportedCommandCodes =
        discovery->add_subcommand("GetSupportedCommandCodes",
                                  "get supported command codes by the device");
    commands.push_back(std::make_unique<GetSupportedCommandCodes>(
        "discovery", "GetSupportedCommandCodes", getSupportedCommandCodes));

    auto queryDeviceIdentification = discovery->add_subcommand(
        "QueryDeviceIdentification",
        "query compliant devices for self-identification information");
    commands.push_back(std::make_unique<QueryDeviceIdentification>(
        "discovery", "QueryDeviceIdentification", queryDeviceIdentification));
}

} // namespace discovery
} // namespace nsmtool
