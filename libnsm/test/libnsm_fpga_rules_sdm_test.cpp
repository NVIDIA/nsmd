// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-tables.h"

#include <array>

#include <gtest/gtest.h>

extern "C" const struct nsm_fpga_part nsm_fpga_part_sdm;

TEST(fpgaRulesSdm, partHoldsTheRules)
{
	EXPECT_EQ(nsm_fpga_part_sdm.rule_count, 3u);
	EXPECT_EQ(nsm_fpga_part_sdm.group_count, 0u);
}

TEST(fpgaRulesSdm, chipIdRule)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *chipId = nsm_fpga_find_rule(p0, "HGX_FPGA_0_Chip_Id");
	ASSERT_NE(chipId, nullptr);
	ASSERT_EQ(chipId->command_count, 9u);
	EXPECT_EQ(chipId->commands[0].op, NSM_FPGA_OP_POP);
	EXPECT_EQ(chipId->commands[0].station, 37);
	EXPECT_EQ(chipId->commands[2].op, NSM_FPGA_OP_WRITE32);
	EXPECT_EQ(chipId->commands[2].u.csr.address, 4u);
	EXPECT_EQ(chipId->commands[2].u.csr.value, 0x07000012u);
	EXPECT_EQ(chipId->commands[4].expect.value, 0x07002000u);
	EXPECT_EQ(chipId->commands[6].capture, NSM_FPGA_BUF_ITEM);
	EXPECT_EQ(chipId->commands[8].capture, NSM_FPGA_BUF_ITEM);
	ASSERT_EQ(chipId->cleanup_count, 1u);
	EXPECT_EQ(chipId->cleanup[0].op, NSM_FPGA_OP_POP);
	EXPECT_EQ(chipId->kind, NSM_FPGA_RULE_IDENTITY);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, chipId));
}

TEST(fpgaRulesSdm, idcodeRule)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *idcode = nsm_fpga_find_rule(p0, "HGX_FPGA_1_Idcode");
	ASSERT_NE(idcode, nullptr);
	ASSERT_EQ(idcode->command_count, 7u);
	EXPECT_EQ(idcode->commands[0].op, NSM_FPGA_OP_POP);
	EXPECT_EQ(idcode->commands[2].u.csr.address, 4u);
	EXPECT_EQ(idcode->commands[2].u.csr.value, 0x07000010u);
	EXPECT_EQ(idcode->commands[4].expect.value, 0x07001000u);
	EXPECT_EQ(idcode->commands[6].capture, NSM_FPGA_BUF_ITEM);
	EXPECT_EQ(idcode->kind, NSM_FPGA_RULE_IDENTITY);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, idcode));
	size_t count = 0;
	const auto *fields = nsm_fpga_rule_identity(idcode, &count);
	ASSERT_EQ(count, 1u);
	EXPECT_EQ(fields[0].asset, NSM_FPGA_ASSET_MODEL);
	EXPECT_EQ(fields[0].length, 4);
	const std::array<uint8_t, 4> raw = {0xdd, 0x30, 0x4c, 0x03};
	char text[NSM_FPGA_IDENTITY_TEXT_MAX];
	ASSERT_TRUE(nsm_fpga_decode_identity(&fields[0], raw.data(), raw.size(),
					     nullptr, 0, text, sizeof(text)));
	EXPECT_STREQ(text, "0x034c30dd");
}
