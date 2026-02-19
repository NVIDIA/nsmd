#include "nsmRetimerPort.hpp"

#include "libnsm/pci-links.h"

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"

#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"
#endif

#include "nsmPort/nsmPortConfigurationInfo.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <ranges>
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

// TODO refactor usage to NsmPCIeLinkSpeed<PortInfoIntf>
NsmPCIeECCGroup1::NsmPCIeECCGroup1(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   std::shared_ptr<PortWidthIntf> portWidthIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_1), objPath(inventoryPath),
    portInfoIntf(portInfoIntf), portWidthIntf(portWidthIntf)
{
    lg2::info("NsmPCIeECCGroup1: {NAME}", "NAME", name.c_str());

    portInfoIntf->maxSpeed(0);
    portInfoIntf->currentSpeed(0);
    portInfoIntf->targetSpeed(0);
    portWidthIntf->width(0);
    portWidthIntf->activeWidth(0);

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup1::NsmPCIeECCGroup1(
    const std::string& name, const std::string& type,
    const std::string& inventoryPath,
    std::shared_ptr<PortInfoIntf> portInfoIntf,
    std::shared_ptr<PortWidthIntf> portWidthIntf,
    std::shared_ptr<PCIeClockModeIntf> pcieClockModeIntf, uint8_t multiPortType,
    uint8_t multiPortIndex, uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_1, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), portInfoIntf(portInfoIntf),
    portWidthIntf(portWidthIntf), pcieClockModeIntf(pcieClockModeIntf)
{
    lg2::info("NsmMultiPCIeECCGroup1: {NAME}", "NAME", name.c_str());

    portInfoIntf->maxSpeed(0);
    portInfoIntf->currentSpeed(0);
    portInfoIntf->targetSpeed(0);
    portWidthIntf->width(0);
    portWidthIntf->activeWidth(0);
    portInfoIntf->maxReadRequestSizeBytes(0);
    portInfoIntf->maxPayloadSizeBytes(0);
    pcieClockModeIntf->commonClockMode(ClockMode::SeparateClockMode);

    updateMetricOnSharedMemory();
}

double NsmPCIeECCGroup1::convertEncodedSpeedToGbps(const uint32_t& speed)
{
    switch (speed)
    {
        case NSM_PCIE_LINK_SPEED_CODE_GEN1:
            return NSM_PCIE_SPEED_GBPS_GEN1;
        case NSM_PCIE_LINK_SPEED_CODE_GEN2:
            return NSM_PCIE_SPEED_GBPS_GEN2;
        case NSM_PCIE_LINK_SPEED_CODE_GEN3:
            return NSM_PCIE_SPEED_GBPS_GEN3;
        case NSM_PCIE_LINK_SPEED_CODE_GEN4:
            return NSM_PCIE_SPEED_GBPS_GEN4;
        case NSM_PCIE_LINK_SPEED_CODE_GEN5:
            return NSM_PCIE_SPEED_GBPS_GEN5;
        case NSM_PCIE_LINK_SPEED_CODE_GEN6:
            return NSM_PCIE_SPEED_GBPS_GEN6;
        default:
            lg2::debug("NsmPCIeECCGroup1: {NAME}, unknown speed {SPEED}",
                       "NAME", getName(), "SPEED", speed);
            return 0;
    }
}

size_t NsmPCIeECCGroup1::convertEncodedWidthToActualWidth(const uint32_t& width)
{
    return (width > 0 && width <= 6) ? (uint32_t)pow(2, width - 1) : 0;
}

size_t NsmPCIeECCGroup1::convertEncodedSizeToBytes(const uint32_t& size)
{
    return (size <= 5) ? (static_cast<size_t>(128) << size) : 0;
}

ClockMode
    NsmPCIeECCGroup1::convertEncodedClockModeToEnum(const uint32_t& clockMode)
{
    switch (clockMode)
    {
        case NSM_PCIE_CLOCK_MODE_SEPARATE:
        {
            return ClockMode::SeparateClockMode;
        }
        case NSM_PCIE_CLOCK_MODE_COMMON:
        {
            return ClockMode::CommonClockMode;
        }
        default:
        {
            LG2_DEBUG_FLT(
                "NsmPCIeECCGroup1: {NAME}, unknown clock mode {CLOCKMODE}",
                "NAME", getName(), "CLOCKMODE", clockMode);
            return ClockMode::Unknown;
        }
    }
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
        portInfoIntf->targetSpeed(
            convertEncodedSpeedToGbps(data.target_link_speed));
        portWidthIntf->width(
            convertEncodedWidthToActualWidth(data.max_link_width));
        portWidthIntf->activeWidth(
            convertEncodedWidthToActualWidth(data.negotiated_link_width));
        portInfoIntf->maxReadRequestSizeBytes(
            convertEncodedSizeToBytes(data.max_read_request_size_bytes));
        portInfoIntf->maxPayloadSizeBytes(
            convertEncodedSizeToBytes(data.max_payload_size_bytes));
        pcieClockModeIntf->commonClockMode(
            convertEncodedClockModeToEnum(data.clock_mode));

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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortInfoName, propName, rawSmbpbiData, variantCS);

    nv::sensor_aggregation::DbusVariantType variantAW{
        portWidthIntf->activeWidth()};
    propName = "ActiveWidth";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortWidthName, propName, rawSmbpbiData, variantAW);
#endif
}

