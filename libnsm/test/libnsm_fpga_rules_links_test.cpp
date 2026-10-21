// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-tables.h"

#include <gtest/gtest.h>

extern "C" const struct nsm_fpga_part nsm_fpga_part_links;
extern "C" const struct nsm_fpga_part nsm_fpga_part_optics;

TEST(fpgaRulesLinks, partsHoldTheRules)
{
	EXPECT_EQ(nsm_fpga_part_links.rule_count, 16u);
	EXPECT_EQ(nsm_fpga_part_links.group_count, 0u);
	EXPECT_EQ(nsm_fpga_part_optics.rule_count, 45u);
	EXPECT_EQ(nsm_fpga_part_optics.group_count, 10u);
}

TEST(fpgaRulesLinks, linkRules)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *ready = nsm_fpga_find_rule(
	    p0, "HGX_Chassis_0_FPGA_0_C2C_1_Lane_3_Pcs_Ready");
	ASSERT_NE(ready, nullptr);
	EXPECT_EQ(ready->kind, NSM_FPGA_RULE_STATE);
	/* lane 3 of connection 1 is channel 0 of bank 13C */
	EXPECT_EQ(ready->commands[0].station, 11);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, ready));
	const auto *ber =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Ethernet_1_High_Ber");
	ASSERT_NE(ber, nullptr);
	EXPECT_EQ(ber->kind, NSM_FPGA_RULE_STATE);
	EXPECT_EQ(ber->commands[0].station, 24);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, ber));
}

