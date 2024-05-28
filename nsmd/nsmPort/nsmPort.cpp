#include "nsmPort.hpp"

#include "common/types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmPortStatus::NsmPortStatus(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type,
    std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Interface,
    std::string& inventoryObjPath) :
    NsmSensor(portName, type), portName(portName), portNumber(portNum)
{
    lg2::debug("NsmPortStatus: {NAME}", "NAME", portName.c_str());

    portStateIntf = std::make_unique<PortStateIntf>(bus,
                                                    inventoryObjPath.c_str());
    portMetricsOem3Intf = portMetricsOem3Interface;

    portStateIntf->linkStatus(PortLinkStatus::Starting);
    portStateIntf->linkState(PortLinkStates::Unknown);
    portMetricsOem3Intf->trainingError(false);
}

std::optional<std::vector<uint8_t>>
    NsmPortStatus::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_port_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_port_status_req(instanceId, portNumber, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_port_status_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPortStatus::handleResponseMsg(const struct nsm_msg* responseMsg,
                                         size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t portState = NSM_PORTSTATE_DOWN;
    uint8_t portStatus = NSM_PORTSTATUS_DISABLED;

    auto rc = decode_query_port_status_resp(responseMsg, responseLen, &cc,
                                            &reasonCode, &dataSize, &portState,
                                            &portStatus);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        switch (portState)
        {
            case NSM_PORTSTATE_DOWN:
            case NSM_PORTSTATE_DOWN_LOCK:
            case NSM_PORTSTATE_SLEEP:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                break;
            case NSM_PORTSTATE_UP:
            case NSM_PORTSTATE_POLLING:
            case NSM_PORTSTATE_RESERVED:
                portStateIntf->linkStatus(PortLinkStatus::LinkUp);
                break;
            case NSM_PORTSTATE_TRAINING:
                portStateIntf->linkStatus(PortLinkStatus::Training);
                break;
            case NSM_PORTSTATE_TRAINING_FAILURE:
                portStateIntf->linkStatus(PortLinkStatus::Training);
                portMetricsOem3Intf->trainingError(true);
                break;
            default:
                portStateIntf->linkStatus(PortLinkStatus::NoLink);
                break;
        }

        switch (portStatus)
        {
            case NSM_PORTSTATUS_DISABLED:
                portStateIntf->linkState(PortLinkStates::Disabled);
                break;
            case NSM_PORTSTATUS_ENABLED:
                portStateIntf->linkState(PortLinkStates::Enabled);
                break;
            default:
                portStateIntf->linkState(PortLinkStates::Unknown);
                break;
        }
    }
    else
    {
        lg2::error(
            "responseHandler: decode_query_port_status_resp unsuccessfull. portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
            "NUM", portNumber, "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPortCharacteristics::NsmPortCharacteristics(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type,
    std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Interface,
    std::string& inventoryObjPath) :
    NsmSensor(portName, type), portName(portName), portNumber(portNum)
{
    lg2::debug("NsmPortCharacteristics: {NAME}", "NAME", portName.c_str());

    portInfoIntf = std::make_unique<PortInfoIntf>(bus,
                                                  inventoryObjPath.c_str());
    portMetricsOem3Intf = portMetricsOem3Interface;

    portInfoIntf->type(PortType::BidirectionalPort);
    portInfoIntf->protocol(PortProtocol::NVLink);
}

std::optional<std::vector<uint8_t>>
    NsmPortCharacteristics::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_port_characteristics_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_port_characteristics_req(instanceId, portNumber,
                                                    requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_query_port_characteristics_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmPortCharacteristics::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_port_characteristics_data data;

    auto rc = decode_query_port_characteristics_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        auto speedGbps = (data.nv_port_line_rate_mbps) / 1000;
        portInfoIntf->currentSpeed(speedGbps);
        portInfoIntf->maxSpeed(speedGbps);

        // not yet clear from spec raised as concern
        portMetricsOem3Intf->txNoProtocolBytes(data.nv_port_data_rate_kbps);
        portMetricsOem3Intf->rxNoProtocolBytes(data.nv_port_data_rate_kbps);
        portMetricsOem3Intf->txWidth(data.status_lane_info);
        portMetricsOem3Intf->rxWidth(data.status_lane_info);
    }
    else
    {
        lg2::error(
            "responseHandler: decode_query_port_characteristics_resp unsuccessfull. portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
            "NUM", portNumber, "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPortMetrics::NsmPortMetrics(sdbusplus::bus::bus& bus, std::string& portName,
                               uint8_t portNum, const std::string& type,
                               std::string& parentObjPath,
                               std::string& inventoryObjPath) :
    NsmSensor(portName, type), portName(portName), portNumber(portNum)
{
    lg2::debug("NsmPortMetrics: {NAME}", "NAME", portName.c_str());

    iBPortIntf = std::make_unique<IBPortIntf>(bus, inventoryObjPath.c_str());

    portMetricsOem2Intf =
        std::make_unique<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    associationDefinitionsIntf =
        std::make_unique<AssociationDefInft>(bus, inventoryObjPath.c_str());
    associationDefinitionsIntf->associations(
        {{"parent_device", "all_states", parentObjPath.c_str()}});
}

void NsmPortMetrics::updateCounterValues(struct nsm_port_counter_data* portData)
{
    if (portData)
    {
        if (iBPortIntf)
        {
            if (portData->supported_counter.port_rcv_pkts)
            {
                iBPortIntf->rxPkts(portData->port_rcv_pkts);
            }

            if (portData->supported_counter.port_multicast_rcv_pkts)
            {
                iBPortIntf->rxMulticastPkts(portData->port_multicast_rcv_pkts);
            }

            if (portData->supported_counter.port_unicast_rcv_pkts)
            {
                iBPortIntf->rxUnicastPkts(portData->port_unicast_rcv_pkts);
            }

            if (portData->supported_counter.port_malformed_pkts)
            {
                iBPortIntf->malformedPkts(portData->port_malformed_pkts);
            }

            if (portData->supported_counter.vl15_dropped)
            {
                iBPortIntf->vL15DroppedPkts(portData->vl15_dropped);
            }

            if (portData->supported_counter.port_rcv_errors)
            {
                iBPortIntf->rxErrors(portData->port_rcv_errors);
            }

            if (portData->supported_counter.port_xmit_pkts)
            {
                iBPortIntf->txPkts(portData->port_xmit_pkts);
            }

            if (portData->supported_counter.port_xmit_pkts_vl15)
            {
                iBPortIntf->vL15TXPkts(portData->port_xmit_pkts_vl15);
            }

            if (portData->supported_counter.port_xmit_data_vl15)
            {
                iBPortIntf->vL15TXData(portData->port_xmit_data_vl15);
            }

            if (portData->supported_counter.port_unicast_xmit_pkts)
            {
                iBPortIntf->txUnicastPkts(portData->port_unicast_xmit_pkts);
            }

            if (portData->supported_counter.port_multicast_xmit_pkts)
            {
                iBPortIntf->txMulticastPkts(portData->port_multicast_xmit_pkts);
            }

            if (portData->supported_counter.port_bcast_xmit_pkts)
            {
                iBPortIntf->txBroadcastPkts(portData->port_bcast_xmit_pkts);
            }

            if (portData->supported_counter.port_xmit_discard)
            {
                iBPortIntf->txDiscardPkts(portData->port_xmit_discard);
            }

            if (portData->supported_counter.port_neighbor_mtu_discards)
            {
                iBPortIntf->mtuDiscard(portData->port_neighbor_mtu_discards);
            }

            if (portData->supported_counter.port_rcv_ibg2_pkts)
            {
                iBPortIntf->ibG2RXPkts(portData->port_rcv_ibg2_pkts);
            }

            if (portData->supported_counter.port_xmit_ibg2_pkts)
            {
                iBPortIntf->ibG2TXPkts(portData->port_xmit_ibg2_pkts);
            }

            if (portData->supported_counter.symbol_error)
            {
                iBPortIntf->symbolError(portData->symbol_error);
            }

            if (portData->supported_counter.link_error_recovery_counter)
            {
                iBPortIntf->linkErrorRecoveryCounter(
                    portData->link_error_recovery_counter);
            }

            if (portData->supported_counter.link_downed_counter)
            {
                iBPortIntf->linkDownCount(portData->link_downed_counter);
            }

            if (portData->supported_counter.port_rcv_remote_physical_errors)
            {
                iBPortIntf->rxRemotePhysicalErrorPkts(
                    portData->port_rcv_remote_physical_errors);
            }

            if (portData->supported_counter.port_rcv_switch_relay_errors)
            {
                iBPortIntf->rxSwitchRelayErrorPkts(
                    portData->port_rcv_switch_relay_errors);
            }

            if (portData->supported_counter.QP1_dropped)
            {
                iBPortIntf->qP1DroppedPkts(portData->QP1_dropped);
            }

            if (portData->supported_counter.xmit_wait)
            {
                iBPortIntf->txWait(portData->xmit_wait);
            }
        }
        else
        {
            lg2::error(
                "NsmPortMetrics: updating counter value failed: iBPortIntf is NULL");
        }

        if (portMetricsOem2Intf)
        {
            if (portData->supported_counter.port_rcv_data)
            {
                portMetricsOem2Intf->rxBytes(portData->port_rcv_data);
            }

            if (portData->supported_counter.port_xmit_data)
            {
                portMetricsOem2Intf->txBytes(portData->port_xmit_data);
            }
        }
        else
        {
            lg2::error(
                "NsmPortMetrics: updating counter value failed: portMetricsOem2Intf is NULL");
        }
    }
    else
    {
        lg2::error(
            "NsmPortMetrics: updating counter value failed: portCounterInfo is NULL");
    }
}

std::optional<std::vector<uint8_t>>
    NsmPortMetrics::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_port_telemetry_counter_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_port_telemetry_counter_req(instanceId, portNumber,
                                                    requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_port_telemetry_counter_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPortMetrics::handleResponseMsg(const struct nsm_msg* responseMsg,
                                          size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_port_counter_data data;

    auto rc = decode_get_port_telemetry_counter_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateCounterValues(&data);
    }
    else
    {
        lg2::error(
            "responseHandler: get_port_telemetry_counter unsuccessfull. portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
            "NUM", portNumber, "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

static void createNsmPortSensor(SensorManager& manager,
                                const std::string& interface,
                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto parentObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ParentObjPath", interface.c_str());
    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto count = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "Count", interface.c_str());
    auto deviceType = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "DeviceType", interface.c_str());
    auto inventoryObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_NVlink PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    // create nvlink [as per count and also they are 1-based]
    for (uint64_t i = 1; i <= count; i++)
    {
        std::string portName = name + '_' + std::to_string(i);
        std::string objPath = inventoryObjPath + "/Ports/" + portName;

        if (deviceType == NSM_DEV_ID_GPU)
        {
            std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf =
                std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

            auto portStatusSensor = std::make_shared<NsmPortStatus>(
                bus, portName, i, type, portMetricsOem3Intf, objPath);
            if (!portStatusSensor)
            {
                lg2::error(
                    "Failed to create NSM Port status sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                    "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                    objPath);
            }
            else
            {
                nsmDevice->deviceSensors.emplace_back(portStatusSensor);
                if (priority)
                {
                    nsmDevice->prioritySensors.emplace_back(portStatusSensor);
                }
                else
                {
                    nsmDevice->roundRobinSensors.emplace_back(portStatusSensor);
                }
            }

            auto portCharacteristicsSensor =
                std::make_shared<NsmPortCharacteristics>(
                    bus, portName, i, type, portMetricsOem3Intf, objPath);
            if (!portCharacteristicsSensor)
            {
                lg2::error(
                    "Failed to create NSM Port characteristics sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                    "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                    objPath);
            }
            else
            {
                nsmDevice->deviceSensors.emplace_back(
                    portCharacteristicsSensor);
                if (priority)
                {
                    nsmDevice->prioritySensors.emplace_back(
                        portCharacteristicsSensor);
                }
                else
                {
                    nsmDevice->roundRobinSensors.emplace_back(
                        portCharacteristicsSensor);
                }
            }
        }

        auto portMetricsSensor = std::make_shared<NsmPortMetrics>(
            bus, portName, i, type, parentObjPath, objPath);
        if (!portMetricsSensor)
        {
            lg2::error(
                "Failed to create NSM Port Metric sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);
        }
        else
        {
            nsmDevice->deviceSensors.emplace_back(portMetricsSensor);
            if (priority)
            {
                nsmDevice->prioritySensors.emplace_back(portMetricsSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.emplace_back(portMetricsSensor);
            }
        }
    }
}

REGISTER_NSM_CREATION_FUNCTION(createNsmPortSensor,
                               "xyz.openbmc_project.Configuration.NSM_NVLink")

} // namespace nsm
