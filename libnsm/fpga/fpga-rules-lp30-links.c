// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
/* clang-format off */
/* Generated from fpga-rules-lp30-links.rows by gen-fpga-rules.py, do not edit. */
#include "fpga-tables.h"

#include <math.h>

static const struct nsm_fpga_command rows_C2C_0_Lane_0_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 9, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_state_entry states_C2C_0_Lane_0_Pcs_Ready[] = {
    {8, 8, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
    {0, 0, NSM_FPGA_STATE_UNAVAILABLE_OFFLINE, NSM_FPGA_HEALTH_WARNING},
};

static const struct nsm_fpga_command rows_C2C_0_Lane_1_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 7, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_0_Lane_2_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 5, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_0_Lane_3_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 3, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_1_Lane_0_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 17, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_1_Lane_1_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 15, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_1_Lane_2_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 13, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_C2C_1_Lane_3_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 11, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_Ethernet_0_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_Ethernet_0_Local_Fault[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x100, 0}}, .expect = {.mask = 0xe0, .value = 0xa0}},
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x4200, 0}}, .expect = {.mask = 1, .value = 1}},
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x120, 0}}},
};

static const struct nsm_fpga_state_entry states_Ethernet_0_Local_Fault[] = {
    {1, 1, NSM_FPGA_STATE_FAULT, NSM_FPGA_HEALTH_CRITICAL},
    {0, 0, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
};

static const struct nsm_fpga_state_entry states_Ethernet_0_Remote_Fault[] = {
    {2, 2, NSM_FPGA_STATE_FAULT, NSM_FPGA_HEALTH_CRITICAL},
    {0, 0, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
};

static const struct nsm_fpga_command rows_Ethernet_0_High_Ber[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x100, 0}}, .expect = {.mask = 0xe0, .value = 0xa0}},
    {.op = NSM_FPGA_OP_READ32, .station = 19, .u = {.csr = {0x4084, 0}}},
};

static const struct nsm_fpga_command rows_Ethernet_1_Pcs_Ready[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x118, 0}}},
};

static const struct nsm_fpga_command rows_Ethernet_1_Local_Fault[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x100, 0}}, .expect = {.mask = 0xe0, .value = 0xa0}},
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x4200, 0}}, .expect = {.mask = 1, .value = 1}},
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x120, 0}}},
};

static const struct nsm_fpga_command rows_Ethernet_1_High_Ber[] = {
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x100, 0}}, .expect = {.mask = 0xe0, .value = 0xa0}},
    {.op = NSM_FPGA_OP_READ32, .station = 24, .u = {.csr = {0x4084, 0}}},
};

