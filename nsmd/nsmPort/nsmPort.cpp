#include "nsmPort.hpp"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
std::string getTopologyObjPath(const std::string& deviceName,
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
        case NSM_DEV_ID_CPU:
            topologyObjPath += "CPU/";
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

requester::Coroutine coGetTopologyData(const std::string& topoObjPath,
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
                auto allCurrentIfaceProperties =
                    co_await utils::coGetAllDbusProperty(
                        utils::entityManagerServiceStr, topoObjPath.c_str(),
                        interface.c_str());

                std::string inventoryObjPath{};
                if (allCurrentIfaceProperties.count("InventoryObjPath"))
                {
                    inventoryObjPath = std::get<std::string>(
                        allCurrentIfaceProperties.at("InventoryObjPath"));
                }
                uint64_t logicalPortNumber{};
                if (allCurrentIfaceProperties.count("LogicalPortNumber"))
                {
                    logicalPortNumber = std::get<uint64_t>(
                        allCurrentIfaceProperties.at("LogicalPortNumber"));
                }
                std::vector<std::string> associations{};
                if (allCurrentIfaceProperties.count("Associations"))
                {
                    associations = std::get<std::vector<std::string>>(
                        allCurrentIfaceProperties.at("Associations"));
                }

                if (associations.size() % 3 != 0)
                {
                    lg2::error(
                        "Association in topology must follow (fwd, bck, absolutePath) for {OBJ}",
                        "OBJ", topoObjPath);
                    continue;
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
    NsmObject(portName, type), portName(portName), portNumber(portNum),
    objPath(inventoryObjPath)
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

requester::Coroutine NsmPortStatus::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
    auto rc = encode_query_port_status_req(0, portNumber, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_query_port_status_req failed. eid={EID} rc={RC}",
                   "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), requestMsg,
                                      responseMsg, responseLen, false);
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

    if (shouldLog("decode_query_port_status_resp", reasonCode, cc, rc))
    {
        LG2_ERROR(
            "decode_query_port_status_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, portState: {PORTSTATE}, portStatus: {PORTSTATUS}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "PORTSTATE",
            portState, "PORTSTATUS", portStatus);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        switch (portState)
        {
            case NSM_PORTSTATE_DOWN:
            case NSM_PORTSTATE_SLEEP:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                break;
            case NSM_PORTSTATE_DOWN_LOCK:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                co_await checkPortCharactersticRCAndPopulateRuntimeErr(
                    nsmDevice);
                break;
            case NSM_PORTSTATE_UP:
                portStateIntf->linkStatus(PortLinkStatus::LinkUp);
                break;
            case NSM_PORTSTATE_POLLING:
                portStateIntf->linkStatus(PortLinkStatus::Starting);
                break;
            case NSM_PORTSTATE_RESERVED:
                portStateIntf->linkStatus(PortLinkStatus::NoLink);
                break;
            case NSM_PORTSTATE_TRAINING:
                portStateIntf->linkStatus(PortLinkStatus::Training);
                break;
            case NSM_PORTSTATE_TRAINING_FAILURE:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                portMetricsOem3Intf->trainingError(true);
                break;
            case NSM_PORTSTATE_TRAINING_FAILURE_LOCKED:
                portStateIntf->linkStatus(PortLinkStatus::LinkDown);
                break;
            case NSM_PORTSTATE_PHYSICAL_UP:
                portStateIntf->linkStatus(PortLinkStatus::Training);
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
    }

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmPortStatus::checkPortCharactersticRCAndPopulateRuntimeErr(
        std::shared_ptr<NsmDevice> nsmDevice)
{
    std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_port_characteristics_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(requestMsg.data());
    auto rc = encode_query_port_characteristics_req(0, portNumber, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_query_port_characteristics_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), requestMsg,
                                      responseMsg, responseLen);
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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceName, propName, rawSmbpbiData, variantTE);

    propName = "RuntimeError";
    nv::sensor_aggregation::DbusVariantType variantRE{
        portMetricsOem3Intf->runtimeError()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceName, propName, rawSmbpbiData, variantRE);

    propName = "LinkStatus";
    ifaceName = std::string(portStateIntf->interface);
    nv::sensor_aggregation::DbusVariantType variantLS{
        portStateIntf->convertLinkStatusTypeToString(
            portStateIntf->linkStatus())};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceName, propName, rawSmbpbiData, variantLS);
#endif
}

NsmPortCharacteristics::NsmPortCharacteristics(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type,
    std::shared_ptr<PortMetricsOem3Intf>& portMetricsOem3Interface,
    std::shared_ptr<IBPortIntf> iBPortIntf, std::string& inventoryObjPath) :
    NsmSensor(portName, type), portName(portName), iBPortIntf(iBPortIntf),
    portNumber(portNum), objPath(inventoryObjPath)
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

    LG2_ERROR_FLT(
        "decode_query_port_characteristics_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        auto speedGbps = (data.nv_port_line_rate_mbps) / 1000;
        portInfoIntf->maxSpeed(speedGbps);

        auto currSpeedGbps = (data.nv_port_data_rate_kbps) * 1e-6;
        portInfoIntf->currentSpeed(currSpeedGbps);

        portMetricsOem3Intf->txNoProtocolBytes(data.nv_port_data_rate_kbps);
        portMetricsOem3Intf->rxNoProtocolBytes(data.nv_port_data_rate_kbps);

        uint16_t width = static_cast<uint16_t>(data.status_lane_info & 0x0F);
        portMetricsOem3Intf->txWidth(width);
        portMetricsOem3Intf->rxWidth(width);
        updateLinkDownCode(data.port_status.port_down_reason_code);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPortCharacteristics::updateLinkDownCode(const uint32_t linkDownCode)
{
    // Map NSM port down reason codes to D-Bus LinkDownReasonCodes
    switch (linkDownCode)
    {
        case NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::NoLinkDown);
            break;
        case NSM_PORT_DOWN_REASON_CODE_UNKNOWN:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::Unknown);
            break;
        case NSM_PORT_DOWN_REASON_CODE_HI_SER_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::HighBitErrorRate);
            break;
        case NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::BlockLockLost);
            break;
        case NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::AlignmentLost);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::FECSyncLost);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PLL_LOCK_LOSS:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::PllLockLost);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FIFO_OVERFLOW:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::FIFOOverflow);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FALSE_SKIP_CONDITION:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::FalseSkipDetected);
            break;
        case NSM_PORT_DOWN_REASON_CODE_MINOR_ERR_THRESHOLD:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::MinorErrorThresholdExceeded);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PHY_LAYER_RETRANSMIT_TIMEOUT:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::PhyRetransmitTimeout);
            break;
        case NSM_PORT_DOWN_REASON_CODE_HEARTBEAT_ERRORS:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::HeartbeatErrors);
            break;
        case NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_CREDIT_MON_WD:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::CreditMonitorWatchdogTimeout);
            break;
        case NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_INTEGRITY_THRESHOLD:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::LinkLayerIntegrityThresholdExceeded);
            break;
        case NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_BUFFER_OVERRUN:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::LinkLayerBufferOverrun);
            break;
        case NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HEALTHY:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::OOBCommandLinkHealthy);
            break;
        case NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HI_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::OOBCommandLinkHighBER);
            break;
        case NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HEALTHY:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::InbandCommandLinkHealthy);
            break;
        case NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HI_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::InbandCommandLinkHighBER);
            break;
        case NSM_PORT_DOWN_REASON_CODE_DOWN_BY_VERIFICATION_GW:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::VerificationGatewayDown);
            break;
        case NSM_PORT_DOWN_REASON_CODE_RECEIVED_REMOTE_FAULT:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::RemoteFaultReceived);
            break;
        case NSM_PORT_DOWN_REASON_CODE_RECEIEVED_TS1:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::TrainingSequenceReceived);
            break;
        case NSM_PORT_DOWN_REASON_CODE_DOWN_BY_MGMT_CMD:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::ManagementCommandDown);
            break;
        case NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::CableDisconnected);
            break;
        case NSM_PORT_DOWN_REASON_CODE_CABLE_ACCESS_ISSUES:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::CableAccessFault);
            break;
        case NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::ThermalShutdown);
            break;
        case NSM_PORT_DOWN_REASON_CODE_CURRENT_ISSUE:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::CurrentIssue);
            break;
        case NSM_PORT_DOWN_REASON_CODE_POWER_BUDGET:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::PowerBudgetExceeded);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_RAW_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::FastRawBERRecovery);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_EFFECTIVE_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::FastEffectiveBERRecovery);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_SYMBOL_BER:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::FastSymbolBERRecovery);
            break;
        case NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_CREDIT_WATCHDOG:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::FastCreditWatchdogRecovery);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_SLEEP:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::PeerSleep);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::PeerDisabled);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE_LOCK:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::PeerDisableLocked);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_THERMAL_EVENT:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::PeerThermalEvent);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_FORCE_EVENT:
            iBPortIntf->linkDownReasonCode(
                LinkDownReasonCodes::PeerForcedEvent);
            break;
        case NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::PeerResetEvent);
            break;
        default:
            iBPortIntf->linkDownReasonCode(LinkDownReasonCodes::NoLinkDown);
            break;
    }
}

