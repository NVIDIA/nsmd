#include "nsmPort.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
static std::string getTopologyObjPath(const std::string& deviceName,
                                      const uint8_t deviceType)
{
    std::string topologyObjPath =
        "/xyz/openbmc_project/inventory/system/linktopology/";
    switch (deviceType)
    {
        case NSM_DEV_ID_GPU:
            topologyObjPath += "GPU/";
            break;
        case NSM_DEV_ID_SWITCH:
            topologyObjPath += "SWITCH/";
            break;
        case NSM_DEV_ID_PCIE_BRIDGE:
            topologyObjPath += "PCIE_BRIDGE/";
            break;
        case NSM_DEV_ID_EROT:
            topologyObjPath += "EROT/";
            break;
        default:
            lg2::error("Topology not defined for device type = {DTYPE}",
                       "DTYPE", deviceType);
            return "";
    }
    topologyObjPath += deviceName;

    return topologyObjPath;
}

using LogicalPortNumber = uint8_t;
using TopologyData =
    std::map<ObjectPath,
             std::pair<LogicalPortNumber, std::vector<utils::Association>>>;

static requester::Coroutine coGetTopologyData(const std::string& topoObjPath,
                                              const std::string& topoIntfSubStr,
                                              TopologyData& topologyData)
{
    auto mapperResponse = co_await utils::coGetServiceMap(topoObjPath,
                                                          dbus::Interfaces{});
    topologyData.clear();
    std::vector<utils::Association> associationTmp;
    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& interface : interfaces)
        {
            if (interface.find(topoIntfSubStr) != std::string::npos)
            {
                auto inventoryObjPath =
                    co_await utils::coGetDbusProperty<std::string>(
                        topoObjPath.c_str(), "InventoryObjPath",
                        interface.c_str());
                auto logicalPortNumber =
                    co_await utils::coGetDbusProperty<uint64_t>(
                        topoObjPath.c_str(), "LogicalPortNumber",
                        interface.c_str());
                auto associations =
                    co_await utils::coGetDbusProperty<std::vector<std::string>>(
                        topoObjPath.c_str(), "Associations", interface.c_str());

                if (associations.size() % 3 != 0)
                {
                    lg2::error(
                        "Association in topology must follow (fwd, bck, absolutePath) for {OBJ}",
                        "OBJ", topoObjPath);
                }

                associationTmp.clear();
                for (uint8_t it = 0; it < associations.size(); it += 3)
                {
                    associationTmp.push_back({});
                    auto& tmp = associationTmp.back();

                    tmp.forward = associations[it];
                    tmp.backward = associations[it + 1];
                    tmp.absolutePath = associations[it + 2];
                    tmp.absolutePath =
                        utils::makeDBusNameValid(tmp.absolutePath);
                }

                inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);
                topologyData[inventoryObjPath] = {logicalPortNumber,
                                                  associationTmp};
            }
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

NsmPortStatus::NsmPortStatus(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type,
    std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Interface,
    std::string& inventoryObjPath) :
    NsmObject(portName, type),
    portName(portName), portNumber(portNum), objPath(inventoryObjPath)
{
    lg2::debug("NsmPortStatus: {NAME} with port number {NUM}", "NAME",
               portName.c_str(), "NUM", portNum);

    portStateIntf = std::make_unique<PortStateIntf>(bus,
                                                    inventoryObjPath.c_str());
    portMetricsOem3Intf = portMetricsOem3Interface;

    portStateIntf->linkStatus(PortLinkStatus::Starting);
    portStateIntf->linkState(PortLinkStates::Unknown);
    portMetricsOem3Intf->trainingError(false);
    portMetricsOem3Intf->runtimeError(false);
    updateMetricOnSharedMemory();
}

