// SPDX-License-Identifier: GPL-2.0
/*
 * Generic netlink for DPLL management framework
 *
 * Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/genetlink.h>
#include "dpll_core.h"

#include <uapi/linux/dpll.h>

static const struct genl_multicast_group dpll_mcgrps[] = {
	{ .name = DPLL_MONITOR_GROUP_NAME,  },
};

static const struct nla_policy dpll_cmd_device_get_policy[] = {
	[DPLLA_ID]		= { .type = NLA_U32 },
	[DPLLA_NAME]		= { .type = NLA_STRING,
				    .len = DPLL_NAME_LEN },
	[DPLLA_FILTER]		= { .type = NLA_U32 },
};

static const struct nla_policy dpll_cmd_device_set_policy[] = {
	[DPLLA_ID]		= { .type = NLA_U32 },
	[DPLLA_NAME]		= { .type = NLA_STRING,
				    .len = DPLL_NAME_LEN },
	[DPLLA_MODE]		= { .type = NLA_U32 },
	[DPLLA_SOURCE_PIN_IDX]	= { .type = NLA_U32 },
};

static const struct nla_policy dpll_cmd_pin_set_policy[] = {
	[DPLLA_ID]		= { .type = NLA_U32 },
	[DPLLA_PIN_IDX]		= { .type = NLA_U32 },
	[DPLLA_PIN_SIGNAL_TYPE]	= { .type = NLA_U32 },
	[DPLLA_PIN_CUSTOM_FREQ] = { .type = NLA_U32 },
	[DPLLA_PIN_MODE]	= { .type = NLA_U32 },
	[DPLLA_PIN_PRIO]	= { .type = NLA_U32 },
};

struct dpll_dump_ctx {
	int dump_filter;
};

static struct genl_family dpll_gnl_family;

static struct dpll_dump_ctx *dpll_dump_context(struct netlink_callback *cb)
{
	return (struct dpll_dump_ctx *)cb->ctx;
}

static int dpll_msg_add_id(struct sk_buff *msg, u32 id)
{
	if (nla_put_u32(msg, DPLLA_ID, id))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_name(struct sk_buff *msg, const char *name)
{
	if (nla_put_string(msg, DPLLA_NAME, name))
		return -EMSGSIZE;

	return 0;
}

static int __dpll_msg_add_mode(struct sk_buff *msg, enum dplla msg_type,
			       enum dpll_mode mode)
{
	if (nla_put_s32(msg, msg_type, mode))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_mode(struct sk_buff *msg, const struct dpll_device *dpll)
{
	enum dpll_mode m;
	int ret;

	if (!dpll->ops->mode_get)
		return 0;
	ret = dpll->ops->mode_get(dpll, &m);
	if (ret)
		return ret;

	return __dpll_msg_add_mode(msg, DPLLA_MODE, m);
}

static int
dpll_msg_add_modes_supported(struct sk_buff *msg,
			     const struct dpll_device *dpll)
{
	enum dpll_mode i;
	int ret = 0;

	if (!dpll->ops->mode_supported)
		return ret;

	for (i = DPLL_MODE_UNSPEC + 1; i <= DPLL_MODE_MAX; i++) {
		if (dpll->ops->mode_supported(dpll, i)) {
			ret = __dpll_msg_add_mode(msg, DPLLA_MODE_SUPPORTED, i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int dpll_msg_add_clock_id(struct sk_buff *msg,
				 const struct dpll_device *dpll)
{
	if (nla_put_64bit(msg, DPLLA_CLOCK_ID, sizeof(dpll->clock_id),
			  &dpll->clock_id, 0))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_clock_class(struct sk_buff *msg,
				    const struct dpll_device *dpll)
{
	if (nla_put_s32(msg, DPLLA_CLOCK_CLASS, dpll->clock_class))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_source_pin(struct sk_buff *msg, struct dpll_device *dpll)
{
	u32 source_idx;
	int ret;

	if (!dpll->ops->source_pin_idx_get)
		return 0;
	ret = dpll->ops->source_pin_idx_get(dpll, &source_idx);
	if (ret)
		return ret;
	if (nla_put_u32(msg, DPLLA_SOURCE_PIN_IDX, source_idx))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_lock_status(struct sk_buff *msg, struct dpll_device *dpll)
{
	enum dpll_lock_status s;
	int ret;

	if (!dpll->ops->lock_status_get)
		return 0;
	ret = dpll->ops->lock_status_get(dpll, &s);
	if (ret)
		return ret;
	if (nla_put_s32(msg, DPLLA_LOCK_STATUS, s))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_temp(struct sk_buff *msg, struct dpll_device *dpll)
{
	s32 temp;
	int ret;

	if (!dpll->ops->temp_get)
		return 0;
	ret = dpll->ops->temp_get(dpll, &temp);
	if (ret)
		return ret;
	if (nla_put_u32(msg, DPLLA_TEMP, temp))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_pin_idx(struct sk_buff *msg, u32 pin_idx)
{
	if (nla_put_u32(msg, DPLLA_PIN_IDX, pin_idx))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_pin_description(struct sk_buff *msg,
					const char *description)
{
	if (nla_put_string(msg, DPLLA_PIN_DESCRIPTION, description))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_pin_parent_idx(struct sk_buff *msg, u32 parent_idx)
{
	if (nla_put_u32(msg, DPLLA_PIN_PARENT_IDX, parent_idx))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_type(struct sk_buff *msg, const struct dpll_device *dpll,
		      const struct dpll_pin *pin)
{
	enum dpll_pin_type t;

	if (dpll_pin_type_get(dpll, pin, &t))
		return 0;

	if (nla_put_s32(msg, DPLLA_PIN_TYPE, t))
		return -EMSGSIZE;

	return 0;
}

static int __dpll_msg_add_pin_signal_type(struct sk_buff *msg,
					  enum dplla attr,
					  enum dpll_pin_signal_type type)
{
	if (nla_put_s32(msg, attr, type))
		return -EMSGSIZE;

	return 0;
}

static int dpll_msg_add_pin_signal_type(struct sk_buff *msg,
					const struct dpll_device *dpll,
					const struct dpll_pin *pin)
{
	enum dpll_pin_signal_type t;
	int ret;

	if (dpll_pin_signal_type_get(dpll, pin, &t))
		return 0;
	ret = __dpll_msg_add_pin_signal_type(msg, DPLLA_PIN_SIGNAL_TYPE, t);
	if (ret)
		return ret;

	if (t == DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ) {
		u32 freq;

		if (dpll_pin_custom_freq_get(dpll, pin, &freq))
			return 0;
		if (nla_put_u32(msg, DPLLA_PIN_CUSTOM_FREQ, freq))
			return -EMSGSIZE;
	}

	return 0;
}

static int
dpll_msg_add_pin_signal_types_supported(struct sk_buff *msg,
					const struct dpll_device *dpll,
					const struct dpll_pin *pin)
{
	const enum dplla da = DPLLA_PIN_SIGNAL_TYPE_SUPPORTED;
	enum dpll_pin_signal_type i;
	bool supported;

	for (i = DPLL_PIN_SIGNAL_TYPE_UNSPEC + 1;
	     i <= DPLL_PIN_SIGNAL_TYPE_MAX; i++) {
		if (dpll_pin_signal_type_supported(dpll, pin, i, &supported))
			continue;
		if (supported) {
			int ret = __dpll_msg_add_pin_signal_type(msg, da, i);

			if (ret)
				return ret;
		}
	}

	return 0;
}

static int dpll_msg_add_pin_modes(struct sk_buff *msg,
				   const struct dpll_device *dpll,
				   const struct dpll_pin *pin)
{
	enum dpll_pin_mode i;
	bool active;

	for (i = DPLL_PIN_MODE_UNSPEC + 1; i <= DPLL_PIN_MODE_MAX; i++) {
		if (dpll_pin_mode_active(dpll, pin, i, &active))
			return 0;
		if (active)
			if (nla_put_s32(msg, DPLLA_PIN_MODE, i))
				return -EMSGSIZE;
	}

	return 0;
}

static int dpll_msg_add_pin_modes_supported(struct sk_buff *msg,
					     const struct dpll_device *dpll,
					     const struct dpll_pin *pin)
{
	enum dpll_pin_mode i;
	bool supported;

	for (i = DPLL_PIN_MODE_UNSPEC + 1; i <= DPLL_PIN_MODE_MAX; i++) {
		if (dpll_pin_mode_supported(dpll, pin, i, &supported))
			return 0;
		if (supported)
			if (nla_put_s32(msg, DPLLA_PIN_MODE_SUPPORTED, i))
				return -EMSGSIZE;
	}

	return 0;
}

static int
dpll_msg_add_pin_prio(struct sk_buff *msg, const struct dpll_device *dpll,
		      const struct dpll_pin *pin)
{
	u32 prio;

	if (dpll_pin_prio_get(dpll, pin, &prio))
		return 0;
	if (nla_put_u32(msg, DPLLA_PIN_PRIO, prio))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_netifindex(struct sk_buff *msg, const struct dpll_device *dpll,
			    const struct dpll_pin *pin)
{
	int netifindex;

	if (dpll_pin_netifindex_get(dpll, pin, &netifindex))
		return 0;
	if (nla_put_s32(msg, DPLLA_PIN_NETIFINDEX, netifindex))
		return -EMSGSIZE;

	return 0;
}

static int
__dpll_cmd_device_dump_one(struct sk_buff *msg, struct dpll_device *dpll)
{
	int ret = dpll_msg_add_id(msg, dpll_id(dpll));

	if (ret)
		return ret;
	ret = dpll_msg_add_name(msg, dpll_dev_name(dpll));

	return ret;
}

static int
__dpll_cmd_pin_dump_one(struct sk_buff *msg, struct dpll_device *dpll,
			struct dpll_pin *pin)
{
	struct dpll_pin *parent = NULL;
	int ret;

	ret = dpll_msg_add_pin_idx(msg, dpll_pin_idx(dpll, pin));
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_description(msg, dpll_pin_description(pin));
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_type(msg, dpll, pin);
	if (ret)
		return ret;
	parent = dpll_pin_parent(pin);
	if (parent) {
		ret = dpll_msg_add_pin_parent_idx(msg, dpll_pin_idx(dpll,
								    parent));
		if (ret)
			return ret;
	}
	ret = dpll_msg_add_pin_signal_type(msg, dpll, pin);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_signal_types_supported(msg, dpll, pin);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_modes(msg, dpll, pin);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_modes_supported(msg, dpll, pin);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_prio(msg, dpll, pin);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_netifindex(msg, dpll, pin);

	return ret;
}

static int __dpll_cmd_dump_pins(struct sk_buff *msg, struct dpll_device *dpll)
{
	struct dpll_pin *pin;
	struct nlattr *attr;
	unsigned long i;
	int ret = 0;

	for_each_pin_on_dpll(dpll, pin, i) {
		attr = nla_nest_start(msg, DPLLA_PIN);
		if (!attr) {
			ret = -EMSGSIZE;
			goto nest_cancel;
		}
		ret = __dpll_cmd_pin_dump_one(msg, dpll, pin);
		if (ret)
			goto nest_cancel;
		nla_nest_end(msg, attr);
	}

	return ret;

nest_cancel:
	nla_nest_cancel(msg, attr);
	return ret;
}

static int
__dpll_cmd_dump_status(struct sk_buff *msg, struct dpll_device *dpll)
{
	int ret = dpll_msg_add_source_pin(msg, dpll);

	if (ret)
		return ret;
	ret = dpll_msg_add_temp(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_lock_status(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_mode(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_modes_supported(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_clock_id(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_clock_class(msg, dpll);

	return ret;
}

static int
dpll_device_dump_one(struct dpll_device *dpll, struct sk_buff *msg,
		     int dump_filter)
{
	int ret;

	dpll_lock(dpll);
	ret = __dpll_cmd_device_dump_one(msg, dpll);
	if (ret)
		goto out_unlock;

	if (dump_filter & DPLL_FILTER_STATUS) {
		ret = __dpll_cmd_dump_status(msg, dpll);
		if (ret) {
			if (ret != -EMSGSIZE)
				ret = -EAGAIN;
			goto out_unlock;
		}
	}
	if (dump_filter & DPLL_FILTER_PINS)
		ret = __dpll_cmd_dump_pins(msg, dpll);
	dpll_unlock(dpll);

	return ret;
out_unlock:
	dpll_unlock(dpll);
	return ret;
}

static int
dpll_pin_set_from_nlattr(struct dpll_device *dpll,
			 struct dpll_pin *pin, struct genl_info *info)
{
	enum dpll_pin_signal_type st;
	enum dpll_pin_mode mode;
	struct nlattr *a;
	int rem, ret = 0;
	u32 prio, freq;

	nla_for_each_attr(a, genlmsg_data(info->genlhdr),
			  genlmsg_len(info->genlhdr), rem) {
		switch (nla_type(a)) {
		case DPLLA_PIN_SIGNAL_TYPE:
			st = nla_get_s32(a);
			ret = dpll_pin_signal_type_set(dpll, pin, st);
			if (ret)
				return ret;
			break;
		case DPLLA_PIN_CUSTOM_FREQ:
			freq = nla_get_u32(a);
			ret = dpll_pin_custom_freq_set(dpll, pin, freq);
			if (ret)
				return ret;
			break;
		case DPLLA_PIN_MODE:
			mode = nla_get_s32(a);
			ret = dpll_pin_mode_set(dpll, pin, mode);
			if (ret)
				return ret;
			break;
		case DPLLA_PIN_PRIO:
			prio = nla_get_u32(a);
			ret = dpll_pin_prio_set(dpll, pin, prio);
			if (ret)
				return ret;
			break;
		default:
			break;
		}
	}

	return ret;
}

static int dpll_cmd_pin_set(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];
	struct nlattr **attrs = info->attrs;
	struct dpll_pin *pin;
	int pin_id;

	if (!attrs[DPLLA_PIN_IDX])
		return -EINVAL;
	pin_id = nla_get_u32(attrs[DPLLA_PIN_IDX]);
	dpll_lock(dpll);
	pin = dpll_pin_get_by_idx(dpll, pin_id);
	dpll_unlock(dpll);
	if (!pin) {
		return -ENODEV;
	}
	return dpll_pin_set_from_nlattr(dpll, pin, info);
}

enum dpll_mode dpll_msg_read_mode(struct nlattr *a)
{
	return nla_get_s32(a);
}

u32 dpll_msg_read_source_pin_id(struct nlattr *a)
{
	return nla_get_u32(a);
}

static int
dpll_set_from_nlattr(struct dpll_device *dpll, struct genl_info *info)
{
	enum dpll_mode m;
	struct nlattr *a;
	int rem, ret = 0;
	u32 source_pin;

	nla_for_each_attr(a, genlmsg_data(info->genlhdr),
			  genlmsg_len(info->genlhdr), rem) {
		switch (nla_type(a)) {
		case DPLLA_MODE:
			m = dpll_msg_read_mode(a);

			ret = dpll_mode_set(dpll, m);
			if (ret)
				return ret;
			break;
		case DPLLA_SOURCE_PIN_IDX:
			source_pin = dpll_msg_read_source_pin_id(a);

			ret = dpll_source_idx_set(dpll, source_pin);
			if (ret)
				return ret;
			break;
		default:
			break;
		}
	}

	return ret;
}

static int dpll_cmd_device_set(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];

	return dpll_set_from_nlattr(dpll, info);
}

static int
dpll_cmd_device_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct dpll_dump_ctx *ctx = dpll_dump_context(cb);
	struct dpll_device *dpll;
	struct nlattr *hdr;
	unsigned long i;
	int ret;

	hdr = genlmsg_put(skb, NETLINK_CB(cb->skb).portid, cb->nlh->nlmsg_seq,
			  &dpll_gnl_family, 0, DPLL_CMD_DEVICE_GET);
	if (!hdr)
		return -EMSGSIZE;

	for_each_dpll(dpll, i) {
		ret = dpll_device_dump_one(dpll, skb, ctx->dump_filter);
		if (ret)
			break;
	}

	if (ret)
		genlmsg_cancel(skb, hdr);
	else
		genlmsg_end(skb, hdr);

	return ret;
}

static int dpll_cmd_device_get(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];
	struct nlattr **attrs = info->attrs;
	struct sk_buff *msg;
	int dump_filter = 0;
	struct nlattr *hdr;
	int ret;

	if (attrs[DPLLA_FILTER])
		dump_filter = nla_get_s32(attrs[DPLLA_FILTER]);

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	hdr = genlmsg_put_reply(msg, info, &dpll_gnl_family, 0,
				DPLL_CMD_DEVICE_GET);
	if (!hdr)
		return -EMSGSIZE;

	ret = dpll_device_dump_one(dpll, msg, dump_filter);
	if (ret)
		goto out_free_msg;
	genlmsg_end(msg, hdr);

	return genlmsg_reply(msg, info);

out_free_msg:
	nlmsg_free(msg);
	return ret;

}

static int dpll_cmd_device_get_start(struct netlink_callback *cb)
{
	const struct genl_dumpit_info *info = genl_dumpit_info(cb);
	struct dpll_dump_ctx *ctx = dpll_dump_context(cb);
	struct nlattr *attr = info->attrs[DPLLA_FILTER];

	if (attr)
		ctx->dump_filter = nla_get_s32(attr);
	else
		ctx->dump_filter = 0;

	return 0;
}

static int dpll_pre_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
			 struct genl_info *info)
{
	struct dpll_device *dpll_id = NULL, *dpll_name = NULL;

	if (!info->attrs[DPLLA_ID] &&
	    !info->attrs[DPLLA_NAME])
		return -EINVAL;

	if (info->attrs[DPLLA_ID]) {
		u32 id = nla_get_u32(info->attrs[DPLLA_ID]);

		dpll_id = dpll_device_get_by_id(id);
		if (!dpll_id)
			return -ENODEV;
		info->user_ptr[0] = dpll_id;
	}
	if (info->attrs[DPLLA_NAME]) {
		const char *name = nla_data(info->attrs[DPLLA_NAME]);

		dpll_name = dpll_device_get_by_name(name);
		if (!dpll_name)
			return -ENODEV;

		if (dpll_id && dpll_name != dpll_id)
			return -EINVAL;
		info->user_ptr[0] = dpll_name;
	}

	return 0;
}

static const struct genl_ops dpll_ops[] = {
	{
		.cmd	= DPLL_CMD_DEVICE_GET,
		.flags  = GENL_UNS_ADMIN_PERM,
		.start	= dpll_cmd_device_get_start,
		.dumpit	= dpll_cmd_device_dump,
		.doit	= dpll_cmd_device_get,
		.policy	= dpll_cmd_device_get_policy,
		.maxattr = ARRAY_SIZE(dpll_cmd_device_get_policy) - 1,
	},
	{
		.cmd	= DPLL_CMD_DEVICE_SET,
		.flags	= GENL_UNS_ADMIN_PERM,
		.doit	= dpll_cmd_device_set,
		.policy	= dpll_cmd_device_set_policy,
		.maxattr = ARRAY_SIZE(dpll_cmd_device_set_policy) - 1,
	},
	{
		.cmd	= DPLL_CMD_PIN_SET,
		.flags	= GENL_UNS_ADMIN_PERM,
		.doit	= dpll_cmd_pin_set,
		.policy	= dpll_cmd_pin_set_policy,
		.maxattr = ARRAY_SIZE(dpll_cmd_pin_set_policy) - 1,
	},
};

static struct genl_family dpll_family __ro_after_init = {
	.hdrsize	= 0,
	.name		= DPLL_FAMILY_NAME,
	.version	= DPLL_VERSION,
	.ops		= dpll_ops,
	.n_ops		= ARRAY_SIZE(dpll_ops),
	.mcgrps		= dpll_mcgrps,
	.n_mcgrps	= ARRAY_SIZE(dpll_mcgrps),
	.pre_doit	= dpll_pre_doit,
	.parallel_ops   = true,
};

static int dpll_event_device_id(struct sk_buff *msg, struct dpll_device *dpll)
{
	int ret = dpll_msg_add_id(msg, dpll_id(dpll));

	if (ret)
		return ret;
	ret = dpll_msg_add_name(msg, dpll_dev_name(dpll));

	return ret;
}

static int dpll_event_device_change(struct sk_buff *msg,
				    struct dpll_device *dpll,
				    struct dpll_pin *pin,
				    enum dpll_event_change event)
{
	int ret = dpll_msg_add_id(msg, dpll_id(dpll));

	if (ret)
		return ret;
	ret = nla_put_s32(msg, DPLLA_CHANGE_TYPE, event);
	if (ret)
		return ret;
	switch (event)	{
	case DPLL_CHANGE_PIN_ADD:
	case DPLL_CHANGE_PIN_SIGNAL_TYPE:
	case DPLL_CHANGE_PIN_MODE:
	case DPLL_CHANGE_PIN_PRIO:
		ret = dpll_msg_add_pin_idx(msg, dpll_pin_idx(dpll, pin));
		break;
	default:
		break;
	}

	return ret;
}

/*
 * Generic netlink DPLL event encoding
 */
