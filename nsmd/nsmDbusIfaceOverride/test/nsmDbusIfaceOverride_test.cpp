/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "nsmApSkuIdIntf.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmMNNVLinkTopologyIntf.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmAssetIntf, Constructor)
{
    const char* path = "/xyz/openbmc_project/inventory/test/asset";
    NsmAssetIntf assetIntf(bus, path);

    // Verify default values are set
    EXPECT_EQ(assetIntf.name(), "NA");
    EXPECT_EQ(assetIntf.partNumber(), "NA");
    EXPECT_EQ(assetIntf.serialNumber(), "NA");
    EXPECT_EQ(assetIntf.manufacturer(), "NA");
    EXPECT_EQ(assetIntf.buildDate(), "0000-00-00T00:00:00Z");
    EXPECT_EQ(assetIntf.model(), "NA");
}

TEST(NsmAssetIntf, SetValues)
{
    const char* path = "/xyz/openbmc_project/inventory/test/asset2";
    NsmAssetIntf assetIntf(bus, path);

    // Set custom values
    assetIntf.name("GPU0");
    assetIntf.partNumber("PN900-12345-0000");
    assetIntf.serialNumber("SN123456789");
    assetIntf.manufacturer("NVIDIA");
    assetIntf.buildDate("2024-01-15T12:00:00Z");
    assetIntf.model("H100");

    // Verify values
    EXPECT_EQ(assetIntf.name(), "GPU0");
    EXPECT_EQ(assetIntf.partNumber(), "PN900-12345-0000");
    EXPECT_EQ(assetIntf.serialNumber(), "SN123456789");
    EXPECT_EQ(assetIntf.manufacturer(), "NVIDIA");
    EXPECT_EQ(assetIntf.buildDate(), "2024-01-15T12:00:00Z");
    EXPECT_EQ(assetIntf.model(), "H100");
}

TEST(NsmApSkuIdIntf, Constructor)
{
    const char* path = "/xyz/openbmc_project/inventory/test/sku";
    NsmApSkuIdIntf skuIntf(bus, path);

    // Verify default value is set
    EXPECT_EQ(skuIntf.sku(), "NA");
}

TEST(NsmApSkuIdIntf, SetValue)
{
    const char* path = "/xyz/openbmc_project/inventory/test/sku2";
    NsmApSkuIdIntf skuIntf(bus, path);

    // Set custom value
    skuIntf.sku("SKU-12345");

    // Verify value
    EXPECT_EQ(skuIntf.sku(), "SKU-12345");
}

TEST(NsmMNNVLinkTopologyIntf, Constructor)
{
    const char* path = "/xyz/openbmc_project/inventory/test/topology";
    NsmMNNVLinkTopologyIntf topoIntf(bus, path);

    // Verify default values
    EXPECT_EQ(topoIntf.chassisSerialNumber(), "NA");
    EXPECT_EQ(topoIntf.traySlotNumber(), UINT32_MAX);
    EXPECT_EQ(topoIntf.traySlotIndex(), UINT32_MAX);
    EXPECT_EQ(topoIntf.hostID(), UINT32_MAX);
    EXPECT_EQ(topoIntf.moduleID(), UINT32_MAX);
    EXPECT_EQ(topoIntf.ibguid(), "NA");
    EXPECT_EQ(topoIntf.peerType(), "NA");
}

TEST(NsmMNNVLinkTopologyIntf, SetValues)
{
    const char* path = "/xyz/openbmc_project/inventory/test/topology2";
    NsmMNNVLinkTopologyIntf topoIntf(bus, path);

    // Set custom values
    topoIntf.chassisSerialNumber("CHASSIS-SN-123");
    topoIntf.traySlotNumber(5);
    topoIntf.traySlotIndex(3);
    topoIntf.hostID(2);
    topoIntf.moduleID(1);
    topoIntf.ibguid("0x1234567890ABCDEF");
    topoIntf.peerType("Direct");

    // Verify values
    EXPECT_EQ(topoIntf.chassisSerialNumber(), "CHASSIS-SN-123");
    EXPECT_EQ(topoIntf.traySlotNumber(), 5);
    EXPECT_EQ(topoIntf.traySlotIndex(), 3);
    EXPECT_EQ(topoIntf.hostID(), 2);
    EXPECT_EQ(topoIntf.moduleID(), 1);
    EXPECT_EQ(topoIntf.ibguid(), "0x1234567890ABCDEF");
    EXPECT_EQ(topoIntf.peerType(), "Direct");
}
