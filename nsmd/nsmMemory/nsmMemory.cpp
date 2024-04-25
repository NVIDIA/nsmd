#include "nsmMemory.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>
#define MEMORY_INTERFACE "xyz.openbmc_project.Configuration.NSM_Memory"

namespace nsm
{

NsmMemoryErrorCorrection::NsmMemoryErrorCorrection(
    std::string& name, std::string& type, std::shared_ptr<DimmIntf> dimmIntf,
    std::string& correctionType) :
    NsmObject(name, type),
    dimmIntf(dimmIntf)
{
    dimmIntf->ecc(DimmIntf::convertEccFromString(correctionType));
}

NsmMemoryDeviceType::NsmMemoryDeviceType(std::string& name, std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf,
                                         std::string& memoryType) :
    NsmObject(name, type),
    dimmIntf(dimmIntf)
{

    dimmIntf->memoryType(DimmIntf::convertDeviceTypeFromString(memoryType));
}

NsmLocationIntfMemory::NsmLocationIntfMemory(sdbusplus::bus::bus& bus,
                                             std::string& name,
                                             std::string& type,
                                             std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    locationIntf =
        std::make_unique<LocationIntfMemory>(bus, inventoryObjPath.c_str());

    locationIntf->locationType(LocationTypesMemory::Embedded);
}

NsmMemoryHealth::NsmMemoryHealth(sdbusplus::bus::bus& bus, std::string& name,
                                 std::string& type,
                                 std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    healthIntf =
        std::make_unique<MemoryHealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(MemoryHealthType::OK);
}

NsmMemoryAssociation::NsmMemoryAssociation(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations) :
    NsmObject(name, type)
{
    associationDef = std::make_unique<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef->associations(associations_list);
}

NsmRowRemapState::NsmRowRemapState(
    std::string& name, std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmRowRemapIntf: create sensor:{NAME}", "NAME", name.c_str());
    memoryRowRemappingStateIntf = memoryRowRemappingIntf;
}

void NsmRowRemapState::updateReading(bitfield8_t flags)
{
    memoryRowRemappingStateIntf->rowRemappingFailureState(flags.bits.bit0);
    memoryRowRemappingStateIntf->rowRemappingPendingState(flags.bits.bit1);
}

std::optional<std::vector<uint8_t>>
    NsmRowRemapState::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_row_remap_state_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmRowRemapState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_row_remap_state_resp(responseMsg, responseLen, &cc,
                                              &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_row_remap_state_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmRowRemappingCounts::NsmRowRemappingCounts(
    std::string& name, std::string& type,
    std::shared_ptr<MemoryRowRemappingIntf> memoryRowRemappingIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmRowRemappingCount: create sensor:{NAME}", "NAME",
              name.c_str());
    memoryRowRemappingCountsIntf = memoryRowRemappingIntf;
}

void NsmRowRemappingCounts::updateReading(uint32_t correctable_error,
                                          uint32_t uncorrectable_error)
{
    memoryRowRemappingCountsIntf->ceRowRemappingCount(correctable_error);
    memoryRowRemappingCountsIntf->ueRowRemappingCount(uncorrectable_error);
}

std::optional<std::vector<uint8_t>>
    NsmRowRemappingCounts::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_row_remapping_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_row_remapping_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmRowRemappingCounts::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    uint32_t correctable_error;
    uint32_t uncorrectable_error;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_row_remapping_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &correctable_error, &uncorrectable_error);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(correctable_error, uncorrectable_error);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_get_row_remapping_counts_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmEccErrorCountsDram::NsmEccErrorCountsDram(
    std::string& name, std::string& type,
    std::shared_ptr<EccModeIntfDram> eccIntf) :
    NsmSensor(name, type),
    eccIntf(eccIntf)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmEccErrorCountsDram::updateReading(
    struct nsm_ECC_error_counts errorCounts)
{
    eccIntf->ceCount(errorCounts.dram_corrected);
    eccIntf->ueCount(errorCounts.dram_uncorrected);
}

std::optional<std::vector<uint8_t>>
    NsmEccErrorCountsDram::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_ECC_error_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmEccErrorCountsDram::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_ECC_error_counts errorCounts;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_ECC_error_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &errorCounts);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(errorCounts);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_ECC_error_counts_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmClockLimitMemory::NsmClockLimitMemory(const std::string& name,
                                         const std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf) :
    NsmSensor(name, type),
    dimmIntf(dimmIntf)

{
    lg2::info("NsmClockLimitMemory: create sensor:{NAME}", "NAME",
              name.c_str());
}

void NsmClockLimitMemory::updateReading(
    const struct nsm_clock_limit& clockLimit)
{
    std::vector<uint16_t> speedLimit{
        static_cast<uint16_t>(clockLimit.present_limit_min),
        static_cast<uint16_t>(clockLimit.present_limit_max)};
    dimmIntf->allowedSpeedsMT(speedLimit);
}