void NsmPortCharacteristics::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePortInfoName = std::string(portInfoIntf->interface);
    auto ifacePortOem3Name = std::string(portMetricsOem3Intf->interface);
    auto iBPortIntfName = std::string(iBPortIntf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantCS{
        portInfoIntf->currentSpeed()};
    std::string propName = "CurrentSpeed";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantCS);

    nv::sensor_aggregation::DbusVariantType variantMS{portInfoIntf->maxSpeed()};
    propName = "MaxSpeed";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantMS);

    nv::sensor_aggregation::DbusVariantType variantTX{
        portMetricsOem3Intf->txNoProtocolBytes()};
    propName = "TXNoProtocolBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantTX);

    nv::sensor_aggregation::DbusVariantType variantRX{
        portMetricsOem3Intf->rxNoProtocolBytes()};
    propName = "RXNoProtocolBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantRX);

    nv::sensor_aggregation::DbusVariantType variantTXW{
        portMetricsOem3Intf->txWidth()};
    propName = "TXWidth";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantTXW);

    nv::sensor_aggregation::DbusVariantType variantRXW{
        portMetricsOem3Intf->rxWidth()};
    propName = "RXWidth";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem3Name, propName, rawSmbpbiData, variantRXW);

    nv::sensor_aggregation::DbusVariantType linkDownReasonCode{
        xyz::openbmc_project::metrics::IBPort::
            convertLinkDownReasonCodesToString(
                iBPortIntf->linkDownReasonCode())};
    propName = "LinkDownReasonCode";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, iBPortIntfName, propName, rawSmbpbiData, linkDownReasonCode);
#endif
}