static int dpll_send_event_create(enum dpll_event event,
				  struct dpll_device *dpll)
{
	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, 0, &dpll_family, 0, event);
	if (!hdr)
		goto out_free_msg;

	ret = dpll_event_device_id(msg, dpll);
	if (ret)
		goto out_cancel_msg;
	genlmsg_end(msg, hdr);
	genlmsg_multicast(&dpll_family, msg, 0, 0, GFP_KERNEL);

	return 0;

out_cancel_msg:
	genlmsg_cancel(msg, hdr);
out_free_msg:
	nlmsg_free(msg);

	return ret;
}

/*
 * Generic netlink DPLL event encoding
 */
static int dpll_send_event_change(struct dpll_device *dpll,
				  struct dpll_pin *pin,
				  enum dpll_event_change event)
{
	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, 0, &dpll_family, 0, DPLL_EVENT_DEVICE_CHANGE);
	if (!hdr)
		goto out_free_msg;

	ret = dpll_event_device_change(msg, dpll, pin, event);
	if (ret)
		goto out_cancel_msg;
	genlmsg_end(msg, hdr);
	genlmsg_multicast(&dpll_family, msg, 0, 0, GFP_KERNEL);

	return 0;

out_cancel_msg:
	genlmsg_cancel(msg, hdr);
out_free_msg:
	nlmsg_free(msg);

	return ret;
}

int dpll_notify_device_create(struct dpll_device *dpll)
{
	return dpll_send_event_create(DPLL_EVENT_DEVICE_CREATE, dpll);
}

int dpll_notify_device_delete(struct dpll_device *dpll)
{
	return dpll_send_event_create(DPLL_EVENT_DEVICE_DELETE, dpll);
}

int dpll_device_notify(struct dpll_device *dpll, enum dpll_event_change event)
{
	return dpll_send_event_change(dpll, NULL, event);
}
EXPORT_SYMBOL_GPL(dpll_device_notify);

int dpll_pin_notify(struct dpll_device *dpll, struct dpll_pin *pin,
		    enum dpll_event_change event)
{
	return dpll_send_event_change(dpll, pin, event);
}
EXPORT_SYMBOL_GPL(dpll_pin_notify);

int __init dpll_netlink_init(void)
{
	return genl_register_family(&dpll_family);
}

void dpll_netlink_finish(void)
{
	genl_unregister_family(&dpll_family);
}

void __exit dpll_netlink_fini(void)
{
	dpll_netlink_finish();
}
