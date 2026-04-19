/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */
#include "nsmNvSwitchDeviceConfiguration.hpp"

#include "base.h"
#include "device-configuration.h"

#include "types.hpp"
#include "utils.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace nsm
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;

namespace
{

/** Read up to @p maxBytes from @p fd (already dup'd for this call). */
std::optional<std::vector<uint8_t>> readPayloadFromFd(int fd,
                                                      std::size_t maxBytes)
{
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        return std::nullopt;
    }

    struct stat st{};
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0)
    {
        const auto sz = static_cast<std::size_t>(st.st_size);
        if (sz > maxBytes)
        {
            return std::nullopt;
        }
        std::vector<uint8_t> out(sz);
        if (!out.empty())
        {
            ssize_t n = read(fd, out.data(), out.size());
            if (n < 0 || static_cast<std::size_t>(n) != out.size())
            {
                return std::nullopt;
            }
        }
        uint8_t probe = 0;
        const ssize_t extra = read(fd, &probe, 1);
        if (extra > 0)
        {
            return std::nullopt;
        }
        return out;
    }

    std::vector<uint8_t> out;
    std::vector<uint8_t> chunk(4096);
    for (;;)
    {
        const std::size_t room = maxBytes - out.size();
        if (room == 0)
        {
            const ssize_t extra = read(fd, chunk.data(),
                                       std::min(chunk.size(), std::size_t{1}));
            if (extra > 0)
            {
                return std::nullopt;
            }
            if (extra < 0)
            {
                return std::nullopt;
            }
            break;
        }
        const std::size_t toRead = std::min(chunk.size(), room);
        const ssize_t n = read(fd, chunk.data(), toRead);
        if (n < 0)
        {
            return std::nullopt;
        }
        if (n == 0)
        {
            break;
        }
        out.insert(out.end(), chunk.begin(), chunk.begin() + n);
    }
    return out;
}

int dupUnixFdOrThrow(sdbusplus::message::unix_fd ufd)
{
    const int raw = ufd;
    if (raw < 0)
    {
        throw InvalidArgument();
    }
    const int dupFd = dup(raw);
    if (dupFd < 0)
    {
        lg2::error("DeviceConfig: dup failed errno={ERRNO} msg={MSG}", "ERRNO",
                   errno, "MSG", strerror(errno));
        throw InternalFailure();
    }
    return dupFd;
}

} // namespace

NsmNvSwitchDeviceConfigurationRequestEvent::
    NsmNvSwitchDeviceConfigurationRequestEvent(
        std::weak_ptr<NsmNvSwitchDeviceConfigurationAsync> target) :
    NsmEvent("NsmNvSwitchDeviceConfigurationRequestEvent",
             "NSM_NVSwitch_DeviceConfiguration_Request"),
    target_(std::move(target))
{}

int NsmNvSwitchDeviceConfigurationRequestEvent::handle(eid_t eid,
                                                       NsmType /*type*/,
                                                       NsmEventId eventId,
                                                       const nsm_msg* event,
                                                       size_t eventLen)
{
    if (eventId != NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1 ||
        event == nullptr)
    {
        return NSM_SW_ERROR_DATA;
    }

    uint16_t eventState = 0;
    const int rc = decode_nsm_device_config_request_event_v1(
        event, eventLen, nullptr, &eventState);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "Device configuration request event: decode_nsm_device_config_request_event_v1 failed rc={RC} eid={EID}",
            "RC", rc, "EID", eid);
        return rc;
    }

    if (auto cfg = target_.lock())
    {
        cfg->emitDeviceConfigurationRequestedSignal();
    }

    return NSM_SW_SUCCESS;
}

NsmNvSwitchDeviceConfigurationAsync::NsmNvSwitchDeviceConfigurationAsync(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& objPath, std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type), NsmNvSwitchDeviceConfigIntf(bus, objPath.c_str()),
    device(std::move(device)), dbusObjPath_(objPath)
{
    lg2::info("NsmNvSwitchDeviceConfigurationAsync: path={PATH}", "PATH",
              objPath.c_str());
}

void NsmNvSwitchDeviceConfigurationAsync::
    emitDeviceConfigurationRequestedSignal()
{
    deviceConfigurationRequested();

    lg2::info(
        "Device configuration request: emitted DeviceConfigurationRequested on {PATH} (Type 5, Event ID 1)",
        "PATH", dbusObjPath_.c_str());
}