requester::Coroutine NsmPortStatus::update(SensorManager& manager, eid_t eid)
{
    std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
    auto rc = encode_query_port_status_req(0, portNumber, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_query_port_status_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, requestMsg, responseMsg,
                                         responseLen, false);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t portState = NSM_PORTSTATE_DOWN;
    uint8_t portStatus = NSM_PORTSTATUS_DISABLED;

    rc = decode_query_port_status_resp(responseMsg.get(), responseLen, &cc,
                                       &reasonCode, &dataSize, &portState,
                                       &portStatus);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        switch (portState)
        {
            case NSM_PORTSTATE_DOWN:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                break;
            case NSM_PORTSTATE_DOWN_LOCK:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                co_await checkPortCharactersticRCAndPopulateRuntimeErr(manager,
                                                                       eid);
                break;
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
        updateMetricOnSharedMemory();
        clearErrorBitMap("decode_query_port_status_resp");
    }
    else
    {
        if (shouldLogError(cc, rc))
        {
            lg2::error(
                "responseHandler: decode_query_port_status_resp unsuccessfull. portName={NAM} portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
                "NAM", portName, "NUM", portNumber, "RSNCOD", reasonCode, "CC",
                cc, "RC", rc);
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // coverity[missing_return]

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmPortStatus::checkPortCharactersticRCAndPopulateRuntimeErr(
        SensorManager& manager, eid_t eid)
{
    std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_characteristics_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
    auto rc = encode_query_port_characteristics_req(0, portNumber, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_query_port_characteristics_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, requestMsg, responseMsg,
                                         responseLen, false);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_port_characteristics_data data;

    rc = decode_query_port_characteristics_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &dataSize, &data);

    if (cc != NSM_SUCCESS && reasonCode != ERR_NULL)
    {
        portMetricsOem3Intf->runtimeError(true);
    }
    else
    {
        portMetricsOem3Intf->runtimeError(false);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmPortStatus::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(portMetricsOem3Intf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};
    std::string propName = "TrainingError";

    nv::sensor_aggregation::DbusVariantType variantTE{
        portMetricsOem3Intf->trainingError()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(objPath, ifaceName, propName,
                                                 rawSmbpbiData, variantTE);

    propName = "RuntimeError";
    nv::sensor_aggregation::DbusVariantType variantRE{
        portMetricsOem3Intf->runtimeError()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(objPath, ifaceName, propName,
                                                 rawSmbpbiData, variantRE);

    propName = "LinkStatus";
    ifaceName = std::string(portStateIntf->interface);
    nv::sensor_aggregation::DbusVariantType variantLS{
        portStateIntf->convertLinkStatusTypeToString(
            portStateIntf->linkStatus())};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(objPath, ifaceName, propName,
                                                 rawSmbpbiData, variantLS);
#endif
}

NsmPortCharacteristics::NsmPortCharacteristics(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type,
    std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Interface,
    std::string& inventoryObjPath) :
    NsmSensor(portName, type),
    portName(portName), portNumber(portNum), objPath(inventoryObjPath)
{
    lg2::debug("NsmPortCharacteristics: {NAME} with port number {NUM}", "NAME",
               portName.c_str(), "NUM", portNum);

    portInfoIntf = std::make_unique<PortInfoIntf>(bus,
                                                  inventoryObjPath.c_str());
    portMetricsOem3Intf = portMetricsOem3Interface;

    portInfoIntf->type(PortType::BidirectionalPort);
    portInfoIntf->protocol(PortProtocol::NVLink);
    updateMetricOnSharedMemory();
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
        lg2::debug(
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

        portMetricsOem3Intf->txNoProtocolBytes(data.nv_port_data_rate_kbps);
        portMetricsOem3Intf->rxNoProtocolBytes(data.nv_port_data_rate_kbps);

        uint16_t width = static_cast<uint16_t>(data.status_lane_info & 0x0F);
        portMetricsOem3Intf->txWidth(width);
        portMetricsOem3Intf->rxWidth(width);

        updateMetricOnSharedMemory();
        clearErrorBitMap("decode_query_port_characteristics_resp");
    }
    else
    {
        if (shouldLogError(cc, rc))
        {
            lg2::error(
                "responseHandler: decode_query_port_characteristics_resp unsuccessfull. portName={NAM} portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
                "NAM", portName, "NUM", portNumber, "RSNCOD", reasonCode, "CC",
                cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

void NsmPortCharacteristics::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePortInfoName = std::string(portInfoIntf->interface);
    auto ifacePortOem3Name = std::string(portMetricsOem3Intf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantCS{
        portInfoIntf->currentSpeed()};
    std::string propName = "CurrentSpeed";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantCS);

    nv::sensor_aggregation::DbusVariantType variantMS{portInfoIntf->maxSpeed()};
    propName = "MaxSpeed";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantMS);

    nv::sensor_aggregation::DbusVariantType variantTX{
        portMetricsOem3Intf->txNoProtocolBytes()};
    propName = "TXNoProtocolBytes";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantTX);

    nv::sensor_aggregation::DbusVariantType variantRX{
        portMetricsOem3Intf->rxNoProtocolBytes()};
    propName = "RXNoProtocolBytes";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantRX);

    nv::sensor_aggregation::DbusVariantType variantTXW{
        portMetricsOem3Intf->txWidth()};
    propName = "TXWidth";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantTXW);

    nv::sensor_aggregation::DbusVariantType variantRXW{
        portMetricsOem3Intf->rxWidth()};
    propName = "RXWidth";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantRXW);
#endif
}

NsmPortMetrics::NsmPortMetrics(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type, const uint8_t deviceType,
    const std::vector<utils::Association>& associations,
    std::string& parentObjPath, std::string& inventoryObjPath) :
    NsmSensor(portName, type),
    portName(portName), portNumber(portNum), typeOfDevice(deviceType),
    objPath(inventoryObjPath)
{
    lg2::debug(
        "NsmPortMetrics: {NAME} with port number {NUM} for device type {DT}",
        "NAME", portName.c_str(), "NUM", portNum, "DT", typeOfDevice);

    iBPortIntf = std::make_unique<IBPortIntf>(bus, inventoryObjPath.c_str());
    portIntf = std::make_unique<PortIntf>(bus, inventoryObjPath.c_str());
    portIntf->portNumber(portNum);

    portMetricsOem2Intf =
        std::make_unique<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    associationDefinitionsIntf =
        std::make_unique<AssociationDefInft>(bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_device", "all_states",
                                  parentObjPath.c_str());
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    associationDefinitionsIntf->associations(associationsList);
    updateMetricOnSharedMemory();
}

void NsmPortMetrics::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    std::vector<uint8_t> rawSmbpbiData = {};
    auto ifaceIBPortName = std::string(iBPortIntf->interface);
    auto ifacePortOem2Name = std::string(portMetricsOem2Intf->interface);

    nv::sensor_aggregation::DbusVariantType variantRXP{iBPortIntf->rxPkts()};
    std::string propName = "RXPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXP);

    nv::sensor_aggregation::DbusVariantType variantRXMP{
        iBPortIntf->rxMulticastPkts()};
    propName = "RXMulticastPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXMP);

    nv::sensor_aggregation::DbusVariantType variantRXUP{
        iBPortIntf->rxUnicastPkts()};
    propName = "RXUnicastPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXUP);

    nv::sensor_aggregation::DbusVariantType variantMP{
        iBPortIntf->malformedPkts()};
    propName = "MalformedPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantMP);

    nv::sensor_aggregation::DbusVariantType variantVLDP{
        iBPortIntf->vL15DroppedPkts()};
    propName = "VL15DroppedPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLDP);

    nv::sensor_aggregation::DbusVariantType variantRXE{iBPortIntf->rxErrors()};
    propName = "RXErrors";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXE);

    nv::sensor_aggregation::DbusVariantType variantTXP{iBPortIntf->txPkts()};
    propName = "TXPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXP);

    nv::sensor_aggregation::DbusVariantType variantVLTXP{
        iBPortIntf->vL15TXPkts()};
    propName = "VL15TXPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLTXP);

    nv::sensor_aggregation::DbusVariantType variantVLTXD{
        iBPortIntf->vL15TXData()};
    propName = "VL15TXData";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLTXD);

    nv::sensor_aggregation::DbusVariantType variantTXUP{
        iBPortIntf->txUnicastPkts()};
    propName = "TXUnicastPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXUP);

    nv::sensor_aggregation::DbusVariantType variantTXMP{
        iBPortIntf->txMulticastPkts()};
    propName = "TXMulticastPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXMP);

    nv::sensor_aggregation::DbusVariantType variantTXBP{
        iBPortIntf->txBroadcastPkts()};
    propName = "TXBroadcastPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXBP);

    nv::sensor_aggregation::DbusVariantType variantTXDP{
        iBPortIntf->txDiscardPkts()};
    propName = "TXDiscardPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXDP);

    nv::sensor_aggregation::DbusVariantType variantMTUD{
        iBPortIntf->mtuDiscard()};
    propName = "MTUDiscard";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantMTUD);

    nv::sensor_aggregation::DbusVariantType variantIBGRXP{
        iBPortIntf->ibG2RXPkts()};
    propName = "IBG2RXPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantIBGRXP);

    nv::sensor_aggregation::DbusVariantType variantIBGTXP{
        iBPortIntf->ibG2TXPkts()};
    propName = "IBG2TXPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantIBGTXP);

    nv::sensor_aggregation::DbusVariantType variantBER{
        iBPortIntf->bitErrorRate()};
    propName = "BitErrorRate";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantBER);

    nv::sensor_aggregation::DbusVariantType variantLERC{
        iBPortIntf->linkErrorRecoveryCounter()};
    propName = "LinkErrorRecoveryCounter";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantLERC);

    nv::sensor_aggregation::DbusVariantType variantLDC{
        iBPortIntf->linkDownCount()};
    propName = "LinkDownCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantLDC);

    nv::sensor_aggregation::DbusVariantType variantRXRPEP{
        iBPortIntf->rxRemotePhysicalErrorPkts()};
    propName = "RXRemotePhysicalErrorPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXRPEP);

    nv::sensor_aggregation::DbusVariantType variantRXSREP{
        iBPortIntf->rxSwitchRelayErrorPkts()};
    propName = "RXSwitchRelayErrorPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXSREP);

    nv::sensor_aggregation::DbusVariantType variantQPDP{
        iBPortIntf->qP1DroppedPkts()};
    propName = "QP1DroppedPkts";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantQPDP);

    nv::sensor_aggregation::DbusVariantType variantTXW{iBPortIntf->txWait()};
    propName = "TXWait";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXW);

    nv::sensor_aggregation::DbusVariantType variantRXB{
        portMetricsOem2Intf->rxBytes()};
    propName = "RXBytes";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem2Name, propName, rawSmbpbiData, variantRXB);

    nv::sensor_aggregation::DbusVariantType variantTXB{
        portMetricsOem2Intf->txBytes()};
    propName = "TXBytes";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortOem2Name, propName, rawSmbpbiData, variantTXB);

    nv::sensor_aggregation::DbusVariantType variantEBER{
        iBPortIntf->effectiveBER()};
    propName = "EffectiveBER";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantEBER);

    nv::sensor_aggregation::DbusVariantType variantEEBER{
        iBPortIntf->estimatedEffectiveBER()};
    propName = "EstimatedEffectiveBER";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantEEBER);
