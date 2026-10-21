// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga/fpga-tables.h"

#include <array>

#include <gtest/gtest.h>

extern "C" const struct nsm_fpga_part nsm_fpga_part_dimm;

TEST(fpgaRulesDimm, partHoldsTheRules)
{
	EXPECT_EQ(nsm_fpga_part_dimm.rule_count, 16u);
	EXPECT_EQ(nsm_fpga_part_dimm.group_count, 0u);
}

TEST(fpgaRulesDimm, calibrationAndTemperatureRules)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *calib = nsm_fpga_find_rule(
	    p0, "HGX_Chassis_0_FPGA_1_DIMM_2_Channel_1_Calibration");
	ASSERT_NE(calib, nullptr);
	ASSERT_EQ(calib->command_count, 1u);
	EXPECT_EQ(calib->commands[0].op, NSM_FPGA_OP_READ32);
	EXPECT_EQ(calib->commands[0].u.csr.address, 0x05000400u);
	EXPECT_EQ(calib->kind, NSM_FPGA_RULE_STATE);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, calib));
	const auto *temp =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_0_DIMM_0_Temp_0");
	ASSERT_NE(temp, nullptr);
	EXPECT_EQ(temp->command_count, 7u);
	// A stale response is acknowledged before the transfer is staged
	EXPECT_EQ(temp->commands[0].remember, 1u);
	EXPECT_EQ(temp->commands[1].op, NSM_FPGA_OP_ACK_CLEAR);
	EXPECT_EQ(temp->commands[1].run_if.buffer, NSM_FPGA_BUF_WORD);
	ASSERT_EQ(temp->cleanup_count, 1u);
	EXPECT_EQ(temp->cleanup[0].op, NSM_FPGA_OP_ACK_CLEAR);
	EXPECT_EQ(temp->kind, NSM_FPGA_RULE_NUMERIC);
	EXPECT_EQ(temp->numeric.format, NSM_FPGA_FORMAT_FIXED);
	EXPECT_EQ(temp->valid_if.mask, 0xe003u);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, temp));
	/* 0x274: bits 2 to 12 are 157 quarter degrees, 39.25 C */
	const std::array<uint8_t, 4> word{0x74, 0x02, 0x00, 0x00};
	double celsius = 0.0;
	EXPECT_TRUE(nsm_fpga_decode_numeric(&temp->numeric, word.data(),
					    word.size(), nullptr, 0, &celsius));
	EXPECT_DOUBLE_EQ(celsius, 39.25);
	/* a reserved bit set fails the validity condition */
	const std::array<uint8_t, 4> reserved{0x01, 0x00, 0x00, 0x00};
	EXPECT_FALSE(nsm_fpga_compare_holds(&temp->valid_if, reserved.data(),
					    reserved.size(), nullptr, 0, 0));
}

TEST(fpgaRulesDimm, inventoryRule)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *inv =
	    nsm_fpga_find_rule(p0, "HGX_Chassis_0_FPGA_1_DIMM_2_Inventory");
	ASSERT_NE(inv, nullptr);
	EXPECT_EQ(inv->kind, NSM_FPGA_RULE_IDENTITY);
	EXPECT_TRUE(nsm_fpga_rule_valid(p0, inv));
	ASSERT_EQ(inv->command_count, 54u);
	/* the selector is read first and captured only when it is page 0 */
	EXPECT_EQ(inv->commands[0].station, 33);
	EXPECT_EQ(inv->commands[0].op, NSM_FPGA_OP_READ32);
	EXPECT_EQ(inv->commands[2].u.csr.value, 0x00a10ba0u);
	EXPECT_EQ(inv->commands[6].capture, NSM_FPGA_BUF_ITEM);
	EXPECT_EQ(inv->commands[6].expect.mask, 0xfbu);
	EXPECT_EQ(inv->commands[6].expect.value, 0u);
	/* page 4 is selected, confirmed, then bytes 512 to 553 read */
	EXPECT_EQ(inv->commands[9].u.csr.value, 0x00040ba0u);
	EXPECT_EQ(inv->commands[11].u.csr.value, 0x20040f02u);
	EXPECT_EQ(inv->commands[20].expect.value, 4u);
	EXPECT_EQ(inv->commands[24].u.csr.value, 0x00a180a0u);
	EXPECT_EQ(inv->commands[25].u.csr.value, 0x00001002u);
	EXPECT_EQ(inv->commands[46].u.csr.value, 0x00a1a0a0u);
	EXPECT_EQ(inv->commands[47].u.csr.value, 0x00000a02u);
	EXPECT_EQ(inv->commands[53].op, NSM_FPGA_OP_ACK_CLEAR);
	// Page 0 is restored only when the selector was 0 or the inventory page
	ASSERT_EQ(inv->cleanup_count, 7u);
	EXPECT_EQ(inv->cleanup[1].run_if.mask, 0xfbu);
	/* cleanup acknowledges, then restores page 0 only when page 0 was seen
	 */
	ASSERT_EQ(inv->cleanup_count, 7u);
	EXPECT_EQ(inv->cleanup[0].op, NSM_FPGA_OP_ACK_CLEAR);
	EXPECT_EQ(inv->cleanup[0].run_if.buffer, NSM_FPGA_BUF_NONE);
	for (size_t i = 1; i < inv->cleanup_count; ++i) {
		EXPECT_EQ(inv->cleanup[i].run_if.buffer, NSM_FPGA_BUF_ITEM);
		EXPECT_EQ(inv->cleanup[i].run_if.offset, 0);
		EXPECT_EQ(inv->cleanup[i].run_if.value, 0u);
	}
	EXPECT_EQ(inv->cleanup[2].u.csr.value, 0x00000ba0u);
	/* the serial number and part number fields follow the selector word */
	size_t count = 0;
	const auto *fields = nsm_fpga_rule_identity(inv, &count);
	ASSERT_EQ(count, 2u);
	EXPECT_EQ(fields[0].asset, NSM_FPGA_ASSET_SERIAL);
	EXPECT_EQ(fields[0].format, NSM_FPGA_IDENTITY_HEX_BE);
	EXPECT_EQ(fields[0].offset, 9);
	EXPECT_EQ(fields[0].length, 4);
	EXPECT_EQ(fields[1].asset, NSM_FPGA_ASSET_PART);
	EXPECT_EQ(fields[1].format, NSM_FPGA_IDENTITY_ASCII);
	EXPECT_EQ(fields[1].offset, 13);
	EXPECT_EQ(fields[1].length, 30);
}

TEST(fpgaRulesDimm, calibrationMapCoversEveryCode)
{
	const auto *p0 = nsm_fpga_map_p0();
	const auto *calib = nsm_fpga_find_rule(
	    p0, "HGX_Chassis_0_FPGA_1_DIMM_2_Channel_1_Calibration");
	ASSERT_NE(calib, nullptr);
	ASSERT_EQ(calib->state_count, 5u);
	uint8_t state = 0;
	uint8_t health = 0;
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 0, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_ABSENT);
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 1, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_ENABLED);
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 2, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_FAULT);
	EXPECT_EQ(health, NSM_FPGA_HEALTH_CRITICAL);
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 4, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_STARTING);
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 3, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_UNAVAILABLE_OFFLINE);
	EXPECT_EQ(health, NSM_FPGA_HEALTH_WARNING);
	/* only the low three bits count */
	EXPECT_TRUE(nsm_fpga_decode_state(calib, 0xfffffff9, &state, &health));
	EXPECT_EQ(state, NSM_FPGA_STATE_ENABLED);
}
