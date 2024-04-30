#pragma once
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/MigMode/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstdint>

namespace nsm
{
using MigModeIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::MigMode>;

class NsmMigModeIntf : public MigModeIntf
{
  public:
    NsmMigModeIntf(sdbusplus::bus::bus& bus, const char* path, uuid_t uuid) :
        MigModeIntf(bus, path), uuid(uuid)
    {}

    void getMigModeFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);
        lg2::info("getMigModeFromDevice for EID: {EID}", "EID", eid);
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_MIG_mode_req(0, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("getMigModeFromDevice encode_get_MIG_mode_req failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);
        }
        const nsm_msg* responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
                                              &responseLen);
        if (rc_)
        {
            lg2::error("SendRecvNsmMsgSync failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc_);
            free((void*)responseMsg);
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        bitfield8_t flags;
        uint16_t data_size = 0;

        rc = decode_get_MIG_mode_resp(responseMsg, responseLen, &cc, &data_size,
                                      &reason_code, &flags);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            MigModeIntf::migModeEnabled(flags.bits.bit0);
            lg2::info("getMigModeFromDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "getMigModeFromDevice: decode_get_MIG_mode_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
    }

    void setMigModeOnDevice(bool migMode)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(uuid);
        auto eid = manager.getEid(device);
        lg2::info("setMigModeOnDevice for EID: {EID}", "EID", eid);
        uint8_t requestedMigMode = static_cast<uint8_t>(migMode);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_MIG_mode_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // first argument instanceid=0 is irrelevant
        auto rc = encode_set_MIG_mode_req(0, requestedMigMode, requestMsg);

        if (rc)
        {
            lg2::error(
                "setMigModeOnDevice encode_set_MIG_mode_req failed. eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        const nsm_msg* responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
                                              &responseLen);
        if (rc_)
        {
            lg2::error(
                "setMigModeOnDevice SendRecvNsmMsgSync failed for while setting MigMode "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc_);
            free((void*)responseMsg);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_MIG_mode_resp(responseMsg, responseLen, &cc,
                                      &reason_code, &data_size);
        free((void*)responseMsg);
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            getMigModeFromDevice();
            lg2::info("setMigModeOnDevice for EID: {EID} completed", "EID",
                      eid);
        }
        else
        {
            lg2::error(
                "setMigModeOnDevice decode_set_MIG_mode_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            lg2::error("throwing write failure exception");
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    bool migModeEnabled() const override
    {
        return MigModeIntf::migModeEnabled();
    }

    bool migModeEnabled(bool migMode) override
    {
        setMigModeOnDevice(migMode);
        return MigModeIntf::migModeEnabled();
    }

    bool migModeEnabled(bool migMode, bool skipSignal) override
    {
        return MigModeIntf::migModeEnabled(migMode, skipSignal);
    }

  private:
    uuid_t uuid;
};
} // namespace nsm