#endif
}

double NsmPortMetrics::getBitErrorRate(uint64_t value)
{
    if (value != 0)
    {
        uint8_t symbol_ber_magnitude = (value >> 8) & 0xFF; // bits 15-8
        uint8_t symbol_ber_coef_float = (value >> 4) & 0xF; // bits 7-4
        uint8_t symbol_ber_coef = value & 0xF;              // bits 3-0

        uint8_t digitCount = 1;
        if (symbol_ber_coef_float != 0)
        {
            digitCount =
                static_cast<int>(std::log10(std::abs(symbol_ber_coef_float))) +
                1;
        }

        // Calculate symbol_ber
        double symbol_ber = (symbol_ber_coef + (symbol_ber_coef_float /
                                                std::pow(10.0, digitCount))) *
                            pow(10, -symbol_ber_magnitude);

        return symbol_ber;
    }
    else
    {
        lg2::debug(
            "NsmPortMetrics: getBitErrorRate failed for {NAME} with port number {NUM} for device type {DT}",
            "NAME", portName.c_str(), "NUM", portNumber, "DT", typeOfDevice);
        return 0.0;
    }
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

            if (portData->supported_counter.symbol_ber)
            {
                iBPortIntf->bitErrorRate(getBitErrorRate(portData->symbol_ber));
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

            if (portData->supported_counter.effective_ber)
            {
                iBPortIntf->effectiveBER(
                    getBitErrorRate(portData->effective_ber));
            }

            if (portData->supported_counter.estimated_effective_ber)
            {
                iBPortIntf->estimatedEffectiveBER(
                    getBitErrorRate(portData->estimated_effective_ber));
            }
        }
        else
        {
            lg2::debug(
                "NsmPortMetrics: updating counter value failed: iBPortIntf is NULL for {NAME} with port number {NUM} for device type {DT}",
                "NAME", portName.c_str(), "NUM", portNumber, "DT",
                typeOfDevice);
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
            lg2::debug(
                "NsmPortMetrics: updating counter value failed: portMetricsOem2Intf is NULL for {NAME} with port number {NUM} for device type {DT}",
                "NAME", portName.c_str(), "NUM", portNumber, "DT",
                typeOfDevice);
        }
    }
    else
    {
        lg2::error(
            "NsmPortMetrics: updating counter value failed: portCounterInfo is NULL for {NAME} with port number {NUM} for device type {DT}",
            "NAME", portName.c_str(), "NUM", portNumber, "DT", typeOfDevice);
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
        lg2::debug(
            "encode_get_port_telemetry_counter_req failed for portNumber={NUM}, deviceType={DT}, eid={EID}, rc={RC}",
            "NUM", portNumber, "DT", typeOfDevice, "EID", eid, "RC", rc);
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
    struct nsm_port_counter_data data = {};

    auto rc = decode_get_port_telemetry_counter_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateCounterValues(&data);
        updateMetricOnSharedMemory();
        clearErrorBitMap("get_port_telemetry_counter");
    }
    else
    {
        if (shouldLogError(cc, rc))
        {
            lg2::error(
                "responseHandler: get_port_telemetry_counter unsuccessfull. deviceType={DT} portName={NAM} portNumber={NUM} reasonCode={RSNCOD} cc={CC} rc={RC}",
                "DT", typeOfDevice, "NAM", portNumber, "NUM", portNumber,
                "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

static requester::Coroutine createNsmPortSensor(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto parentObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "ParentObjPath", interface.c_str());
    auto priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto count = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "Count", interface.c_str());
    auto deviceType = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "DeviceType", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_NVlink PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    // get topology information from EM
    std::string deviceName =
        parentObjPath.substr(parentObjPath.find_last_of('/') + 1);
    std::string topologyIntfSubStr =
        "xyz.openbmc_project.Configuration.NVLinkTopology.Topology";
    std::string topologyObjPath = getTopologyObjPath(deviceName, deviceType);
    TopologyData deviceTopologies{};
    co_await coGetTopologyData(topologyObjPath, topologyIntfSubStr,
                               deviceTopologies);

    // create nvlink [as per count and also they are 1-based]
    for (uint64_t i = 0; i < count; i++)
    {
        uint8_t logicalPortNum = i + 1;
        std::string portName = name + '_' + std::to_string(i);
        std::string objPath = parentObjPath + "/Ports/" + portName;
        std::vector<utils::Association> associations;

        auto deviceTopologyIt = deviceTopologies.find(objPath);
        if (deviceTopologies.size() != 0 &&
            deviceTopologyIt != deviceTopologies.end())
        {
            logicalPortNum = deviceTopologyIt->second.first;
            associations = deviceTopologyIt->second.second;
        }
        else
        {
            lg2::debug(
                "Topology information not found for object port number: {PNUM} and path: {OBJP}",
                "PNUM", logicalPortNum, "OBJP", objPath);
        }

        if (deviceType == NSM_DEV_ID_GPU)
        {
            std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf =
                std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

            auto portStatusSensor = std::make_shared<NsmPortStatus>(
                bus, portName, logicalPortNum, type, portMetricsOem3Intf,
                objPath);
            nsmDevice->deviceSensors.emplace_back(portStatusSensor);
            if (priority)
            {
                nsmDevice->prioritySensors.emplace_back(portStatusSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.emplace_back(portStatusSensor);
            }

            auto portCharacteristicsSensor =
                std::make_shared<NsmPortCharacteristics>(
                    bus, portName, logicalPortNum, type, portMetricsOem3Intf,
                    objPath);
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
            bus, portName, logicalPortNum, type, deviceType, associations,
            parentObjPath, objPath);
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
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmPortSensor,
                               "xyz.openbmc_project.Configuration.NSM_NVLink")

} // namespace nsm
