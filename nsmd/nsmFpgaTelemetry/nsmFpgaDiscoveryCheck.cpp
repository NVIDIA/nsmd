// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaDiscoveryCheck.hpp"

#include "device-configuration.h"

#include "nsmDevice.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace nsm
{

namespace
{
// Hex of a 32-byte digest for logs, through the shared 256-bit formatter
std::string
    digestHex(const std::array<uint8_t, NSM_FPGA_MAP_DIGEST_SIZE>& digest)
{
    return utils::vectorTo256BitHexString(
        std::vector<uint8_t>(digest.begin(), digest.end()));
}
} // namespace

NsmFpgaDiscoveryCheck::NsmFpgaDiscoveryCheck(
    const std::string& name, const std::string& type, CheckKind checkKind,
    uint32_t configurationType, uint64_t digestAddress,
    size_t digestLengthDwords, std::shared_ptr<NsmFpgaStatus> status,
    const nsm_fpga_map_identity& map) :
    NsmSensor(name, type), checkKind(checkKind),
    configurationType(configurationType), digestAddress(digestAddress),
    digestLengthDwords(digestLengthDwords), status(std::move(status)),
    mapName(map.name), expectedDigest(fpgaMapDigest(map))
{}

std::optional<std::vector<uint8_t>>
    NsmFpgaDiscoveryCheck::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    if (checkKind == CheckKind::ConfigurationType)
    {
        std::vector<uint8_t> request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_configs_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_supported_device_configs_req(
            instanceId, typeHandle, requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "encode_get_supported_device_configs_req failed. name={NAME} eid={EID} rc={RC}",
                "NAME", getName(), "EID", eid, "RC", rc);
            return std::nullopt;
        }
        return request;
    }
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_config_v2_req) - 1 +
                                 sizeof(nsm_mmio_access_query));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_config_mmio_req(
        instanceId, configurationType, digestAddress,
        static_cast<uint16_t>(digestLengthDwords), requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_device_config_mmio_req failed. name={NAME} eid={EID} rc={RC}",
            "NAME", getName(), "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmFpgaDiscoveryCheck::handleTypeResponse(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint32_t nextHandle = 0;
    uint32_t totalCount = 0;
    uint16_t advertisedCount = 0;
    std::array<uint32_t, 64> advertisedTypes{};

    auto rc = decode_get_supported_device_configs_resp(
        responseMsg, responseLen, &cc, &reasonCode, &nextHandle, &totalCount,
        advertisedTypes.data(), static_cast<uint16_t>(advertisedTypes.size()),
        &advertisedCount);

    const bool decoded = rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS;
    const auto advertisedEnd = advertisedTypes.begin() + advertisedCount;
    bool typeAdvertised = decoded &&
                          std::find(advertisedTypes.begin(), advertisedEnd,
                                    configurationType) != advertisedEnd;
    if (!typeAdvertised && decoded && nextHandle != 0 &&
        nextHandle != typeHandle)
    {
        // The type may sit in a later chunk: update() asks for it next
        typeHandle = nextHandle;
        moreTypeChunks = true;
        responded = true;
        return NSM_SUCCESS;
    }
    typeHandle = 0;
    moreTypeChunks = false;

    responded = true;
    const std::string outcome =
        typeAdvertised
            ? "ok"
            : "rc=" + std::to_string(rc) + " cc=" + std::to_string(cc) +
                  " reason=" + std::to_string(reasonCode) +
                  " count=" + std::to_string(advertisedCount);
    if (outcomeChanged(outcome))
    {
        if (typeAdvertised)
        {
            lg2::info(
                "FPGA advertises configuration type {TYPE}. name={NAME} count={COUNT}",
                "TYPE", configurationType, "NAME", getName(), "COUNT",
                advertisedCount);
        }
        else
        {
            lg2::error(
                "FPGA configuration type check failed. name={NAME} type={TYPE} reasonCode={REASONCODE} cc={CC} rc={RC} count={COUNT} nextHandle={NEXT}",
                "NAME", getName(), "TYPE", configurationType, "REASONCODE",
                reasonCode, "CC", cc, "RC", rc, "COUNT", advertisedCount,
                "NEXT", nextHandle);
        }
    }

    passedFlag = typeAdvertised;
    if (status)
    {
        status->setTypeCheck(typeAdvertised);
    }
    return cc ? cc : static_cast<uint8_t>(rc);
}

