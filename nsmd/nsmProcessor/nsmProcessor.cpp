#include "nsmProcessor.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <optional>
#include <vector>

#define PROCESSOR_INTERFACE "xyz.openbmc_project.Configuration.NSM_Processor"

namespace nsm
{

NsmAcceleratorIntf::NsmAcceleratorIntf(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath):
    NsmObject(name, type)
{
    acceleratorIntf = std::make_unique<AcceleratorIntf>(bus, inventoryObjPath.c_str());
    acceleratorIntf->type(accelaratorType::GPU);
}

NsmMigMode::NsmMigMode(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath) :
    NsmSensor(name, type)

{
    lg2::info("NsmMigMode: create sensor:{NAME}", "NAME", name.c_str());
    migModeIntf = std::make_unique<MigModeIntf>(bus, inventoryObjPath.c_str());
}

void NsmMigMode::updateReading(bitfield8_t flags)
{
    migModeIntf->migModeEnabled(flags.bits.bit0);
}

std::optional<std::vector<uint8_t>>
    NsmMigMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_MIG_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmMigMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_MIG_mode_resp(responseMsg, responseLen, &cc,
                                       &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_temperature_reading_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}


NsmEccMode::NsmEccMode(std::string& name, std::string& type,
                       std::shared_ptr<EccModeIntf> eccIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmEccMode: create sensor:{NAME}", "NAME", name.c_str());
    eccModeIntf = eccIntf;
}

void NsmEccMode::updateReading(bitfield8_t flags)
{
    eccModeIntf->eccModeEnabled(flags.bits.bit0);
    eccModeIntf->pendingECCState(flags.bits.bit1);
}

std::optional<std::vector<uint8_t>>
    NsmEccMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_ECC_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEccMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_ECC_mode_resp(responseMsg, responseLen, &cc,
                                       &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_ECC_mode_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}


NsmEccErrorCounts::NsmEccErrorCounts(std::string& name, std::string& type,
                                     std::shared_ptr<EccModeIntf> eccIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    eccErrorCountIntf = eccIntf;
}

void NsmEccErrorCounts::updateReading(struct nsm_ECC_error_counts errorCounts)
{
    eccErrorCountIntf->ceCount(errorCounts.sram_corrected);
    int64_t ueCount = errorCounts.sram_uncorrected_secded +
                      errorCounts.sram_uncorrected_parity;
    eccErrorCountIntf->ueCount(ueCount);
}

std::optional<std::vector<uint8_t>>
    NsmEccErrorCounts::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_common_req));
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

uint8_t NsmEccErrorCounts::handleResponseMsg(const struct nsm_msg* responseMsg,
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

static void
    createNsmProcessorSensor(const std::string& interface,
                             const std::string& objPath,
                             NsmDeviceTable& nsmDevices)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", PROCESSOR_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", interface.c_str());

        auto nsmDevice = findNsmDeviceByUUID(nsmDevices, uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }

        if(type == "NSM_Processor")
        {
            auto sensor =
                std::make_shared<NsmAcceleratorIntf>(bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);
        }

        if (type == "NSM_MIG")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

            auto sensor =
                std::make_shared<NsmMigMode>(bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
            }
        }
        else if (type == "NSM_ECC")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

            auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
            auto eccModeSensor =
                std::make_shared<NsmEccMode>(name, type, eccIntf);

            nsmDevice->deviceSensors.push_back(eccModeSensor);

            auto eccErrorCntSensor =
                std::make_shared<NsmEccErrorCounts>(name, type, eccIntf);
            
            nsmDevice->deviceSensors.push_back(eccErrorCntSensor);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(eccModeSensor);
                nsmDevice->prioritySensors.push_back(eccErrorCntSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(eccModeSensor);
                nsmDevice->roundRobinSensors.push_back(eccErrorCntSensor);
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

REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor,
                                "xyz.openbmc_project.Configuration.NSM_Processor")
REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor,
                                "xyz.openbmc_project.Configuration.NSM_Processor.MIGMode")
REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor,
                                 "xyz.openbmc_project.Configuration.NSM_Processor.ECCMode")


} // namespace
