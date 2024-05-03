#pragma once
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Control/Power/Cap/server.hpp>

#include <cstdint>

namespace nsm
{
using PowerCap = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Control::Power::server::Cap>;

class NsmPowerCapIntf : public PowerCap
{
  public:
    NsmPowerCapIntf(sdbusplus::bus::bus& bus, const char* path, uuid_t uuid) :
        PowerCap(bus, path), uuid(uuid)
    {}

    void getECCModeFromDevice()
    {
        // SensorManager& manager = SensorManager::getInstance();
        // auto device = manager.getNsmDevice(uuid);
        // auto eid = manager.getEid(device);
        // lg2::info("getECCModeFromDevice for EID: {EID}", "EID", eid);
        // std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
        //                              sizeof(nsm_common_req));
        // auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        // auto rc = encode_get_ECC_mode_req(0, requestMsg);
        // if (rc != NSM_SW_SUCCESS)
        // {
        //     lg2::error("getECCModeFromDevice encode_get_ECC_mode_req failed.
        //     "
        //                "eid={EID} rc={RC}",
        //                "EID", eid, "RC", rc);
        // }
        // const nsm_msg* responseMsg = NULL;
        // size_t responseLen = 0;
        // auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
        //                                       &responseLen);
        // if (rc_)
        // {
        //     lg2::error("SendRecvNsmMsgSync failed. "
        //                "eid={EID} rc={RC}",
        //                "EID", eid, "RC", rc_);
        //     free((void*)responseMsg);
        //     return;
        // }
        // uint8_t cc = NSM_ERROR;
        // uint16_t reason_code = ERR_NULL;
        // bitfield8_t flags;
        // uint16_t data_size = 0;

        // rc = decode_get_ECC_mode_resp(responseMsg, responseLen, &cc,
        // &data_size,
        //                               &reason_code, &flags);
        // if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        // {
        //     EccModeIntf::eccModeEnabled(flags.bits.bit0);
        //     EccModeIntf::pendingECCState(flags.bits.bit1);
        //     lg2::info("getECCModeFromDevice for EID: {EID} completed", "EID",
        //               eid);
        // }
        // else
        // {
        //     lg2::error(
        //         "getECCModeFromDevice: decode_get_ECC_mode_resp with
        //         reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
        //         "REASONCODE", reason_code, "CC", cc, "RC", rc);
        // }
    }

    void setECCModeOnDevice(/*bool eccMode*/)
    {
        // SensorManager& manager = SensorManager::getInstance();
        // auto device = manager.getNsmDevice(uuid);
        // auto eid = manager.getEid(device);
        // lg2::info("setECCModeOnDevice for EID: {EID}", "EID", eid);
        // // NSM spec expects  requestedECCMode mode to be uint8_t
        // uint8_t requestedECCMode = static_cast<uint8_t>(eccMode);
        // Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
        // auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        // // first argument instanceid=0 is irrelevant
        // auto rc = encode_set_ECC_mode_req(0, requestedECCMode, requestMsg);

        // if (rc)
        // {
        //     lg2::error(
        //         "setECCModeOnDevice encode_set_ECC_mode_req failed. eid={EID}
        //         rc={RC}", "EID", eid, "RC", rc);
        //     throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
        //         WriteFailure();
        //     return;
        // }

        // const nsm_msg* responseMsg = NULL;
        // size_t responseLen = 0;
        // auto rc_ = manager.SendRecvNsmMsgSync(eid, request, &responseMsg,
        //                                       &responseLen);
        // if (rc_)
        // {
        //     lg2::error(
        //         "setECCModeOnDevice SendRecvNsmMsgSync failed for while
        //         setting ECCMode " "eid={EID} rc={RC}", "EID", eid, "RC",
        //         rc_);
        //     free((void*)responseMsg);
        //     throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
        //         WriteFailure();
        //     return;
        // }

        // uint8_t cc = NSM_SUCCESS;
        // uint16_t reason_code = ERR_NULL;
        // uint16_t data_size = 0;
        // rc = decode_set_ECC_mode_resp(responseMsg, responseLen, &cc,
        //                               &reason_code, &data_size);
        // if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        // {
        //     // verify setting is applied on the device
        //     getECCModeFromDevice();
        //     lg2::info("setECCModeOnDevice for EID: {EID} completed", "EID",
        //               eid);
        // }
        // else
        // {
        //     lg2::error(
        //         "setECCModeOnDevice decode_set_ECC_mode_resp failed.
        //         eid={EID} CC={CC} reasoncode={RC} RC={A}", "EID", eid, "CC",
        //         cc, "RC", reason_code, "A", rc);
        //     lg2::error("throwing write failure exception");
        //     throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
        //         WriteFailure();
        // }
    }

    bool powerCap() const override
    {
        return PowerCap::powerCap();
    }

    bool powerCap(uint32 power_limit) override
    {
        // setECCModeOnDevice(eccMode);
        return PowerCap::powerCap();
    }

  private:
    uuid_t uuid;
};
} // namespace nsm
