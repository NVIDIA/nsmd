#include "nsmPort.hpp"

#include "common/types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmPort::NsmPort(sdbusplus::bus::bus& bus, std::string& portName,
                 uint8_t portNum, const std::string& type,
                 std::string& parentObjPath, std::string& inventoryObjPath) :
    NsmSensor(portName, type), portName(portName), portNumber(portNum)
{
    std::string objPath = inventoryObjPath + "/Ports/" + portName;
    lg2::debug("NsmPort: create port: {NAME}", "NAME", portName.c_str());

    iBPortIntf = std::make_unique<IBPortIntf>(bus, objPath.c_str());

    portMetricsOem2Intf =
        std::make_unique<PortMetricsOem2Intf>(bus, objPath.c_str());

    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    associationDefinitionsIntf->associations(
        {{"parent_device", "all_states", parentObjPath.c_str()}});
}

void NsmPort::updateCounterValues(struct nsm_port_counter_data* portData)
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
                "NsmPort: updating counter value failed: iBPortIntf is NULL");
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
                "NsmPort: updating counter value failed: portMetricsOem2Intf is NULL");
        }
    }
    else
    {
        lg2::error(
            "NsmPort: updating counter value failed: portCounterInfo is NULL");
    }
}

std::optional<std::vector<uint8_t>> NsmPort::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
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

uint8_t NsmPort::handleResponseMsg(const struct nsm_msg* responseMsg,
                                   size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_port_counter_data data;

    auto rc = decode_get_port_telemetry_counter_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateCounterValues(&data);
    }
    else
    {
        lg2::error(
            "responseHandler: get_port_telemetry_counter unsuccessfull. portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
            "NUM", portNumber, "RSNCOD", reason_code, "CC", cc, "RC", rc);
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
        auto portSensor = std::make_shared<NsmPort>(
            bus, portName, i, type, parentObjPath, inventoryObjPath);
        if (!portSensor)
        {
            lg2::error(
                "Failed to create NSM Port : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);

            return;
        }

        nsmDevice->deviceSensors.emplace_back(portSensor);
        if (priority)
        {
            nsmDevice->prioritySensors.emplace_back(portSensor);
        }
        else
        {
            nsmDevice->roundRobinSensors.emplace_back(portSensor);
        }
    }
}

REGISTER_NSM_CREATION_FUNCTION(createNsmPortSensor,
                               "xyz.openbmc_project.Configuration.NSM_NVLink")

} // namespace nsm