#pragma once
#include "diagnostics.h"
#include "pci-links.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Control/Processor/Reset/server.hpp>
#include <xyz/openbmc_project/Control/Processor/ResetAsync/server.hpp>
#include <xyz/openbmc_project/Control/Reset/server.hpp>
#include <xyz/openbmc_project/Control/ResetAsync/server.hpp>

#include <cstdint>

namespace nsm
{
using ResetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::Reset>;

using ResetAsyncIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::ResetAsync>;
using ResetDeviceIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::Reset>;

using ResetDeviceAsyncIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::ResetAsync>;

class NsmResetIntf : public ResetIntf
{
  public:
    using ResetIntf::ResetIntf;

    int reset() override
    {
        return 0;
    }
};

class NsmResetAsyncIntf : public ResetAsyncIntf
{
  public:
    NsmResetAsyncIntf(sdbusplus::bus::bus& bus, const char* path,
                      std::shared_ptr<NsmDevice> device, uint8_t deviceIndex) :
        ResetAsyncIntf(bus, path),
        device(device), deviceIndex(deviceIndex)
    {}

    requester::Coroutine assertFundamentalReset(uint8_t action)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        std::vector<uint8_t> request(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_assert_pcie_fundamental_reset_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_assert_pcie_fundamental_reset_req(0, deviceIndex,
                                                           action, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "assertFundamentalReset encode_assert_pcie_fundamental_reset_req failed. "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                                   responseLen);
        if (rc_)
        {
            lg2::error("SendRecvNsmMsg failed for gpuFundamentalReset"
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc_);
            // coverity[missing_return]
            co_return rc_;
        }

        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;

        rc = decode_assert_pcie_fundamental_reset_resp(
            responseMsg.get(), responseLen, &cc, &data_size, &reason_code);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info(
                "assertFundamentalReset for EID: {EID} completed for action {ACTION}",
                "EID", eid, "ACTION", action);
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
        else
        {
            lg2::error(
                "assertFundamentalReset: decode_assert_pcie_fundamental_reset_resp for action = {ACTION} with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "ACTION", action, "REASONCODE", reason_code, "CC", cc, "RC",
                rc);
        }
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    requester::Coroutine
        doResetOnDevice(std::shared_ptr<AsyncStatusIntf> statusInterface)
    {
        AsyncOperationStatusType status{AsyncOperationStatusType::Success};

        auto rc = co_await assertFundamentalReset(RESET);
        if (rc != 0)
        {
            lg2::error("assertFundamentalReset failed while doing RESET");
            status = AsyncOperationStatusType::InternalFailure;
        }
        else
        {
            auto rc_ = co_await assertFundamentalReset(NOT_RESET);
            if (rc_ != 0)
            {
                lg2::error(
                    "assertFundamentalReset failed while doing NOT_RESET");
                status = AsyncOperationStatusType::InternalFailure;
            }
        }

        statusInterface->status(status);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    sdbusplus::message::object_path reset() override
    {
        const auto [objectPath, statusInterface, _] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        if (objectPath.empty())
        {
            lg2::error(
                "Reset failed. No available result Object to allocate for the Post Request.");
            throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
        }

        doResetOnDevice(statusInterface).detach();

        return objectPath;
    }

  private:
    std::shared_ptr<NsmDevice> device;
    uint8_t deviceIndex;
};

class NsmResetDeviceIntf : public ResetDeviceIntf
{
  public:
    using ResetDeviceIntf::ResetDeviceIntf;

    void reset() override
    {
        return;
    }
};

class NsmNetworkDeviceResetAsyncIntf : public ResetDeviceAsyncIntf
{
  public:
    NsmNetworkDeviceResetAsyncIntf(sdbusplus::bus::bus& bus, const char* path,
                                   std::shared_ptr<NsmDevice> device) :
        ResetDeviceAsyncIntf(bus, path),
        device(device)
    {}

    requester::Coroutine resetOnDevice(AsyncOperationStatusType* status)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_reset_network_device_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_reset_network_device_req(0, START_AFTER_RESPONSE,
                                                  requestMsg);

        if (rc)
        {
            lg2::error(
                "resetOnDevice encode_reset_network_device_req failed. eid={EID}, rc={RC}",
                "EID", eid, "RC", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                                   responseLen);
        if (rc_)
        {
            lg2::error(
                "resetOnDevice SendRecvNsmMsgSync failed for while setting power limit for eid = {EID} rc = {RC}",
                "EID", eid, "RC", rc_);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        rc = decode_reset_network_device_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            lg2::info("resetOnDevice for EID: {EID} completed", "EID", eid);
        }
        else
        {
            lg2::error(
                "resetOnDevice decode_reset_network_device_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    requester::Coroutine
        doResetOnDevice(std::shared_ptr<AsyncStatusIntf> statusInterface)
    {
        AsyncOperationStatusType status{AsyncOperationStatusType::Success};
        const auto rc_ = co_await resetOnDevice(&status);
        statusInterface->status(status);
        // coverity[missing_return]
        co_return rc_;
    }

    sdbusplus::message::object_path reset() override
    {
        const auto [objectPath, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        if (objectPath.empty())
        {
            lg2::error(
                "Reset failed. No available result Object to allocate for the Post request.");
            throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
        }

        doResetOnDevice(statusInterface).detach();
        return objectPath;
    }

  private:
    std::shared_ptr<NsmDevice> device;
};
} // namespace nsm