uint8_t
    NsmFpgaDiscoveryCheck::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    return checkKind == CheckKind::ConfigurationType
               ? handleTypeResponse(responseMsg, responseLen)
               : handleDigestResponse(responseMsg, responseLen);
}

uint8_t NsmFpgaDiscoveryCheck::handleDigestResponse(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> digestBytes(4u * digestLengthDwords);

    // libnsm checks the lengths: exactly the DWORDs asked for, nothing pending
    auto rc = decode_get_device_config_mmio_resp(
        responseMsg, responseLen, static_cast<uint16_t>(digestLengthDwords),
        &cc, &reasonCode, digestBytes.data());

    const bool decoded = rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS &&
                         digestBytes.size() >= expectedDigest.size();

    responded = true;
    std::array<uint8_t, 32> liveDigest{};
    if (decoded)
    {
        std::copy_n(digestBytes.begin(), liveDigest.size(), liveDigest.begin());
    }
    // The FPGA must run the build the platform's map row names
    const bool digestKnown = decoded && liveDigest == expectedDigest;
    const std::string outcome =
        digestKnown ? "ok " + mapName
                    : "rc=" + std::to_string(rc) + " cc=" + std::to_string(cc) +
                          " reason=" + std::to_string(reasonCode) +
                          " live=" + digestHex(liveDigest);
    if (outcomeChanged(outcome))
    {
        if (digestKnown)
        {
            lg2::info(
                "FPGA map digest matches the platform map. name={NAME} map={MAP} digest={DIGEST}",
                "NAME", getName(), "MAP", mapName, "DIGEST",
                digestHex(liveDigest));
        }
        else
        {
            lg2::error(
                "FPGA map digest check failed. name={NAME} reasonCode={REASONCODE} cc={CC} rc={RC} expected={EXPECTED} live={LIVE}",
                "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc,
                "EXPECTED", digestHex(expectedDigest), "LIVE",
                digestHex(liveDigest));
        }
    }

    passedFlag = digestKnown;
    if (status)
    {
        status->setDigestCheck(digestKnown);
    }
    return cc ? cc : static_cast<uint8_t>(rc);
}

bool NsmFpgaDiscoveryCheck::outcomeChanged(const std::string& outcome)
{
    if (outcome == lastOutcome)
    {
        return false;
    }
    lastOutcome = outcome;
    return true;
}

