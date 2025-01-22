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
    const std::string& name, const std::string& type,
    ReconfigSettingsIntf::FeatureType feature,
    std::shared_ptr<ReconfigSettingsIntf> hostConfigIntf,
    std::shared_ptr<ReconfigSettingsIntf> doeConfigIntf) :
    NsmSensor(name, type),
    feature(feature), hostConfigIntf(hostConfigIntf),
    doeConfigIntf(doeConfigIntf)
{
    // Validates feature value during object creation
    index = getIndex(feature);
    hostConfigIntf->type(feature);
    doeConfigIntf->type(feature);
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
        case ReconfigSettingsIntf::FeatureType::EGMMode:
            return RP_EGM_MODE;
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
        lg2::debug(
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
        hostConfigIntf->allowOneShotConfig(data.host_oneshot);
        hostConfigIntf->allowPersistentConfig(data.host_persistent);
        hostConfigIntf->allowFLRPersistentConfig(data.host_flr_persistent);
        doeConfigIntf->allowOneShotConfig(data.DOE_oneshot);
        doeConfigIntf->allowPersistentConfig(data.DOE_persistent);
        doeConfigIntf->allowFLRPersistentConfig(data.DOE_flr_persistent);
        clearErrorBitMap("decode_get_reconfiguration_permissions_v1_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_reconfiguration_permissions_v1_resp",
                             reasonCode, cc, rc);
    }

    return cc ? cc : rc;
}

requester::Coroutine NsmReconfigPermissions::patchHostOneShotConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    lg2::info("patchHostOneShotConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));

    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t permission;
    if (*allowValue)
    {
        if (doeConfigIntf->allowOneShotConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
    }
    else
    {
        if (doeConfigIntf->allowOneShotConfig())
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }

    co_return co_await setAllowPermission(RP_ONESHOOT_HOT_RESET, permission,
                                          *status, device);
}

requester::Coroutine NsmReconfigPermissions::patchDOEOneShotConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const auto allowValue = std::get_if<bool>(&value);
    lg2::info("patchDOEOneShotConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t permission;
    if (*allowValue)
    {
        if (hostConfigIntf->allowOneShotConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
    }
    else
    {
        if (doeConfigIntf->allowOneShotConfig())
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }

    co_return co_await setAllowPermission(RP_ONESHOOT_HOT_RESET, permission,
                                          *status, device);
}

requester::Coroutine NsmReconfigPermissions::patchHostPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    lg2::info("patchHostPersistentConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));

    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    uint8_t permission;
    if (*allowValue)
    {
        if (doeConfigIntf->allowPersistentConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
    }
    else
    {
        if (doeConfigIntf->allowPersistentConfig())
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }
    co_return co_await setAllowPermission(RP_PERSISTENT, permission, *status,
                                          device);
}

requester::Coroutine NsmReconfigPermissions::patchDOEPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    lg2::info("patchDOEPersistentConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t permission;
    if (*allowValue)
    {
        if (hostConfigIntf->allowPersistentConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
    }
    else
    {
        if (doeConfigIntf->allowPersistentConfig())
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }

    co_return co_await setAllowPermission(RP_PERSISTENT, permission, *status,
                                          device);
}

requester::Coroutine NsmReconfigPermissions::patchHostFLRPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    lg2::info("patchHostFLRPersistentConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t permission;
    if (*allowValue)
    {
        if (doeConfigIntf->allowFLRPersistentConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
    }
    else
    {
        if (doeConfigIntf->allowFLRPersistentConfig())
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }

    co_return co_await setAllowPermission(RP_ONESHOT_FLR, permission, *status,
                                          device);
}

requester::Coroutine NsmReconfigPermissions::patchDOEFLRPersistentConfig(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    lg2::info("patchDOEFLRPersistentConfig On Device for EID: {EID}", "EID",
              SensorManager::getInstance().getEid(device));
    const auto allowValue = std::get_if<bool>(&value);

    if (!allowValue)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    uint8_t permission;
    if (*allowValue)
    {
        if (hostConfigIntf->allowFLRPersistentConfig())
        {
            permission = ALLOW_HOST_ALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_ALLOW_DOE;
        }
    }
    else
    {
        if (hostConfigIntf->allowFLRPersistentConfig())
        {
            permission = ALLOW_HOST_DISALLOW_DOE;
        }
        else
        {
            permission = DISALLOW_HOST_DISALLOW_DOE;
        }
    }

    co_return co_await setAllowPermission(RP_ONESHOT_FLR, permission, *status,
                                          device);
}

requester::Coroutine NsmReconfigPermissions::setAllowPermission(
    reconfiguration_permissions_v1_setting configuration, const uint8_t value,
    AsyncOperationStatusType& status, std::shared_ptr<NsmDevice> device)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        0, index, configuration, value, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_set_reconfiguration_permissions_v1_req({SI}) failed. eid={EID} rc={RC}",
            "SI", (int)index, "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmSetReconfigSettings::setAllowPermission: SendRecvNsmMsgSync failed."
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_set_reconfiguration_permissions_v1_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmSetReconfigSettings::setAllowPermission decode_set_reconfiguration_permissions_v1_resp success - value={VALUE}, settingIndex={SI}",
            "VALUE", value, "SI", (int)index);
    }
    else
    {
        lg2::error(
            "NsmSetReconfigSettings::setAllowPermission: decode_set_reconfiguration_permissions_v1_resp failed with reasonCode = {REASONCODE}, cc = {CC} and rc = {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        status = AsyncOperationStatusType::WriteFailure;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

} // namespace nsm
