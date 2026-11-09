// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "fpga/fpga-maps.h"

#include "nsmFpgaStatus.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace nsm
{

// Digest of a platform's map as the array the checks compare against
inline std::array<uint8_t, NSM_FPGA_MAP_DIGEST_SIZE>
    fpgaMapDigest(const nsm_fpga_map_identity& map)
{
    std::array<uint8_t, NSM_FPGA_MAP_DIGEST_SIZE> digest{};
    std::copy_n(map.digest, digest.size(), digest.begin());
    return digest;
}

// Per-FPGA discovery check, re-runs each polling round until it passes.
class NsmFpgaDiscoveryCheck : public NsmSensor
{
  public:
    // Which capability the check reads: the advertised configuration type
    // (command 0x12) or the register map digest (command 0x11)
    enum class CheckKind
    {
        ConfigurationType,
        MapDigest,
    };

    NsmFpgaDiscoveryCheck(const std::string& name, const std::string& type,
                          CheckKind checkKind, uint32_t configurationType,
                          uint64_t digestAddress, size_t digestLengthDwords,
                          std::shared_ptr<NsmFpgaStatus> status,
                          const nsm_fpga_map_identity& map);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

    // Polled each round until it passes. A passed check runs again only
    // when the status object asks for it, after a bank activation
    bool needsUpdate(const uint64_t& currentTimestampInUsec) const override
    {
        return !(passedFlag && statusHolds()) &&
               NsmObject::needsUpdate(currentTimestampInUsec);
    }
    bool passed() const
    {
        return passedFlag;
    }
    // False once the status object asked for the checks to run again
    bool statusHolds() const
    {
        if (!status)
        {
            return true;
        }
        return checkKind == CheckKind::ConfigurationType
                   ? status->typeCheckPassed()
                   : status->digestCheckPassed();
    }
    CheckKind getCheckKind() const
    {
        return checkKind;
    }
    const std::array<uint8_t, 32>& getExpectedDigest() const
    {
        return expectedDigest;
    }

  private:
    // handleResponseMsg hands the reply to the decoder of this check's kind
    uint8_t handleTypeResponse(const struct nsm_msg* responseMsg,
                               size_t responseLen);
    uint8_t handleDigestResponse(const struct nsm_msg* responseMsg,
                                 size_t responseLen);
    // True when the outcome differs from the last one, so it is logged once
    bool outcomeChanged(const std::string& outcome);
    CheckKind checkKind;
    uint32_t configurationType;
    uint64_t digestAddress;
    size_t digestLengthDwords;
    // Get Supported Device Configs is chunked: each poll reads one chunk,
    // typeHandle names the next one and the search ends after maxTypeChunks
    uint32_t typeHandle = 0;
    bool moreTypeChunks = false;
    size_t typeChunksRead = 0;
    static constexpr size_t maxTypeChunks = 16;
    std::shared_ptr<NsmFpgaStatus> status;
    // The platform's map: the FPGA must report exactly this digest
    std::string mapName;
    std::array<uint8_t, 32> expectedDigest;
    bool passedFlag = false;
    bool responded = false;
    std::string lastOutcome;
};

requester::Coroutine createNsmFpgaDiscoveryCheck(SensorManager& manager,
                                                 const std::string& interface,
                                                 const std::string& objPath);

} // namespace nsm