requester::Coroutine
    NsmFpgaDiscoveryCheck::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    responded = false;
    uint8_t rc = co_await NsmSensor::update(nsmDevice);
    if (moreTypeChunks)
    {
        // The type was not in this chunk: the next poll reads the next one,
        // until the chunk bound says the device does not advertise it
        if (++typeChunksRead < maxTypeChunks)
        {
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
        moreTypeChunks = false;
        typeHandle = 0;
        typeChunksRead = 0;
        passedFlag = false;
        if (status)
        {
            status->setTypeCheck(false);
        }
        if (outcomeChanged("type not advertised in " +
                           std::to_string(maxTypeChunks) + " chunks"))
        {
            lg2::error(
                "FPGA configuration type check failed. name={NAME} type={TYPE}: not in the first {CHUNKS} chunks",
                "NAME", getName(), "TYPE", configurationType, "CHUNKS",
                maxTypeChunks);
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    typeChunksRead = 0;
    if (rc != NSM_SW_SUCCESS)
    {
        // No reply reached the handler: transport, timeout or command check
        if (!responded && outcomeChanged("no reply rc=" + std::to_string(rc)))
        {
            lg2::error(
                "FPGA discovery check got no reply. name={NAME} eid={EID} rc={RC}",
                "NAME", getName(), "EID", nsmDevice->getEid(), "RC", rc);
        }
        passedFlag = false;
        if (status)
        {
            if (checkKind == CheckKind::ConfigurationType)
            {
                status->setTypeCheck(false);
            }
            else
            {
                status->setDigestCheck(false);
            }
        }
    }
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine createNsmFpgaDiscoveryCheck(SensorManager& manager,
                                                 const std::string& interface,
                                                 const std::string& objPath)
{
    auto properties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    const std::string type = interface.substr(interface.find_last_of('.') + 1);
    const std::string name = utils::makeDBusNameValid(
        utils::getOptionalString(properties, "Name").value_or(""));
    const uuid_t uuid =
        utils::getOptionalString(properties, "UUID").value_or("");

    // The Platform picks the map row: type, identity register and digest
    const std::string platform =
        utils::getOptionalString(properties, "Platform").value_or("");
    const nsm_fpga_map_identity* map =
        nsm_fpga_map_identity_by_platform(platform.c_str());
    if (map == nullptr)
    {
        lg2::error(
            "NSM_FpgaDiscoveryCheck entry rejected: unknown Platform. Name={NAME} Platform={PLATFORM}",
            "NAME", name, "PLATFORM", platform);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    const nsm_fpga_map_identity& defaults = *map;
    const uint64_t configurationType =
        utils::getOptionalUnsigned(properties, "ConfigurationType")
            .value_or(defaults.configuration_type);
    const uint64_t digestAddress = utils::getOptionalUnsigned(properties,
                                                              "DigestAddress")
                                       .value_or(defaults.identity_address);
    const uint64_t digestLength = utils::getOptionalUnsigned(properties,
                                                             "DigestLength")
                                      .value_or(defaults.identity_dwords);

    // Range checks happen on the 64-bit values, before any narrowing cast
    if (configurationType > UINT32_MAX || (digestAddress % 4) != 0 ||
        digestLength < NSM_MMIO_ACCESS_MIN_LENGTH_DWORDS ||
        digestLength > NSM_MMIO_ACCESS_MAX_LENGTH_DWORDS ||
        4u * digestLength < NSM_FPGA_MAP_DIGEST_SIZE)
    {
        lg2::error(
            "NSM_FpgaDiscoveryCheck entry rejected: invalid DigestAddress or DigestLength. Name={NAME} Path={PATH}",
            "NAME", name, "PATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "NSM_FpgaDiscoveryCheck entry rejected: UUID matches no NsmDevice. UUID={UUID} Name={NAME}",
            "UUID", uuid, "NAME", name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    const std::string statusName =
        "HGX_FPGA_" + std::to_string(nsmDevice->getInstanceNumber());
    // Status lives on the FPGA processor object so Redfish shows its State
    const std::string inventoryPath =
        utils::getOptionalString(properties, "InventoryObjPath")
            .value_or(fpgaProcessorPath(nsmDevice->getInstanceNumber()));
    // One status object per FPGA, shared with the telemetry items: only the
    // entry that creates it registers it with the device
    bool statusCreatedHere = false;
    auto status = NsmFpgaStatus::getOrCreate(uuid, statusName, inventoryPath,
                                             statusCreatedHere);
    if (statusCreatedHere)
    {
        // Its static refresh takes the online state, the device callbacks
        // carry every later transition
        std::shared_ptr<NsmObject> statusObject = status;
        nsmDevice->addSensor(statusObject, PollingType::Static);
    }

    auto typeCheck = std::make_shared<NsmFpgaDiscoveryCheck>(
        name + "_ConfigurationType", type,
        NsmFpgaDiscoveryCheck::CheckKind::ConfigurationType,
        static_cast<uint32_t>(configurationType), digestAddress, digestLength,
        status, *map);
    auto digestCheck = std::make_shared<NsmFpgaDiscoveryCheck>(
        name + "_MapDigest", type, NsmFpgaDiscoveryCheck::CheckKind::MapDigest,
        static_cast<uint32_t>(configurationType), digestAddress, digestLength,
        status, *map);

    for (auto& check : {typeCheck, digestCheck})
    {
        // Refreshed with the capabilities at discovery, then round robin
        std::shared_ptr<NsmObject> object = check;
        nsmDevice->addCapabilityRefreshSensor(object);
        nsmDevice->addSensor(object, PollingType::RoundRobin);
    }
    lg2::info(
        "Created NSM FPGA discovery checks: UUID={UUID} Name={NAME} DigestAddress={ADDR}",
        "UUID", uuid, "NAME", name, "ADDR", digestAddress);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmFpgaDiscoveryCheck,
    "xyz.openbmc_project.Configuration.NSM_FpgaDiscoveryCheck")

} // namespace nsm