NsmPortMetrics::NsmPortMetrics(
    sdbusplus::bus::bus& bus, std::string& portName, uint8_t portNum,
    const std::string& type, const uint8_t deviceType,
    const std::vector<utils::Association>& associations,
    std::string& parentObjPath, std::string& inventoryObjPath,
    std::shared_ptr<IBPortIntf> iBPortIntf,
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf) :
    NsmSensor(portName, type), portName(portName), iBPortIntf(iBPortIntf),
    portMetricsOem2Intf(portMetricsOem2Intf),
    portPacketCountersIntf(portPacketCountersIntf), portNumber(portNum),
    typeOfDevice(deviceType), objPath(inventoryObjPath)
{
    lg2::debug(
        "NsmPortMetrics: {NAME} with port number {NUM} for device type {DT}",
        "NAME", portName.c_str(), "NUM", portNum, "DT", typeOfDevice);

    portIntf = std::make_unique<PortIntf>(bus, inventoryObjPath.c_str());
    portIntf->portNumber(portNum);

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
    auto ifacePortPacketCountersName =
        std::string(portPacketCountersIntf->interface);

    nv::sensor_aggregation::DbusVariantType variantRXP{iBPortIntf->rxPkts()};
    std::string propName = "RXPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXP);

    nv::sensor_aggregation::DbusVariantType variantRXMP{
        portPacketCountersIntf->rxMulticastPkts()};
    propName = "RXMulticastPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortPacketCountersName, propName, rawSmbpbiData,
        variantRXMP);

    nv::sensor_aggregation::DbusVariantType variantRXUP{
        portPacketCountersIntf->rxUnicastPkts()};
    propName = "RXUnicastPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortPacketCountersName, propName, rawSmbpbiData,
        variantRXUP);

    nv::sensor_aggregation::DbusVariantType variantMP{
        iBPortIntf->malformedPkts()};
    propName = "MalformedPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantMP);

    nv::sensor_aggregation::DbusVariantType variantVLDP{
        iBPortIntf->vL15DroppedPkts()};
    propName = "VL15DroppedPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLDP);

    nv::sensor_aggregation::DbusVariantType variantRXE{iBPortIntf->rxErrors()};
    propName = "RXErrors";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXE);

    nv::sensor_aggregation::DbusVariantType variantTXP{iBPortIntf->txPkts()};
    propName = "TXPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXP);

    nv::sensor_aggregation::DbusVariantType variantVLTXP{
        iBPortIntf->vL15TXPkts()};
    propName = "VL15TXPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLTXP);

    nv::sensor_aggregation::DbusVariantType variantVLTXD{
        iBPortIntf->vL15TXData()};
    propName = "VL15TXData";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantVLTXD);

    nv::sensor_aggregation::DbusVariantType variantTXUP{
        portPacketCountersIntf->txUnicastPkts()};
    propName = "TXUnicastPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortPacketCountersName, propName, rawSmbpbiData,
        variantTXUP);

    nv::sensor_aggregation::DbusVariantType variantTXMP{
        portPacketCountersIntf->txMulticastPkts()};
    propName = "TXMulticastPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortPacketCountersName, propName, rawSmbpbiData,
        variantTXMP);

    nv::sensor_aggregation::DbusVariantType variantTXBP{
        portPacketCountersIntf->txBroadcastPkts()};
    propName = "TXBroadcastPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortPacketCountersName, propName, rawSmbpbiData,
        variantTXBP);

    nv::sensor_aggregation::DbusVariantType variantTXDP{
        iBPortIntf->txDiscardPkts()};
    propName = "TXDiscardPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXDP);

    nv::sensor_aggregation::DbusVariantType variantMTUD{
        iBPortIntf->mtuDiscard()};
    propName = "MTUDiscard";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantMTUD);

    nv::sensor_aggregation::DbusVariantType variantIBGRXP{
        iBPortIntf->ibG2RXPkts()};
    propName = "IBG2RXPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantIBGRXP);

    nv::sensor_aggregation::DbusVariantType variantIBGTXP{
        iBPortIntf->ibG2TXPkts()};
    propName = "IBG2TXPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantIBGTXP);

    nv::sensor_aggregation::DbusVariantType variantBER{
        iBPortIntf->bitErrorRate()};
    propName = "BitErrorRate";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantBER);

    nv::sensor_aggregation::DbusVariantType variantLERC{
        iBPortIntf->linkErrorRecoveryCounter()};
    propName = "LinkErrorRecoveryCounter";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantLERC);

    nv::sensor_aggregation::DbusVariantType variantLDC{
        iBPortIntf->linkDownCount()};
    propName = "LinkDownCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantLDC);

    nv::sensor_aggregation::DbusVariantType variantRXRPEP{
        iBPortIntf->rxRemotePhysicalErrorPkts()};
    propName = "RXRemotePhysicalErrorPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXRPEP);

    nv::sensor_aggregation::DbusVariantType variantRXSREP{
        iBPortIntf->rxSwitchRelayErrorPkts()};
    propName = "RXSwitchRelayErrorPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRXSREP);

    nv::sensor_aggregation::DbusVariantType variantQPDP{
        iBPortIntf->qP1DroppedPkts()};
    propName = "QP1DroppedPkts";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantQPDP);

    nv::sensor_aggregation::DbusVariantType variantTXW{iBPortIntf->txWait()};
    propName = "TXWait";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTXW);

    nv::sensor_aggregation::DbusVariantType variantRXB{
        portMetricsOem2Intf->rxBytes()};
    propName = "RXBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem2Name, propName, rawSmbpbiData, variantRXB);

    nv::sensor_aggregation::DbusVariantType variantTXB{
        portMetricsOem2Intf->txBytes()};
    propName = "TXBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortOem2Name, propName, rawSmbpbiData, variantTXB);

    nv::sensor_aggregation::DbusVariantType variantEBER{
        iBPortIntf->effectiveBER()};
    propName = "EffectiveBER";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantEBER);

    nv::sensor_aggregation::DbusVariantType variantEEBER{
        iBPortIntf->estimatedEffectiveBER()};
    propName = "EstimatedEffectiveBER";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantEEBER);

    nv::sensor_aggregation::DbusVariantType variantEER{
        iBPortIntf->effectiveError()};
    propName = "EffectiveError";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantEER);

    nv::sensor_aggregation::DbusVariantType variantSE{
        iBPortIntf->symbolErrors()};
    propName = "SymbolErrors";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantSE);

    nv::sensor_aggregation::DbusVariantType variantRBER{
        iBPortIntf->totalRawBER()};
    propName = "TotalRawBER";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantRBER);

    nv::sensor_aggregation::DbusVariantType variantTRERR{
        iBPortIntf->totalRawError()};
    propName = "TotalRawError";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantTRERR);

    nv::sensor_aggregation::DbusVariantType variantULD{
        iBPortIntf->unintentionalLinkDownCount()};
    propName = "UnintentionalLinkDownCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantULD);

    nv::sensor_aggregation::DbusVariantType variantILD{
        iBPortIntf->intentionalLinkDownCount()};
    propName = "IntentionalLinkDownCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifaceIBPortName, propName, rawSmbpbiData, variantILD);
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

            if (portData->supported_counter.total_raw_error)
            {
                iBPortIntf->totalRawError(portData->total_raw_error);
            }

            if (portData->supported_counter.effective_error)
            {
                iBPortIntf->effectiveError(portData->effective_error);
            }

            if (portData->supported_counter.symbol_error)
            {
                iBPortIntf->symbolErrors(portData->symbol_error);
            }

            if (portData->supported_counter.total_raw_ber)
            {
                iBPortIntf->totalRawBER(
                    getBitErrorRate(portData->total_raw_ber));
            }

            if (portData->supported_counter.unintentional_link_down_count)
            {
                iBPortIntf->unintentionalLinkDownCount(
                    portData->unintentional_link_down_count);
            }

            if (portData->supported_counter.intentional_link_down_count)
            {
                iBPortIntf->intentionalLinkDownCount(
                    portData->intentional_link_down_count);
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

        if (portPacketCountersIntf)
        {
            if (portData->supported_counter.port_multicast_rcv_pkts)
            {
                portPacketCountersIntf->rxMulticastPkts(
                    portData->port_multicast_rcv_pkts);
            }

            if (portData->supported_counter.port_unicast_rcv_pkts)
            {
                portPacketCountersIntf->rxUnicastPkts(
                    portData->port_unicast_rcv_pkts);
            }

            if (portData->supported_counter.port_unicast_xmit_pkts)
            {
                portPacketCountersIntf->txUnicastPkts(
                    portData->port_unicast_xmit_pkts);
            }

            if (portData->supported_counter.port_multicast_xmit_pkts)
            {
                portPacketCountersIntf->txMulticastPkts(
                    portData->port_multicast_xmit_pkts);
            }

            if (portData->supported_counter.port_bcast_xmit_pkts)
            {
                portPacketCountersIntf->txBroadcastPkts(
                    portData->port_bcast_xmit_pkts);
            }
        }
        else
        {
            lg2::debug(
                "NsmPortMetrics: updating counter value failed: portPacketCountersIntf is NULL for {NAME} with port number {NUM} for device type {DT}",
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

    LG2_ERROR_FLT(
        "get_port_telemetry_counter failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateCounterValues(&data);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

EthPortTelemetryAggregator::EthPortTelemetryAggregator(
    sdbusplus::bus::bus& bus, std::string& portName, uint16_t portNumber,
    const std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf) :
    NsmSensorAggregator(portName, type), portName(portName),
    portNumber(portNumber), objPath(inventoryObjPath),
    portMetricsOem2Intf(portMetricsOem2Intf),
    portPacketCountersIntf(portPacketCountersIntf),
    tagToPropertyMap({
        {0, "RXBytes"},                   // Total Bytes Received
        {1, "TXBytes"},                   // Total Bytes Transmitted
        {2, "RXUnicastPkts"},             // Total Unicast Packets Received
        {3, "RXMulticastPkts"},           // Total Multicast Packets Received
        {4, "RXBroadcastPkts"},           // Total Broadcast Packets Received
        {5, "TXUnicastPkts"},             // Total Unicast Packets Transmitted
        {6, "TXMulticastPkts"},           // Total Multicast Packets Transmitted
        {7, "TXBroadcastPkts"},           // Total Broadcast Packets Transmitted
        {8, "RXFCSErrors"},               // FCS Receive Errors
        {9, "RXAlignmentErrors"},         // Alignment Errors
        {10, "RXFalseCarrierDetections"}, // False Carrier Detections
        {11, "RXRuntPkts"},               // Runt Packets Received
        {12, "RXJabberPkts"},             // Jabber Packets Received
        {13, "RXXONFrames"},              // Pause XON Frames Received
        {14, "RXXOFFFrames"},             // Pause XOFF Frames Received
        {15, "TXXONFrames"},              // Pause XON Frames Transmitted
        {16, "TXXOFFFrames"},             // Pause XOFF Frames Transmitted
        {17, "TXSingleCollisionFrames"},  // Single Collision Transmit Frames
        {18, "TXMultipleCollisionFrames"}, // Multiple Collision Transmit Frames
        {19, "TXLateCollisionFrames"},     // Late Collision Frames
        {20, "TXExcessCollisionFrames"},   // Excessive Collision Frames
    })
{
    lg2::debug("EthPortTelemetryAggregator: {NAME}", "NAME", portName.c_str());

    ethPortIntf = std::make_unique<EthPortIntf>(bus, inventoryObjPath.c_str());

    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    EthPortTelemetryAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_ethernet_port_telemetry_counter_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    // Use the eth port telemetry counter command
    auto rc = encode_get_eth_port_telemetry_counter_req(instanceId, portNumber,
                                                        requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_eth_port_telemetry_counter_req failed. eid={EID} rc={RC} port={PORT}",
            "EID", eid, "RC", rc, "PORT", portNumber);
        return std::nullopt;
    }

    return request;
}

int EthPortTelemetryAggregator::handleSample(const TelemetrySample& sample)
{
    bool result = NSM_SW_SUCCESS;
    if (!sample.valid ||
        sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        return result;

    nsm_ethernet_port_counter_data counterValue = {};
    size_t dataLen = sample.data_len;

    int rc = decode_aggregate_eth_port_telemetry_data(
        sample.data, &dataLen, sample.tag, &counterValue);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "Failed to decode Ethernet port telemetry data for tag {TAG} : rc = {RC}",
            "TAG", sample.tag, "RC", rc);
        result = false;
        return result;
    }

    updateCounterValues(sample.tag, &counterValue);
#ifdef NVIDIA_SHMEM
    std::vector<uint8_t> smbusData = {};
    std::string ifaceName = std::string(ethPortIntf->interface);
    std::string propName = "";
    auto it = tagToPropertyMap.find(sample.tag);
    if (it != tagToPropertyMap.end())
    {
        propName = it->second;
        getInterfaceName(propName, ifaceName);
        nv::sensor_aggregation::DbusVariantType dbusValue{
            counterValue.ethernet_port_counter_data_64bit};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            objPath, ifaceName, propName, smbusData, dbusValue);
    }

#endif

    return result;
}

void EthPortTelemetryAggregator::updateCounterValues(
    uint8_t tag, nsm_ethernet_port_counter_data* counterValue)
{
    auto it = tagToPropertyMap.find(tag);
    if (it != tagToPropertyMap.end())
    {
        const std::string& propName = it->second;

        try
        {
            if (propName == "RXBytes")
                portMetricsOem2Intf->rxBytes(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "TXBytes")
                portMetricsOem2Intf->txBytes(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "RXUnicastPkts")
                portPacketCountersIntf->rxUnicastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "RXMulticastPkts")
                portPacketCountersIntf->rxMulticastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "RXBroadcastPkts")
                portPacketCountersIntf->rxBroadcastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "TXUnicastPkts")
                portPacketCountersIntf->txUnicastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "TXMulticastPkts")
                portPacketCountersIntf->txMulticastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "TXBroadcastPkts")
                portPacketCountersIntf->txBroadcastPkts(
                    counterValue->ethernet_port_counter_data_64bit);
            else if (propName == "RXFCSErrors")
                ethPortIntf->rxfcsErrors(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXAlignmentErrors")
                ethPortIntf->rxAlignmentErrors(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXFalseCarrierDetections")
                ethPortIntf->rxFalseCarrierDetections(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXRuntPkts")
                ethPortIntf->rxRuntPkts(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXJabberPkts")
                ethPortIntf->rxJabberPkts(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXXONFrames")
                ethPortIntf->rxxonFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "RXXOFFFrames")
                ethPortIntf->rxxoffFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXXONFrames")
                ethPortIntf->txxonFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXXOFFFrames")
                ethPortIntf->txxoffFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXSingleCollisionFrames")
                ethPortIntf->txSingleCollisionFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXMultipleCollisionFrames")
                ethPortIntf->txMultipleCollisionFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXLateCollisionFrames")
                ethPortIntf->txLateCollisionFrames(
                    counterValue->ethernet_port_counter_data_32bit);
            else if (propName == "TXExcessCollisionFrames")
                ethPortIntf->txExcessCollisionFrames(
                    counterValue->ethernet_port_counter_data_32bit);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to update property {PROP} with value {VALUE}: {ERR}",
                "PROP", propName, "VALUE", counterValue, "ERR", e.what());
            return;
        }
    }
}