// TODO refactor usage to NsmPCIeErrors(2)
NsmPCIeECCGroup2::NsmPCIeECCGroup2(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_2), objPath(inventoryPath),
    pcieEccIntf(pcieEccIntf)
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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNFC);

    nv::sensor_aggregation::DbusVariantType variantFC{pcieEccIntf->feCount()};
    propName = "feCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantFC);

    nv::sensor_aggregation::DbusVariantType variantCC{pcieEccIntf->ceCount()};
    propName = "ceCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantCC);

    nv::sensor_aggregation::DbusVariantType variantURC{
        pcieEccIntf->unsupportedRequestCount()};
    propName = "UnsupportedRequestCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantURC);
#endif
}

// TODO refactor usage to NsmPCIeErrors(3)
NsmPCIeECCGroup3::NsmPCIeECCGroup3(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_3), objPath(inventoryPath),
    pcieEccIntf(pcieEccIntf)
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
    pcieEccIntf->trainingSequenceErrorCount(0);
    pcieEccIntf->framingErrorCount(0);
    pcieEccIntf->linkDownedCount(0);
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
        pcieEccIntf->trainingSequenceErrorCount(data.training_seq_errors);
        pcieEccIntf->framingErrorCount(data.framing_errors);
        pcieEccIntf->linkDownedCount(data.link_down_count);

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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantL0TRC);

    nv::sensor_aggregation::DbusVariantType variantTSEC{
        pcieEccIntf->trainingSequenceErrorCount()};
    propName = "TrainingSequenceErrorCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantTSEC);

    nv::sensor_aggregation::DbusVariantType variantFEC{
        pcieEccIntf->framingErrorCount()};
    propName = "FramingErrorCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantFEC);

    nv::sensor_aggregation::DbusVariantType variantLDC{
        pcieEccIntf->linkDownedCount()};
    propName = "LinkDownedCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantLDC);
#endif
}

// TODO refactor usage to NsmPCIeErrors(4)
NsmPCIeECCGroup4::NsmPCIeECCGroup4(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PCIeEccIntf> pcieEccIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_4), objPath(inventoryPath),
    pcieEccIntf(pcieEccIntf)
{
    lg2::info("NsmPCIeECCGroup4: {NAME}", "NAME", name.c_str());

    pcieEccIntf->replayCount(0);
    pcieEccIntf->replayRolloverCount(0);
    pcieEccIntf->nakSentCount(0);
    pcieEccIntf->nakReceivedCount(0);
    pcieEccIntf->fcTimeoutErrors(0);
    pcieEccIntf->badTLPCount(0);
    pcieEccIntf->receiverErrorCount(0);

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
    pcieEccIntf->fcTimeoutErrors(0);
    pcieEccIntf->badTLPCount(0);
    pcieEccIntf->receiverErrorCount(0);
    pcieEccIntf->dllpcrcErrorCount(0);

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
        pcieEccIntf->fcTimeoutErrors(data.FC_timeout_err_cnt);
        pcieEccIntf->badTLPCount(data.bad_TLP_cnt);
        pcieEccIntf->receiverErrorCount(data.recv_err_cnt);
        pcieEccIntf->dllpcrcErrorCount(data.dllp_crc_errors);

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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantRC);

    nv::sensor_aggregation::DbusVariantType variantRRC{
        pcieEccIntf->replayRolloverCount()};
    propName = "ReplayRolloverCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantRRC);

    nv::sensor_aggregation::DbusVariantType variantNSC{
        pcieEccIntf->nakSentCount()};
    propName = "NAKSentCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNSC);

    nv::sensor_aggregation::DbusVariantType variantNRC{
        pcieEccIntf->nakReceivedCount()};
    propName = "NAKReceivedCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantNRC);

    nv::sensor_aggregation::DbusVariantType variantFCTE{
        pcieEccIntf->fcTimeoutErrors()};
    propName = "FCTimeoutErrors";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantFCTE);
    nv::sensor_aggregation::DbusVariantType variantREC{
        pcieEccIntf->receiverErrorCount()};
    propName = "ReceiverErrorCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantREC);

    nv::sensor_aggregation::DbusVariantType variantBTC{
        pcieEccIntf->badTLPCount()};
    propName = "BadTLPCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantBTC);

    nv::sensor_aggregation::DbusVariantType variantDLLPCRC{
        pcieEccIntf->dllpcrcErrorCount()};
    propName = "DLLPCRCErrorCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePCIeEccName, propName, rawSmbpbiData, variantDLLPCRC);
#endif
}

