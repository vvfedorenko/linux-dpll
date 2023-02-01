/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/* Do not edit directly, auto-generated from: */
/*	Documentation/netlink/specs/dpll.yaml */
/* YNL-GEN kernel header */

#ifndef _LINUX_DPLL_GEN_H
#define _LINUX_DPLL_GEN_H

#include <net/netlink.h>
#include <net/genetlink.h>

#include <linux/dpll.h>

int dpll_pre_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		  struct genl_info *info);
int dpll_pin_pre_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		      struct genl_info *info);
void
dpll_post_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
	       struct genl_info *info);
void
dpll_pin_post_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		   struct genl_info *info);
int dpll_pre_dumpit(struct netlink_callback *cb);
int dpll_pin_pre_dumpit(struct netlink_callback *cb);
int dpll_post_dumpit(struct netlink_callback *cb);
int dpll_pin_post_dumpit(struct netlink_callback *cb);

int dpll_nl_device_get_doit(struct sk_buff *skb, struct genl_info *info);
int dpll_nl_device_get_dumpit(struct sk_buff *skb, struct netlink_callback *cb);
int dpll_nl_device_set_doit(struct sk_buff *skb, struct genl_info *info);
int dpll_nl_pin_get_doit(struct sk_buff *skb, struct genl_info *info);
int dpll_nl_pin_get_dumpit(struct sk_buff *skb, struct netlink_callback *cb);
int dpll_nl_pin_set_doit(struct sk_buff *skb, struct genl_info *info);

enum {
	DPLL_NLGRP_MONITOR,
};

extern struct genl_family dpll_nl_family;

#endif /* _LINUX_DPLL_GEN_H */
