// SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause)
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/dpll.yaml */
/* YNL-GEN kernel source */

#include <net/netlink.h>
#include <net/genetlink.h>

#include "dpll_nl.h"

#include <linux/dpll.h>

/* DPLL_CMD_DEVICE_GET - do */
static const struct nla_policy dpll_device_get_nl_policy[DPLL_A_BUS_NAME + 1] = {
	[DPLL_A_ID] = { .type = NLA_U32, },
	[DPLL_A_BUS_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_DEV_NAME] = { .type = NLA_NUL_STRING, },
};

/* DPLL_CMD_DEVICE_SET - do */
static const struct nla_policy dpll_device_set_nl_policy[DPLL_A_MODE + 1] = {
	[DPLL_A_ID] = { .type = NLA_U32, },
	[DPLL_A_BUS_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_DEV_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_MODE] = NLA_POLICY_MAX(NLA_U8, 4),
};

/* DPLL_CMD_PIN_GET - do */
static const struct nla_policy dpll_pin_get_do_nl_policy[DPLL_A_PIN_IDX + 1] = {
	[DPLL_A_ID] = { .type = NLA_U32, },
	[DPLL_A_BUS_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_DEV_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_PIN_IDX] = { .type = NLA_U32, },
};

/* DPLL_CMD_PIN_GET - dump */
static const struct nla_policy dpll_pin_get_dump_nl_policy[DPLL_A_BUS_NAME + 1] = {
	[DPLL_A_ID] = { .type = NLA_U32, },
	[DPLL_A_BUS_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_DEV_NAME] = { .type = NLA_NUL_STRING, },
};

/* DPLL_CMD_PIN_SET - do */
static const struct nla_policy dpll_pin_set_nl_policy[DPLL_A_PIN_PARENT_IDX + 1] = {
	[DPLL_A_ID] = { .type = NLA_U32, },
	[DPLL_A_BUS_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_DEV_NAME] = { .type = NLA_NUL_STRING, },
	[DPLL_A_PIN_IDX] = { .type = NLA_U32, },
	[DPLL_A_PIN_FREQUENCY] = { .type = NLA_U64, },
	[DPLL_A_PIN_DIRECTION] = NLA_POLICY_MAX(NLA_U8, 2),
	[DPLL_A_PIN_PRIO] = { .type = NLA_U32, },
	[DPLL_A_PIN_STATE] = NLA_POLICY_MAX(NLA_U8, 3),
	[DPLL_A_PIN_PARENT_IDX] = { .type = NLA_U32, },
};

/* Ops table for dpll */
static const struct genl_split_ops dpll_nl_ops[] = {
	{
		.cmd		= DPLL_CMD_DEVICE_GET,
		.pre_doit	= dpll_pre_doit,
		.doit		= dpll_nl_device_get_doit,
		.post_doit	= dpll_post_doit,
		.policy		= dpll_device_get_nl_policy,
		.maxattr	= DPLL_A_BUS_NAME,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd	= DPLL_CMD_DEVICE_GET,
		.start	= dpll_pre_dumpit,
		.dumpit	= dpll_nl_device_get_dumpit,
		.done	= dpll_post_dumpit,
		.flags	= GENL_ADMIN_PERM | GENL_CMD_CAP_DUMP,
	},
	{
		.cmd		= DPLL_CMD_DEVICE_SET,
		.pre_doit	= dpll_pre_doit,
		.doit		= dpll_nl_device_set_doit,
		.post_doit	= dpll_post_doit,
		.policy		= dpll_device_set_nl_policy,
		.maxattr	= DPLL_A_MODE,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd		= DPLL_CMD_PIN_GET,
		.pre_doit	= dpll_pin_pre_doit,
		.doit		= dpll_nl_pin_get_doit,
		.post_doit	= dpll_pin_post_doit,
		.policy		= dpll_pin_get_do_nl_policy,
		.maxattr	= DPLL_A_PIN_IDX,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
	{
		.cmd		= DPLL_CMD_PIN_GET,
		.start		= dpll_pin_pre_dumpit,
		.dumpit		= dpll_nl_pin_get_dumpit,
		.done		= dpll_pin_post_dumpit,
		.policy		= dpll_pin_get_dump_nl_policy,
		.maxattr	= DPLL_A_BUS_NAME,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DUMP,
	},
	{
		.cmd		= DPLL_CMD_PIN_SET,
		.pre_doit	= dpll_pin_pre_doit,
		.doit		= dpll_nl_pin_set_doit,
		.post_doit	= dpll_pin_post_doit,
		.policy		= dpll_pin_set_nl_policy,
		.maxattr	= DPLL_A_PIN_PARENT_IDX,
		.flags		= GENL_ADMIN_PERM | GENL_CMD_CAP_DO,
	},
};

static const struct genl_multicast_group dpll_nl_mcgrps[] = {
	[DPLL_NLGRP_MONITOR] = { "monitor", },
};

struct genl_family dpll_nl_family __ro_after_init = {
	.name		= DPLL_FAMILY_NAME,
	.version	= DPLL_FAMILY_VERSION,
	.netnsok	= true,
	.parallel_ops	= true,
	.module		= THIS_MODULE,
	.split_ops	= dpll_nl_ops,
	.n_split_ops	= ARRAY_SIZE(dpll_nl_ops),
	.mcgrps		= dpll_nl_mcgrps,
	.n_mcgrps	= ARRAY_SIZE(dpll_nl_mcgrps),
};
