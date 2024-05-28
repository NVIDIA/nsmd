#include "nsmPCIeRetimerPD.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

std::string convertToPCIeTypeStr(uint32_t link_speed)
{
    switch (link_speed)
    {
        case 1:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen1";
        case 2:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen2";
        case 3:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen3";
        case 4:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen4";
        case 5:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Gen5";
        default:
            return "xyz.openbmc_project.Inventory.Item.PCIeDevice.PCIeTypes.Unknown";
    }
}

size_t convertToLaneCount(uint32_t link_width)
{
    if (link_width < 1 || link_width > 6)
    {
        // covers unknown link width and unexpected values
        return 0;
    }
    return pow(2, (link_width - 1));
}

std::string convertToLTSSMStateStr(uint32_t ltssm_state)
{
    switch (ltssm_state)
    {
        case 0x0:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Detect";
        case 0x1:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Polling";
        case 0x2:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Configuration";
        case 0x3:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Recovery";
        case 0x4:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.RecoveryEQ";
        case 0x5:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L0";
        case 0x6:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L0s";
        case 0x7:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L1";
        case 0x8:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L1_PLL_PD";
        case 0x9:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L2";
        case 0xA:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L1_CPM";
        case 0xB:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L1_1";
        case 0xC:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.L1_2";
        case 0xD:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.HotReset";
        case 0xE:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Loopback";
        case 0xF:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.Disabled";
        case 0x10:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.LinkDown";
        case 0x11:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.LinkReady";
        case 0x12:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.LanesInSleep";
        case 0xFF:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.IllegalState";
        default:
            return "xyz.openbmc_project.PCIe.LTSSMState.State.NA";
    }
}