std::optional<std::vector<uint8_t>>
    NsmClockLimitMemory::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_clock_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = MEMORY_CLOCK;
    auto rc = encode_get_clock_limit_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_clock_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmClockLimitMemory::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_clock_limit clockLimit;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_clock_limit_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockLimit);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockLimit);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_clock_limit_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmMemCurrClockFreq::NsmMemCurrClockFreq(const std::string& name,
                                         const std::string& type,
                                         std::shared_ptr<DimmIntf> dimmIntf) :
    NsmSensor(name, type),
    dimmIntf(dimmIntf)

{
    lg2::info("NsmMemCurrClockFreq: create sensor:{NAME}", "NAME",
              name.c_str());
}

void NsmMemCurrClockFreq::updateReading(const uint32_t& clockFreq)
{
    dimmIntf->memoryConfiguredSpeedInMhz(clockFreq);
}

std::optional<std::vector<uint8_t>>
    NsmMemCurrClockFreq::genRequestMsg(eid_t eid, uint8_t instanceId)
{

    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_curr_clock_freq_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = MEMORY_CLOCK;
    auto rc = encode_get_curr_clock_freq_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_curr_clock_freq_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmMemCurrClockFreq::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    uint32_t clockFreq = 1;
    uint16_t data_size;
    uint16_t reason_code;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockFreq);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockFreq);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_get_curr_clock_freq_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

static void createNsmMemorySensor(SensorManager& manager,
                                  const std::string& interface,
                                  const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", MEMORY_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", MEMORY_INTERFACE);

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", MEMORY_INTERFACE);
        inventoryObjPath = inventoryObjPath + "_DRAM_0";
        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }

        if (type == "NSM_Memory")
        {
            auto dimmIntf =
                std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
            auto correctionType =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "ErrorCorrection", interface.c_str());
            auto sensorErrorCorrection =
                std::make_shared<NsmMemoryErrorCorrection>(name, type, dimmIntf,
                                                           correctionType);
            nsmDevice->deviceSensors.push_back(sensorErrorCorrection);
            auto deviceType = utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "DeviceType", interface.c_str());
            auto sensorDeviceType = std::make_shared<NsmMemoryDeviceType>(
                name, type, dimmIntf, deviceType);
            nsmDevice->deviceSensors.push_back(sensorDeviceType);
            auto sensorHealth = std::make_shared<NsmMemoryHealth>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorHealth);
            auto sensorMemoryLocation = std::make_shared<NsmLocationIntfMemory>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensorMemoryLocation);
            auto associations =
                utils::getAssociations(objPath, interface + ".Associations");
            auto associationSensor = std::make_shared<NsmMemoryAssociation>(
                bus, name, type, inventoryObjPath, associations);
            nsmDevice->deviceSensors.push_back(associationSensor);

            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto clockLimitSensor =
                std::make_shared<NsmClockLimitMemory>(name, type, dimmIntf);
            auto currClockFreqSensor =
                std::make_shared<NsmMemCurrClockFreq>(name, type, dimmIntf);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(clockLimitSensor);
                nsmDevice->prioritySensors.push_back(currClockFreqSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(clockLimitSensor);
                nsmDevice->roundRobinSensors.push_back(currClockFreqSensor);
            }
        }
        else if (type == "NSM_RowRemapping")
        {
            auto rowRemapIntf = std::make_shared<MemoryRowRemappingIntf>(
                bus, inventoryObjPath.c_str());
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto sensorRowRemapState =
                std::make_shared<NsmRowRemapState>(name, type, rowRemapIntf);
            auto sensorRowRemappingCounts =
                std::make_shared<NsmRowRemappingCounts>(name, type,
                                                        rowRemapIntf);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensorRowRemapState);
                nsmDevice->prioritySensors.push_back(sensorRowRemappingCounts);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensorRowRemapState);
                nsmDevice->roundRobinSensors.push_back(
                    sensorRowRemappingCounts);
            }
        }
        else if (type == "NSM_ECC")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto eccModeIntf = std::make_shared<EccModeIntfDram>(
                bus, inventoryObjPath.c_str());
            auto sensor = std::make_shared<NsmEccErrorCountsDram>(name, type,
                                                                  eccModeIntf);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
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

REGISTER_NSM_CREATION_FUNCTION(createNsmMemorySensor,
                               "xyz.openbmc_project.Configuration.NSM_Memory")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmMemorySensor,
    "xyz.openbmc_project.Configuration.NSM_Memory.ECCMode")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmMemorySensor,
    "xyz.openbmc_project.Configuration.NSM_Memory.RowRemapping")

} // namespace nsm