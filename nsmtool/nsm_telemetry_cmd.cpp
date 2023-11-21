// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]

#include "nsm_telemetry_cmd.hpp"

#include "base.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"

#include <CLI/CLI.hpp>

namespace nsmtool
{

namespace telemetry
{

namespace
{

using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class GetInvectoryInformation : public CommandInterface
{
  public:
    ~GetInvectoryInformation() = default;
    GetInvectoryInformation() = delete;
    GetInvectoryInformation(const GetInvectoryInformation&) = delete;
    GetInvectoryInformation(GetInvectoryInformation&&) = default;
    GetInvectoryInformation& operator=(const GetInvectoryInformation&) = delete;
    GetInvectoryInformation& operator=(GetInvectoryInformation&&) = default;

    using CommandInterface::CommandInterface;

    void exec() override
    {
        std::cout << "[\n";

        // Retrieve all inventory information starting from the transfer handle
        // = 0
        transferHandle = 0;
        uint32_t prevTransferHandle = 0;
        std::map<uint32_t, uint32_t> transferHandleSeen;

        do
        {
            CommandInterface::exec();
            // transferHandle is updated to nextTransferHandle when
            // CommandInterface::exec() is successful, inside
            // parseResponseMsg().

            if (transferHandle == prevTransferHandle)
            {
                return;
            }

            // check for circular references.
            auto result =
                transferHandleSeen.emplace(transferHandle, prevTransferHandle);
            if (!result.second)
            {
                std::cerr << "Transfer handle " << transferHandle
                          << " has multiple references: "
                          << result.first->second << ", " << prevTransferHandle
                          << "\n";
                return;
            }
            prevTransferHandle = transferHandle;

            if (transferHandle != 0)
            {
                std::cout << ",";
            }
        } while (transferHandle != 0);
        std::cout << "]\n";
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_inventory_information_req(instanceId,
                                                       transferHandle, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t dataLen = 0;
        uint32_t nextTransferHandle = 0;
        std::vector<uint8_t> inventoryInformation(65535, 0);

        auto rc = decode_get_inventory_information_resp(
            responsePtr, payloadLength, &cc, &dataLen, &nextTransferHandle,
            inventoryInformation.data());
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc << "\n";
            return;
        }

        printInventoryInfo(nextTransferHandle, dataLen, &inventoryInformation);
        transferHandle = nextTransferHandle;
    }

    void printInventoryInfo(uint32_t& nextTransferHandle,
                            const uint16_t dataLen,
                            const std::vector<uint8_t>* inventoryInformation)
    {
        if (inventoryInformation == NULL)
        {
            std::cerr << "Failed to get inventory information" << std::endl;
            return;
        }

        ordered_json result;
        result["Next Transfer Handle"] = nextTransferHandle;
        result["Data Length"] = dataLen;

        ordered_json propRecordResult;
        nsm_inventory_property_record* propertyRecord =
            (nsm_inventory_property_record*)inventoryInformation->data();
        propRecordResult["Property ID"] = propertyRecord->property_id;
        propRecordResult["Data Type"] =
            static_cast<uint8_t>(propertyRecord->data_type);
        propRecordResult["Reserved"] =
            static_cast<uint8_t>(propertyRecord->reserved);
        propRecordResult["Data Length"] =
            static_cast<uint16_t>(propertyRecord->data_length);

        union Data
        {
            int8_t bool8Val;
            uint8_t nvu8Val;
            int8_t nvs8Val;
            uint16_t nvu16Val;
            int16_t nvs16Val;
            uint32_t nvu32Val;
            int32_t nvs32Val;
            uint64_t nvu64Val;
            int64_t nvs64Val;
            int32_t nvs24_8Val;
        } dataPayload;

        switch (static_cast<uint8_t>(propertyRecord->data_type))
        {
            case NvBool8:
                std::memcpy(&(dataPayload.bool8Val), propertyRecord->data,
                            sizeof(int8_t));
                propRecordResult["Data"] = dataPayload.bool8Val;
                break;
            case NvU8:
                std::memcpy(&(dataPayload.nvu8Val), propertyRecord->data,
                            sizeof(uint8_t));
                propRecordResult["Data"] = dataPayload.nvu8Val;
                break;
            case NvS8:
                std::memcpy(&(dataPayload.nvs8Val), propertyRecord->data,
                            sizeof(int8_t));
                propRecordResult["Data"] = dataPayload.nvs8Val;
                break;
            case NvU16:
                std::memcpy(&(dataPayload.nvu16Val), propertyRecord->data,
                            sizeof(uint16_t));
                propRecordResult["Data"] = dataPayload.nvu16Val;
                break;
            case NvS16:
                std::memcpy(&(dataPayload.nvs16Val), propertyRecord->data,
                            sizeof(int16_t));
                propRecordResult["Data"] = dataPayload.nvs16Val;
                break;
            case NvU32:
                std::memcpy(&(dataPayload.nvu32Val), propertyRecord->data,
                            sizeof(uint32_t));
                propRecordResult["Data"] = dataPayload.nvu32Val;
                break;
            case NvS32:
                std::memcpy(&(dataPayload.nvs32Val), propertyRecord->data,
                            sizeof(int32_t));
                propRecordResult["Data"] = dataPayload.nvs32Val;
                break;
            case NvU64:
                std::memcpy(&(dataPayload.nvu64Val), propertyRecord->data,
                            sizeof(uint64_t));
                propRecordResult["Data"] = dataPayload.nvu64Val;
                break;
            case NvS64:
                std::memcpy(&(dataPayload.nvs64Val), propertyRecord->data,
                            sizeof(int64_t));
                propRecordResult["Data"] = dataPayload.nvs64Val;
                break;
            case NvS24_8:
                std::memcpy(&(dataPayload.nvs24_8Val), propertyRecord->data,
                            sizeof(int32_t));
                propRecordResult["Data"] = dataPayload.nvs24_8Val;
                break;
            case NvCString:
                propRecordResult["Data"] =
                    reinterpret_cast<char*>(propertyRecord->data);
                break;
            case NvCharArray:
                propRecordResult["Data"] =
                    reinterpret_cast<char*>(propertyRecord->data);
                break;
            default:
                std::cerr
                    << "Incorrect data type recieved in get inventory information"
                    << std::endl;
                return;
        }

        result["Inventory Information"] = propRecordResult;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint32_t transferHandle;
};

class GetTemperatureReading : public CommandInterface
{
  public:
    ~GetTemperatureReading() = default;
    GetTemperatureReading() = delete;
    GetTemperatureReading(const GetTemperatureReading&) = delete;
    GetTemperatureReading(GetTemperatureReading&&) = default;
    GetTemperatureReading& operator=(const GetTemperatureReading&) = delete;
    GetTemperatureReading& operator=(GetTemperatureReading&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetTemperatureReading(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto tempReadingOptionGroup = app->add_option_group(
            "Required",
            "Sensor Id for which temperature value is to be retrieved.");

        sensorId = 00;
        tempReadingOptionGroup->add_option(
            "-s, --sensorId", sensorId,
            "retrieve temperature reading for sensorId");
        tempReadingOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_temperature_reading_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        real32_t temperatureReading = 0;

        auto rc = decode_get_temperature_reading_resp(
            responsePtr, payloadLength, &cc, &temperatureReading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_temperature_reading_resp));

            return;
        }

        ordered_json result;
        result["Compiletion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Temperature Reading"] = temperatureReading;
        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t sensorId;
};

void registerCommand(CLI::App& app)
{
    auto telemetry = app.add_subcommand(
        "telemetry", "Network, PCI link and platform telemetry type command");
    telemetry->require_subcommand(1);

    auto getInvectoryInformation = telemetry->add_subcommand(
        "GetInvectoryInformation", "get inventory information");
    commands.push_back(std::make_unique<GetInvectoryInformation>(
        "telemetry", "GetInvectoryInformation", getInvectoryInformation));

    auto getTemperatureReading = telemetry->add_subcommand(
        "GetTemperatureReading", "get temperature reading of a sensor");
    commands.push_back(std::make_unique<GetTemperatureReading>(
        "telemetry", "GetTemperatureReading", getTemperatureReading));
}

} // namespace telemetry

} // namespace nsmtool