// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-tables.h"

#include <gtest/gtest.h>

extern "C" const struct nsm_fpga_part nsm_fpga_part_pdb;

TEST(fpgaRulesPdb, partHoldsOneRulePerRail)
{
	EXPECT_EQ(nsm_fpga_part_pdb.rule_count, 6u);
	EXPECT_EQ(nsm_fpga_part_pdb.group_count, 0u);
}

TEST(fpgaRulesPdb, railPowerGoodIsOneStatusWordTransaction)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *rail =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_PDB_Rail_5_Power_Good");
	ASSERT_NE(rail, nullptr);
	EXPECT_EQ(rail->kind, NSM_FPGA_RULE_STATE);
	EXPECT_STREQ(rail->group, "PDB");
	ASSERT_EQ(rail->command_count, 1u);
	EXPECT_EQ(rail->commands[0].op, NSM_FPGA_OP_I2C_XFER);
	EXPECT_EQ(rail->commands[0].station, 2);
	EXPECT_EQ(rail->commands[0].u.xfer.dev_addr, 0x5a);
	EXPECT_EQ(rail->commands[0].u.xfer.write_count, 1);
	EXPECT_EQ(rail->commands[0].u.xfer.write[0], 0x79);
	EXPECT_EQ(rail->commands[0].u.xfer.read_count, 2);
	EXPECT_EQ(rail->commands[0].capture, NSM_FPGA_BUF_NONE);
	/* bit 11 clear is power good, anything else is a critical fault */
	ASSERT_EQ(rail->state_count, 2u);
	EXPECT_EQ(rail->states[0].mask, 0x800u);
	EXPECT_EQ(rail->states[0].value, 0u);
	EXPECT_EQ(rail->states[0].state, NSM_FPGA_STATE_ENABLED);
	EXPECT_EQ(rail->states[1].mask, 0u);
	EXPECT_EQ(rail->states[1].state, NSM_FPGA_STATE_FAULT);
	EXPECT_EQ(rail->states[1].health, NSM_FPGA_HEALTH_CRITICAL);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, rail));
	const auto *standby =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_PDB_Rail_0_Power_Good");
	ASSERT_NE(standby, nullptr);
	EXPECT_EQ(standby->commands[0].u.xfer.dev_addr, 0x55);
}
