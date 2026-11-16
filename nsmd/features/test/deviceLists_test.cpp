/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "features/devices/Bluefield_SMA.hpp"
#include "features/devices/baseboard.hpp"
#include "features/devices/bluefield.hpp"
#include "features/devices/bmc_hpm_sma.hpp"
#include "features/devices/bmc_irot.hpp"
#include "features/devices/cpu.hpp"
#include "features/devices/cx_io.hpp"
#include "features/devices/cx_nic.hpp"
#include "features/devices/cx_sma.hpp"
#include "features/devices/gpu.hpp"
#include "features/devices/gpuSxm.hpp"
#include "features/devices/gpu_sma.hpp"
#include "features/devices/hmc_hpm_sma.hpp"
#include "features/devices/nvswitch.hpp"
#include "features/devices/vr_bmc_erot.hpp"
#include "features/devices/vr_hmc_erot.hpp"
#include "features/paramTemplates.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace nsm::features;
using namespace std::string_literals;

namespace
{

struct NamedList
{
    const char* name;
    FeatureList (*build)();
    bool mayBeEmpty;
};

// mayBeEmpty: lists that are placeholders today (no features yet)
const std::vector<NamedList>& allLists()
{
    static const std::vector<NamedList> lists = {
        {"gpu", gpuFeatures, false},
        {"cpu", cpuFeatures, false},
        {"nvswitch", nvswitchFeatures, true},
        {"baseboard", baseboardFeatures, true},
        {"erot", erotFeatures, true},
        {"bmc_irot", irotFeatures, false},
        {"cx", cxFeatures, true},
        {"bluefield_nic", bluefieldFeatures, false},
        {"cx9_nic", cxNicFeatures, false},
        {"cx9_io", cxIoFeatures, false},
        {"bluefield_sma", bluefieldSmaFeatures, false},
        {"cx_sma", cxSmaFeatures, false},
        {"gpu_sxm", gpuSxmFeatures, true},
        {"gpu_sma", gpuSmaFeatures, false},
        {"hmc_hpm_sma", hmcHpmSmaFeatures, false},
        {"bmc_hpm_sma", bmcHpmSmaFeatures, false},
        {"hmc_erot", vrHmcErotFeatures, false},
        {"bmc_erot", vrBmcErotFeatures, false},
    };
    return lists;
}

/** @brief Names of the "{...}" tokens in a text */
void collectTokens(const std::string& text, std::set<std::string>& names)
{
    size_t pos = 0;
    while ((pos = text.find('{', pos)) != std::string::npos)
    {
        auto close = text.find('}', pos + 1);
        if (close == std::string::npos)
        {
            return;
        }
        names.insert(text.substr(pos + 1, close - pos - 1));
        pos = close + 1;
    }
}

/** @brief A device whose variables cover every token of the feature, so
 *  resolving can fail only because of a malformed template or binding
 */
DeviceContext deviceFor(const Feature& feature)
{
    DeviceContext dev;
    dev.name = "HGX_DEV_0";
    dev.kind = "test";
    dev.instance = 1;
    dev.platformPrefix = "HGX_";
    dev.parentChassis = "board/HGX_Parent_0";

    for (const auto& [key, param] : feature.params)
    {
        if (!param.var.empty() && param.var.size() > 2)
        {
            auto name = param.var.substr(1, param.var.size() - 2);
            if (!isBuiltinVariable(name))
            {
                // The declared type is the type the creator needs
                dev.variables[name] = param.value;
            }
        }
    }
    std::set<std::string> names;
    for (const auto& [key, param] : feature.params)
    {
        if (!param.var.empty())
        {
            continue;
        }
        if (const auto* text = std::get_if<std::string>(&param.value))
        {
            collectTokens(*text, names);
        }
        else if (const auto* texts =
                     std::get_if<std::vector<std::string>>(&param.value))
        {
            for (const auto& t : *texts)
            {
                collectTokens(t, names);
            }
        }
    }
    for (const auto& name : names)
    {
        if (!isBuiltinVariable(name) && !dev.variables.contains(name))
        {
            dev.variables[name] = "x"s;
        }
    }
    return dev;
}

} // namespace

TEST(DeviceListsTest, EveryListBuilds_AndIsNotEmptyUnlessPlaceholder)
{
    for (const auto& l : allLists())
    {
        SCOPED_TRACE(l.name);
        auto list = l.build();
        if (!l.mayBeEmpty)
        {
            EXPECT_FALSE(list.empty());
        }
    }
}

TEST(DeviceListsTest, FeatureIdsAreUniqueAndNonEmptyWithinEachList)
{
    for (const auto& l : allLists())
    {
        SCOPED_TRACE(l.name);
        std::set<std::string> ids;
        for (const auto& f : l.build())
        {
            EXPECT_FALSE(f.id.empty());
            EXPECT_TRUE(ids.insert(std::string(f.id)).second)
                << "duplicate feature id " << f.id;
        }
    }
}

TEST(DeviceListsTest, EveryFeatureHasACreator)
{
    for (const auto& l : allLists())
    {
        SCOPED_TRACE(l.name);
        for (const auto& f : l.build())
        {
            EXPECT_NE(f.create, nullptr) << "feature " << f.id;
        }
    }
}

TEST(DeviceListsTest, EveryFeatureParamsResolve_WithoutDroppedOptionals)
{
    for (const auto& l : allLists())
    {
        for (const auto& f : l.build())
        {
            SCOPED_TRACE(std::string(l.name) + "/" + std::string(f.id));
            auto dev = deviceFor(f);
            std::string error;
            std::vector<std::string> dropped;
            auto resolved = resolveParams(dev, f.params, error, dropped);
            EXPECT_TRUE(resolved.has_value()) << error;
            EXPECT_TRUE(dropped.empty());
            if (resolved)
            {
                EXPECT_EQ(resolved->size(), f.params.size());
            }
        }
    }
}

TEST(DeviceListsTest, EveryNonEmptyListEnablesAtLeastOneFeature)
{
    // A feature may be off by default (enabled per platform by an override)
    // but a list that creates nothing is a mistake
    for (const auto& l : allLists())
    {
        SCOPED_TRACE(l.name);
        auto list = l.build();
        if (list.empty())
        {
            continue;
        }
        EXPECT_TRUE(std::any_of(list.begin(), list.end(),
                                [](const Feature& f) { return f.enabled; }));
    }
}

TEST(DeviceListsTest, DeclaredOptionalKeys_DoNotCollideWithParams)
{
    for (const auto& l : allLists())
    {
        SCOPED_TRACE(l.name);
        for (const auto& f : l.build())
        {
            for (const auto& [key, value] : f.optional)
            {
                EXPECT_FALSE(f.params.contains(key))
                    << "feature " << f.id << " key " << key;
            }
        }
    }
}

TEST(DeviceListsTest, ListsAreBuiltFreshEveryTime)
{
    // Coordinator copies the defaults per device; two builds must not share
    // state
    auto a = gpuFeatures();
    auto b = gpuFeatures();
    ASSERT_EQ(a.size(), b.size());
    ASSERT_FALSE(a.empty());
    a[0].enabled = false;
    EXPECT_TRUE(b[0].enabled);
}