void EthPortTelemetryAggregator::getInterfaceName(const std::string propName,
                                                  std::string& ifaceName)
{
    if (propName == "RXBytes")
    {
        ifaceName = std::string(portMetricsOem2Intf->interface);
    }
    else if (propName == "TXBytes")
    {
        ifaceName = std::string(portMetricsOem2Intf->interface);
    }
    else if (propName == "RXUnicastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
    else if (propName == "RXMulticastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
    else if (propName == "RXBroadcastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
    else if (propName == "TXUnicastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
    else if (propName == "TXMulticastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
    else if (propName == "TXBroadcastPkts")
    {
        ifaceName = std::string(portPacketCountersIntf->interface);
    }
}

NsmNetworkAddressAggregator::NsmNetworkAddressAggregator(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& objPath, const std::string& nodeGuidObjPath,
    const std::string& ethernetMacAddressObjPath,
    const std::string& permanentMacAddressObjPath, uint16_t portNumber) :
    NsmSensor(name, type), portNumber(portNumber)
{
    linkTypeIntf = std::make_unique<LinkTypeIntf>(bus, objPath.c_str());
    portGuidIntf = std::make_unique<GuidIntf>(bus, objPath.c_str());
    nodeGuidIntf = std::make_unique<GuidIntf>(bus, nodeGuidObjPath.c_str());
    macAddressIntf = std::make_unique<MACAddressIntf>(
        bus, ethernetMacAddressObjPath.c_str());
    permanentMacAddressIntf = std::make_unique<MACAddressIntf>(
        bus, permanentMacAddressObjPath.c_str());
    samples.reserve(256);
}

std::optional<std::vector<uint8_t>>
    NsmNetworkAddressAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_network_addresses_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, portNumber,
                                               requestPtr);
    if (rc)
    {
        lg2::debug("encode_get_network_addresses_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmNetworkAddressAggregator::handleResponseMsg(const nsm_msg* responseMsg,
                                                   size_t responseLen)
{
    uint8_t returnValue = NSM_SW_SUCCESS;
    uint8_t cc = NSM_SUCCESS;
    size_t consumedLen{};
    auto responseData = reinterpret_cast<const uint8_t*>(responseMsg);

    uint16_t telemetryCount{};

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumedLen, &cc,
                                    &telemetryCount);

    if (shouldLog("decode_aggregate_resp", uint16_t(0), cc, rc))
    {
        LG2_ERROR("decode_aggregate_resp | cc: {CC}, rc: {RC}", "CC", cc, "RC",
                  rc);
    }

    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        return cc ? cc : rc;
    }
    responseData += consumedLen;
    responseLen -= consumedLen;
    samples.clear();
    while (telemetryCount--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t dataLen;

        auto sampleData =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(responseData);

        rc = decode_aggregate_resp_sample(sampleData, responseLen, &consumedLen,
                                          &tag, &valid, &data, &dataLen);

        responseData += consumedLen;
        responseLen -= consumedLen;

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "responseHandler: decode_aggregate_resp_sample failed. "
                "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}, valid_bit={VALID}",
                "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC", rc,
                "VALID", valid);
            continue;
        }
        if (tag == NSM_TAG_LINK_TYPE)
        {
            switch (data[0])
            {
                case NSM_PORT_PROTOCOL_ETHERNET:
                    linkType = NSM_PORT_PROTOCOL_ETHERNET;
                    break;
                case NSM_PORT_PROTOCOL_INFINIBAND:
                    linkType = NSM_PORT_PROTOCOL_INFINIBAND;
                    break;
                default:
                    break;
            }
            continue;
        }
        samples.emplace_back(tag, dataLen, data, valid);
    }
    if (linkType == NSM_PORT_PROTOCOL_UNKNOWN)
    {
        lg2::debug("Failed to get link type for port number {PNUM}", "PNUM",
                   portNumber);
        return NSM_SW_ERROR_DATA;
    }
    for (const auto& sample : samples)
    {
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            continue;
        }
        if (!sample.valid)
        {
            continue;
        }

        network_address_sample_data data;
        auto decodeRc = decode_aggregate_network_address_data(
            sample.tag, sample.data, sample.data_len, &data);
        if (decodeRc != NSM_SUCCESS)
        {
            lg2::error("Failed to decode network address sample data: {RC}",
                       "RC", decodeRc);
            returnValue = decodeRc;
            continue;
        }

        if (linkType == NSM_PORT_PROTOCOL_ETHERNET)
        {
            linkTypeIntf->linkType(PossibleLinks::Ethernet);
            if (sample.tag == NSM_TAG_MAC_ADDRESS)
            {
                std::string macStr;
                utils::convertMacAddressToString(data.mac_address,
                                                 MAC_ADDRESS_LENGTH, macStr);
                macAddressIntf->macAddress(macStr.c_str());
            }
            else if (sample.tag == NSM_TAG_PERMANENT_MAC_ADDRESS)
            {
                std::string permanentMacStr;
                utils::convertMacAddressToString(
                    data.mac_address, MAC_ADDRESS_LENGTH, permanentMacStr);
                permanentMacAddressIntf->macAddress(permanentMacStr.c_str());
            }
            else
            {
                lg2::error("Invalid Tag = {TAG} for link type: {LT}", "TAG",
                           sample.tag, "LT", linkType);
            }
        }
        else if (linkType == NSM_PORT_PROTOCOL_INFINIBAND)
        {
            linkTypeIntf->linkType(PossibleLinks::InfiniBand);
            if (sample.tag == NSM_TAG_NODE_GUID)
            {
                std::string IBNodeGuidStr;
                utils::convertGuid64ToString(data.network_identifier_64bit,
                                             IBNodeGuidStr);
                nodeGuidIntf->guid(IBNodeGuidStr);
            }
            else if (sample.tag == NSM_TAG_PORT_GUID)
            {
                std::string IBPortGuidStr;
                utils::convertGuid64ToString(data.network_identifier_64bit,
                                             IBPortGuidStr);
                portGuidIntf->guid(IBPortGuidStr);
            }
            else
            {
                lg2::error("Invalid Tag = {TAG} for link type: {LT}", "TAG",
                           sample.tag, "LT", linkType);
            }
        }
        else
        {
            lg2::error("Invalid Port link type: {LT}", "LT", linkType);
        }
    }
    return returnValue;
}

