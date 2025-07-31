#include "nsmPCIeRetimerSwitchDI.hpp"

#include "dBusAsyncUtils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmPCIeRetimerSwitchDI::NsmPCIeRetimerSwitchDI(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, std::string& inventoryObjPath, uint8_t deviceIdx) :
    NsmObject(name, type), deviceIndex(deviceIdx)
{
    auto objPath = inventoryObjPath + name;
    lg2::debug("NsmPCIeRetimerSwitchDI: {NAME}", "NAME", name.c_str());

    // initialize members
    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    switchIntf = std::make_unique<SwitchIntf>(bus, objPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    associationDefIntf->associations(associationsList);

    std::vector<SwitchIntf::SwitchType> supported_protocols;
    supported_protocols.emplace_back(SwitchIntf::SwitchType::PCIe);

    switchIntf->type(SwitchIntf::SwitchType::PCIe);
    switchIntf->supportedProtocols(supported_protocols);
    switchIntf->deviceId("");
    switchIntf->vendorId("");
}

requester::Coroutine NsmPCIeRetimerSwitchDI::update(SensorManager& manager,
                                                    eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_query_scalar_group_telemetry_v1_req(
        0, deviceIndex, GROUP_ID_0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    struct nsm_query_scalar_group_telemetry_group_0 data;

    rc = decode_query_scalar_group_telemetry_v1_group0_resp(
        responseMsg.get(), responseLen, &cc, &dataSize, &reasonCode, &data);

    LG2_ERROR_FLT(
        "query_scalar_group_telemetry_v1_group0 failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // update values
        std::stringstream hexaDeviceId;
        std::stringstream hexaVendorId;
        hexaDeviceId << "0x" << std::setfill('0') << std::setw(4) << std::hex
                     << data.pci_device_id;
        hexaVendorId << "0x" << std::setfill('0') << std::setw(4) << std::hex
                     << data.pci_vendor_id;

        switchIntf->deviceId(hexaDeviceId.str());
        switchIntf->vendorId(hexaVendorId.str());
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmPCIeRetimerSwitchGetClockState::NsmPCIeRetimerSwitchGetClockState(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const uint64_t& deviceInstance, std::string& inventoryObjPath) :
    NsmSensor(name, type)
{
    auto objPath = inventoryObjPath + name;
    lg2::debug("NsmPCIeRetimerSwitchGetClockState: {NAME}", "NAME",
               name.c_str());

    // initialize members
    pcieRefClockIntf = std::make_unique<PCIeRefClockIntf>(bus, objPath.c_str());
    clkBufIndex = PCIE_CLKBUF_INDEX;
    deviceInstanceNumber = static_cast<uint8_t>(deviceInstance);
}

std::optional<std::vector<uint8_t>>
    NsmPCIeRetimerSwitchGetClockState::genRequestMsg(eid_t eid,
                                                     uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(instanceId, clkBufIndex,
                                                       requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_clock_output_enable_state_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeRetimerSwitchGetClockState::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t clkBuf = 0;

    auto rc = decode_get_clock_output_enable_state_resp(
        responseMsg, responseLen, &cc, &reasonCode, &dataSize, &clkBuf);

    LG2_ERROR_FLT(
        "get_clock_output_enable_state failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // update values
        pcieRefClockIntf->pcIeReferenceClockEnabled(
            getRetimerClockState(clkBuf));
    }
    return cc ? cc : rc;
}

bool NsmPCIeRetimerSwitchGetClockState::getRetimerClockState(
    uint32_t clockBuffer)
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

static requester::Coroutine
    CreatePCIeRetimerSwitch(SensorManager& manager,
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
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string inventoryObjPath{};
    if (allCurrentIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("InventoryObjPath"));
    }
    bool priority{};
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }
    uint64_t deviceInstance{};
    if (allCurrentIfaceProperties.count("DeviceInstance"))
    {
        deviceInstance =
            std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceInstance"));
    }

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    // device_index are between [1 to 8] for retimers, which is
    // calculated as device_instance + PCIE_RETIMER_DEVICE_INDEX_START
    uint8_t deviceIndex = static_cast<uint8_t>(deviceInstance) +
                          PCIE_RETIMER_DEVICE_INDEX_START;

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NSM_PCIeRetimer_Switch PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto retimerSwitchDi = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, inventoryObjPath, deviceIndex);
    if (!retimerSwitchDi)
    {
        lg2::error(
            "Failed to create pcie retimer switch device inventory: UUID={UUID}, Type={TYPE}, Object_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    nsmDevice->standByToDcRefreshSensors.emplace_back(retimerSwitchDi);

    // update sensor
    nsmDevice->addStaticSensor(retimerSwitchDi);

    auto retimerSwitchRefClock =
        std::make_shared<NsmPCIeRetimerSwitchGetClockState>(
            bus, name, type, deviceInstance, inventoryObjPath);
    if (!retimerSwitchRefClock)
    {
        lg2::error(
            "Failed to create pcie retimer switch reference clock: UUID={UUID}, Type={TYPE}, Object_Path={OBJPATH}",
            "UUID", uuid, "TYPE", type, "OBJPATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(retimerSwitchRefClock);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(retimerSwitchRefClock);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePCIeRetimerSwitch,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Switch")

} // namespace nsm