requester::Coroutine NsmNvSwitchDeviceConfigurationAsync::doSet(
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    ConfigUpdaterConfigurationType configurationType, std::vector<uint8_t> data)
{
    AsyncOperationStatusType opStatus{AsyncOperationStatusType::Success};
    auto eid = device->getEid();
    const uint32_t cfgTypeU32 = static_cast<uint32_t>(configurationType);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) +
                    sizeof(uint32_t) + data.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_config_v2_req(
        0, cfgTypeU32, data.data(), static_cast<uint16_t>(data.size()),
        requestMsg);
    std::string msg = utils::requestMsgToHexString(request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_device_config_v2_req failed eid={EID} rc={RC} msg={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto ioRc = co_await device->postPatchIO(eid, request, responseMsg,
                                             responseLen);
    if (ioRc)
    {
        lg2::error(
            "SetDeviceConfiguration postPatchIO failed eid={EID} rc={RC} msg={MSG}",
            "EID", eid, "RC", utils::nsmSwCodeToString(ioRc), "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_device_config_v2_resp(responseMsg.get(), responseLen, &cc,
                                          &reasonCode);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_set_device_config_v2_resp failed eid={EID} cc={CC} reason={REASON} librc={RC} msg={MSG}",
            "EID", eid, "CC", cc, "REASON", reasonCode, "RC", rc, "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    lg2::debug("SetDeviceConfiguration: success eid={EID} cfgType={TYPE}",
               "EID", eid, "TYPE", cfgTypeU32);

    statusInterface->status(opStatus);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmNvSwitchDeviceConfigurationAsync::setDeviceConfiguration(
        ConfigUpdaterConfigurationType configurationType,
        sdbusplus::message::unix_fd data)
{
    const uint32_t cfgTypeU32 = static_cast<uint32_t>(configurationType);
    const auto [objectPath, statusInterface] =
        AsyncOperationManager::getInstance()->getNewStatusInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "SetDeviceConfiguration: FAILED - no async result object available (pool exhausted) cfgType={TYPE}",
            "TYPE", cfgTypeU32);
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    lg2::debug(
        "SetDeviceConfiguration: async result object allocated path={PATH} cfgType={TYPE}",
        "PATH", objectPath, "TYPE", cfgTypeU32);

    const int payloadFd = dupUnixFdOrThrow(data);
    auto payload = readPayloadFromFd(payloadFd,
                                     std::numeric_limits<uint16_t>::max());
    close(payloadFd);
    if (!payload)
    {
        lg2::error(
            "SetDeviceConfiguration: FAILED - could not read payload from fd (fd invalid or unreadable) cfgType={TYPE}",
            "TYPE", cfgTypeU32);
        throw InvalidArgument();
    }

    lg2::debug(
        "SetDeviceConfiguration: payload read successfully cfgType={TYPE} payloadLen={LEN}",
        "TYPE", cfgTypeU32, "LEN", payload->size());

    doSet(statusInterface, configurationType, std::move(*payload)).detach();
    return sdbusplus::message::object_path(objectPath);
}

requester::Coroutine NsmNvSwitchDeviceConfigurationAsync::doGet(
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    std::shared_ptr<AsyncValueIntf> valueInterface,
    ConfigUpdaterConfigurationType configurationType,
    std::vector<uint8_t> query)
{
    AsyncOperationStatusType opStatus{AsyncOperationStatusType::Success};
    auto eid = device->getEid();
    const uint32_t cfgTypeU32 = static_cast<uint32_t>(configurationType);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) +
                    sizeof(uint32_t) + query.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_config_v2_req(
        0, cfgTypeU32, query.data(), static_cast<uint16_t>(query.size()),
        requestMsg);
    std::string msg = utils::requestMsgToHexString(request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_device_config_v2_req failed eid={EID} rc={RC} msg={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto ioRc = co_await device->postPatchIO(eid, request, responseMsg,
                                             responseLen);
    if (ioRc)
    {
        lg2::error(
            "GetDeviceConfiguration postPatchIO failed eid={EID} rc={RC} msg={MSG}",
            "EID", eid, "RC", utils::nsmSwCodeToString(ioRc), "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    std::vector<uint8_t> currentBuf(responseLen);
    uint16_t currentLen = 0;
    std::vector<uint8_t> pendingBuf(responseLen);
    uint16_t pendingLen = 0;

    rc = decode_get_device_config_v2_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, currentBuf.data(),
        &currentLen, pendingBuf.data(), &pendingLen);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_get_device_config_v2_resp failed eid={EID} cc={CC} reason={REASON} librc={RC} msg={MSG}",
            "EID", eid, "CC", cc, "REASON", reasonCode, "RC", rc, "MSG", msg);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    const std::size_t packedSize = static_cast<std::size_t>(4) + currentLen +
                                   pendingLen;
    int memFd = memfd_create("nvswitch-device-config", 0);
    if (memFd < 0)
    {
        lg2::error(
            "GetDeviceConfiguration: memfd_create failed errno={ERRNO} msg={MSG}",
            "ERRNO", errno, "MSG", strerror(errno));
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR;
    }

    std::vector<uint8_t> packed(packedSize);
    packed[0] = static_cast<uint8_t>(currentLen & 0xff);
    packed[1] = static_cast<uint8_t>((currentLen >> 8) & 0xff);
    packed[2] = static_cast<uint8_t>(pendingLen & 0xff);
    packed[3] = static_cast<uint8_t>((pendingLen >> 8) & 0xff);
    if (currentLen > 0)
    {
        std::memcpy(packed.data() + 4, currentBuf.data(), currentLen);
    }
    if (pendingLen > 0)
    {
        std::memcpy(packed.data() + 4 + currentLen, pendingBuf.data(),
                    pendingLen);
    }

    const uint8_t* writePtr = packed.data();
    std::size_t writeLeft = packed.size();
    while (writeLeft != 0)
    {
        const ssize_t nw = write(memFd, writePtr, writeLeft);
        if (nw < 0)
        {
            lg2::error(
                "GetDeviceConfiguration: write memfd failed errno={ERRNO} msg={MSG}",
                "ERRNO", errno, "MSG", strerror(errno));
            close(memFd);
            opStatus = AsyncOperationStatusType::WriteFailure;
            statusInterface->status(opStatus);
            valueInterface->value(std::vector<uint8_t>{});
            co_return NSM_SW_ERROR;
        }
        writePtr += static_cast<std::size_t>(nw);
        writeLeft -= static_cast<std::size_t>(nw);
    }

    if (lseek(memFd, 0, SEEK_SET) < 0)
    {
        lg2::error(
            "GetDeviceConfiguration: lseek memfd failed errno={ERRNO} msg={MSG}",
            "ERRNO", errno, "MSG", strerror(errno));
        close(memFd);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR;
    }

    // Dup for the async Value property: `unix_fd` is non-owning; close the
    // memfd used for writes once a separate descriptor is stored for D-Bus.
    const int valueFd = dup(memFd);
    if (valueFd < 0)
    {
        lg2::error(
            "GetDeviceConfiguration: dup memfd failed errno={ERRNO} msg={MSG}",
            "ERRNO", errno, "MSG", strerror(errno));
        close(memFd);
        opStatus = AsyncOperationStatusType::WriteFailure;
        statusInterface->status(opStatus);
        valueInterface->value(std::vector<uint8_t>{});
        co_return NSM_SW_ERROR;
    }
    close(memFd);
    valueInterface->value(sdbusplus::message::unix_fd(valueFd));
    statusInterface->status(opStatus);
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmNvSwitchDeviceConfigurationAsync::getDeviceConfiguration(
        ConfigUpdaterConfigurationType configurationType,
        sdbusplus::message::unix_fd query)
{
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error("GetDeviceConfiguration: no async result object available");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    const int queryFd = dupUnixFdOrThrow(query);
    auto queryBytes = readPayloadFromFd(queryFd,
                                        std::numeric_limits<uint16_t>::max());
    close(queryFd);
    if (!queryBytes)
    {
        throw InvalidArgument();
    }

    doGet(statusInterface, valueInterface, configurationType,
          std::move(*queryBytes))
        .detach();
    return sdbusplus::message::object_path(objectPath);
}

void addNvSwitchDeviceConfigurationSensorIfEnabled(
    const bool supportNvSwitchDeviceConfiguration, sdbusplus::bus::bus& bus,
    const std::string& name, const std::string& dbusObjPath,
    std::shared_ptr<NsmDevice> device)
{
    if (!supportNvSwitchDeviceConfiguration || !device)
    {
        return;
    }

    constexpr auto kNvSwitchConfigType = "NSM_NVSwitch_DeviceConfiguration";
    auto cfg = std::make_shared<NsmNvSwitchDeviceConfigurationAsync>(
        bus, name, kNvSwitchConfigType, dbusObjPath, device);

    auto requestEvent =
        std::make_shared<NsmNvSwitchDeviceConfigurationRequestEvent>(
            std::weak_ptr<NsmNvSwitchDeviceConfigurationAsync>(cfg));
    device->addDeviceEvent(requestEvent, NSM_TYPE_DEVICE_CONFIGURATION,
                           NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1);

    device->addDeviceSensors(cfg);
}

} // namespace nsm