NsmPCIeECCGroup5::NsmPCIeECCGroup5(
    const std::string& name, const std::string& type,
    const std::string& inventoryPath,
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
    uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_5), objPath(inventoryPath),
    portMetricsOem2Intf(portMetricsOem2Intf)
{
    lg2::info("NsmPCIeECCGroup5: {NAME}", "NAME", name.c_str());

    portMetricsOem2Intf->txBytes(0);
    portMetricsOem2Intf->rxBytes(0);

    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup5::NsmPCIeECCGroup5(
    const std::string& name, const std::string& type,
    const std::string& inventoryPath,
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf,
    uint8_t multiPortType, uint8_t multiPortIndex,
    uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_5, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), portMetricsOem2Intf(portMetricsOem2Intf)
{
    lg2::info("NsmMultiPCIeECCGroup5: {NAME}", "NAME", name.c_str());

    portMetricsOem2Intf->txBytes(0);
    portMetricsOem2Intf->rxBytes(0);

    updateMetricOnSharedMemory();
}

uint8_t NsmPCIeECCGroup5::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_5 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup5 decode_query_scalar_group_telemetry_v1_group5_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        uint64_t txBytes = static_cast<uint64_t>(data.PCIeTXDwords) *
                           BYTES_PER_DWORD;
        uint64_t rxBytes = static_cast<uint64_t>(data.PCIeRXDwords) *
                           BYTES_PER_DWORD;

        portMetricsOem2Intf->txBytes(txBytes);
        portMetricsOem2Intf->rxBytes(rxBytes);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

void NsmPCIeECCGroup5::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifacePortMetricsOem2Name = std::string(portMetricsOem2Intf->interface);
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantTXB{
        portMetricsOem2Intf->txBytes()};
    std::string propName = "TXBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortMetricsOem2Name, propName, rawSmbpbiData, variantTXB);

    nv::sensor_aggregation::DbusVariantType variantRXB{
        portMetricsOem2Intf->rxBytes()};
    propName = "RXBytes";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        objPath, ifacePortMetricsOem2Name, propName, rawSmbpbiData, variantRXB);
#endif
}

NsmPCIeECCGroup7::NsmPCIeECCGroup7(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_7), objPath(inventoryPath),
    portInfoIntf(portInfoIntf)
{
    lg2::info("NsmPCIeECCGroup7: {NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup7::NsmPCIeECCGroup7(const std::string& name,
                                   const std::string& type,
                                   const std::string& inventoryPath,
                                   std::shared_ptr<PortInfoIntf> portInfoIntf,
                                   uint8_t multiPortType,
                                   uint8_t multiPortIndex,
                                   uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_7, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), portInfoIntf(portInfoIntf)
{
    lg2::info("NsmMultiPCIeECCGroup7: {NAME}", "NAME", name.c_str());
}

NsmPCIeECCGroup7::NsmPCIeECCGroup7(
    const std::string& name, const std::string& type,
    std::shared_ptr<PCIeDeviceIntf> pcieDeviceIntf,
    const std::vector<uint64_t>& functionIds, uint8_t multiPortType,
    uint8_t multiPortIndex, uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_7, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    pcieDeviceIntf(pcieDeviceIntf), functionIds(functionIds)
{
    lg2::info("NsmPCIeECCGroup7 (PCIeDevice): {NAME}, functionCount: {CNT}",
              "NAME", name.c_str(), "CNT", functionIds.size());
}

uint8_t NsmPCIeECCGroup7::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_7 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group7_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup7 decode_query_scalar_group_telemetry_v1_group7_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // Handle PortInfoIntf case (retimer ports - sets PortType)
        if (portInfoIntf)
        {
            PortType portType;
            auto pciePortType = static_cast<nsm_pcie_port_type>(data.port_type);
            switch (pciePortType)
            {
                case NSM_PCIE_PORT_TYPE_ROOT_PORT:
                    portType = PortType::RootPort;
                    break;
                case NSM_PCIE_PORT_TYPE_UPSTREAM:
                    portType = PortType::UpstreamPort;
                    break;
                case NSM_PCIE_PORT_TYPE_DOWNSTREAM:
                    portType = PortType::DownstreamPort;
                    break;
                case NSM_PCIE_PORT_TYPE_ENDPOINT:
                    portType = PortType::EndpointPort;
                    break;
                default:
                    portType = PortType::DownstreamPort;
                    break;
            }
            portInfoIntf->type(portType);
        }

        // Handle PCIeDeviceIntf case - sets Function{N}BusNumber for all
        // functions
        if (pcieDeviceIntf)
        {
            auto busNumber = static_cast<uint8_t>(data.pcie_bus_number);
            for (const auto& funcId : functionIds)
            {
                switch (funcId)
                {
                    case 0:
                        pcieDeviceIntf->function0BusNumber(busNumber);
                        break;
                    case 1:
                        pcieDeviceIntf->function1BusNumber(busNumber);
                        break;
                    case 2:
                        pcieDeviceIntf->function2BusNumber(busNumber);
                        break;
                    case 3:
                        pcieDeviceIntf->function3BusNumber(busNumber);
                        break;
                    case 4:
                        pcieDeviceIntf->function4BusNumber(busNumber);
                        break;
                    case 5:
                        pcieDeviceIntf->function5BusNumber(busNumber);
                        break;
                    case 6:
                        pcieDeviceIntf->function6BusNumber(busNumber);
                        break;
                    case 7:
                        pcieDeviceIntf->function7BusNumber(busNumber);
                        break;
                }
            }
        }
    }
    return cc ? cc : rc;
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
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant);
#endif
}

NsmPCIeECCGroup9::NsmPCIeECCGroup9(
    const std::string& name, const std::string& type,
    const std::string& inventoryPath,
    std::shared_ptr<NsmRetimerAERErrorStatusIntf> aerErrorStatusIntf,
    uint8_t deviceIndex) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_9), objPath(inventoryPath),
    aerErrorStatusIntf(aerErrorStatusIntf)
{
    lg2::info("NsmPCIeECCGroup9: {NAME}", "NAME", name.c_str());

    aerErrorStatusIntf->aerUncorrectableErrorStatus("0x00000000");
    aerErrorStatusIntf->aerCorrectableErrorStatus("0x00000000");
}

NsmPCIeECCGroup9::NsmPCIeECCGroup9(
    const std::string& name, const std::string& type,
    const std::string& inventoryPath,
    std::shared_ptr<NsmRetimerAERErrorStatusIntf> aerErrorStatusIntf,
    uint8_t multiPortType, uint8_t multiPortIndex,
    uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_9, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    objPath(inventoryPath), aerErrorStatusIntf(aerErrorStatusIntf)
{
    lg2::info("NsmMultiPCIeECCGroup9: {NAME}", "NAME", name.c_str());

    aerErrorStatusIntf->aerUncorrectableErrorStatus("0x00000000");
    aerErrorStatusIntf->aerCorrectableErrorStatus("0x00000000");
}

uint8_t NsmPCIeECCGroup9::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_9 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group9_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    LG2_ERROR_FLT(
        "NsmPCIeECCGroup9 decode_query_scalar_group_telemetry_v1_group9_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        auto hexFormat = [](const uint32_t value) -> std::string {
            std::string hexStr(sizeof("0x00000000"), '\0');
            snprintf(hexStr.data(), hexStr.size(), "0x%08X", value);
            return hexStr;
        };

        aerErrorStatusIntf->aerUncorrectableErrorStatus(
            hexFormat(data.aer_uncorrectable_error_status));
        aerErrorStatusIntf->aerCorrectableErrorStatus(
            hexFormat(data.aer_correctable_error_status));
    }
    return cc ? cc : rc;
}

NsmPCIeECCGroup10::NsmPCIeECCGroup10(const std::string& name,
                                     const std::string& type,
                                     const std::string& inventoryObjPath,
                                     uint8_t deviceIndex,
                                     bool includeInboundCounters) :
    NsmPcieGroup(name, type, deviceIndex, GROUP_ID_10),
    inventoryObjPath(inventoryObjPath),
    includeInboundCounters(includeInboundCounters)
{
    lg2::info("NsmPCIeECCGroup10: create sensor:{NAME} includeInbound:{INC}",
              "NAME", name.c_str(), "INC", includeInboundCounters);
    registerProperties();
    updateMetricOnSharedMemory();
}