TEST(fpgaRulesLinks, opticsRules)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *group = nsm_fpga_find_group(p0, "Optics_4");
	ASSERT_NE(group, nullptr);
	ASSERT_EQ(group->setup_count, 19u);
	ASSERT_EQ(group->cleanup_count, 2u);
	/* the mux mask found is captured first, then the expander pins */
	EXPECT_EQ(group->setup[0].op, NSM_FPGA_OP_I2C_XFER);
	EXPECT_EQ(group->setup[0].u.xfer.dev_addr, 0x71);
	EXPECT_EQ(group->setup[0].u.xfer.read_count, 1);
	EXPECT_EQ(group->setup[0].capture, NSM_FPGA_BUF_GROUP);
	EXPECT_EQ(group->setup[1].u.xfer.write[0], 0x04);
	EXPECT_EQ(group->setup[3].u.xfer.dev_addr, 0x70);
	EXPECT_EQ(group->setup[3].u.xfer.write_count, 1);
	EXPECT_EQ(group->setup[3].u.xfer.read_count, 1);
	EXPECT_EQ(group->setup[4].op, NSM_FPGA_OP_CHECK);
	EXPECT_EQ(group->setup[4].group_state, NSM_FPGA_STATE_ABSENT);
	EXPECT_EQ(group->setup[5].group_state,
		  NSM_FPGA_STATE_UNAVAILABLE_OFFLINE);
	EXPECT_EQ(group->setup[5].group_health, NSM_FPGA_HEALTH_WARNING);
	/* the expander output latch and direction reads for power good */
	EXPECT_EQ(group->setup[6].u.xfer.write[0], 0x01);
	EXPECT_EQ(group->setup[6].u.xfer.read_count, 1);
	EXPECT_EQ(group->setup[7].u.xfer.write[0], 0x03);
	EXPECT_EQ(group->setup[7].u.xfer.read_count, 1);
	EXPECT_EQ(group->setup[8].u.xfer.write[0], 0x04);
	/* the CMIS header: one pointer byte, four bytes read into the group */
	EXPECT_EQ(group->setup[10].u.xfer.dev_addr, 0x50);
	EXPECT_EQ(group->setup[10].u.xfer.read_count, 4);
	EXPECT_EQ(group->setup[10].capture, NSM_FPGA_BUF_GROUP);
	/* cleanup gives the bank and page, then the mux, back from the buffer
	 */
	EXPECT_EQ(group->cleanup[0].u.xfer.write_count, 3);
	EXPECT_EQ(group->cleanup[0].u.xfer.captured, 0x6);
	EXPECT_EQ(group->cleanup[0].u.xfer.captured_offset, 6);
	EXPECT_EQ(group->cleanup[1].u.xfer.dev_addr, 0x71);
	EXPECT_EQ(group->cleanup[1].u.xfer.captured, 0x1);
	EXPECT_EQ(group->cleanup[1].run_if.offset, 0);
	/* the lane vectors read through a second group with the same setup */
	const auto *lanes = nsm_fpga_find_group(p0, "Optics_4_Lanes");
	ASSERT_NE(lanes, nullptr);
	EXPECT_EQ(lanes->setup_count, group->setup_count);
	EXPECT_EQ(lanes->cleanup_count, group->cleanup_count);
	EXPECT_EQ(lanes->setup[1].u.xfer.write[0], 0x04);
	const auto *txp =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_4_Tx_Power");
	ASSERT_NE(txp, nullptr);
	EXPECT_STREQ(txp->group, "Optics_4_Lanes");
	const auto *presence =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_4");
	ASSERT_NE(presence, nullptr);
	EXPECT_EQ(presence->command_count, 0u);
	ASSERT_EQ(presence->state_count, 2u);
	EXPECT_EQ(presence->states[0].mask, 0x10u);
	EXPECT_EQ(presence->states[1].state, NSM_FPGA_STATE_ABSENT);
	EXPECT_EQ(presence->numeric.offset, 1);
	EXPECT_STREQ(presence->group, "Optics_4");
	/* power good: bit 1 of the same expander byte, no rows of its own */
	const auto *pgood =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_4_Power_Good");
	ASSERT_NE(pgood, nullptr);
	EXPECT_EQ(pgood->command_count, 0u);
	EXPECT_EQ(pgood->numeric.offset, 1);
	ASSERT_EQ(pgood->state_count, 2u);
	EXPECT_EQ(pgood->states[0].mask, 0x02u);
	EXPECT_EQ(pgood->states[0].state, NSM_FPGA_STATE_ENABLED);
	EXPECT_EQ(pgood->states[1].state, NSM_FPGA_STATE_FAULT);
	EXPECT_EQ(pgood->states[1].health, NSM_FPGA_HEALTH_CRITICAL);
	const auto *state = nsm_fpga_find_rule(
	    p0, "HGX_Chassis_0_FPGA_0_Optics_0_Module_State");
	ASSERT_NE(state, nullptr);
	EXPECT_EQ(state->state_count, 6u);
	EXPECT_EQ(state->states[0].mask, 0x0eu);
	/* inventory: page select, confirm, then 53 bytes in four-byte steps */
	const auto *inventory =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_0_Inventory");
	ASSERT_NE(inventory, nullptr);
	EXPECT_EQ(inventory->kind, NSM_FPGA_RULE_IDENTITY);
	EXPECT_EQ(inventory->command_count, 16u);
	EXPECT_EQ(inventory->commands[2].u.xfer.write[0], 129);
	EXPECT_EQ(inventory->commands[2].u.xfer.read_count, 4);
	EXPECT_EQ(inventory->commands[15].u.xfer.write[0], 181);
	EXPECT_EQ(inventory->commands[15].u.xfer.read_count, 1);
	ASSERT_EQ(inventory->identity_count, 4u);
	EXPECT_EQ(inventory->identity[0].asset, NSM_FPGA_ASSET_MANUFACTURER);
	EXPECT_EQ(inventory->identity[2].asset, NSM_FPGA_ASSET_REVISION);
	EXPECT_EQ(inventory->identity[3].offset, 37);
	const auto *temp =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_2_Temp_0");
	ASSERT_NE(temp, nullptr);
	EXPECT_EQ(temp->command_count, 1u);
	EXPECT_EQ(temp->numeric.format, NSM_FPGA_FORMAT_S16BE);
	EXPECT_EQ(temp->valid_if.offset, 11);
	const auto *power =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_1_Rx_Power");
	ASSERT_NE(power, nullptr);
	EXPECT_EQ(power->kind, NSM_FPGA_RULE_ARRAY);
	EXPECT_EQ(power->numeric.vector, NSM_FPGA_VECTOR_RX_POWER);
	EXPECT_EQ(power->numeric.stride_bits, 16);
	EXPECT_EQ(power->command_count, 24u);
	/* banks 1 to 3 run only when the bank count code allows them */
	EXPECT_EQ(power->commands[0].run_if.buffer, NSM_FPGA_BUF_NONE);
	EXPECT_EQ(power->commands[6].run_if.op, NSM_FPGA_CMP_GE);
	EXPECT_EQ(power->commands[6].run_if.value, 1u);
	EXPECT_EQ(power->commands[12].run_if.op, NSM_FPGA_CMP_EQ);
	EXPECT_EQ(power->commands[12].run_if.value, 2u);
	EXPECT_EQ(power->run_if.offset, 5);
	EXPECT_EQ(power->run_if.value, 0x06u);
	EXPECT_EQ(power->valid_if.mask, 0x04u);
	const auto *fault =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_Optics_3_Tx_Fault");
	ASSERT_NE(fault, nullptr);
	EXPECT_EQ(fault->numeric.vector, NSM_FPGA_VECTOR_TX_FAULT);
	EXPECT_EQ(fault->numeric.stride_bits, 1);
	EXPECT_EQ(fault->command_count, 12u);
	EXPECT_EQ(fault->commands[2].u.xfer.write[0], 135);
}
