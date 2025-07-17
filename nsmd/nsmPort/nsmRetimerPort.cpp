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
    NsmObject(portName, type),
    portName(portName)
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

// TODO refactor usage to NsmPCIeLinkSpeed<PortInfoIntf>
NsmPCIeECCGroup1::NsmPCIeECCGroup1(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   std::shared_ptr<PortWidthIntf> portWidthIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_1),
    objPath(inventoryPath), portInfoIntf(portInfoIntf),
    portWidthIntf(portWidthIntf)
{
    lg2::info("NsmPCIeECCGroup1: {NAME}", "NAME", name.c_str());

    portInfoIntf->maxSpeed(0);
    portInfoIntf->currentSpeed(0);
    portWidthIntf->width(0);
    portWidthIntf->activeWidth(0);

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup1::NsmPCIeECCGroup1(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   std::shared_ptr<PortWidthIntf> portWidthIntf,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_1, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), portInfoIntf(portInfoIntf),
    portWidthIntf(portWidthIntf)
{
    lg2::info("NsmMultiPCIeECCGroup1: {NAME}", "NAME", name.c_str());

    portInfoIntf->maxSpeed(0);
    portInfoIntf->currentSpeed(0);
    portWidthIntf->width(0);
    portWidthIntf->activeWidth(0);

    updateMetricOnSharedMemory();
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
            lg2::debug("NsmPCIeECCGroup1: {NAME}, unknown speed {SPEED}",
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
    uint16_t dataSize;
    uint16_t reasonCode = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_1 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup1 decode_query_scalar_group_telemetry_v1_group1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        portInfoIntf->maxSpeed(convertEncodedSpeedToGbps(data.max_link_speed));
        portInfoIntf->currentSpeed(
            convertEncodedSpeedToGbps(data.negotiated_link_speed));
        portWidthIntf->width(
            convertEncodedWidthToActualWidth(data.max_link_width));
        portWidthIntf->activeWidth(
            convertEncodedWidthToActualWidth(data.negotiated_link_width));
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPCIeECCGroup1::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePortInfoName = std::string(portInfoIntf->interface);
    auto ifacePortWidthName = std::string(portWidthIntf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantCS{
        portInfoIntf->currentSpeed()};
    std::string propName = "CurrentSpeed";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantCS);

    nv::sensor_aggregation::DbusVariantType variantAW{
        portWidthIntf->activeWidth()};
    propName = "ActiveWidth";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePortWidthName, propName, rawSmbpbiData, variantAW);
#endif
}

// TODO refactor usage to NsmPCIeErrors(2)
NsmPCIeECCGroup2::NsmPCIeECCGroup2(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_2),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmPCIeECCGroup2: {NAME}", "NAME", name.c_str());

    pcieEccIntf->nonfeCount(0);
    pcieEccIntf->feCount(0);
    pcieEccIntf->ceCount(0);
    pcieEccIntf->unsupportedRequestCount(0);

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup2::NsmPCIeECCGroup2(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_2, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmMultiPCIeECCGroup2: {NAME}", "NAME", name.c_str());

    pcieEccIntf->nonfeCount(0);
    pcieEccIntf->feCount(0);
    pcieEccIntf->ceCount(0);
    pcieEccIntf->unsupportedRequestCount(0);

    updateMetricOnSharedMemory();
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

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup2 decode_query_scalar_group_telemetry_v1_group2_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        pcieEccIntf->nonfeCount(data.non_fatal_errors);
        pcieEccIntf->feCount(data.fatal_errors);
        pcieEccIntf->ceCount(data.correctable_errors);
        pcieEccIntf->unsupportedRequestCount(data.unsupported_request_count);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPCIeECCGroup2::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePCIeEccName = std::string(pcieEccIntf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantNFC{
        pcieEccIntf->nonfeCount()};
    std::string propName = "nonfeCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNFC);

    nv::sensor_aggregation::DbusVariantType variantFC{pcieEccIntf->feCount()};
    propName = "feCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantFC);

    nv::sensor_aggregation::DbusVariantType variantCC{pcieEccIntf->ceCount()};
    propName = "ceCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantCC);

    nv::sensor_aggregation::DbusVariantType variantURC{
        pcieEccIntf->unsupportedRequestCount()};
    propName = "UnsupportedRequestCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantURC);
#endif
}

// TODO refactor usage to NsmPCIeErrors(3)
NsmPCIeECCGroup3::NsmPCIeECCGroup3(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_3),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmPCIeECCGroup3: {NAME}", "NAME", name.c_str());

    pcieEccIntf->l0ToRecoveryCount(0);
    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup3::NsmPCIeECCGroup3(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_3, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmMultiPCIeECCGroup3: {NAME}", "NAME", name.c_str());

    pcieEccIntf->l0ToRecoveryCount(0);
    updateMetricOnSharedMemory();
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

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup3 decode_query_scalar_group_telemetry_v1_group3_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        pcieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPCIeECCGroup3::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePCIeEccName = std::string(pcieEccIntf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantL0TRC{
        pcieEccIntf->l0ToRecoveryCount()};
    std::string propName = "L0ToRecoveryCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantL0TRC);
#endif
}

// TODO refactor usage to NsmPCIeErrors(4)
NsmPCIeECCGroup4::NsmPCIeECCGroup4(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_4),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmPCIeECCGroup4: {NAME}", "NAME", name.c_str());

    pcieEccIntf->replayCount(0);
    pcieEccIntf->replayRolloverCount(0);
    pcieEccIntf->nakSentCount(0);
    pcieEccIntf->nakReceivedCount(0);

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup4::NsmPCIeECCGroup4(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_4, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmMultiPCIeECCGroup4: {NAME}", "NAME", name.c_str());

    pcieEccIntf->replayCount(0);
    pcieEccIntf->replayRolloverCount(0);
    pcieEccIntf->nakSentCount(0);
    pcieEccIntf->nakReceivedCount(0);

    updateMetricOnSharedMemory();
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

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup4 decode_query_scalar_group_telemetry_v1_group4_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        pcieEccIntf->replayCount(data.replay_cnt);
        pcieEccIntf->replayRolloverCount(data.replay_rollover_cnt);
        pcieEccIntf->nakSentCount(data.NAK_sent_cnt);
        pcieEccIntf->nakReceivedCount(data.NAK_recv_cnt);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPCIeECCGroup4::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePCIeEccName = std::string(pcieEccIntf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantRC{
        pcieEccIntf->replayCount()};
    std::string propName = "ReplayCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantRC);

    nv::sensor_aggregation::DbusVariantType variantRRC{
        pcieEccIntf->replayRolloverCount()};
    propName = "ReplayRolloverCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantRRC);

    nv::sensor_aggregation::DbusVariantType variantNSC{
        pcieEccIntf->nakSentCount()};
    propName = "NAKSentCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNSC);

    nv::sensor_aggregation::DbusVariantType variantNRC{
        pcieEccIntf->nakReceivedCount()};
    propName = "NAKReceivedCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNRC);
#endif
}

// TODO refactor usage to NsmPCIeErrors(8)
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

NsmPCIeECCGroup8::NsmPCIeECCGroup8(const std::string& name,
                                   const std::string& type,
                                   std::shared_ptr<LaneErrorIntf> laneErrorIntf,
                                   const std::string& inventoryObjPath,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_8, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    laneErrorIntf(laneErrorIntf), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMultiPCIeECCGroup8: create sensor:{NAME}", "NAME",
              name.c_str());
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

    LG2_ERROR_FLT(
        "decode_query_scalar_group_telemetry_v1_group8_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        std::vector<uint32_t> error_counts;

        for (int idx = 0; idx < TOTAL_PCIE_LANE_COUNT; idx++)
        {
            error_counts.push_back(data.error_counts[idx]);
        }

        laneErrorIntf->rxErrorsPerLane(error_counts);
        updateMetricOnSharedMemory();
    }

    return cc ? cc : rc;
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

NsmPCIeECCGroup10::NsmPCIeECCGroup10(sdbusplus::bus::bus& bus,
                                     const std::string& name,
                                     const std::string& type,
                                     const std::string& inventoryObjPath,
                                     uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_10),
    inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmPCIeECCGroup10: create sensor:{NAME}", "NAME", name.c_str());
    pcieTransactionCounterIntf = std::make_unique<PCIeTransactionCounterIntf>(
        bus, inventoryObjPath.c_str());

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup10::NsmPCIeECCGroup10(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath, uint8_t multiPortType,
    uint8_t multiPortIndex, uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_10, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMultiPCIeECCGroup10: create sensor:{NAME}", "NAME",
              name.c_str());
    pcieTransactionCounterIntf = std::make_unique<PCIeTransactionCounterIntf>(
        bus, inventoryObjPath.c_str());

    updateMetricOnSharedMemory();
}

uint8_t NsmPCIeECCGroup10::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_10 data = {};

    auto rc = decode_query_scalar_group_telemetry_v1_group10_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_query_scalar_group_telemetry_v1_group10_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        pcieTransactionCounterIntf->outboundReadPktCount(
            static_cast<uint64_t>(data.outbound_read_tlp_count));
        pcieTransactionCounterIntf->outboundWritePktCount(
            static_cast<uint64_t>(data.outbound_write_tlp_count));
        pcieTransactionCounterIntf->outboundTLPCount(
            static_cast<uint64_t>(data.outbound_completion_tlp_count));

        uint64_t outboundReadTransfer =
            (static_cast<uint64_t>(
                 data.dwords_transferred_in_outbound_read_tlp_high)
             << 32) |
            data.dwords_transferred_in_outbound_read_tlp_low;
        uint64_t outboundWriteTransfer =
            (static_cast<uint64_t>(
                 data.dwords_transferred_in_outbound_write_tlp_high)
             << 32) |
            data.dwords_transferred_in_outbound_write_tlp_low;
        // convert dwords to bytes and update dwords
        pcieTransactionCounterIntf->outboundReadTransfer(outboundReadTransfer *
                                                         4);
        pcieTransactionCounterIntf->outboundWriteTransfer(
            outboundWriteTransfer * 4);
        pcieTransactionCounterIntf->outboundTLPsTransfer(
            static_cast<uint64_t>(
                data.dwords_transferred_in_outbound_completion) *
            4);

        pcieTransactionCounterIntf->reqDroppedTag(
            static_cast<uint64_t>(data.read_requests_dropped_tag_unavailable));
        pcieTransactionCounterIntf->reqDroppedCreditCompletion(
            static_cast<uint64_t>(
                data.read_requests_dropped_credit_exhaustion));
        pcieTransactionCounterIntf->reqDroppedNonPostCredit(
            static_cast<uint64_t>(
                data.read_requests_dropped_credit_not_posted));

        updateMetricOnSharedMemory();
    }

    return cc ? cc : rc;
}

static requester::Coroutine
    createNsmPCIeRetimerPorts(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
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
    uint64_t deviceInstance{};
    if (allCurrentIfaceProperties.count("DeviceInstance"))
    {
        deviceInstance =
            std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceInstance"));
    }
    std::string inventoryObjPath{};
    if (allCurrentIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("InventoryObjPath"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string portProtocol{};
    if (allCurrentIfaceProperties.count("PortProtocol"))
    {
        portProtocol =
            std::get<std::string>(allCurrentIfaceProperties.at("PortProtocol"));
    }
    std::string portType{};
    if (allCurrentIfaceProperties.count("PortType"))
    {
        portType =
            std::get<std::string>(allCurrentIfaceProperties.at("PortType"));
    }

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
            portName, type, objPath, portInfoIntf, portWidthIntf, deviceIndex);
        auto pcieECCIntfSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
            portName, type, objPath, pcieECCIntf, deviceIndex);

        if (!pcieSensorGroup1 || !pcieECCIntfSensorGroup3)
        {
            lg2::error(
                "Failed to create NSM PCIe Port sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }

        nsmDevice->addSensor(pcieSensorGroup1, priority);
        nsmDevice->addSensor(pcieECCIntfSensorGroup3, priority);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

static requester::Coroutine
    createNsmMultiPCIeRetimerPorts(SensorManager& manager,
                                   const std::string& interface,
                                   const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name = utils::getPropertyFromCollection<std::string>(properties,
                                                                     "Name")
                           .value();
    name = utils::makeDBusNameValid(name);
    const bool priority =
        utils::getPropertyFromCollection<bool>(properties, "Priority").value();
    const uint64_t count =
        utils::getPropertyFromCollection<uint64_t>(properties, "Count").value();
    std::string inventoryObjPath =
        utils::getPropertyFromCollection<std::string>(properties,
                                                      "InventoryObjPath")
            .value();
    std::string uuid =
        utils::getPropertyFromCollection<uuid_t>(properties, "UUID").value();
    std::string portProtocol = utils::getPropertyFromCollection<std::string>(
                                   properties, "PortProtocol")
                                   .value();
    std::string portType =
        utils::getPropertyFromCollection<std::string>(properties, "PortType")
            .value();
    const uint64_t upstreamPortNumber =
        utils::getPropertyFromCollection<uint64_t>(properties,
                                                   "UpstreamPortNumber")
            .value();

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    uint8_t portTypeVal = NSM_PORT_TYPE_UPSTREAM;
    if (portType ==
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort")
    {
        portTypeVal = NSM_PORT_TYPE_UPSTREAM;
    }
    else if (
        portType ==
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.DownstreamPort")
    {
        portTypeVal = NSM_PORT_TYPE_DOWNSTREAM;
    }
    else
    {
        lg2::error(
            "Invalid port type expected upstream/downstream, received {TYPE}",
            "TYPE", portType);
        co_return NSM_ERROR;
    }

    auto nsmDevice = manager.getNsmDevice(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_MultiPCIeLink PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
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

        auto multipcieSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
            portName, type, objPath, portInfoIntf, portWidthIntf, portTypeVal,
            i, static_cast<uint8_t>(upstreamPortNumber));
        auto multipcieSensorGroup2 = std::make_shared<NsmPCIeECCGroup2>(
            portName, type, objPath, pcieECCIntf, portTypeVal, i,
            static_cast<uint8_t>(upstreamPortNumber));
        auto multipcieSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
            portName, type, objPath, pcieECCIntf, portTypeVal, i,
            static_cast<uint8_t>(upstreamPortNumber));
        auto multipcieSensorGroup4 = std::make_shared<NsmPCIeECCGroup4>(
            portName, type, objPath, pcieECCIntf, portTypeVal, i,
            static_cast<uint8_t>(upstreamPortNumber));
        auto multipcieSensorGroup10 = std::make_shared<NsmPCIeECCGroup10>(
            bus, portName, type, objPath, portTypeVal, i,
            static_cast<uint8_t>(upstreamPortNumber));

        if (!multipcieSensorGroup1 || !multipcieSensorGroup2 ||
            !multipcieSensorGroup3 || !multipcieSensorGroup4 ||
            !multipcieSensorGroup10)
        {
            lg2::error(
                "Failed to create NSM Multi PCIe Port sensor : UUID={UUID}, Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
                "UUID", uuid, "NAME", portName, "TYPE", type, "OBJPATH",
                objPath);
            co_return NSM_ERROR;
        }

        nsmDevice->addSensor(multipcieSensorGroup1, priority);
        nsmDevice->addSensor(multipcieSensorGroup2, priority);
        nsmDevice->addSensor(multipcieSensorGroup3, priority);
        nsmDevice->addSensor(multipcieSensorGroup4, priority);
        nsmDevice->addSensor(multipcieSensorGroup10, priority);
    }

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmPCIeRetimerPorts,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeLink")

REGISTER_NSM_CREATION_FUNCTION(
    createNsmMultiPCIeRetimerPorts,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_MultiPCIeLink")

} // namespace nsm
