#pragma once
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cpu/OperatingConfig/server.hpp>

#include <cstdint>

namespace nsm
{
using CpuOperatingConfigIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Cpu::server::OperatingConfig>;

enum class clockLimitFlag{
    PERSISTENCE = 0,
    CLEAR = 1
};

class NsmCpuOperatingConfigIntf : public CpuOperatingConfigIntf
{
  public:
    NsmCpuOperatingConfigIntf(sdbusplus::bus::bus& bus, const char* path,
                              std::shared_ptr<NsmDevice> device, uint8_t clockId) :
        CpuOperatingConfigIntf(bus, path),
        device(device), clockId(clockId)
    {}

    int getMinGraphicsClockLimit(uint32_t& minClockLimit)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        uint8_t propertyIdentifier = MINIMUM_GRAPHICS_CLOCK_LIMIT;
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_inventory_information_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                       requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getMinGraphicsClockLimit: encode_get_inventory_information_req failed. "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
        if (rc)
        {
            lg2::error("SendRecvNsmMsgSync failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);

            return rc;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        uint32_t value;
        std::vector<uint8_t> data(4, 0);
        rc = decode_get_inventory_information_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &dataSize,
            data.data());

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS &&
            dataSize == sizeof(value))
        {
            memcpy(&value, &data[0], sizeof(value));
            minClockLimit = le32toh(value);
            return NSM_SW_SUCCESS;
        }
        else
        {
            lg2::error(
                "getMinGraphicsClockLimit: decode_get_inventory_information_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    int getMaxGraphicsClockLimit(uint32_t& maxClockLimit)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        uint8_t propertyIdentifier = MAXIMUM_GRAPHICS_CLOCK_LIMIT;
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_inventory_information_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                       requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getMaxGraphicsClockLimit: encode_get_inventory_information_req failed. "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
        if (rc)
        {
            lg2::error("SendRecvNsmMsgSync failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);

            return rc;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        uint32_t value;
        std::vector<uint8_t> data(4, 0);
        rc = decode_get_inventory_information_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &dataSize,
            data.data());

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS &&
            dataSize == sizeof(value))
        {
            memcpy(&value, &data[0], sizeof(value));
            maxClockLimit = le32toh(value);
            return NSM_SW_SUCCESS;
        }
        else
        {
            lg2::error(
                "getMaxGraphicsClockLimit: decode_get_inventory_information_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    void getClockLimitFromDevice(nsm_clock_limit& clockLimit)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_clock_limit_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_clock_limit_req(0, clockId, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getClockLimitFromDevice: encode_get_clock_limit_req failed. "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            return ;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
        if (rc)
        {
            lg2::error("SendRecvNsmMsgSync failed. "
                       "eid={EID} rc={RC}",
                       "EID", eid, "RC", rc);

            return ;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;

        rc = decode_get_clock_limit_resp(responseMsg.get(), responseLen, &cc,
                                         &data_size, &reason_code, &clockLimit);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            if (clockLimit.present_limit_max ==
                clockLimit.present_limit_min)
            {
                CpuOperatingConfigIntf::speedConfig(std::make_tuple(
                    true, (uint32_t)clockLimit.present_limit_max));
            }
            else
            {
                CpuOperatingConfigIntf::speedConfig(std::make_tuple(
                    false, (uint32_t)clockLimit.present_limit_max));
            }
        }
        else
        {
            lg2::error(
                "getClockLimitFromDevice: decode_get_clock_limit_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
                "REASONCODE", reason_code, "CC", cc, "RC", rc);
        }
    }

    void setClockLimitOnDevice(bool speedLocked, uint32_t requestedSpeedLimit)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);

        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

        uint32_t minClockLimit;
        auto rc = getMinGraphicsClockLimit(minClockLimit);
        if (rc)
        {
            lg2::error(
                "error while doing getMinGraphicsClockLimit eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint32_t maxClockLimit;
        rc = getMaxGraphicsClockLimit(maxClockLimit);
        if (rc)
        {
            lg2::error(
                "error while doing getMinGraphicsClockLimit eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        if (requestedSpeedLimit < minClockLimit ||
            requestedSpeedLimit > maxClockLimit)
        {
            lg2::error("invalid argument for speed limit");
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
            return;
        }

        flag = uint8_t(clockLimitFlag::PERSISTENCE);
        if (speedLocked)
        {
            limitMax = requestedSpeedLimit;
            limitMin = requestedSpeedLimit;
        }
        else
        {
            limitMax = requestedSpeedLimit;
            limitMin = minClockLimit;
        }
        // first argument instanceid=0 is irrelevant
        rc = encode_set_clock_limit_req(0, clockId, flag, limitMin, limitMax,
                                        requestMsg);

        if (rc)
        {
            lg2::error(
                "setClockLimitOnDevice encode_set_clock_limit_req failed. eid={EID} rc={RC}",
                "EID", eid, "RC", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = manager.SendRecvNsmMsgSync(eid, request, responseMsg, responseLen);
        if (rc)
        {
            lg2::error(
                "setClockLimitOnDevice SendRecvNsmMsgSync failed for while setting clock limits "
                "eid={EID} rc={RC}",
                "EID", eid, "RC", rc);

            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
            return;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t data_size = 0;
        rc = decode_set_clock_limit_resp(responseMsg.get(), responseLen, &cc,
                                         &reason_code, &data_size);

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            // verify setting is applied on the device
            struct nsm_clock_limit clockLimit;
            getClockLimitFromDevice(clockLimit);
        }
        else
        {
            lg2::error(
                "setClockLimitOnDevice decode_set_clock_limit_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
                "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
            throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
                WriteFailure();
        }
    }

    std::tuple<bool, uint32_t>
        speedConfig(std::tuple<bool, uint32_t> value) override
    {
        setClockLimitOnDevice(std::get<0>(value), std::get<1>(value));
        return CpuOperatingConfigIntf::speedConfig();
    }

  private:
    std::shared_ptr<NsmDevice> device;
    uint8_t clockId;
    uint8_t flag;
    uint32_t limitMin;
    uint32_t limitMax;
};
} // namespace nsm