NsmPCIeECCGroup10::NsmPCIeECCGroup10(const std::string& name,
                                     const std::string& type,
                                     const std::string& inventoryObjPath,
                                     uint8_t multiPortType,
                                     uint8_t multiPortIndex,
                                     uint8_t multiPortUpstreamPortNumber,
                                     bool includeInboundCounters) :
    NsmPcieGroup(name, type, GROUP_ID_10, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    inventoryObjPath(inventoryObjPath),
    includeInboundCounters(includeInboundCounters)
{
    lg2::info(
        "NsmMultiPCIeECCGroup10: create sensor:{NAME} includeInbound:{INC}",
        "NAME", name.c_str(), "INC", includeInboundCounters);
    registerProperties();
    updateMetricOnSharedMemory();
}

uint64_t NsmPCIeECCGroup10::join64BitCounterLowHigh(uint32_t high, uint32_t low)
{
    return (static_cast<uint64_t>(high) << 32) | low;
}

uint64_t NsmPCIeECCGroup10::convertDwordsSizeToBytes(uint64_t dwords)
{
    return dwords * static_cast<uint64_t>(BYTES_PER_DWORD);
}

void NsmPCIeECCGroup10::registerProperties()
{
    pcieTransactionCounterIntf =
        SensorManager::getInstance().getObjServer().add_unique_interface(
            inventoryObjPath,
            "xyz.openbmc_project.PCIe.PCIeTransactionCounter");

    // Common properties (available on all platforms)
    pcieTransactionCounterIntf->register_property("OutboundWritePktCount",
                                                  uint64_t(0));
    pcieTransactionCounterIntf->register_property("OutboundWriteTransfer",
                                                  uint64_t(0));
    pcieTransactionCounterIntf->register_property("ReqDroppedTag", uint64_t(0));
    pcieTransactionCounterIntf->register_property("ReqDroppedCreditCompletion",
                                                  uint64_t(0));
    pcieTransactionCounterIntf->register_property("ReqDroppedNonPostCredit",
                                                  uint64_t(0));

    if (!includeInboundCounters)
    {
        pcieTransactionCounterIntf->register_property("OutboundReadPktCount",
                                                      uint64_t(0));
        pcieTransactionCounterIntf->register_property("OutboundReadTransfer",
                                                      uint64_t(0));
        pcieTransactionCounterIntf->register_property("OutboundTLPCount",
                                                      uint64_t(0));
        pcieTransactionCounterIntf->register_property("OutboundTLPsTransfer",
                                                      uint64_t(0));
    }
    else
    {
        pcieTransactionCounterIntf->register_property("InboundTLPCount",
                                                      uint64_t(0));
        pcieTransactionCounterIntf->register_property("InboundTLPsTransfer",
                                                      uint64_t(0));
    }
    pcieTransactionCounterIntf->initialize();
}

uint8_t NsmPCIeECCGroup10::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    int rc;

    if (includeInboundCounters)
    {
        nsm_query_scalar_group_telemetry_group_10_extended data = {};
        rc = decode_query_scalar_group_telemetry_v1_group10_extended_resp(
            responseMsg, responseLen, &cc, &reasonCode, &data);

        LG2_ERROR_FLT(
            "decode_query_scalar_group_telemetry_v1_group10_extended_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);

        if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
        {
            outboundWritePktCount =
                static_cast<uint64_t>(data.outbound_write_tlp_count);
            pcieTransactionCounterIntf->set_property("OutboundWritePktCount",
                                                     outboundWritePktCount);

            outboundWriteTransfer =
                convertDwordsSizeToBytes(join64BitCounterLowHigh(
                    data.dwords_transferred_in_outbound_write_tlp_high,
                    data.dwords_transferred_in_outbound_write_tlp_low));
            pcieTransactionCounterIntf->set_property("OutboundWriteTransfer",
                                                     outboundWriteTransfer);

            reqDroppedTag = static_cast<uint64_t>(
                data.read_requests_dropped_tag_unavailable);
            pcieTransactionCounterIntf->set_property("ReqDroppedTag",
                                                     reqDroppedTag);

            reqDroppedCreditCompletion = static_cast<uint64_t>(
                data.read_requests_dropped_credit_exhaustion);
            pcieTransactionCounterIntf->set_property(
                "ReqDroppedCreditCompletion", reqDroppedCreditCompletion);

            reqDroppedNonPostCredit = static_cast<uint64_t>(
                data.read_requests_dropped_credit_not_posted);
            pcieTransactionCounterIntf->set_property("ReqDroppedNonPostCredit",
                                                     reqDroppedNonPostCredit);

            inboundTLPCount =
                static_cast<uint64_t>(data.inbound_completion_tlp_count);
            pcieTransactionCounterIntf->set_property("InboundTLPCount",
                                                     inboundTLPCount);

            inboundTLPsTransfer = convertDwordsSizeToBytes(
                join64BitCounterLowHigh(data.inbound_completion_tlp_bytes_high,
                                        data.inbound_completion_tlp_bytes_low));
            pcieTransactionCounterIntf->set_property("InboundTLPsTransfer",
                                                     inboundTLPsTransfer);

            updateMetricOnSharedMemory();
        }
    }
    else
    {
        nsm_query_scalar_group_telemetry_group_10 data = {};
        rc = decode_query_scalar_group_telemetry_v1_group10_resp(
            responseMsg, responseLen, &cc, &reasonCode, &data);

        LG2_ERROR_FLT(
            "decode_query_scalar_group_telemetry_v1_group10_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);

        if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
        {
            outboundReadPktCount =
                static_cast<uint64_t>(data.outbound_read_tlp_count);
            pcieTransactionCounterIntf->set_property("OutboundReadPktCount",
                                                     outboundReadPktCount);

            outboundTLPCount =
                static_cast<uint64_t>(data.outbound_completion_tlp_count);
            pcieTransactionCounterIntf->set_property("OutboundTLPCount",
                                                     outboundTLPCount);

            outboundReadTransfer =
                convertDwordsSizeToBytes(join64BitCounterLowHigh(
                    data.dwords_transferred_in_outbound_read_tlp_high,
                    data.dwords_transferred_in_outbound_read_tlp_low));
            pcieTransactionCounterIntf->set_property("OutboundReadTransfer",
                                                     outboundReadTransfer);

            outboundTLPsTransfer = convertDwordsSizeToBytes(
                data.dwords_transferred_in_outbound_completion);
            pcieTransactionCounterIntf->set_property("OutboundTLPsTransfer",
                                                     outboundTLPsTransfer);

            outboundWritePktCount =
                static_cast<uint64_t>(data.outbound_write_tlp_count);
            pcieTransactionCounterIntf->set_property("OutboundWritePktCount",
                                                     outboundWritePktCount);

            outboundWriteTransfer =
                convertDwordsSizeToBytes(join64BitCounterLowHigh(
                    data.dwords_transferred_in_outbound_write_tlp_high,
                    data.dwords_transferred_in_outbound_write_tlp_low));
            pcieTransactionCounterIntf->set_property("OutboundWriteTransfer",
                                                     outboundWriteTransfer);

            reqDroppedTag = static_cast<uint64_t>(
                data.read_requests_dropped_tag_unavailable);
            pcieTransactionCounterIntf->set_property("ReqDroppedTag",
                                                     reqDroppedTag);

            reqDroppedCreditCompletion = static_cast<uint64_t>(
                data.read_requests_dropped_credit_exhaustion);
            pcieTransactionCounterIntf->set_property(
                "ReqDroppedCreditCompletion", reqDroppedCreditCompletion);

            reqDroppedNonPostCredit = static_cast<uint64_t>(
                data.read_requests_dropped_credit_not_posted);
            pcieTransactionCounterIntf->set_property("ReqDroppedNonPostCredit",
                                                     reqDroppedNonPostCredit);
            updateMetricOnSharedMemory();
        }
    }

    return cc ? cc : rc;
}

void NsmPCIeECCGroup10::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName =
        std::string("xyz.openbmc_project.PCIe.PCIeTransactionCounter");
    std::vector<uint8_t> rawSmbpbiData = {};

    nv::sensor_aggregation::DbusVariantType variantOWPC{outboundWritePktCount};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, "OutboundWritePktCount", rawSmbpbiData,
        variantOWPC);

    nv::sensor_aggregation::DbusVariantType variantOWT{outboundWriteTransfer};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, "OutboundWriteTransfer", rawSmbpbiData,
        variantOWT);

    nv::sensor_aggregation::DbusVariantType variantRDT{reqDroppedTag};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, "ReqDroppedTag", rawSmbpbiData,
        variantRDT);

    nv::sensor_aggregation::DbusVariantType variantRDCC{
        reqDroppedCreditCompletion};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, "ReqDroppedCreditCompletion",
        rawSmbpbiData, variantRDCC);

    nv::sensor_aggregation::DbusVariantType variantRDNPC{
        reqDroppedNonPostCredit};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, "ReqDroppedNonPostCredit", rawSmbpbiData,
        variantRDNPC);

    if (includeInboundCounters)
    {
        nv::sensor_aggregation::DbusVariantType variantITC{inboundTLPCount};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "InboundTLPCount", rawSmbpbiData,
            variantITC);

        nv::sensor_aggregation::DbusVariantType variantITT{inboundTLPsTransfer};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "InboundTLPsTransfer", rawSmbpbiData,
            variantITT);
    }
    else
    {
        nv::sensor_aggregation::DbusVariantType variantORPC{
            outboundReadPktCount};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "OutboundReadPktCount", rawSmbpbiData,
            variantORPC);

        nv::sensor_aggregation::DbusVariantType variantORT{
            outboundReadTransfer};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "OutboundReadTransfer", rawSmbpbiData,
            variantORT);

        nv::sensor_aggregation::DbusVariantType variantOTC{outboundTLPCount};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "OutboundTLPCount", rawSmbpbiData,
            variantOTC);

        nv::sensor_aggregation::DbusVariantType variantOTT{
            outboundTLPsTransfer};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            inventoryObjPath, ifaceName, "OutboundTLPsTransfer", rawSmbpbiData,
            variantOTT);
    }
