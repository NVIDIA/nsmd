/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmGPIOState.hpp"

#include "base.h"

#include <phosphor-logging/lg2.hpp>

#include <map>

namespace nsm
{

NsmGPIOState::NsmGPIOState(
    sdbusplus::bus::bus& bus, const std::string& type, const std::string& name,
    std::string& inventoryObjPath,
    const std::vector<std::tuple<std::string, std::string, std::string>>&
        associations,
    std::shared_ptr<GPIOStateIntf> gpioStateIntf) : NsmSensor(name, type)
{
    lg2::debug("NsmGPIOState: create sensor: {NAME}", "NAME", name.c_str());

    this->gpioStateIntf = gpioStateIntf;
    this->gpioStateIntf->lastChangeTime(0);
    // Initialize LineStates to a silent empty (all-false) baseline; the static
    // GetGPIO seed and the genuine event path build on this without the seed
    // itself raising a PropertiesChanged.
    this->gpioStateIntf->lineStates({}, true);
    objPath = inventoryObjPath;

    associationDefIntf = std::make_unique<AssociationDefInft>(bus,
                                                              objPath.c_str());
    associationDefIntf->associations(associations);
}

std::optional<std::vector<uint8_t>>
    NsmGPIOState::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_gpio_state_req));
    uint16_t offset = 0;
    uint16_t length = 0;

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceNumber, offset, length,
                                        requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT("encode_get_gpio_state_req failed for eid={EID} rc={RC}",
                      "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmGPIOState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t offset = 0;
    uint16_t length = 0;
    uint32_t gpioValuesSize = 0;
    std::vector<uint8_t> gpioValues(65535, 0);

    auto rc = decode_get_gpio_state_resp(responseMsg, responseLen, &cc,
                                         &reasonCode, &offset, &length,
                                         gpioValues.data(), &gpioValuesSize);

    LG2_ERROR_FLT(
        "decode_get_gpio_state_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        std::map<uint16_t, bool> gpioStateMap;

        for (uint32_t byteIndex = 0; byteIndex < gpioValuesSize; ++byteIndex)
        {
            uint8_t byteValue = gpioValues[byteIndex];
            for (uint8_t bitIndex = 0; bitIndex < 8; ++bitIndex)
            {
                uint16_t gpioIndex = (byteIndex * 8) + bitIndex;
                gpioIndex += offset;
                bool bitValue = (byteValue >> bitIndex) & 0x01;

                gpioStateMap[gpioIndex] = bitValue;
            }
        }

        if (gpioStateIntf != nullptr && !gpioStateMap.empty())
        {
            // NsmGPIOState is a static sensor: this GetGPIO response only seeds
            // LineStates on D-Bus. Publish it without emitting PropertiesChanged
            // (skipSignal=true) so the {} -> {...} seed is not misread as a
            // genuine HIGH->LOW transition by monitor-eventing, which would
            // raise spurious Critical events (e.g. HPM_SMA-PCIE_CLKBUF-FAULTY)
            // for any line already in its abnormal state. Genuine transitions
            // are published, with signal, by NsmGPIOStateChangeEvent.
            gpioStateIntf->lineStates(gpioStateMap, true);
        }
    }

    return cc ? cc : rc;
}

} // namespace nsm