NsmGetPortECCCounters::NsmGetPortECCCounters(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath, uint8_t portNumber) :
    NsmSensor(name, type), objPath(inventoryObjPath), portNumber(portNumber)
{
    portECCIntf = std::make_unique<PortECCIntf>(bus, objPath.c_str());
#ifdef NVIDIA_SHMEM
    updateMetricOnSharedMemory();
#endif
}

std::optional<std::vector<uint8_t>>
    NsmGetPortECCCounters::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_port_ecc_counters_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_port_ecc_counters_req(instanceId, portNumber,
                                               requestPtr);
    if (rc)
    {
        lg2::debug("encode_get_port_ecc_counters_req failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmGetPortECCCounters::handleResponseMsg(const nsm_msg* responseMsg,
                                                 size_t responseLen)
{
    uint8_t returnValue = NSM_SW_SUCCESS;
    uint8_t cc = NSM_SUCCESS;
    size_t consumedLen{};
    auto responseData = reinterpret_cast<const uint8_t*>(responseMsg);

    uint16_t telemetryCount{};

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumedLen, &cc,
                                    &telemetryCount);

    if (shouldLog("decode_aggregate_resp", uint16_t(0), cc, rc))
    {
        LG2_ERROR("decode_aggregate_resp | cc: {CC}, rc: {RC}", "CC", cc, "RC",
                  rc);
    }

    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        return cc ? cc : rc;
    }
    responseData += consumedLen;
    responseLen -= consumedLen;
    bool updateRawErrorsPerLane = false;
    std::vector<uint64_t> rawErrorsPerLane(RAW_ERRORS_PER_LANE_COUNT, 0);
    while (telemetryCount--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t dataLen;

        auto sampleData =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(responseData);

        rc = decode_aggregate_resp_sample(sampleData, responseLen, &consumedLen,
                                          &tag, &valid, &data, &dataLen);

        responseData += consumedLen;
        responseLen -= consumedLen;

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "responseHandler: decode_aggregate_resp_sample failed. "
                "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}, valid_bit={VALID}",
                "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC", rc,
                "VALID", valid);
            returnValue = rc;
            continue;
        }
        if (!valid || tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            continue;
        }
        uint64_t counterData;
        auto decodeRc = decode_aggregate_port_ecc_counter_data(
            tag, data, dataLen, &counterData);
        if (decodeRc != NSM_SUCCESS)
        {
            lg2::debug("Failed to decode port ecc counters sample data: {RC}",
                       "RC", decodeRc);
            returnValue = decodeRc;
        }
        else
        {
            switch (tag)
            {
                case NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES:
                    portECCIntf->symbolErrorRXBytes(counterData);
                    break;
                case NSM_TAG_ECC_CORRECTED_BITS:
                    portECCIntf->correctedBits(counterData);
                    break;
                case NSM_TAG_ECC_RAW_ERRORS_LANE_0:
                    updateRawErrorsPerLane = true;
                    rawErrorsPerLane[0] += counterData;
                    break;
                case NSM_TAG_ECC_RAW_ERRORS_LANE_1:
                    updateRawErrorsPerLane = true;
                    rawErrorsPerLane[1] += counterData;
                    break;
                case NSM_TAG_ECC_RAW_ERRORS_LANE_2:
                    updateRawErrorsPerLane = true;
                    rawErrorsPerLane[2] += counterData;
                    break;
                case NSM_TAG_ECC_RAW_ERRORS_LANE_3:
                    updateRawErrorsPerLane = true;
                    rawErrorsPerLane[3] += counterData;
                    break;
                default:
                    break;
            }
        }
    }
    if (updateRawErrorsPerLane)
    {
        portECCIntf->rawErrorsPerLane(rawErrorsPerLane);
    }

    updateMetricOnSharedMemory();
    return returnValue;
}