#endif
}

NsmPCIeVectorGroup1::NsmPCIeVectorGroup1(
    const std::string& name, const std::string& type,
    const std::string& laneObjPath,
    std::shared_ptr<PCIeLaneErrorIntf> laneErrorIntf,
    std::shared_ptr<AssociationDefIntf> laneAssocIntf, uint8_t laneIndex,
    uint8_t linkSpeedCode, uint8_t multiPortType, uint8_t multiPortIndex,
    uint8_t multiPortUpstreamPortNumber) :
    NsmPcieGroup(name, type, GROUP_ID_1, multiPortType, multiPortIndex,
                 multiPortUpstreamPortNumber),
    laneObjPath(laneObjPath), laneErrorIntf(laneErrorIntf),
    laneAssocIntf(laneAssocIntf), laneIndex(laneIndex),
    linkSpeedCode(linkSpeedCode)
{
    lg2::info("NsmPCIeVectorGroup1: {NAME} lane={LANE} speedCode={SPEED}",
              "NAME", name.c_str(), "LANE", laneIndex, "SPEED", linkSpeedCode);

    laneErrorIntf->cdrErrorCount(0);
}

std::optional<std::vector<uint8_t>>
    NsmPCIeVectorGroup1::genRequestMsg([[maybe_unused]] eid_t eid,
                                       uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_vector_group_telemetry_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_query_vector_group_telemetry_v2_req(
        instanceId, getMultiPortUpstreamPortNumber(), getMultiPortType(),
        getMultiPortIndex(), getGroupId(), linkSpeedCode, laneIndex,
        requestPtr);

    if (rc)
    {
        LG2_ERROR_FLT(
            "NsmPCIeVectorGroup1 encode_query_vector_group_telemetry_v2_req "
            "failed | laneIndex={LANE} linkSpeedCode={SPEED} rc={RC}",
            "LANE", laneIndex, "SPEED", linkSpeedCode, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmPCIeVectorGroup1::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    struct nsm_query_vector_group_1_data data;

    auto rc = decode_query_vector_group_telemetry_v2_group1_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT("NsmPCIeVectorGroup1 decode failure | lane={LANE} "
                      "reasonCode={REASONCODE} cc={CC} rc={RC}",
                      "LANE", laneIndex, "REASONCODE", reasonCode, "CC", cc,
                      "RC", rc);
        return cc ? cc : rc;
    }

    uint32_t cdrErrorCount = data.cdr_error_per_lane;
    laneErrorIntf->cdrErrorCount(static_cast<uint64_t>(cdrErrorCount));

    LG2_DEBUG_FLT("NsmPCIeVectorGroup1: lane {LANE} CDR error count: {COUNT}",
                  "LANE", laneIndex, "COUNT", cdrErrorCount);

    return NSM_SUCCESS;
}

NsmPCIeLaneManager::NsmPCIeLaneManager(
    const std::string& name, const std::string& type,
    const std::string& portObjPath, std::shared_ptr<PortInfoIntf> portInfoIntf,
    std::shared_ptr<PortWidthIntf> portWidthIntf, uint8_t multiPortType,
    uint8_t multiPortIndex, uint8_t multiPortUpstreamPortNumber) :
    NsmSensor(name, type), portObjPath(portObjPath), portInfoIntf(portInfoIntf),
    portWidthIntf(portWidthIntf), multiPortType(multiPortType),
    multiPortIndex(multiPortIndex),
    multiPortUpstreamPortNumber(multiPortUpstreamPortNumber)
{
    lg2::info("NsmPCIeLaneManager: {NAME} portPath={PATH}", "NAME",
              name.c_str(), "PATH", portObjPath.c_str());
}

uint8_t NsmPCIeLaneManager::convertSpeedGbpsToLinkSpeedCode(double speedGbps)
{
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN6)
        return NSM_PCIE_LINK_SPEED_CODE_GEN6;
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN5)
        return NSM_PCIE_LINK_SPEED_CODE_GEN5;
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN4)
        return NSM_PCIE_LINK_SPEED_CODE_GEN4;
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN3)
        return NSM_PCIE_LINK_SPEED_CODE_GEN3;
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN2)
        return NSM_PCIE_LINK_SPEED_CODE_GEN2;
    if (speedGbps >= NSM_PCIE_SPEED_GBPS_GEN1)
        return NSM_PCIE_LINK_SPEED_CODE_GEN1;
    return NSM_PCIE_LINK_SPEED_CODE_UNKNOWN;
}

