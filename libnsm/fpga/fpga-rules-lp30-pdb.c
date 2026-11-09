// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
/* clang-format off */
/* Generated from fpga-rules-lp30-pdb.rows by gen-fpga-rules.py, do not edit. */
#include "fpga-tables.h"

#include <math.h>

static const struct nsm_fpga_command rows_PDB_Rail_0_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x55, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_state_entry states_PDB_Rail_0_Power_Good[] = {
    {0x800, 0, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
    {0, 0, NSM_FPGA_STATE_FAULT, NSM_FPGA_HEALTH_CRITICAL},
};

static const struct nsm_fpga_command rows_PDB_Rail_1_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x56, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_command rows_PDB_Rail_2_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x57, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_command rows_PDB_Rail_3_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x58, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_command rows_PDB_Rail_4_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x59, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_command rows_PDB_Rail_5_Power_Good[] = {
    {.op = NSM_FPGA_OP_I2C_XFER, .station = 2, .u = {.xfer = {0x5a, {0x79, 0, 0}, 1, 2, 0, 0}}},
};

static const struct nsm_fpga_rule rules_pdb[] = {
    {.name = "PDB_Rail_0_Power_Good",
     .commands = rows_PDB_Rail_0_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_0_Power_Good) / sizeof(rows_PDB_Rail_0_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
    {.name = "PDB_Rail_1_Power_Good",
     .commands = rows_PDB_Rail_1_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_1_Power_Good) / sizeof(rows_PDB_Rail_1_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
    {.name = "PDB_Rail_2_Power_Good",
     .commands = rows_PDB_Rail_2_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_2_Power_Good) / sizeof(rows_PDB_Rail_2_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
    {.name = "PDB_Rail_3_Power_Good",
     .commands = rows_PDB_Rail_3_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_3_Power_Good) / sizeof(rows_PDB_Rail_3_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
    {.name = "PDB_Rail_4_Power_Good",
     .commands = rows_PDB_Rail_4_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_4_Power_Good) / sizeof(rows_PDB_Rail_4_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
    {.name = "PDB_Rail_5_Power_Good",
     .commands = rows_PDB_Rail_5_Power_Good,
     .command_count = sizeof(rows_PDB_Rail_5_Power_Good) / sizeof(rows_PDB_Rail_5_Power_Good[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "PDB",
     .states = states_PDB_Rail_0_Power_Good,
     .state_count = sizeof(states_PDB_Rail_0_Power_Good) / sizeof(states_PDB_Rail_0_Power_Good[0])},
};

extern const struct nsm_fpga_part nsm_fpga_part_pdb;
const struct nsm_fpga_part nsm_fpga_part_pdb = {
    .rules = rules_pdb,
    .rule_count = sizeof(rules_pdb) / sizeof(rules_pdb[0]),
};
