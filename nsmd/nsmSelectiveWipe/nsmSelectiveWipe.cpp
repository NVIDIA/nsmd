// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmSelectiveWipe.hpp"

#include "diagnostics.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

namespace nsm
{

NsmSelectiveWipeObject::DbusInterface::DbusInterface(
    sdbusplus::bus_t& bus, const char* path, NsmSelectiveWipeObject& owner) :
    SelectiveDataWipeIntf(bus, path), owner(owner)
{
    wipeStatus(WipeOperationStatus::Unavailable);
}

void NsmSelectiveWipeObject::DbusInterface::wipe(
    std::vector<WipeDataClass> targets)
{
    owner.wipe(std::move(targets));
}

NsmSelectiveWipeObject::NsmSelectiveWipeObject(
    sdbusplus::bus_t& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath, const uuid_t& uuid) :
    NsmObject(name, type), uuid(uuid)
{
    const auto objPath = inventoryObjPath + name;
    dbusInterface = std::make_unique<DbusInterface>(bus, objPath.c_str(),
                                                    *this);
    lg2::info("SelectiveDataWipe: created object at {PATH}", "PATH", objPath);
}

NsmSelectiveWipeObject::NsmSelectiveWipeObject(const std::string& name,
                                               const std::string& type,
                                               const uuid_t& uuid) :
    NsmObject(name, type), uuid(uuid)
{}

void NsmSelectiveWipeObject::setWipeStatus(WipeOperationStatus status)
{
    wipeStatus = status;
    if (dbusInterface)
    {
        dbusInterface->wipeStatus(status);
    }
}

void NsmSelectiveWipeObject::wipe(std::vector<WipeDataClass> targets)
{
    if (wipeStatus == WipeOperationStatus::InProgress)
    {
        lg2::error("SelectiveDataWipe: wipe already in progress");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    uint16_t targetMask = 0;
    for (const auto target : targets)
    {
        switch (target)
        {
            case WipeDataClass::ScratchData:
                targetMask |= NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(
                    NSM_WIPE_TARGET_SCRATCH_DATA);
                break;
            case WipeDataClass::DiagnosticData:
                targetMask |= NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(
                    NSM_WIPE_TARGET_DIAGNOSTIC_DATA);
                break;
            case WipeDataClass::LogData:
                targetMask |= NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(
                    NSM_WIPE_TARGET_LOG_DATA);
                break;
            case WipeDataClass::UserData:
                targetMask |= NSM_SELECTIVE_DATA_WIPE_TARGET_BIT(
                    NSM_WIPE_TARGET_USER_DATA);
                break;
            default:
                lg2::error("SelectiveDataWipe: unknown wipe data class");
                throw sdbusplus::error::xyz::openbmc_project::common::
                    InvalidArgument{};
        }
    }

    if (targetMask == 0)
    {
        lg2::error("SelectiveDataWipe: empty wipe target list");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    setWipeStatus(WipeOperationStatus::InProgress);
    wipeOnDevice(targetMask).detach();
}

requester::Coroutine NsmSelectiveWipeObject::wipeOnDevice(uint16_t targetMask)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    if (device == nullptr)
    {
        lg2::error("SelectiveDataWipe: no NSM device for UUID {UUID}", "UUID",
                   uuid);
        setWipeStatus(WipeOperationStatus::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    const auto eid = device->getEid();
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_selective_data_wipe_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_selective_data_wipe_req(0, targetMask, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "SelectiveDataWipe: encode request failed. eid={EID} mask={MASK} rc={RC}",
            "EID", eid, "MASK", lg2::hex, targetMask, "RC", rc);
        setWipeStatus(WipeOperationStatus::InvalidArgument);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "SelectiveDataWipe: postPatchIO failed. eid={EID} mask={MASK} rc={RC}",
            "EID", eid, "MASK", lg2::hex, targetMask, "RC", rc);
        setWipeStatus(rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE
                          ? WipeOperationStatus::UnsupportedRequest
                          : WipeOperationStatus::WriteFailure);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_selective_data_wipe_resp(responseMsg.get(), responseLen, &cc,
                                         &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "SelectiveDataWipe: command failed. eid={EID} mask={MASK} rc={RC} cc={CC} reasonCode={REASON}",
            "EID", eid, "MASK", lg2::hex, targetMask, "RC", rc, "CC", cc,
            "REASON", reasonCode);
        setWipeStatus(cc == NSM_ERR_INVALID_DATA
                          ? WipeOperationStatus::InvalidArgument
                      : cc == NSM_ERR_UNSUPPORTED_COMMAND_CODE
                          ? WipeOperationStatus::UnsupportedRequest
                          : WipeOperationStatus::WriteFailure);
        co_return cc != NSM_SUCCESS ? cc : rc;
    }

    // Success confirms immediate logical invalidation only. The protocol does
    // not report deferred physical sanitization status.
    setWipeStatus(WipeOperationStatus::Success);
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm
