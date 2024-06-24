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
    // mockValue is only used by mock-responder and should be discarded for
    // other use cases.
    double mockValue;
};

// clang-format off
// mockValue is only used by mock-responder and should be discarded for other
// use cases.
const static std::unordered_map<uint8_t, MetricsInfo> metricsTable{
    {0,  {"GraphicsEngineActivityPercent",          GPMMetricsUnit::PERCENTAGE,     34.526}},
    {1,  {"SMActivityPercent",                      GPMMetricsUnit::PERCENTAGE,     54.483}},
    {2,  {"SMOccupancyPercent",                     GPMMetricsUnit::PERCENTAGE,     10.034}},
    {3,  {"TensorCoreActivityPercent",              GPMMetricsUnit::PERCENTAGE,     80.344}},
    {4,  {"DRAMUsagePercent",                       GPMMetricsUnit::PERCENTAGE,     65.360}},
    {5,  {"FP64ActivityPercent",                    GPMMetricsUnit::PERCENTAGE,     78.134}},
    {6,  {"FP32ActivityPercent",                    GPMMetricsUnit::PERCENTAGE,     18.345}},
    {7,  {"FP16ActivityPercent",                    GPMMetricsUnit::PERCENTAGE,     48.803}},
    {8,  {"PCIeRawRxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH,      0.345 * 1024 * 1024 * 128}},
    {9,  {"PCIeRawTxBandwidthGbps",                 GPMMetricsUnit::BANDWIDTH,      1.034 * 1024 * 1024 * 128}},
    {10, {"NVLinkRawTxBandwidthGbps",               GPMMetricsUnit::BANDWIDTH,      0.248 * 1024 * 1024 * 128}},
    {11, {"NVLinkDataTxBandwidthGbps",              GPMMetricsUnit::BANDWIDTH,      2.483 * 1024 * 1024 * 128}},
    {12, {"NVLinkRawRxBandwidthGbps",               GPMMetricsUnit::BANDWIDTH,      3.108 * 1024 * 1024 * 128}},
    {13, {"NVLinkDataRxBandwidthGbps",              GPMMetricsUnit::BANDWIDTH,      0.983 * 1024 * 1024 * 128}},
    {14, {"NVDecUtilizationPercent",                GPMMetricsUnit::PERCENTAGE,     53.443}},
    {15, {"NVJpgUtilizationPercent",                GPMMetricsUnit::PERCENTAGE,     11.839}},
    {16, {"NVOfaUtilizationPercent",                GPMMetricsUnit::PERCENTAGE,     08.345}},
    {17, {"IntergerActivityUtilizationPercent",     GPMMetricsUnit::PERCENTAGE,     93.343}},
    {18, {"DMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE,     19.053}},
    {19, {"HMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE,     46.304}},
    {20, {"IMMAUtilizationPercent",                 GPMMetricsUnit::PERCENTAGE,     39.013}}};
// clang-format on
