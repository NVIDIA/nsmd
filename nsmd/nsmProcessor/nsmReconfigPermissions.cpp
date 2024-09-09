/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsmReconfigPermissions.hpp"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

namespace nsm
{

NsmReconfigPermissions::NsmReconfigPermissions(
    const NsmInterfaceProvider<ReconfigSettingsIntf>& provider,
    ReconfigSettingsIntf::FeatureType feature) :
    NsmSensor(provider),
    NsmInterfaceContainer(provider), feature(feature)
{
    // Validates feature value during object creation
    index = getIndex(feature);
}

reconfiguration_permissions_v1_index
    NsmReconfigPermissions::getIndex(ReconfigSettingsIntf::FeatureType feature)
{
    switch (feature)
    {
        case ReconfigSettingsIntf::FeatureType::InSystemTest:
            return RP_IN_SYSTEM_TEST;
        case ReconfigSettingsIntf::FeatureType::FusingMode:
            return RP_FUSING_MODE;
        case ReconfigSettingsIntf::FeatureType::CCMode:
            return RP_CONFIDENTIAL_COMPUTE;
        case ReconfigSettingsIntf::FeatureType::BAR0Firewall:
            return RP_BAR0_FIREWALL;
        case ReconfigSettingsIntf::FeatureType::CCDevMode:
            return RP_CONFIDENTIAL_COMPUTE_DEV_MODE;
        case ReconfigSettingsIntf::FeatureType::TGPCurrentLimit:
            return RP_TOTAL_GPU_POWER_CURRENT_LIMIT;
        case ReconfigSettingsIntf::FeatureType::TGPRatedLimit:
            return RP_TOTAL_GPU_POWER_RATED_LIMIT;
        case ReconfigSettingsIntf::FeatureType::TGPMaxLimit:
            return RP_TOTAL_GPU_POWER_MAX_LIMIT;
        case ReconfigSettingsIntf::FeatureType::TGPMinLimit:
            return RP_TOTAL_GPU_POWER_MIN_LIMIT;
        case ReconfigSettingsIntf::FeatureType::ClockLimit:
            return RP_CLOCK_LIMIT;
        case ReconfigSettingsIntf::FeatureType::NVLinkDisable:
            return RP_NVLINK_DISABLE;
        case ReconfigSettingsIntf::FeatureType::ECCEnable:
            return RP_ECC_ENABLE;
        case ReconfigSettingsIntf::FeatureType::PCIeVFConfiguration:
            return RP_PCIE_VF_CONFIGURATION;
        case ReconfigSettingsIntf::FeatureType::RowRemappingAllowed:
            return RP_ROW_REMAPPING_ALLOWED;
        case ReconfigSettingsIntf::FeatureType::RowRemappingFeature:
            return RP_ROW_REMAPPING_FEATURE;
        case ReconfigSettingsIntf::FeatureType::HBMFrequencyChange:
            return RP_HBM_FREQUENCY_CHANGE;
        case ReconfigSettingsIntf::FeatureType::HULKLicenseUpdate:
            return RP_HULK_LICENSE_UPDATE;
        case ReconfigSettingsIntf::FeatureType::ForceTestCoupling:
            return RP_FORCE_TEST_COUPLING;
        case ReconfigSettingsIntf::FeatureType::BAR0TypeConfig:
            return RP_BAR0_TYPE_CONFIG;
        case ReconfigSettingsIntf::FeatureType::EDPpScalingFactor:
            return RP_EDPP_SCALING_FACTOR;
        case ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel1:
            return RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1;
        case ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel2:
            return RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2;
        default:
            throw std::invalid_argument("Invalid feature :" +
                                        std::to_string(uint64_t(feature)));
    }
}

std::optional<Request>
    NsmReconfigPermissions::genRequestMsg(eid_t eid, uint8_t instanceNumber)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(instanceNumber,
                                                            index, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_reconfiguration_permissions_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmReconfigPermissions::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    nsm_reconfiguration_permissions_v1 data;

    auto rc = decode_get_reconfiguration_permissions_v1_resp(
        responseMsg, responseLen, &cc, &reasonCode, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        pdi().allowOneShotConfig(data.oneshot);
        pdi().allowPersistentConfig(data.persistent);
        pdi().allowFLRPersistentConfig(data.flr_persistent);
        pdi().type(feature);
    }
    else
    {
        pdi().type(ReconfigSettingsIntf::FeatureType::Unknown);
        lg2::error(
            "handleResponseMsg: decode_get_reconfiguration_permissions_v1_resp sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }

    return cc ? cc : rc;
}
} // namespace nsm