bool NsmPCIeLaneManager::hasLaneSensor(uint8_t laneIdx) const
{
    return laneSensors.find(laneIdx) != laneSensors.end();
}

void NsmPCIeLaneManager::clearAllLaneSensors()
{
    laneSensors.clear();
    lanesInitialized = false;
    currentLaneCount = 0;
    lg2::debug("NsmPCIeLaneManager: cleared all lane sensors for {NAME}",
               "NAME", getName());
}

void NsmPCIeLaneManager::createLaneSensor(std::shared_ptr<NsmDevice> nsmDevice,
                                          uint8_t laneIdx, uint8_t speedCode)
{
    if (hasLaneSensor(laneIdx))
    {
        return;
    }

    auto& bus = utils::DBusHandler::getBus();
    std::string laneObjPath = portObjPath + "/Lanes/" + std::to_string(laneIdx);
    std::string laneName = getName() + "_Lane_" + std::to_string(laneIdx);

    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        laneAssociations;
    laneAssociations.emplace_back("associated_port", "associated_lanes",
                                  portObjPath);
    laneAssocIntf->associations(laneAssociations);

    auto laneSensor = std::make_shared<NsmPCIeVectorGroup1>(
        laneName, getType(), laneObjPath, laneErrorIntf, laneAssocIntf, laneIdx,
        speedCode, multiPortType, multiPortIndex, multiPortUpstreamPortNumber);

    laneSensors[laneIdx] = laneSensor;
    nsmDevice->addSensor(laneSensor, false);

    lg2::info(
        "NsmPCIeLaneManager: created lane sensor {NAME} at {PATH} speedCode={SPEED}",
        "NAME", laneName.c_str(), "PATH", laneObjPath.c_str(), "SPEED",
        speedCode);
}

requester::Coroutine
    NsmPCIeLaneManager::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    size_t activeLaneCount = 0;
    uint8_t speedCode = NSM_PCIE_LINK_SPEED_CODE_UNKNOWN;

    try
    {
        activeLaneCount = portWidthIntf->activeWidth();
        double currentSpeedGbps = portInfoIntf->currentSpeed();
        speedCode = convertSpeedGbpsToLinkSpeedCode(currentSpeedGbps);
    }
    catch (const std::exception& e)
    {
        LG2_DEBUG_FLT(
            "NsmPCIeLaneManager: failed to read activeLaneCount/currentSpeed: {ERR}",
            "ERR", e.what());
        co_return NSM_ERROR;
    }

    bool validLaneCount = (activeLaneCount > 0 &&
                           activeLaneCount <= NSM_PCIE_LANE_COUNT_MAX);
    bool validSpeedCode = (speedCode != NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);

    if (!validLaneCount || !validSpeedCode)
    {
        if (lanesInitialized)
        {
            LG2_DEBUG_FLT(
                "NsmPCIeLaneManager: activeLaneCount={LANECOUNT} or speedCode={SPEED} "
                "invalid, clearing lane sensors",
                "LANECOUNT", activeLaneCount, "SPEED", speedCode);
            clearAllLaneSensors();
        }
        co_return NSM_SUCCESS;
    }

    uint8_t newLaneCount = static_cast<uint8_t>(activeLaneCount);

    if (lanesInitialized &&
        (newLaneCount != currentLaneCount || speedCode != currentSpeedCode))
    {
        lg2::info(
            "NsmPCIeLaneManager: lane config changed "
            "(lanes: {OLD_LANES}->{NEW_LANES}, speed: {OLD_SPEED}->{NEW_SPEED}), "
            "recreating sensors",
            "OLD_LANES", currentLaneCount, "NEW_LANES", newLaneCount,
            "OLD_SPEED", currentSpeedCode, "NEW_SPEED", speedCode);
        clearAllLaneSensors();
    }

    if (!lanesInitialized)
    {
        for (uint8_t laneIdx = 0; laneIdx < newLaneCount; laneIdx++)
        {
            createLaneSensor(nsmDevice, laneIdx, speedCode);
        }
        lanesInitialized = true;
        currentLaneCount = newLaneCount;
        currentSpeedCode = speedCode;

        lg2::info(
            "NsmPCIeLaneManager: initialized {COUNT} lane sensors for {NAME}",
            "COUNT", newLaneCount, "NAME", getName());
    }

    co_return NSM_SUCCESS;
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

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
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
        auto pcieSensorGroup7 = std::make_shared<NsmPCIeECCGroup7>(
            portName, type, objPath, portInfoIntf, deviceIndex);

        if (!pcieSensorGroup1 || !pcieECCIntfSensorGroup3 || !pcieSensorGroup7)
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
        nsmDevice->addSensor(pcieSensorGroup7, priority);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

