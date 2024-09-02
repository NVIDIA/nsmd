#include "nsmRetimerPort.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmProcessor/nsmProcessor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmPort::NsmPort(sdbusplus::bus::bus& bus, std::string& portName,
                 const std::string& type,
                 const std::vector<utils::Association>& associations,
                 const std::string& inventoryObjPath) :
    NsmObject(portName, type), portName(portName)
{
    lg2::info("NsmPCIePort: create sensor:{NAME}", "NAME", portName.c_str());
    portIntf = std::make_unique<PortIntf>(bus, inventoryObjPath.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
}

NsmPCIeECCGroup1::NsmPCIeECCGroup1(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   std::shared_ptr<PortWidthIntf> portWidthIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, 1), portInfoIntf(portInfoIntf),
    portWidthIntf(portWidthIntf)

{
    lg2::info("NsmPCIeECCGroup1: {NAME}", "NAME", name.c_str());
}

double NsmPCIeECCGroup1::convertEncodedSpeedToGbps(const uint32_t& speed)
{
    switch (speed)
    {
        case 1:
        {
            return 2.5;
        }
        case 2:
        {
            return 5.0;
        }
        case 3:
        {
            return 8.0;
        }
        case 4:
        {
            return 16.0;
        }
        case 5:
        {
            return 32.0;
        }
        case 6:
        {
            return 64.0;
        }
        default:
        {
            lg2::error("NsmPCIeECCGroup1: {NAME}, unknown speed {SPEED}",
                       "NAME", getName(), "SPEED", speed);
            return 0;
        }
    }
}

size_t NsmPCIeECCGroup1::convertEncodedWidthToActualWidth(const uint32_t& width)
{
    return (width > 0 && width <= 6) ? (uint32_t)pow(2, width - 1) : 0;
}

uint8_t NsmPCIeECCGroup1::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_1 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        portInfoIntf->maxSpeed(convertEncodedSpeedToGbps(data.max_link_speed));
        portInfoIntf->currentSpeed(
            convertEncodedSpeedToGbps(data.negotiated_link_speed));
        portWidthIntf->width(
            convertEncodedWidthToActualWidth(data.max_link_width));
        portWidthIntf->activeWidth(
            convertEncodedWidthToActualWidth(data.negotiated_link_width));
    }
    else
    {
        lg2::error(
            "NsmPCIeECCGroup1: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group1_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPCIeECCGroup2::NsmPCIeECCGroup2(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_2), pcieEccIntf(pcieEccIntf)

{
    lg2::info("NsmPCIeECCGroup2: {NAME}", "NAME", name.c_str());
}

uint8_t NsmPCIeECCGroup2::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_2 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pcieEccIntf->nonfeCount(data.non_fatal_errors);
        pcieEccIntf->feCount(data.fatal_errors);
        pcieEccIntf->ceCount(data.correctable_errors);
        pcieEccIntf->unsupportedRequestCount(data.unsupported_request_count);
    }
    else
    {
        lg2::error(
            "NsmPCIeECCGroup2: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group2_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPCIeECCGroup3::NsmPCIeECCGroup3(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_3), pcieEccIntf(pcieEccIntf)

{
    lg2::info("NsmPCIeECCGroup3: {NAME}", "NAME", name.c_str());
}

uint8_t NsmPCIeECCGroup3::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_3 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pcieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    }
    else
    {
        lg2::error(
            "NsmPCIeECCGroup3: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group3_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPCIeECCGroup4::NsmPCIeECCGroup4(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_4), pcieEccIntf(pcieEccIntf)

{
    lg2::info("NsmPCIeECCGroup4: {NAME}", "NAME", name.c_str());
}

uint8_t NsmPCIeECCGroup4::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_4 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pcieEccIntf->replayCount(data.replay_cnt);
        pcieEccIntf->replayRolloverCount(data.replay_rollover_cnt);
        pcieEccIntf->nakSentCount(data.NAK_sent_cnt);
        pcieEccIntf->nakReceivedCount(data.NAK_recv_cnt);
    }
    else
    {
        lg2::error(
            "NsmPCIeECCGroup4: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group4_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPCIeECCGroup8::NsmPCIeECCGroup8(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<LaneErrorIntf> laneErrorIntf,
                                   uint8_t deviceIndex,
                                   const std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_8),
    laneErrorIntf(laneErrorIntf), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmPCIeECCGroup8: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

uint8_t NsmPCIeECCGroup8::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_8 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group8_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        std::vector<uint32_t> error_counts;

        for (int idx = 0; idx < TOTAL_PCIE_LANE_COUNT; idx++)
        {
            error_counts.push_back(data.error_counts[idx]);
        }

        laneErrorIntf->rxErrorsPerLane(error_counts);
        updateMetricOnSharedMemory();
    }
    else
    {
        lg2::error(
            "NsmPCIeECCGroup8: decode_query_scalar_group_telemetry_v1_group8_resp failed. rc={RC}, cc={CC}",
            "RC", rc, "CC", cc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

void NsmPCIeECCGroup8::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(laneErrorIntf->interface);
    nv::sensor_aggregation::DbusVariantType valueVariant{
        laneErrorIntf->rxErrorsPerLane()};
    std::vector<uint8_t> smbusData = {};
    std::string propName = "RXErrorsPerLane";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant);
#endif
}

static requester::Coroutine
    createNsmPCIeRetimerPorts(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto count = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "Count", interface.c_str());
    auto deviceInstance = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "DeviceInstance", interface.c_str());
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto portProtocol = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PortProtocol", interface.c_str());
    auto portType = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PortType", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto type = interface.substr(interface.find_last_of('.') + 1);

    // device_index are between [1 to 8] for retimers, which is
    // calculated as device_instance + PCIE_RETIMER_DEVICE_INDEX_START
    uint8_t deviceIndex = static_cast<uint8_t>(deviceInstance) +
                          PCIE_RETIMER_DEVICE_INDEX_START;

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_PCIeLink PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    // create pcie link [as per count]
    for (uint64_t i = 0; i < count; i++)
    {
        std::string portName = name + '_' + std::to_string(i);
        std::string objPath = inventoryObjPath + portName;

        auto pciePortIntfSensor = std::make_shared<NsmPort>(
            bus, portName, type, associations, objPath);
        nsmDevice->addStaticSensor(pciePortIntfSensor);

        auto pcieECCIntf = std::make_shared<PCIeEccIntf>(bus, objPath.c_str());
        auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                           objPath.c_str());
        auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                             objPath.c_str());

        portInfoIntf->protocol(
            PortInfoIntf::convertPortProtocolFromString(portProtocol));
        portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(portType));

        auto pcieSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
            portName, type, portInfoIntf, portWidthIntf, deviceIndex);
        auto pcieECCIntfSensorGroup2 = std::make_shared<NsmPCIeECCGroup2>(
            portName, type, pcieECCIntf, deviceIndex);
        auto pcieECCIntfSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
            portName, type, pcieECCIntf, deviceIndex);
        auto pcieECCIntfSensorGroup4 = std::make_shared<NsmPCIeECCGroup4>(
            portName, type, pcieECCIntf, deviceIndex);

        if (!pcieSensorGroup1 || !pcieECCIntfSensorGroup2 ||
            !pcieECCIntfSensorGroup3 || !pcieECCIntfSensorGroup4)
        {
            lg2::error(
                "Failed to create NSM PCIe Port sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }

        nsmDevice->addSensor(pcieSensorGroup1, priority);
        nsmDevice->addSensor(pcieECCIntfSensorGroup2, priority);
        nsmDevice->addSensor(pcieECCIntfSensorGroup3, priority);
        nsmDevice->addSensor(pcieECCIntfSensorGroup4, priority);
    }
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmPCIeRetimerPorts,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeLink")

} // namespace nsm
