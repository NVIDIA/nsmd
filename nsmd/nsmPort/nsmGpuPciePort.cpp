#include "nsmGpuPciePort.hpp"
#define GPU_PCIe_INTERFACE "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0"
namespace nsm
{

NsmGpuPciePort::NsmGpuPciePort(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& health, const std::string& chasisState,
    const std::vector<utils::Association>& associations,
    const std::string& inventoryObjPath) : NsmObject(name, type)
{
    lg2::info("NsmGpuPciePort: create sensor:{NAME}", "NAME", name.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());

    chasisStateIntf =
        std::make_unique<ChasisStateIntf>(bus, inventoryObjPath.c_str());
    chasisStateIntf->currentPowerState(
        ChasisStateIntf::convertPowerStateFromString(chasisState));

    healthIntf = std::make_unique<HealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(HealthIntf::convertHealthTypeFromString(health));

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

NsmGpuPciePortInfo::NsmGpuPciePortInfo(
    const std::string& name, const std::string& type,
    const std::string& portType, const std::string& portProtocol,
    std::shared_ptr<PortInfoIntf> portInfoIntf) :
    NsmObject(name, type), portInfoIntf(portInfoIntf)
{
    lg2::info("NsmGpuPciePortInfo: create sensor:{NAME}", "NAME", name.c_str());
    portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(portType));
    portInfoIntf->protocol(
        PortInfoIntf::convertPortProtocolFromString(portProtocol));
}

NsmClearPCIeCounters::NsmClearPCIeCounters(
    const std::string& name, const std::string& type, const uint8_t groupId,
    uint8_t deviceIndex, std::shared_ptr<ClearPCIeIntf> clearPCIeIntf) :
    NsmObject(name, type), groupId(groupId), deviceIndex(deviceIndex),
    clearPCIeIntf(clearPCIeIntf)
{
    lg2::info("NsmClearPCIeCounters:: create sensor {NAME} of group {GROUP_ID}",
              "NAME", name, "GROUP_ID", groupId);
}

requester::Coroutine NsmClearPCIeCounters::update(SensorManager& manager,
                                                  eid_t eid)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        0, deviceIndex, groupId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_query_available_clearable_scalar_data_sources_v1_req failed for group {A}. eid={EID} rc={RC}",
            "A", groupId, "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmClearPCIeCounters SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t mask_length;
    bitfield8_t availableSource[MAX_SCALAR_SOURCE_MASK_SIZE];
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE];

    rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
        responseMsg.get(), responseLen, &cc, &dataSize, &reason_code,
        &mask_length, (uint8_t*)availableSource, (uint8_t*)clearableSource);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clearableSource);
    }
    else
    {
        lg2::error(
            "decode_query_available_clearable_scalar_data_sources_v1_respp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

void NsmClearPCIeCounters::findAndUpdateCounter(
    CounterType counter, std::vector<CounterType>& clearableCounters)
{
    if (std::find(clearableCounters.begin(), clearableCounters.end(),
                  counter) == clearableCounters.end())
    {
        clearableCounters.push_back(counter);
    }
}

void NsmClearPCIeCounters::updateReading(const bitfield8_t clearableSource[])
{
    std::vector<CounterType> clearableCounters =
        clearPCIeIntf->clearableCounters();

    switch (groupId)
    {
        case 2:
            if (clearableSource[0].bits.bit0)
            {
                findAndUpdateCounter(CounterType::NonFatalErrorCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit1)
            {
                findAndUpdateCounter(CounterType::FatalErrorCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit2)
            {
                findAndUpdateCounter(CounterType::UnsupportedRequestCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit3)
            {
                findAndUpdateCounter(CounterType::CorrectableErrorCount,
                                     clearableCounters);
            }
            break;
        case 3:
            if (clearableSource[0].bits.bit0)
            {
                findAndUpdateCounter(CounterType::L0ToRecoveryCount,
                                     clearableCounters);
            }
            break;
        case 4:
            if (clearableSource[0].bits.bit1)
            {
                findAndUpdateCounter(CounterType::NAKReceivedCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit2)
            {
                findAndUpdateCounter(CounterType::NAKSentCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit4)
            {
                findAndUpdateCounter(CounterType::ReplayRolloverCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit6)
            {
                findAndUpdateCounter(CounterType::ReplayCount,
                                     clearableCounters);
            }
            break;
        default:
            break;
    }
    clearPCIeIntf->clearableCounters(clearableCounters);
}

static void createNsmGpuPcieSensor(SensorManager& manager,
                                   const std::string& interface,
                                   const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", GPU_PCIe_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", GPU_PCIe_INTERFACE);

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", GPU_PCIe_INTERFACE);
        inventoryObjPath = inventoryObjPath + "/Ports/PCIe_0";

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_GPU_PCIe_0 PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }
        if (type == "NSM_GPU_PCIe_0")
        {
            auto associations =
                utils::getAssociations(objPath, interface + ".Associations");
            auto health = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "Health", interface.c_str());
            auto chasisState =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "ChasisPowerState", interface.c_str());
            auto sensor = std::make_shared<NsmGpuPciePort>(
                bus, name, type, health, chasisState, associations,
                inventoryObjPath);
            nsmDevice->deviceSensors.emplace_back(sensor);
            auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceIndex", GPU_PCIe_INTERFACE);

            auto clearableScalarGroup =
                utils::DBusHandler().getDbusProperty<std::vector<uint64_t>>(
                    objPath.c_str(), "ClearableScalarGroup",
                    GPU_PCIe_INTERFACE);

            auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
                bus, inventoryObjPath.c_str());

            for (auto groupId : clearableScalarGroup)
            {
                auto clearPCIeSensorGroup =
                    std::make_shared<NsmClearPCIeCounters>(
                        name, type, groupId, deviceIndex, clearPCIeIntf);
                nsmDevice->addStaticSensor(clearPCIeSensorGroup);
            }
        }
        else if (type == "NSM_PortInfo")
        {
            auto portType = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "PortType", interface.c_str());
            auto portProtocol =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "PortProtocol", interface.c_str());
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto deviceIndex = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceIndex", GPU_PCIe_INTERFACE);
            auto portInfoIntf =
                std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
            auto portWidthIntf =
                std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
            auto portInfoSensor = std::make_shared<NsmGpuPciePortInfo>(
                name, type, portType, portProtocol, portInfoIntf);
            nsmDevice->deviceSensors.emplace_back(portInfoSensor);
            auto pcieECCIntfSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
                name, type, portInfoIntf, portWidthIntf, deviceIndex);

            if (priority)
            {
                nsmDevice->prioritySensors.emplace_back(
                    pcieECCIntfSensorGroup1);
            }
            else
            {
                nsmDevice->roundRobinSensors.emplace_back(
                    pcieECCIntfSensorGroup1);
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        return;
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor, "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortInfo")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortState")
} // namespace nsm
