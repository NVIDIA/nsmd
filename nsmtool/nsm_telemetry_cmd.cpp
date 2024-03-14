// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]

#include "nsm_telemetry_cmd.hpp"

#include "base.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"

#include <CLI/CLI.hpp>

#include <ctime>

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

    explicit GetInvectoryInformation(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto getInvectoryInformationOptionGroup = app->add_option_group(
            "Required",
            "Property Id for which Inventory Information is to be retrieved.");

        propertyId = 0;
        getInvectoryInformationOptionGroup->add_option(
            "-p, --propertyId", propertyId,
            "retrieve inventory information for propertyId");
        getInvectoryInformationOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_inventory_information_req(instanceId, propertyId,
                                                       request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        std::vector<uint8_t> inventoryInformation(65535, 0);

        auto rc = decode_get_inventory_information_resp(
            responsePtr, payloadLength, &cc, &reason_code, &dataSize,
            inventoryInformation.data());
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }

        printInventoryInfo(propertyId, dataSize, &inventoryInformation);
    }

    void printInventoryInfo(uint8_t& propertyIdentifier,
                            const uint16_t dataSize,
                            const std::vector<uint8_t>* inventoryInformation)
    {
        if (inventoryInformation == NULL)
        {
            std::cerr << "Failed to get inventory information" << std::endl;
            return;
        }

        ordered_json result;
        ordered_json propRecordResult;
        nsm_inventory_property_record* propertyRecord =
            (nsm_inventory_property_record*)inventoryInformation->data();
        propRecordResult["Property ID"] = propertyIdentifier;
        propRecordResult["Data Length"] = static_cast<uint16_t>(dataSize);

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

        // todo: display aggregate data
        switch (propertyIdentifier)
        {
            case MAXIMUM_MEMORY_CAPACITY:
            case PRODUCT_LENGTH:
            case PRODUCT_WIDTH:
            case PRODUCT_HEIGHT:
            case RATED_DEVICE_POWER_LIMIT:
            case MINIMUM_DEVICE_POWER_LIMIT:
            case MAXIMUM_DEVICE_POWER_LIMIT:
            case MINIMUM_MODULE_POWER_LIMIT:
            case MAXMUM_MODULE_POWER_LIMIT:
            case RATED_MODULE_POWER_LIMIT:
            case DEFAULT_BOOST_CLOCKS:
            case DEFAULT_BASE_CLOCKS:
                std::memcpy(&(dataPayload.nvs32Val), propertyRecord->data,
                            sizeof(int32_t));
                propRecordResult["Data"] = dataPayload.nvs32Val;
                break;
            case BOARD_PART_NUMBER:
            case SERIAL_NUMBER:
            case MARKETING_NAME:
            case DEVICE_PART_NUMBER:
            case FRU_PART_NUMBER:
            case MEMORY_VENDOR:
            case MEMORY_PART_NUMBER:
            case BUILD_DATE:
            case FIRMWARE_VERSION:
            case INFO_ROM_VERSION:
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
    uint8_t propertyId;
};

class AggregateResponseParser
{
  private:
    virtual int handleSampleData(uint8_t tag, const uint8_t* data,
                                 size_t data_len,
                                 ordered_json& sample_json) = 0;

  public:
    virtual ~AggregateResponseParser() = default;

    void parseAggregateResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        auto response_data = reinterpret_cast<const uint8_t*>(responsePtr);

        auto rc = decode_aggregate_resp(responsePtr, msg_len, &consumed_len,
                                        &cc, &telemetry_count);

        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc << "\n";

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sample Count"] = telemetry_count;

        uint64_t timestamp{};
        constexpr size_t time_size{100};
        char time[time_size];
        bool has_timestamp{false};

        std::vector<ordered_json> samples;
        while (telemetry_count--)
        {
            uint8_t tag;
            bool valid;
            const uint8_t* data;
            size_t data_len;

            msg_len -= consumed_len;

            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, msg_len, &consumed_len,
                                              &tag, &valid, &data, &data_len);

            if (rc != NSM_SW_SUCCESS || !valid)
            {
                std::cerr
                    << "Response message error while parsing sample header: "
                    << "tag=" << tag << ", rc=" << rc << "\n";

                continue;
            }

            if (tag == 0xFF)
            {
                if (data_len != 8)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample : "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                rc =
                    decode_aggregate_timestamp_data(data, data_len, &timestamp);
                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample data : "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                has_timestamp = true;
                auto stime = static_cast<std::time_t>(timestamp);
                std::strftime(time, time_size, "%F %T %Z",
                              std::localtime(&stime));
            }
            else if (tag < 0xF0)
            {
                ordered_json sample_json;

                rc = handleSampleData(tag, data, data_len, sample_json);

                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing sample data: "
                        << "tag=" << tag << ", rc=" << rc << "\n";

                    continue;
                }

                if (has_timestamp)
                {
                    sample_json["Timestamp"] = time;
                }

                samples.push_back(std::move(sample_json));
            }
        }

        result["Samples"] = std::move(samples);
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetTemperatureReading : public CommandInterface
{
  public:
    ~GetTemperatureReading() = default;
    GetTemperatureReading() = delete;
    GetTemperatureReading(const GetTemperatureReading&) = delete;
    GetTemperatureReading(GetTemperatureReading&&) = default;
    GetTemperatureReading& operator=(const GetTemperatureReading&) = delete;
    GetTemperatureReading& operator=(GetTemperatureReading&&) = delete;

    using CommandInterface::CommandInterface;

    explicit GetTemperatureReading(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        double temperatureReading = 0;

        auto rc = decode_get_temperature_reading_resp(
            responsePtr, payloadLength, &cc, &reason_code, &temperatureReading);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_temperature_reading_resp));
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Temperature Reading"] = temperatureReading;
        nsmtool::helper::DisplayInJson(result);
    }

    class GetTemperatureAggregateResponseParser : public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            double reading{};
            auto rc = decode_aggregate_temperature_reading_data(data, data_len,
                                                                &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Temperature Reading"] = reading;
            }

            return rc;
        }
    };

  public:
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
        if (sensorId == AggregateSensorId)
        {
            GetTemperatureAggregateResponseParser{}.parseAggregateResponse(
                responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
};

class GetCurrentPowerDraw : public CommandInterface
{
  public:
    GetCurrentPowerDraw() = delete;
    GetCurrentPowerDraw(const GetCurrentPowerDraw&) = delete;
    GetCurrentPowerDraw(GetCurrentPowerDraw&&) = default;
    GetCurrentPowerDraw& operator=(const GetCurrentPowerDraw&) = delete;
    GetCurrentPowerDraw& operator=(GetCurrentPowerDraw&&) = delete;

    explicit GetCurrentPowerDraw(const char* type, const char* name,
                                 CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();

        app->add_option("-a, --averagingInterval", averagingInterval,
                        "averaging interval of current power draw reading")
            ->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint32_t reading;

        auto rc = decode_get_current_power_draw_resp(responsePtr, msg_len, &cc,
                                                     &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_current_power_draw_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Averaging Interval"] = averagingInterval;
        result["Current Power Draw"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetCurrentPowerDrawAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint32_t reading;
            auto rc = decode_aggregate_get_current_power_draw_reading(
                data, data_len, &reading);
            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Current Power Draw"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_current_power_draw_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_current_power_draw_req(instanceId, sensorId,
                                                    averagingInterval, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetCurrentPowerDrawAggregateResponseParser{}.parseAggregateResponse(
                responsePtr, payloadLength);
        }
        else
        {
            parseRegularResponse(responsePtr, payloadLength);
        }
    }

  private:
    uint8_t sensorId;
    static constexpr uint8_t AggregateSensorId{255};
    uint8_t averagingInterval;
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

    auto getCurrentPowerDraw = telemetry->add_subcommand(
        "GetCurrentPowerDraw", "get current power draw of a device");
    commands.push_back(std::make_unique<GetCurrentPowerDraw>(
        "telemetry", "GetCurrentPowerDraw", getCurrentPowerDraw));
}

} // namespace telemetry

} // namespace nsmtool