NsmPCIeDeviceQueryScalarTelemetry::NsmPCIeDeviceQueryScalarTelemetry(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& deviceType,
    const uint8_t deviceIndex, std::string& inventoryObjPath) :
    NsmSensor(name, type), deviceIndex(deviceIndex)
{
    auto objPath = inventoryObjPath + name;
    lg2::debug("NsmPCIeDeviceQueryScalarTelemetry: {NAME}", "NAME",
               name.c_str());

    // initialize members
    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    pcieDeviceIntf = std::make_unique<PCIeDeviceIntf>(bus, objPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
    pcieDeviceIntf->deviceType(deviceType);
}

std::optional<std::vector<uint8_t>>
    NsmPCIeDeviceQueryScalarTelemetry::genRequestMsg(eid_t eid,
                                                     uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_1, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeDeviceQueryScalarTelemetry::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_query_scalar_group_telemetry_group_1 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        pcieDeviceIntf->PCIeDeviceIntf::pcIeType(
            PCIeDeviceIntf::convertPCIeTypesFromString(
                convertToPCIeTypeStr(data.negotiated_link_speed)));
        pcieDeviceIntf->PCIeDeviceIntf::maxPCIeType(
            PCIeDeviceIntf::convertPCIeTypesFromString(
                convertToPCIeTypeStr(data.max_link_speed)));
        pcieDeviceIntf->PCIeDeviceIntf::lanesInUse(
            convertToLaneCount(data.negotiated_link_width));
        pcieDeviceIntf->PCIeDeviceIntf::maxLanes(
            convertToLaneCount(data.max_link_width));
    }
    else
    {
        lg2::error(
            "responseHandler: query_scalar_group_telemetry_v1_group1 unsuccessfull. reasonCode={RSNCOD} cc={CC} rc={RC}",
            "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

NsmPCIeDeviceGetClockOutput::NsmPCIeDeviceGetClockOutput(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const uint64_t& deviceInstance, std::string& inventoryObjPath) :
    NsmSensor(name, type)
{
    auto objPath = inventoryObjPath + name;
    lg2::debug("NsmPCIeDeviceGetClockOutput: {NAME}", "NAME", name.c_str());

    // initialize members
    pcieRefClockIntf = std::make_unique<PCIeRefClockIntf>(bus, objPath.c_str());
    clkBufIndex = PCIE_CLKBUF_INDEX;
    deviceInstanceNumber = static_cast<uint8_t>(deviceInstance);
}

std::optional<std::vector<uint8_t>>
    NsmPCIeDeviceGetClockOutput::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(instanceId, clkBufIndex,
                                                       requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_clock_output_enable_state_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeDeviceGetClockOutput::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t clkBuf = 0;

    auto rc = decode_get_clock_output_enable_state_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &clkBuf);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        pcieRefClockIntf->pcIeReferenceClockEnabled(
            getRetimerClockState(clkBuf));
    }
    else
    {
        lg2::error(
            "responseHandler: get_clock_output_enable_state unsuccessfull. reasonCode={RSNCOD} cc={CC} rc={RC}",
            "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

bool NsmPCIeDeviceGetClockOutput::getRetimerClockState(uint32_t clockBuffer)
{
    auto clkBuf = reinterpret_cast<nsm_pcie_clock_buffer_data*>(&clockBuffer);

    switch (deviceInstanceNumber)
    {
        case 0:
            return static_cast<bool>(clkBuf->clk_buf_retimer1);
        case 1:
            return static_cast<bool>(clkBuf->clk_buf_retimer2);
        case 2:
            return static_cast<bool>(clkBuf->clk_buf_retimer3);
        case 3:
            return static_cast<bool>(clkBuf->clk_buf_retimer4);
        case 4:
            return static_cast<bool>(clkBuf->clk_buf_retimer5);
        case 5:
            return static_cast<bool>(clkBuf->clk_buf_retimer6);
        case 6:
            return static_cast<bool>(clkBuf->clk_buf_retimer7);
        case 7:
            return static_cast<bool>(clkBuf->clk_buf_retimer8);
        default:
            return false;
    }
}

NsmPCIeDeviceQueryLTSSMState::NsmPCIeDeviceQueryLTSSMState(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const uint8_t& deviceIndex, std::string& inventoryObjPath) :
    NsmSensor(name, type), deviceIndex(deviceIndex)
{
    auto objPath = inventoryObjPath + name;
    lg2::debug("NsmPCIeDeviceQueryLTSSMState: {NAME}", "NAME", name.c_str());

    ltssmStateIntf = std::make_unique<LTSSMStateIntf>(bus, objPath.c_str());
}

std::optional<std::vector<uint8_t>>
    NsmPCIeDeviceQueryLTSSMState::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_6, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeDeviceQueryLTSSMState::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_query_scalar_group_telemetry_group_6 data;

    auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        ltssmStateIntf->ltssmState(LTSSMStateIntf::convertStateFromString(
            convertToLTSSMStateStr(data.ltssm_state)));
    }
    else
    {
        lg2::error(
            "responseHandler: query_scalar_group_telemetry_v1_group6 unsuccessfull. reasonCode = {RSNCOD} cc = {CC} rc = { RC }",
            "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return NSM_SW_SUCCESS;
}

static void CreatePCIeRetimerChassisPCIeDevice(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto inventoryObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    auto deviceType = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "DeviceType", interface.c_str());
    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    auto deviceInstance = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "DeviceInstance", interface.c_str());
    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");

    // device_index are between [1 to 8] for retimers, which is
    // calculated as device_instance + PCIE_RETIMER_DEVICE_INDEX_START
    uint8_t deviceIndex = static_cast<uint8_t>(deviceInstance) +
                          PCIE_RETIMER_DEVICE_INDEX_START;

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_PCIeDevices PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto retimerScalarTelemetry =
        std::make_shared<NsmPCIeDeviceQueryScalarTelemetry>(
            bus, name, associations, type, deviceType, deviceIndex,
            inventoryObjPath);
    if (!retimerScalarTelemetry)
    {
        lg2::error(
            "Failed to create pcie device scalar telemetry: UUID={UUID}, Type={TYPE}, Object_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", objPath);

        return;
    }
    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(retimerScalarTelemetry);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(retimerScalarTelemetry);
    }

    auto retimerRefClock = std::make_shared<NsmPCIeDeviceGetClockOutput>(
        bus, name, type, deviceInstance, inventoryObjPath);
    if (!retimerRefClock)
    {
        lg2::error(
            "Failed to create pcie device reference clock: UUID={UUID}, Type={TYPE}, Object_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", objPath);

        return;
    }

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(retimerRefClock);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(retimerRefClock);
    }

    auto retimerLtssmState = std::make_shared<NsmPCIeDeviceQueryLTSSMState>(
        bus, name, type, deviceIndex, inventoryObjPath);
    if (!retimerLtssmState)
    {
        lg2::error(
            "Failed to create pcie device ltssm state sensor: UUID={UUID}, Type={TYPE}, Object_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", objPath);

        return;
    }

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(retimerLtssmState);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(retimerLtssmState);
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePCIeRetimerChassisPCIeDevice,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeDevices")

} // namespace nsm
