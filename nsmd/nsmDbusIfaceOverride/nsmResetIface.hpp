#pragma once
#include "pci-links.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Control/Processor/Reset/server.hpp>

#include <cstdint>

namespace nsm
{
using ResetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::Reset>;

class NsmResetIntf : public ResetIntf
{
  public:
    NsmResetIntf(sdbusplus::bus::bus& bus, const char* path, std::shared_ptr<NsmDevice> device,
                 uint8_t deviceIndex) :
        ResetIntf(bus, path),
        device(device), deviceIndex(deviceIndex)
    {}

    int assertFundamentalReset(uint8_t action)
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
            return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg = NULL;
        size_t responseLen = 0;
        auto rc_ = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                              responseLen);
        if (rc_)
        {

            lg2::error("SendRecvNsmMsgSync failed for gpuFundamentalReset"
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc_);

            return rc_;
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
            return NSM_SW_SUCCESS;
        }
        else
        {
            lg2::error(
                "assertFundamentalReset: decode_assert_pcie_fundamental_reset_resp for action = {ACTION} with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "ACTION", action, "REASONCODE", reason_code, "CC", cc, "RC",
                rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    int reset() override
    {
        auto rc = assertFundamentalReset(RESET);
        if (rc != 0)
        {
            lg2::error("assertFundamentalReset failed while doing RESET");
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InternalFailure();
            return 1;
        }
        else
        {
            auto rc_ = assertFundamentalReset(NOT_RESET);
            if (rc_ != 0)
            {
                lg2::error(
                    "assertFundamentalReset failed while doing NOT_RESET");
                throw sdbusplus::xyz::openbmc_project::Common::Error::
                    InternalFailure();
                return 1;
            }
        }
        return 0;
    }

  private:
    std::shared_ptr<NsmDevice> device;
    uint8_t deviceIndex;
};
} // namespace nsm