inline void createNsmPCIePortConfigurationInfo(
    sdbusplus::bus::bus& bus, const std::string& portName,
    const std::string& type, const std::string& objPath, const bool priority,
    const uint64_t upstreamPortNumber, const uint8_t portTypeVal,
    const uint64_t index, std::shared_ptr<NsmDevice> nsmDevice)
{
    auto pciePortConfigurationInfoIntf =
        std::make_shared<PCIePortConfigurationInfoIntf>(bus, objPath.c_str());
    auto pciePortConfigurationInfo =
        std::make_shared<NsmPCIePortConfigurationInfo>(
            portName, type, pciePortConfigurationInfoIntf,
            static_cast<uint8_t>(upstreamPortNumber), portTypeVal, index,
            objPath);
    nsmDevice->addSensor(pciePortConfigurationInfo, priority);

    nsm::AsyncSetOperationHandler setPortConfigurationHandler =
        std::bind(&NsmPCIePortConfigurationInfo::setPortConfiguration,
                  pciePortConfigurationInfo, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(objPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.PCIe.PCIePortConfigurationInfo", "TxAmplitude",
            AsyncSetOperationInfo{setPortConfigurationHandler,
                                  pciePortConfigurationInfo, nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(objPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.PCIe.PCIePortConfigurationInfo", "Preset0",
            AsyncSetOperationInfo{setPortConfigurationHandler,
                                  pciePortConfigurationInfo, nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(objPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.PCIe.PCIePortConfigurationInfo", "Preset1",
            AsyncSetOperationInfo{setPortConfigurationHandler,
                                  pciePortConfigurationInfo, nsmDevice});
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
    const std::vector<uint64_t> counts =
        utils::getPropertyFromCollection<std::vector<uint64_t>>(properties,
                                                                "Counts")
            .value();
    std::string inventoryObjPath =
        utils::getPropertyFromCollection<std::string>(properties,
                                                      "InventoryObjPath")
            .value();
    std::string uuid =
        utils::getPropertyFromCollection<uuid_t>(properties, "UUID").value();
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string portType =
        utils::getPropertyFromCollection<std::string>(properties, "PortType")
            .value();
    const std::vector<uint64_t> upstreamPortNumbers =
        utils::getPropertyFromCollection<std::vector<uint64_t>>(
            properties, "UpstreamPortNumbers")
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

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        // cannot find a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_MultiPCIeLink PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        co_return NSM_ERROR;
    }

    bool includeInboundCounters =
        (nsmDevice->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9);

    for (auto [count, upstreamPortNumber] :
         std::views::zip(counts, upstreamPortNumbers))
    {
        // create pcie link [as per count]
        for (uint64_t index = 0; index < count; index++)
        {
            std::string portName = name + '_' +
                                   std::to_string(index + upstreamPortNumber);
            std::string objPath = inventoryObjPath + portName;

            auto pciePortIntfSensor = std::make_shared<NsmPort>(
                bus, portName, type, associations, objPath);
            nsmDevice->addStaticSensor(pciePortIntfSensor);

            auto pcieECCIntf = std::make_shared<PCIeEccIntf>(bus,
                                                             objPath.c_str());
            auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                               objPath.c_str());
            auto portWidthIntf =
                std::make_shared<PortWidthIntf>(bus, objPath.c_str());
            auto pcieClockModeIntf =
                std::make_shared<PCIeClockModeIntf>(bus, objPath.c_str());
            auto portMetricsOem2Intf =
                std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());

            portInfoIntf->protocol(
                PortInfoIntf::convertPortProtocolFromString(portProtocol));
            portInfoIntf->type(
                PortInfoIntf::convertPortTypeFromString(portType));
            auto multipcieSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
                portName, type, objPath, portInfoIntf, portWidthIntf,
                pcieClockModeIntf, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup2 = std::make_shared<NsmPCIeECCGroup2>(
                portName, type, objPath, pcieECCIntf, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup3 = std::make_shared<NsmPCIeECCGroup3>(
                portName, type, objPath, pcieECCIntf, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup4 = std::make_shared<NsmPCIeECCGroup4>(
                portName, type, objPath, pcieECCIntf, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup5 = std::make_shared<NsmPCIeECCGroup5>(
                portName, type, objPath, portMetricsOem2Intf, portTypeVal,
                index, static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup7 = std::make_shared<NsmPCIeECCGroup7>(
                portName, type, objPath, portInfoIntf, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber));
            auto multipcieSensorGroup10 = std::make_shared<NsmPCIeECCGroup10>(
                portName, type, objPath, portTypeVal, index,
                static_cast<uint8_t>(upstreamPortNumber),
                includeInboundCounters);

            if (!multipcieSensorGroup1 || !multipcieSensorGroup2 ||
                !multipcieSensorGroup3 || !multipcieSensorGroup4 ||
                !multipcieSensorGroup5 || !multipcieSensorGroup7 ||
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
            nsmDevice->addSensor(multipcieSensorGroup5, priority);
            nsmDevice->addSensor(multipcieSensorGroup7, priority);
            nsmDevice->addSensor(multipcieSensorGroup10, priority);

            auto laneManager = std::make_shared<NsmPCIeLaneManager>(
                portName, type, objPath, portInfoIntf, portWidthIntf,
                portTypeVal, index, static_cast<uint8_t>(upstreamPortNumber));
            nsmDevice->addSensor(laneManager, priority);

            if (portTypeVal == NSM_PORT_TYPE_UPSTREAM)
            {
                createNsmPCIePortConfigurationInfo(
                    bus, portName, type, objPath, priority, upstreamPortNumber,
                    portTypeVal, index, nsmDevice);
            }
        }
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