void NsmGetPortECCCounters::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(portECCIntf->interface);
    std::vector<uint8_t> smbusData = {};

    // Update SymbolErrorRXBytes
    {
        nv::sensor_aggregation::DbusVariantType symbolErrorsValue{
            portECCIntf->symbolErrorRXBytes()};
        std::string propertyName = "SymbolErrorRXBytes";
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            objPath, ifaceName, propertyName, smbusData, symbolErrorsValue);
    }

    // Update CorrectedBits
    {
        nv::sensor_aggregation::DbusVariantType correctedBitsValue{
            portECCIntf->correctedBits()};
        std::string propertyName = "CorrectedBits";
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            objPath, ifaceName, propertyName, smbusData, correctedBitsValue);
    }
#endif
}

requester::Coroutine createNsmPortSensor(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath,
                                         bool enableNetworkPortAddresses)
{
    auto& bus = utils::DBusHandler::getBus();
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    std::string parentObjPath{};
    if (allCurrentIfaceProperties.count("ParentObjPath"))
    {
        parentObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("ParentObjPath"));
    }
    bool priority{};
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }
    uint64_t count{};
    if (allCurrentIfaceProperties.count("Count"))
    {
        count = std::get<uint64_t>(allCurrentIfaceProperties.at("Count"));
    }
    uint64_t deviceType{};
    if (allCurrentIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceType"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
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
        std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
        std::string ethernetMacAddressObjPath = objPath +
                                                "/Ethernet_MAC_Address";
        std::string permanentMacAddressObjPath = objPath +
                                                 "/Permanent_MAC_Address";
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

        if (enableNetworkPortAddresses)
        {
            associations.emplace_back("parent_device",
                                      "network_device_functions",
                                      parentObjPath.c_str());
            associations.emplace_back("associated_infiniband_port_address",
                                      "associated_port",
                                      nodeGuidObjPath.c_str());
            associations.emplace_back("associated_ethernet_port_address",
                                      "associated_port",
                                      ethernetMacAddressObjPath.c_str());
            associations.emplace_back("associated_ethernet_port_address",
                                      "associated_port",
                                      permanentMacAddressObjPath.c_str());

            auto aggregateNetworkAddressesSensor =
                std::make_shared<NsmNetworkAddressAggregator>(
                    bus, portName, type, objPath, nodeGuidObjPath,
                    ethernetMacAddressObjPath, permanentMacAddressObjPath,
                    static_cast<uint16_t>(logicalPortNum));
            nsmDevice->addSensor(aggregateNetworkAddressesSensor, false);
        }

        auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());
        auto portMetricsOem2Intf =
            std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
        auto portPacketCountersIntf =
            std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());
        if (deviceType == NSM_DEV_ID_GPU)
        {
            std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf =
                std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());

            auto portStatusSensor = std::make_shared<NsmPortStatus>(
                bus, portName, logicalPortNum, type, portMetricsOem3Intf,
                objPath);
            nsmDevice->addSensor(portStatusSensor, priority);

            auto portCharacteristicsSensor =
                std::make_shared<NsmPortCharacteristics>(
                    bus, portName, logicalPortNum, type, portMetricsOem3Intf,
                    iBPortIntf, objPath);
            if (!portCharacteristicsSensor)
            {
                lg2::error(
                    "Failed to create NSM Port characteristics sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                    "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                    objPath);
            }
            else
            {
                nsmDevice->addSensor(portCharacteristicsSensor, priority);
            }
        }

        auto portMetricsSensor = std::make_shared<NsmPortMetrics>(
            bus, portName, logicalPortNum, type, deviceType, associations,
            parentObjPath, objPath, iBPortIntf, portMetricsOem2Intf,
            portPacketCountersIntf);
        if (nsmDevice->getDeviceType() == NSM_DEV_ID_PCIE_BRIDGE &&
            (nsmDevice->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX8 ||
             nsmDevice->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9 ||
             nsmDevice->getDeviceRole() ==
                 NSM_PCIE_BRIDGE_DEV_ROLE_CX_BLUEFIELD_NIC))
        {
            auto portECCCountersSensor =
                std::make_shared<NsmGetPortECCCounters>(
                    bus, portName, type, objPath,
                    static_cast<uint16_t>(logicalPortNum));
            nsmDevice->addSensor(portECCCountersSensor, priority);
        }

        if (!portMetricsSensor)
        {
            lg2::error(
                "Failed to create NSM Port Metric sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);
        }
        else
        {
            nsmDevice->addSensor(portMetricsSensor, priority);
        }

        if (nsmDevice->getDeviceType() == NSM_DEV_ID_PCIE_BRIDGE &&
            (nsmDevice->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX8 ||
             nsmDevice->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9 ||
             nsmDevice->getDeviceRole() ==
                 NSM_PCIE_BRIDGE_DEV_ROLE_CX_BLUEFIELD_NIC))
        {
            auto ethPortMetricsSensor =
                std::make_shared<EthPortTelemetryAggregator>(
                    bus, portName, static_cast<uint16_t>(logicalPortNum), type,
                    objPath, portMetricsOem2Intf, portPacketCountersIntf);
            nsmDevice->addSensor(ethPortMetricsSensor, priority);
        }

#ifdef NVIDIA_HISTOGRAM
        if (deviceType != NSM_DEV_ID_PCIE_BRIDGE)
        {
            // add FEC histogram
            std::string histoObjName = "FEC_0";
            std::string histoDbusObjPath = objPath + "/Histograms/" +
                                           histoObjName;

            uint32_t fecHistogramID = 0;
            fecHistogramID =
                (static_cast<uint32_t>(NSM_HISTOGRAM_NAMESPACE_ID_ERROR)
                 << SHIFT_BITS_24) |
                (static_cast<uint32_t>(NSM_HISTOGRAM_REVISION_ID_0)
                 << SHIFT_BITS_16) |
                (static_cast<uint32_t>(NSM_HISTOGRAM_ID_FEC));

            auto fecHistoFormatIntf =
                std::make_shared<FormatIntf>(bus, histoDbusObjPath.c_str());
            auto histoBucketDataIntf =
                std::make_shared<BucketInfoIntf>(bus, histoDbusObjPath.c_str());
            std::vector<std::tuple<std::string, std::string, std::string>>
                associationsList;
            associationsList.emplace_back("parent_port", "histograms", objPath);
            auto getFECHistoFormatObject = std::make_shared<NsmHistogramFormat>(
                bus, histoObjName, deviceName + "_FEC_Histogram",
                fecHistoFormatIntf, histoBucketDataIntf, objPath,
                associationsList, fecHistogramID, logicalPortNum);

            auto getFECHistoDataObject = std::make_shared<NsmHistogramData>(
                histoObjName, deviceName + "_FEC_Histogram", fecHistoFormatIntf,
                histoBucketDataIntf, fecHistogramID, logicalPortNum);

            nsmDevice->addStaticSensor(getFECHistoFormatObject);
            nsmDevice->addSensor(getFECHistoDataObject, false);
        }
#endif

        manager.deviceToPortMap[nsmDevice][logicalPortNum] = portName;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createNsmPortSensorWithNetworkPortAddresses(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)
{
    auto rc = co_await createNsmPortSensor(manager, interface, objPath, true);
    co_return rc;
}

requester::Coroutine createNsmPortSensorGeneric(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)
{
    auto rc = co_await createNsmPortSensor(manager, interface, objPath, false);
    co_return rc;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmPortSensorWithNetworkPortAddresses,
    "xyz.openbmc_project.Configuration.NSM_NVLinkWithNetworkPortAddresses")
REGISTER_NSM_CREATION_FUNCTION(createNsmPortSensorGeneric,
                               "xyz.openbmc_project.Configuration.NSM_NVLink")

} // namespace nsm
