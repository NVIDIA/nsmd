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

#pragma once

#include <cstdint>
#include <unordered_map>

enum class GPMMetricsUnit : uint8_t
{
    PERCENTAGE,
    BANDWIDTH
};

struct MetricsInfo
{
    const char* name;
    GPMMetricsUnit unit;
};

// clang-format off
const static std::unordered_map<uint8_t, MetricsInfo> metricsTable{
    {0,  {"GraphicsEngineActivityPercent",          GPMMetricsUnit::PERCENTAGE}},
    {1,  {"SMActivityPercent",                      GPMMetricsUnit::PERCENTAGE}},
    {2,  {"SMOccupancyPercent",                     GPMMetricsUnit::PERCENTAGE}},
    {3,  {"TensorCoreActivityPercent",              GPMMetricsUnit::PERCENTAGE}},
    {4,  {"DRAMUsagePercent",                       GPMMetricsUnit::PERCENTAGE}},
    {5,  {"FP64ActivityPercent",                    GPMMetricsUnit::PERCENTAGE}},
    {6,  {"FP32ActivityPercent",                    GPMMetricsUnit::PERCENTAGE}},
    {7,  {"FP16ActivityPercent",                    GPMMetricsUnit::PERCENTAGE}},
    {8,  {"PCIeRawTxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH,}},
    {9,  {"PCIeRawRxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH,}},
    {10, {"NVLinkRawTxBandwidthGbps",               GPMMetricsUnit::BANDWIDTH,}},
    {11, {"NVLinkDataTxBandwidthGbps",              GPMMetricsUnit::BANDWIDTH,}},
    {12, {"NVLinkRawRxBandwidthGbps",               GPMMetricsUnit::BANDWIDTH,}},
    {13, {"NVLinkDataRxBandwidthGbps",              GPMMetricsUnit::BANDWIDTH,}},
    {14, {"NVDecUtilizationPercent",                GPMMetricsUnit::PERCENTAGE}},
    {15, {"NVJpgUtilizationPercent",                GPMMetricsUnit::PERCENTAGE}},
    {16, {"NVOfaUtilizationPercent",                GPMMetricsUnit::PERCENTAGE}},
    {17, {"IntergerActivityUtilizationPercent",     GPMMetricsUnit::PERCENTAGE}},
    {18, {"DMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE}},
    {19, {"HMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE}},
    {20, {"IMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE}},
    {21, {"NVEncUtilizationPercent",                GPMMetricsUnit::PERCENTAGE}},
    {22, {"HostMemoryCacheHitPercent",              GPMMetricsUnit::PERCENTAGE}},
    {23, {"HostMemoryCacheMissPercent",             GPMMetricsUnit::PERCENTAGE}},
    {24, {"PeerMemoryCacheHitPercent",              GPMMetricsUnit::PERCENTAGE}},
    {25, {"PeerMemoryCacheMissPercent",             GPMMetricsUnit::PERCENTAGE}},
    {26, {"DRAMCacheHitPercent",                    GPMMetricsUnit::PERCENTAGE}},
    {27, {"DRAMCacheMissPercent",                   GPMMetricsUnit::PERCENTAGE}},
    {28, {"C2CRawTxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH}},
    {29, {"C2CRawRxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH}},
    {30, {"C2CDataTxBandwidthGbps",                GPMMetricsUnit::BANDWIDTH}},
    {31, {"C2CDataRxBandwidthGbps",                GPMMetricsUnit::BANDWIDTH}},
};
// clang-format on
