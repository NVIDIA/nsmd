// NSM: Nvidia Message type
//           - Network Ports            [Type 1]
//           - PCI links                [Type 2]
//           - Platform environments    [Type 3]

#include "nsm_telemetry_cmd.hpp"

#include "base.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

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

class GetPortTelemetryCounter : public CommandInterface
{
  public:
    ~GetPortTelemetryCounter() = default;
    GetPortTelemetryCounter() = delete;
    GetPortTelemetryCounter(const GetPortTelemetryCounter&) = delete;
    GetPortTelemetryCounter(GetPortTelemetryCounter&&) = default;
    GetPortTelemetryCounter& operator=(const GetPortTelemetryCounter&) = delete;
    GetPortTelemetryCounter& operator=(GetPortTelemetryCounter&&) = default;

    using CommandInterface::CommandInterface;

    explicit GetPortTelemetryCounter(const char* type, const char* name,
                                     CLI::App* app) :
        CommandInterface(type, name, app)
    {
        auto portTeleOptionGroup = app->add_option_group(
            "Required",
            "Port number for which counter value is to be retrieved.");

        portNumber = 00;
        portTeleOptionGroup->add_option(
            "-p, --portNum", portNumber,
            "retrieve counter values for Port number");
        portTeleOptionGroup->require_option(1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_port_telemetry_counter_req(instanceId, portNumber,
                                                        request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataLen = 0;
        struct nsm_port_counter_data portTeleData;

        auto rc = decode_get_port_telemetry_counter_resp(
            responsePtr, payloadLength, &cc, &reason_code, &dataLen,
            &portTeleData);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            printPortTeleInfo(dataLen, &portTeleData);
        }
        else
        {
            std::cerr
                << "Response message error: decode_get_port_telemetry_resp fail"
                << "rc=" << rc << ", cc=" << (int)cc
                << ", reasonCode=" << (int)reason_code << "\n";
            return;
        }
        return;
    }

    void printPortTeleInfo(uint16_t dataLen,
                           const struct nsm_port_counter_data* portData)
    {
        if (portData == NULL)
        {
            std::cerr << "Failed to get port counter information" << std::endl;
            return;
        }

        ordered_json result;
        result["Port Number"] = portNumber;
        result["Data Length"] = dataLen;
        std::string key("Supported Counters");

        ordered_json countersResult;
        if (portData->supported_counter.port_rcv_pkts)
        {
            result[key].push_back(0);
            countersResult["Port Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_rcv_pkts);
        }

        if (portData->supported_counter.port_rcv_data)
        {
            result[key].push_back(1);
            countersResult["Port Rcv Data"] =
                static_cast<uint64_t>(portData->port_rcv_data);
        }

        if (portData->supported_counter.port_multicast_rcv_pkts)
        {
            result[key].push_back(2);
            countersResult["Port Multicast Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_multicast_rcv_pkts);
        }

        if (portData->supported_counter.port_unicast_rcv_pkts)
        {
            result[key].push_back(3);
            countersResult["Port Unicast Rcv Pkt"] =
                static_cast<uint64_t>(portData->port_unicast_rcv_pkts);
        }

        if (portData->supported_counter.port_malformed_pkts)
        {
            result[key].push_back(4);
            countersResult["Port Malformed Pkt"] =
                static_cast<uint64_t>(portData->port_malformed_pkts);
        }

        if (portData->supported_counter.vl15_dropped)
        {
            result[key].push_back(5);
            countersResult["Vl15 Dropped"] =
                static_cast<uint64_t>(portData->vl15_dropped);
        }

        if (portData->supported_counter.port_rcv_errors)
        {
            result[key].push_back(6);
            countersResult["Port Rcv Error"] =
                static_cast<uint64_t>(portData->port_rcv_errors);
        }

        if (portData->supported_counter.port_xmit_pkts)
        {
            result[key].push_back(7);
            countersResult["Port Tx Pkt"] =
                static_cast<uint64_t>(portData->port_xmit_pkts);
        }

        if (portData->supported_counter.port_xmit_pkts_vl15)
        {
            result[key].push_back(8);
            countersResult["Port Tx Pkt Vl15"] =
                static_cast<uint64_t>(portData->port_xmit_pkts_vl15);
        }

        if (portData->supported_counter.port_xmit_data)
        {
            result[key].push_back(9);
            countersResult["Port Tx Data"] =
                static_cast<uint64_t>(portData->port_xmit_data);
        }

        if (portData->supported_counter.port_xmit_data_vl15)
        {
            result[key].push_back(10);
            countersResult["Port Tx Data Vl15"] =
                static_cast<uint64_t>(portData->port_xmit_data_vl15);
        }

        if (portData->supported_counter.port_unicast_xmit_pkts)
        {
            result[key].push_back(11);
            countersResult["Port Unicast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_unicast_xmit_pkts);
        }

        if (portData->supported_counter.port_multicast_xmit_pkts)
        {
            result[key].push_back(12);
            countersResult["Port Multicast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_multicast_xmit_pkts);
        }

        if (portData->supported_counter.port_bcast_xmit_pkts)
        {
            result[key].push_back(13);
            countersResult["Port Broadcast Tx Pkt"] =
                static_cast<uint64_t>(portData->port_bcast_xmit_pkts);
        }

        if (portData->supported_counter.port_xmit_discard)
        {
            result[key].push_back(14);
            countersResult["Port Tx Discard"] =
                static_cast<uint64_t>(portData->port_xmit_discard);
        }

        if (portData->supported_counter.port_neighbor_mtu_discards)
        {
            result[key].push_back(15);
            countersResult["Port Neighbour MTU Discard"] =
                static_cast<uint64_t>(portData->port_neighbor_mtu_discards);
        }

        if (portData->supported_counter.port_rcv_ibg2_pkts)
        {
            result[key].push_back(16);
            countersResult["Port Rcv IBG2 Pkt"] =
                static_cast<uint64_t>(portData->port_rcv_ibg2_pkts);
        }

        if (portData->supported_counter.port_xmit_ibg2_pkts)
        {
            result[key].push_back(17);
            countersResult["Port Tx IBG2 Pkt"] =
                static_cast<uint64_t>(portData->port_xmit_ibg2_pkts);
        }

        if (portData->supported_counter.symbol_error)
        {
            result[key].push_back(18);
            countersResult["Symbol Error"] =
                static_cast<uint64_t>(portData->symbol_error);
        }

        if (portData->supported_counter.link_error_recovery_counter)
        {
            result[key].push_back(19);
            countersResult["Link Error Recovery Counter"] =
                static_cast<uint64_t>(portData->link_error_recovery_counter);
        }

        if (portData->supported_counter.link_downed_counter)
        {
            result[key].push_back(20);
            countersResult["Link Downed Counter"] =
                static_cast<uint64_t>(portData->link_downed_counter);
        }

        if (portData->supported_counter.port_rcv_remote_physical_errors)
        {
            result[key].push_back(21);
            countersResult["Port Rcv Remote Physical Error"] =
                static_cast<uint64_t>(
                    portData->port_rcv_remote_physical_errors);
        }

        if (portData->supported_counter.port_rcv_switch_relay_errors)
        {
            result[key].push_back(22);
            countersResult["Port Rcv Switch Relay Error"] =
                static_cast<uint64_t>(portData->port_rcv_switch_relay_errors);
        }

        if (portData->supported_counter.QP1_dropped)
        {
            result[key].push_back(23);
            countersResult["QP1 Dropped"] =
                static_cast<uint64_t>(portData->QP1_dropped);
        }

        if (portData->supported_counter.xmit_wait)
        {
            result[key].push_back(24);
            countersResult["Tx Wait"] =
                static_cast<uint64_t>(portData->xmit_wait);
        }
        result["Port Counter Information"] = countersResult;

        nsmtool::helper::DisplayInJson(result);
    }

  private:
    uint8_t portNumber;
};

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

class GetDriverInfo : public CommandInterface
{
  public:
    ~GetDriverInfo() = default;
    GetDriverInfo() = delete;
    GetDriverInfo(const GetDriverInfo&) = delete;
    GetDriverInfo(GetDriverInfo&&) = default;
    GetDriverInfo& operator=(const GetDriverInfo&) = delete;
    GetDriverInfo& operator=(GetDriverInfo&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_driver_info_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        enum8 driverState = 0;
        char driverVersion[MAX_VERSION_STRING_SIZE] = {0};

        auto rc = decode_get_driver_info_resp(responsePtr, payloadLength, &cc,
                                              &reasonCode, &driverState,
                                              driverVersion);

        std::string version(driverVersion);

        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << static_cast<int>(cc)
                      << ", reasonCode=" << static_cast<int>(reasonCode) << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) + sizeof(driverState) +
                          version.length());
            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Driver State"] = static_cast<int>(driverState);
        result["Driver Version"] = version;
        nsmtool::helper::DisplayInJson(result);
    }
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

class GetCurrentEnergyCount : public CommandInterface
{
  public:
    GetCurrentEnergyCount() = delete;
    GetCurrentEnergyCount(const GetCurrentEnergyCount&) = delete;
    GetCurrentEnergyCount(GetCurrentEnergyCount&&) = default;
    GetCurrentEnergyCount& operator=(const GetCurrentEnergyCount&) = delete;
    GetCurrentEnergyCount& operator=(GetCurrentEnergyCount&&) = delete;

    explicit GetCurrentEnergyCount(const char* type, const char* name,
                                   CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint64_t reading;

        auto rc = decode_get_current_energy_count_resp(
            responsePtr, msg_len, &cc, &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_current_energy_count_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Current Energy Count"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetCurrentEnergyCountAggregateResponseParser :
        public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint64_t reading{};
            auto rc =
                decode_aggregate_energy_count_data(data, data_len, &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Current Energy Count"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc =
            encode_get_current_energy_count_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetCurrentEnergyCountAggregateResponseParser{}
                .parseAggregateResponse(responsePtr, payloadLength);
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

class GetVoltage : public CommandInterface
{
  public:
    GetVoltage() = delete;
    GetVoltage(const GetVoltage&) = delete;
    GetVoltage(GetVoltage&&) = default;
    GetVoltage& operator=(const GetVoltage&) = delete;
    GetVoltage& operator=(GetVoltage&&) = delete;

    explicit GetVoltage(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-s, --sensorId", sensorId, "sensor Id")->required();
    }

  private:
    void parseRegularResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        const size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        uint8_t cc;
        uint16_t reason_code;
        uint32_t reading;

        auto rc = decode_get_voltage_resp(responsePtr, msg_len, &cc,
                                          &reason_code, &reading);
        if (rc != NSM_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_voltage_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sensor Id"] = sensorId;
        result["Voltage"] = reading;

        nsmtool::helper::DisplayInJson(result);
    }

    class GetVoltageAggregateResponseParser : public AggregateResponseParser
    {
      private:
        int handleSampleData(uint8_t tag, const uint8_t* data, size_t data_len,
                             ordered_json& sample_json) final
        {
            uint32_t reading{};
            auto rc = decode_aggregate_voltage_data(data, data_len, &reading);

            if (rc == NSM_SUCCESS)
            {
                sample_json["Sensor Id"] = tag;
                sample_json["Voltage"] = reading;
            }

            return rc;
        }
    };

  public:
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_get_voltage_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_voltage_req(instanceId, sensorId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        if (sensorId == AggregateSensorId)
        {
            GetVoltageAggregateResponseParser{}.parseAggregateResponse(
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

class GetMigMode : public CommandInterface
{
  public:
    ~GetMigMode() = default;
    GetMigMode() = delete;
    GetMigMode(const GetMigMode&) = delete;
    GetMigMode(GetMigMode&&) = default;
    GetMigMode& operator=(const GetMigMode&) = delete;
    GetMigMode& operator=(GetMigMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_MIG_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield8_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_MIG_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_MIG_mode_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        if (flags.bits.bit0)
        {
            result["MigModeEnabled"] = true;
        }
        else
        {
            result["MigModeEnabled"] = false;
        }
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetEccMode : public CommandInterface
{
  public:
    ~GetEccMode() = default;
    GetEccMode() = delete;
    GetEccMode(const GetEccMode&) = delete;
    GetEccMode(GetEccMode&&) = default;
    GetEccMode& operator=(const GetEccMode&) = delete;
    GetEccMode& operator=(GetEccMode&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_ECC_mode_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        bitfield8_t flags;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_ECC_mode_resp(responsePtr, payloadLength, &cc,
                                           &data_size, &reason_code, &flags);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_ECC_mode_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        if (flags.bits.bit0)
        {
            result["ECCModeEnabled"] = true;
        }
        else
        {
            result["ECCModeEnabled"] = false;
        }
        if (flags.bits.bit1)
        {
            result["PendingECCState"] = true;
        }
        else
        {
            result["PendingECCState"] = false;
        }
        nsmtool::helper::DisplayInJson(result);
    }
};

class GetEccErrorCounts : public CommandInterface
{
  public:
    ~GetEccErrorCounts() = default;
    GetEccErrorCounts() = delete;
    GetEccErrorCounts(const GetEccErrorCounts&) = delete;
    GetEccErrorCounts(GetEccErrorCounts&&) = default;
    GetEccErrorCounts& operator=(const GetEccErrorCounts&) = delete;
    GetEccErrorCounts& operator=(GetEccErrorCounts&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_common_req));
        auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
        auto rc = encode_get_ECC_error_counts_req(instanceId, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(nsm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = NSM_ERROR;
        struct nsm_ECC_error_counts errorCounts;
        uint16_t data_size;
        uint16_t reason_code = ERR_NULL;
        auto rc = decode_get_ECC_error_counts_resp(responsePtr, payloadLength,
                                                   &cc, &data_size,
                                                   &reason_code, &errorCounts);
        if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc
                      << ", reasonCode=" << (int)reason_code << "\n"
                      << payloadLength << "...."
                      << (sizeof(struct nsm_msg_hdr) +
                          sizeof(struct nsm_get_ECC_error_counts_resp));

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["ueCount"] = errorCounts.sram_uncorrected_parity +
                            errorCounts.sram_uncorrected_secded;
        result["ecCount"] = (int)errorCounts.sram_corrected;
        nsmtool::helper::DisplayInJson(result);
    }
};

void registerCommand(CLI::App& app)
{
    auto telemetry = app.add_subcommand(
        "telemetry", "Network, PCI link and platform telemetry type command");
    telemetry->require_subcommand(1);

    auto getPortTelemetryCounter = telemetry->add_subcommand(
        "GetPortTelemetryCounter", "get port telemetry counter");
    commands.push_back(std::make_unique<GetPortTelemetryCounter>(
        "telemetry", "GetPortTelemetryCounter", getPortTelemetryCounter));

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

    auto getCurrentEnergyCount = telemetry->add_subcommand(
        "GetCurrentEnergyCount",
        "get current energy counter value of a device");
    commands.push_back(std::make_unique<GetCurrentEnergyCount>(
        "telemetry", "GetCurrentPowerDraw", getCurrentEnergyCount));

    auto getVoltage =
        telemetry->add_subcommand("GetVoltage", "get voltage of a device");
    commands.push_back(std::make_unique<GetVoltage>(
        "telemetry", "GetCurrentPowerDraw", getVoltage));

    auto getDriverInfo =
        telemetry->add_subcommand("GetDriverInfo", "get Driver info");
    commands.push_back(std::make_unique<GetDriverInfo>(
        "telemetry", "GetDriverInfo", getDriverInfo));

    auto getMigMode = telemetry->add_subcommand("GetMigModes", "get MIG modes");
    commands.push_back(
        std::make_unique<GetMigMode>("telemetry", "GetMigMode", getMigMode));

    auto getEccMode = telemetry->add_subcommand("GetEccMode", "get ECC modes");
    commands.push_back(
        std::make_unique<GetEccMode>("telemetry", "GetEccMode", getEccMode));

    auto getEccErrorCounts =
        telemetry->add_subcommand("GetEccErrorCounts", "get ECC error counts");
    commands.push_back(std::make_unique<GetEccErrorCounts>(
        "telemetry", "GetEccErrorCounts", getEccErrorCounts));
}

} // namespace telemetry

} // namespace nsmtool