static const struct nsm_fpga_rule rules_links[] = {
    {.name = "C2C_0_Lane_0_Pcs_Ready",
     .commands = rows_C2C_0_Lane_0_Pcs_Ready,
     .command_count = sizeof(rows_C2C_0_Lane_0_Pcs_Ready) / sizeof(rows_C2C_0_Lane_0_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_0_Lane_1_Pcs_Ready",
     .commands = rows_C2C_0_Lane_1_Pcs_Ready,
     .command_count = sizeof(rows_C2C_0_Lane_1_Pcs_Ready) / sizeof(rows_C2C_0_Lane_1_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_0_Lane_2_Pcs_Ready",
     .commands = rows_C2C_0_Lane_2_Pcs_Ready,
     .command_count = sizeof(rows_C2C_0_Lane_2_Pcs_Ready) / sizeof(rows_C2C_0_Lane_2_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_0_Lane_3_Pcs_Ready",
     .commands = rows_C2C_0_Lane_3_Pcs_Ready,
     .command_count = sizeof(rows_C2C_0_Lane_3_Pcs_Ready) / sizeof(rows_C2C_0_Lane_3_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_1_Lane_0_Pcs_Ready",
     .commands = rows_C2C_1_Lane_0_Pcs_Ready,
     .command_count = sizeof(rows_C2C_1_Lane_0_Pcs_Ready) / sizeof(rows_C2C_1_Lane_0_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_1_Lane_1_Pcs_Ready",
     .commands = rows_C2C_1_Lane_1_Pcs_Ready,
     .command_count = sizeof(rows_C2C_1_Lane_1_Pcs_Ready) / sizeof(rows_C2C_1_Lane_1_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_1_Lane_2_Pcs_Ready",
     .commands = rows_C2C_1_Lane_2_Pcs_Ready,
     .command_count = sizeof(rows_C2C_1_Lane_2_Pcs_Ready) / sizeof(rows_C2C_1_Lane_2_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "C2C_1_Lane_3_Pcs_Ready",
     .commands = rows_C2C_1_Lane_3_Pcs_Ready,
     .command_count = sizeof(rows_C2C_1_Lane_3_Pcs_Ready) / sizeof(rows_C2C_1_Lane_3_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "C2C_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "Ethernet_0_Pcs_Ready",
     .commands = rows_Ethernet_0_Pcs_Ready,
     .command_count = sizeof(rows_Ethernet_0_Pcs_Ready) / sizeof(rows_Ethernet_0_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "Ethernet_0_Local_Fault",
     .commands = rows_Ethernet_0_Local_Fault,
     .command_count = sizeof(rows_Ethernet_0_Local_Fault) / sizeof(rows_Ethernet_0_Local_Fault[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Local_Fault,
     .state_count = sizeof(states_Ethernet_0_Local_Fault) / sizeof(states_Ethernet_0_Local_Fault[0])},
    {.name = "Ethernet_0_Remote_Fault",
     .commands = rows_Ethernet_0_Local_Fault,
     .command_count = sizeof(rows_Ethernet_0_Local_Fault) / sizeof(rows_Ethernet_0_Local_Fault[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Remote_Fault,
     .state_count = sizeof(states_Ethernet_0_Remote_Fault) / sizeof(states_Ethernet_0_Remote_Fault[0])},
    {.name = "Ethernet_0_High_Ber",
     .commands = rows_Ethernet_0_High_Ber,
     .command_count = sizeof(rows_Ethernet_0_High_Ber) / sizeof(rows_Ethernet_0_High_Ber[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Remote_Fault,
     .state_count = sizeof(states_Ethernet_0_Remote_Fault) / sizeof(states_Ethernet_0_Remote_Fault[0])},
    {.name = "Ethernet_1_Pcs_Ready",
     .commands = rows_Ethernet_1_Pcs_Ready,
     .command_count = sizeof(rows_Ethernet_1_Pcs_Ready) / sizeof(rows_Ethernet_1_Pcs_Ready[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_C2C_0_Lane_0_Pcs_Ready,
     .state_count = sizeof(states_C2C_0_Lane_0_Pcs_Ready) / sizeof(states_C2C_0_Lane_0_Pcs_Ready[0])},
    {.name = "Ethernet_1_Local_Fault",
     .commands = rows_Ethernet_1_Local_Fault,
     .command_count = sizeof(rows_Ethernet_1_Local_Fault) / sizeof(rows_Ethernet_1_Local_Fault[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Local_Fault,
     .state_count = sizeof(states_Ethernet_0_Local_Fault) / sizeof(states_Ethernet_0_Local_Fault[0])},
    {.name = "Ethernet_1_Remote_Fault",
     .commands = rows_Ethernet_1_Local_Fault,
     .command_count = sizeof(rows_Ethernet_1_Local_Fault) / sizeof(rows_Ethernet_1_Local_Fault[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Remote_Fault,
     .state_count = sizeof(states_Ethernet_0_Remote_Fault) / sizeof(states_Ethernet_0_Remote_Fault[0])},
    {.name = "Ethernet_1_High_Ber",
     .commands = rows_Ethernet_1_High_Ber,
     .command_count = sizeof(rows_Ethernet_1_High_Ber) / sizeof(rows_Ethernet_1_High_Ber[0]),
     .kind = NSM_FPGA_RULE_STATE,
     .group = "Ethernet_Link",
     .states = states_Ethernet_0_Remote_Fault,
     .state_count = sizeof(states_Ethernet_0_Remote_Fault) / sizeof(states_Ethernet_0_Remote_Fault[0])},
};

extern const struct nsm_fpga_part nsm_fpga_part_links;
const struct nsm_fpga_part nsm_fpga_part_links = {
    .rules = rules_links,
    .rule_count = sizeof(rules_links) / sizeof(rules_links[0]),
};
