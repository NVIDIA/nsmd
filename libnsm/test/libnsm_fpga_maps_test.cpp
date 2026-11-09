// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-maps.h"

#include <gtest/gtest.h>

TEST(fpgaMaps, onePlatformRowSelectedByName)
{
	const auto *maps = nsm_fpga_map_identity_table;
	EXPECT_EQ(NSM_FPGA_PLATFORM_COUNT, 1);
	const auto *lp30 = &maps[NSM_FPGA_PLATFORM_LP30];
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("LP30"), lp30);
	EXPECT_STREQ(lp30->platform, "LP30");
	EXPECT_STREQ(lp30->name, "P0 ibc1 schema 13");
	EXPECT_EQ(lp30->configuration_type, 2u);
	EXPECT_EQ(lp30->identity_address, 0x1080u);
	EXPECT_EQ(lp30->identity_dwords, 8u);
	EXPECT_EQ(NSM_FPGA_P0_CONFIGURATION_TYPE, 2);
	EXPECT_EQ(NSM_FPGA_P0_IDENTITY_ADDRESS, 0x1080);
	EXPECT_EQ(NSM_FPGA_P0_IDENTITY_DWORDS, 8);
	EXPECT_EQ(lp30->digest[0], 0x30);
	EXPECT_EQ(lp30->digest[31], 0x0f);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform("lp30"), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform(""), nullptr);
	EXPECT_EQ(nsm_fpga_map_identity_by_platform(nullptr), nullptr);
}
