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
#include "dpll_nl.h"
#include <uapi/linux/dpll.h>

struct dpll_dump_ctx {
	unsigned long idx;
};

static struct dpll_dump_ctx *dpll_dump_context(struct netlink_callback *cb)
{
	return (struct dpll_dump_ctx *)cb->ctx;
}

static int
dpll_msg_add_dev_handle(struct sk_buff *msg, struct dpll_device *dpll)
{
	if (nla_put_u32(msg, DPLL_A_ID, dpll->id))
		return -EMSGSIZE;
	if (nla_put_string(msg, DPLL_A_BUS_NAME, dev_bus_name(&dpll->dev)))
		return -EMSGSIZE;
	if (nla_put_string(msg, DPLL_A_DEV_NAME, dev_name(&dpll->dev)))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_mode(struct sk_buff *msg, struct dpll_device *dpll,
		  struct netlink_ext_ack *extack)
{
	const struct dpll_device_ops *ops = dpll_device_ops(dpll);
	enum dpll_mode mode;

	if (WARN_ON(!ops->mode_get))
		return -EOPNOTSUPP;
	if (ops->mode_get(dpll, dpll_priv(dpll), &mode, extack))
		return -EFAULT;
	if (nla_put_u8(msg, DPLL_A_MODE, mode))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_lock_status(struct sk_buff *msg, struct dpll_device *dpll,
			 struct netlink_ext_ack *extack)
{
	const struct dpll_device_ops *ops = dpll_device_ops(dpll);
	enum dpll_lock_status status;

	if (WARN_ON(!ops->lock_status_get))
		return -EOPNOTSUPP;
	if (ops->lock_status_get(dpll, dpll_priv(dpll), &status, extack))
		return -EFAULT;
	if (nla_put_u8(msg, DPLL_A_LOCK_STATUS, status))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_temp(struct sk_buff *msg, struct dpll_device *dpll,
		  struct netlink_ext_ack *extack)
{
	const struct dpll_device_ops *ops = dpll_device_ops(dpll);
	s32 temp;

	if (!ops->temp_get)
		return -EOPNOTSUPP;
	if (ops->temp_get(dpll, dpll_priv(dpll), &temp, extack))
		return -EFAULT;
	if (nla_put_s32(msg, DPLL_A_TEMP, temp))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_prio(struct sk_buff *msg, const struct dpll_pin *pin,
		      struct dpll_pin_ref *ref,
		      struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
	const struct dpll_device *dpll = ref->dpll;
	u32 prio;

	if (!ops->prio_get)
		return -EOPNOTSUPP;
	if (ops->prio_get(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
			  dpll_priv(dpll), &prio, extack))
		return -EFAULT;
	if (nla_put_u32(msg, DPLL_A_PIN_PRIO, prio))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_on_dpll_state(struct sk_buff *msg, const struct dpll_pin *pin,
			       struct dpll_pin_ref *ref,
			       struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
	const struct dpll_device *dpll = ref->dpll;
	enum dpll_pin_state state;

	if (!ops->state_on_dpll_get)
		return -EOPNOTSUPP;
	if (ops->state_on_dpll_get(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
				   dpll_priv(dpll), &state, extack))
		return -EFAULT;
	if (nla_put_u8(msg, DPLL_A_PIN_STATE, state))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_direction(struct sk_buff *msg, const struct dpll_pin *pin,
			   struct dpll_pin_ref *ref,
			   struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
	const struct dpll_device *dpll = ref->dpll;
	enum dpll_pin_direction direction;

	if (!ops->direction_get)
		return -EOPNOTSUPP;
	if (ops->direction_get(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
			       dpll_priv(dpll), &direction, extack))
		return -EFAULT;
	if (nla_put_u8(msg, DPLL_A_PIN_DIRECTION, direction))
		return -EMSGSIZE;

	return 0;
}

static int
dpll_msg_add_pin_freq(struct sk_buff *msg, const struct dpll_pin *pin,
		      struct dpll_pin_ref *ref, struct netlink_ext_ack *extack,
		      bool dump_freq_supported)
{
	const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
	const struct dpll_device *dpll = ref->dpll;
	struct nlattr *nest;
	u64 freq;
	int fs;

	if (!ops->frequency_get)
		return -EOPNOTSUPP;
	if (ops->frequency_get(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
			       dpll_priv(dpll), &freq, extack))
		return -EFAULT;
	if (nla_put_64bit(msg, DPLL_A_PIN_FREQUENCY, sizeof(freq), &freq, 0))
		return -EMSGSIZE;
	if (!dump_freq_supported)
		return 0;
	for (fs = 0; fs < pin->prop.freq_supported_num; fs++) {
		nest = nla_nest_start(msg, DPLL_A_PIN_FREQUENCY_SUPPORTED);
		if (!nest)
			return -EMSGSIZE;
		freq = pin->prop.freq_supported[fs].min;
		if (nla_put_64bit(msg, DPLL_A_PIN_FREQUENCY_MIN, sizeof(freq),
				   &freq, 0)) {
			nla_nest_cancel(msg, nest);
			return -EMSGSIZE;
		}
		freq = pin->prop.freq_supported[fs].max;
		if (nla_put_64bit(msg, DPLL_A_PIN_FREQUENCY_MAX, sizeof(freq),
				   &freq, 0)) {
			nla_nest_cancel(msg, nest);
			return -EMSGSIZE;
		}
		nla_nest_end(msg, nest);
	}

	return 0;
}

static int
dpll_msg_add_pin_parents(struct sk_buff *msg, struct dpll_pin *pin,
			 struct dpll_pin_ref *dpll_ref,
			 struct netlink_ext_ack *extack)
{
	enum dpll_pin_state state;
	struct dpll_pin_ref *ref;
	struct dpll_pin *ppin;
	struct nlattr *nest;
	unsigned long index;
	int ret;

	xa_for_each(&pin->parent_refs, index, ref) {
		const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
		void *parent_priv;

		ppin = ref->pin;
		parent_priv = dpll_pin_on_dpll_priv(dpll_ref->dpll, ppin);
		if (WARN_ON(!ops->state_on_pin_get))
			return -EFAULT;
		ret = ops->state_on_pin_get(pin,
					    dpll_pin_on_pin_priv(ppin, pin),
					    ppin, parent_priv, &state, extack);
		if (ret)
			return -EFAULT;
		nest = nla_nest_start(msg, DPLL_A_PIN_PARENT);
		if (!nest)
			return -EMSGSIZE;
		if (nla_put_u32(msg, DPLL_A_PIN_PARENT_IDX, ppin->pin_idx)) {
			ret = -EMSGSIZE;
			goto nest_cancel;
		}
		if (nla_put_u8(msg, DPLL_A_PIN_STATE, state)) {
			ret = -EMSGSIZE;
			goto nest_cancel;
		}
		nla_nest_end(msg, nest);
	}

	return 0;

nest_cancel:
	nla_nest_cancel(msg, nest);
	return ret;
}

static int
dpll_msg_add_pin_dplls(struct sk_buff *msg, struct dpll_pin *pin,
		       struct netlink_ext_ack *extack)
{
	struct dpll_pin_ref *ref;
	struct nlattr *attr;
	unsigned long index;
	int ret;

	xa_for_each(&pin->dpll_refs, index, ref) {
		attr = nla_nest_start(msg, DPLL_A_DEVICE);
		if (!attr)
			return -EMSGSIZE;
		ret = dpll_msg_add_dev_handle(msg, ref->dpll);
		if (ret)
			goto nest_cancel;
		ret = dpll_msg_add_pin_on_dpll_state(msg, pin, ref, extack);
		if (ret && ret != -EOPNOTSUPP)
			goto nest_cancel;
		ret = dpll_msg_add_pin_prio(msg, pin, ref, extack);
		if (ret && ret != -EOPNOTSUPP)
			goto nest_cancel;
		ret = dpll_msg_add_pin_direction(msg, pin, ref, extack);
		if (ret)
			goto nest_cancel;
		nla_nest_end(msg, attr);
	}

	return 0;

nest_cancel:
	nla_nest_end(msg, attr);
	return ret;
}

static int
dpll_cmd_pin_fill_details(struct sk_buff *msg, struct dpll_pin *pin,
			  struct dpll_pin_ref *ref, struct netlink_ext_ack *extack)
{
	int ret;

	ret = dpll_msg_add_pin_handle(msg, pin);
	if (ret)
		return ret;
	if (nla_put_string(msg, DPLL_A_PIN_LABEL, pin->prop.label))
		return -EMSGSIZE;
	if (nla_put_u8(msg, DPLL_A_PIN_TYPE, pin->prop.type))
		return -EMSGSIZE;
	if (nla_put_u32(msg, DPLL_A_PIN_DPLL_CAPS, pin->prop.capabilities))
		return -EMSGSIZE;
	ret = dpll_msg_add_pin_freq(msg, pin, ref, extack, true);
	if (ret && ret != -EOPNOTSUPP)
		return ret;
	if (pin->rclk_dev_name)
		if (nla_put_string(msg, DPLL_A_PIN_RCLK_DEVICE,
				   pin->rclk_dev_name))
			return -EMSGSIZE;
	return 0;
}

size_t dpll_msg_pin_handle_size(struct dpll_pin *pin)
{
	// TMP- THE HANDLE IS GOING TO CHANGE TO DRIVERNAME/CLOCKID/PIN_INDEX
	// LEAVING ORIG HANDLE NOW AS PUT IN THE LAST RFC VERSION
	return nla_total_size(4); /* DPLL_A_PIN_IDX */
}
EXPORT_SYMBOL_GPL(dpll_msg_pin_handle_size);

int dpll_msg_add_pin_handle(struct sk_buff *msg, struct dpll_pin *pin)
{
	// TMP- THE HANDLE IS GOING TO CHANGE TO DRIVERNAME/CLOCKID/PIN_INDEX
	// LEAVING ORIG HANDLE NOW AS PUT IN THE LAST RFC VERSION
	if (nla_put_u32(msg, DPLL_A_PIN_IDX, pin->pin_idx))
		return -EMSGSIZE;
	return 0;
}
EXPORT_SYMBOL_GPL(dpll_msg_add_pin_handle);

static int
__dpll_cmd_pin_dump_one(struct sk_buff *msg, struct dpll_pin *pin,
			struct netlink_ext_ack *extack)
{
	struct dpll_pin_ref *ref;
	int ret;

	ref = dpll_xa_ref_dpll_first(&pin->dpll_refs);
	if (!ref)
		return -EFAULT;
	ret = dpll_cmd_pin_fill_details(msg, pin, ref, extack);
	if (ret)
		return ret;
	ret = dpll_msg_add_pin_parents(msg, pin, ref, extack);
	if (ret)
		return ret;
	if (!xa_empty(&pin->dpll_refs)) {
		ret = dpll_msg_add_pin_dplls(msg, pin, extack);
		if (ret)
			return ret;
	}

	return 0;
}

static int
dpll_device_get_one(struct dpll_device *dpll, struct sk_buff *msg,
		    struct netlink_ext_ack *extack)
{
	enum dpll_mode mode;
	int ret;

	ret = dpll_msg_add_dev_handle(msg, dpll);
	if (ret)
		return ret;
	ret = dpll_msg_add_temp(msg, dpll, extack);
	if (ret && ret != -EOPNOTSUPP)
		return ret;
	ret = dpll_msg_add_lock_status(msg, dpll, extack);
	if (ret)
		return ret;
	ret = dpll_msg_add_mode(msg, dpll, extack);
	if (ret)
		return ret;
	for (mode = DPLL_MODE_MANUAL; mode <= DPLL_MODE_MAX; mode++)
		if (test_bit(mode, &dpll->mode_supported_mask))
			if (nla_put_s32(msg, DPLL_A_MODE_SUPPORTED, mode))
				return -EMSGSIZE;
	if (nla_put_64bit(msg, DPLL_A_CLOCK_ID, sizeof(dpll->clock_id),
			  &dpll->clock_id, 0))
		return -EMSGSIZE;
	if (nla_put_u8(msg, DPLL_A_TYPE, dpll->type))
		return -EMSGSIZE;

	return ret;
}

static bool dpll_pin_is_freq_supported(struct dpll_pin *pin, u32 freq)
{
	int fs;

	for (fs = 0; fs < pin->prop.freq_supported_num; fs++)
		if (freq >=  pin->prop.freq_supported[fs].min &&
		    freq <=  pin->prop.freq_supported[fs].max)
			return true;
	return false;
}

static int
dpll_pin_freq_set(struct dpll_pin *pin, struct nlattr *a,
		  struct netlink_ext_ack *extack)
{
	u64 freq = nla_get_u64(a);
	struct dpll_pin_ref *ref;
	unsigned long i;
	int ret;

	if (!dpll_pin_is_freq_supported(pin, freq))
		return -EINVAL;

	xa_for_each(&pin->dpll_refs, i, ref) {
		const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
		struct dpll_device *dpll = ref->dpll;

		ret = ops->frequency_set(pin, dpll_pin_on_dpll_priv(dpll, pin),
					 dpll, dpll_priv(dpll), freq, extack);
		if (ret)
			return -EFAULT;
		dpll_pin_notify(dpll, pin, DPLL_A_PIN_FREQUENCY);
	}

	return 0;
}

static int
dpll_pin_on_pin_state_set(struct dpll_device *dpll, struct dpll_pin *pin,
			  u32 parent_idx, enum dpll_pin_state state,
			  struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops;
	struct dpll_pin_ref *pin_ref, *parent_ref;

	if (!(DPLL_PIN_CAPS_STATE_CAN_CHANGE & pin->prop.capabilities))
		return -EOPNOTSUPP;
	parent_ref = xa_load(&pin->parent_refs, parent_idx);
	       //	dpll_pin_get_by_idx(dpll, parent_idx);
	if (!parent_ref)
		return -EINVAL;
	pin_ref = xa_load(&dpll->pin_refs, pin->pin_idx);
	if (!pin_ref)
		return -EINVAL;
	ops = dpll_pin_ops(pin_ref);
	if (!ops->state_on_pin_set)
		return -EOPNOTSUPP;
	if (ops->state_on_pin_set(pin_ref->pin,
				  dpll_pin_on_pin_priv(parent_ref->pin,
						       pin_ref->pin),
				  parent_ref->pin,
				  dpll_pin_on_dpll_priv(dpll, parent_ref->pin),
				  state, extack))
		return -EFAULT;
	dpll_pin_parent_notify(dpll, pin_ref->pin, parent_ref->pin,
			       DPLL_A_PIN_STATE);

	return 0;
}

static int
dpll_pin_state_set(struct dpll_device *dpll, struct dpll_pin *pin,
		   enum dpll_pin_state state,
		   struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops;
	struct dpll_pin_ref *ref;

	if (!(DPLL_PIN_CAPS_STATE_CAN_CHANGE & pin->prop.capabilities))
		return -EOPNOTSUPP;
	ref = xa_load(&pin->dpll_refs, dpll->device_idx);
	if (!ref)
		return -EFAULT;
	ops = dpll_pin_ops(ref);
	if (!ops->state_on_dpll_set)
		return -EOPNOTSUPP;
	if (ops->state_on_dpll_set(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
				   dpll_priv(dpll), state, extack))
		return -EINVAL;
	dpll_pin_notify(dpll, pin, DPLL_A_PIN_STATE);

	return 0;
}

static int
dpll_pin_prio_set(struct dpll_device *dpll, struct dpll_pin *pin,
		  struct nlattr *prio_attr, struct netlink_ext_ack *extack)
{
	const struct dpll_pin_ops *ops;
	struct dpll_pin_ref *ref;
	u32 prio = nla_get_u8(prio_attr);

	if (!(DPLL_PIN_CAPS_PRIORITY_CAN_CHANGE & pin->prop.capabilities))
		return -EOPNOTSUPP;
	ref = xa_load(&pin->dpll_refs, dpll->device_idx);
	if (!ref)
		return -EFAULT;
	ops = dpll_pin_ops(ref);
	if (!ops->prio_set)
		return -EOPNOTSUPP;
	if (ops->prio_set(pin, dpll_pin_on_dpll_priv(dpll, pin), dpll,
			  dpll_priv(dpll), prio, extack))
		return -EINVAL;
	dpll_pin_notify(dpll, pin, DPLL_A_PIN_PRIO);

	return 0;
}

static int
dpll_pin_direction_set(struct dpll_pin *pin, struct nlattr *a,
		       struct netlink_ext_ack *extack)
{
	enum dpll_pin_direction direction = nla_get_u8(a);
	struct dpll_pin_ref *ref;
	unsigned long i;

	if (!(DPLL_PIN_CAPS_DIRECTION_CAN_CHANGE & pin->prop.capabilities))
		return -EOPNOTSUPP;

	xa_for_each(&pin->dpll_refs, i, ref) {
		const struct dpll_pin_ops *ops = dpll_pin_ops(ref);
		struct dpll_device *dpll = ref->dpll;

		if (ops->direction_set(pin, dpll_pin_on_dpll_priv(dpll, pin),
				       dpll, dpll_priv(dpll), direction,
				       extack))
			return -EFAULT;
		dpll_pin_notify(dpll, pin, DPLL_A_PIN_DIRECTION);
	}

	return 0;
}

static int
dpll_pin_set_from_nlattr(struct dpll_device *dpll,
			 struct dpll_pin *pin, struct genl_info *info)
{
	enum dpll_pin_state state = 0;
	bool parent_present = false;
	int rem, ret = -EINVAL;
	struct nlattr *a;
	u32 parent_idx;

	nla_for_each_attr(a, genlmsg_data(info->genlhdr),
			  genlmsg_len(info->genlhdr), rem) {
		switch (nla_type(a)) {
		case DPLL_A_PIN_FREQUENCY:
			ret = dpll_pin_freq_set(pin, a, info->extack);
			if (ret)
				return ret;
			break;
		case DPLL_A_PIN_DIRECTION:
			ret = dpll_pin_direction_set(pin, a, info->extack);
			if (ret)
				return ret;
			break;
		case DPLL_A_PIN_PRIO:
			ret = dpll_pin_prio_set(dpll, pin, a, info->extack);
			if (ret)
				return ret;
			break;
		case DPLL_A_PIN_PARENT_IDX:
			parent_present = true;
			parent_idx = nla_get_u32(a);
			break;
		case DPLL_A_PIN_STATE:
			state = nla_get_u8(a);
			break;
		default:
			break;
		}
	}
	if (state) {
		if (!parent_present) {
			ret = dpll_pin_state_set(dpll, pin, state,
						 info->extack);
			if (ret)
				return ret;
		} else {
			ret = dpll_pin_on_pin_state_set(dpll, pin, parent_idx,
							state, info->extack);
			if (ret)
				return ret;
		}
	}

	return ret;
}

int dpll_nl_pin_set_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];
	struct dpll_pin *pin = info->user_ptr[1];

	return dpll_pin_set_from_nlattr(dpll, pin, info);
}

int dpll_nl_pin_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_pin *pin = info->user_ptr[1];
	struct sk_buff *msg;
	struct nlattr *hdr;
	int ret;

	if (!pin)
		return -ENODEV;
	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	hdr = genlmsg_put_reply(msg, info, &dpll_nl_family, 0,
				DPLL_CMD_PIN_GET);
	if (!hdr)
		return -EMSGSIZE;
	ret = __dpll_cmd_pin_dump_one(msg, pin, info->extack);
	if (ret) {
		nlmsg_free(msg);
		return ret;
	}
	genlmsg_end(msg, hdr);

	return genlmsg_reply(msg, info);
}

int dpll_nl_pin_get_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct dpll_dump_ctx *ctx = dpll_dump_context(cb);
	struct dpll_pin *pin;
	struct nlattr *hdr;
	unsigned long i;
	int ret = 0;

	xa_for_each_start(&dpll_pin_xa, i, pin, ctx->idx) {
		if (xa_empty(&pin->dpll_refs))
			continue;
		hdr = genlmsg_put(skb, NETLINK_CB(cb->skb).portid,
				  cb->nlh->nlmsg_seq,
				  &dpll_nl_family, NLM_F_MULTI,
				  DPLL_CMD_PIN_GET);
		if (!hdr) {
			ret = -EMSGSIZE;
			break;
		}
		ret = __dpll_cmd_pin_dump_one(skb, pin, cb->extack);
		if (ret) {
			genlmsg_cancel(skb, hdr);
			break;
		}
		genlmsg_end(skb, hdr);
	}
	if (ret == -EMSGSIZE) {
		ctx->idx = i;
		return skb->len;
	}
	return ret;
}

static int
dpll_set_from_nlattr(struct dpll_device *dpll, struct genl_info *info)
{
	const struct dpll_device_ops *ops = dpll_device_ops(dpll);
	struct nlattr *attr;
	enum dpll_mode mode;
	int rem, ret = 0;

	nla_for_each_attr(attr, genlmsg_data(info->genlhdr),
			  genlmsg_len(info->genlhdr), rem) {
		switch (nla_type(attr)) {
		case DPLL_A_MODE:
			mode = nla_get_u8(attr);

			ret = ops->mode_set(dpll, dpll_priv(dpll), mode,
					    info->extack);
			if (ret)
				return ret;
			break;
		default:
			break;
		}
	}

	return ret;
}

int dpll_nl_device_set_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];

	return dpll_set_from_nlattr(dpll, info);
}

int dpll_nl_device_get_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct dpll_device *dpll = info->user_ptr[0];
	struct sk_buff *msg;
	struct nlattr *hdr;
	int ret;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	hdr = genlmsg_put_reply(msg, info, &dpll_nl_family, 0,
				DPLL_CMD_DEVICE_GET);
	if (!hdr)
		return -EMSGSIZE;

	ret = dpll_device_get_one(dpll, msg, info->extack);
	if (ret) {
		nlmsg_free(msg);
		return ret;
	}
	genlmsg_end(msg, hdr);

	return genlmsg_reply(msg, info);
}

int dpll_nl_device_get_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct dpll_dump_ctx *ctx = dpll_dump_context(cb);
	struct dpll_device *dpll;
	struct nlattr *hdr;
	unsigned long i;
	int ret = 0;

	xa_for_each_start(&dpll_device_xa, i, dpll, ctx->idx) {
		if (!xa_get_mark(&dpll_device_xa, i, DPLL_REGISTERED))
			continue;
		hdr = genlmsg_put(skb, NETLINK_CB(cb->skb).portid,
				  cb->nlh->nlmsg_seq, &dpll_nl_family,
				  NLM_F_MULTI, DPLL_CMD_DEVICE_GET);
		if (!hdr) {
			ret = -EMSGSIZE;
			break;
		}
		ret = dpll_device_get_one(dpll, skb, cb->extack);
		if (ret) {
			genlmsg_cancel(skb, hdr);
			break;
		}
		genlmsg_end(skb, hdr);
	}
	if (ret == -EMSGSIZE) {
		ctx->idx = i;
		return skb->len;
	}
	return ret;
}

int dpll_pre_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		  struct genl_info *info)
{
	struct dpll_device *dpll_id = NULL;
	u32 id;

	if (!info->attrs[DPLL_A_ID])
		return -EINVAL;

	mutex_lock(&dpll_xa_lock);
	id = nla_get_u32(info->attrs[DPLL_A_ID]);

	dpll_id = dpll_device_get_by_id(id);
	if (!dpll_id)
		goto unlock;
	info->user_ptr[0] = dpll_id;
	return 0;
unlock:
	mutex_unlock(&dpll_xa_lock);
	return -ENODEV;
}

void dpll_post_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		    struct genl_info *info)
{
	mutex_unlock(&dpll_xa_lock);
}

int dpll_pre_dumpit(struct netlink_callback *cb)
{
	mutex_lock(&dpll_xa_lock);

	return 0;
}

int dpll_post_dumpit(struct netlink_callback *cb)
{
	mutex_unlock(&dpll_xa_lock);

	return 0;
}

int dpll_pin_pre_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
		      struct genl_info *info)
{
	int ret = dpll_pre_doit(ops, skb, info);
	struct dpll_pin_ref *pin_ref;
	struct dpll_device *dpll;

	if (ret)
		return ret;
	dpll = info->user_ptr[0];
	if (!info->attrs[DPLL_A_PIN_IDX]) {
		ret = -EINVAL;
		goto unlock_dev;
	}
	pin_ref = xa_load(&dpll->pin_refs,
			  nla_get_u32(info->attrs[DPLL_A_PIN_IDX]));
	if (!pin_ref) {
		ret = -ENODEV;
		goto unlock_dev;
	}
	info->user_ptr[1] = pin_ref->pin;

	return 0;

unlock_dev:
	mutex_unlock(&dpll_xa_lock);
	return ret;
}

void dpll_pin_post_doit(const struct genl_split_ops *ops, struct sk_buff *skb,
			struct genl_info *info)
{
	dpll_post_doit(ops, skb, info);
}

int dpll_pin_pre_dumpit(struct netlink_callback *cb)
{
	return dpll_pre_dumpit(cb);
}

int dpll_pin_post_dumpit(struct netlink_callback *cb)
{
	return dpll_post_dumpit(cb);
}

static int
dpll_event_device_change(struct sk_buff *msg, struct dpll_device *dpll,
			 struct dpll_pin *pin, struct dpll_pin *parent,
			 enum dplla attr)
{
	int ret = dpll_msg_add_dev_handle(msg, dpll);
	struct dpll_pin_ref *ref = NULL;
	enum dpll_pin_state state;

	if (ret)
		return ret;
	if (pin && nla_put_u32(msg, DPLL_A_PIN_IDX, pin->pin_idx))
		return -EMSGSIZE;

	switch (attr) {
	case DPLL_A_MODE:
		ret = dpll_msg_add_mode(msg, dpll, NULL);
		break;
	case DPLL_A_LOCK_STATUS:
		ret = dpll_msg_add_lock_status(msg, dpll, NULL);
		break;
	case DPLL_A_TEMP:
		ret = dpll_msg_add_temp(msg, dpll, NULL);
		break;
	case DPLL_A_PIN_FREQUENCY:
		ref = xa_load(&pin->dpll_refs, dpll->device_idx);
		if (!ref)
			return -EFAULT;
		ret = dpll_msg_add_pin_freq(msg, pin, ref, NULL, false);
		break;
	case DPLL_A_PIN_PRIO:
		ref = xa_load(&pin->dpll_refs, dpll->device_idx);
		if (!ref)
			return -EFAULT;
		ret = dpll_msg_add_pin_prio(msg, pin, ref, NULL);
		break;
	case DPLL_A_PIN_STATE:
		if (parent) {
			void *parent_priv = dpll_pin_on_dpll_priv(dpll, parent);
			void *priv = dpll_pin_on_pin_priv(parent, pin);
			const struct dpll_pin_ops *ops;

			ref = xa_load(&pin->parent_refs, parent->pin_idx);
			if (!ref)
				return -EFAULT;
			ops = dpll_pin_ops(ref);
			if (!ops->state_on_pin_get)
				return -EOPNOTSUPP;
			ret = ops->state_on_pin_get(pin, priv, parent,
						    parent_priv, &state, NULL);
			if (ret)
				return ret;
			if (nla_put_u32(msg, DPLL_A_PIN_PARENT_IDX,
					parent->pin_idx))
				return -EMSGSIZE;
		} else {
			ref = xa_load(&pin->dpll_refs, dpll->device_idx);
			if (!ref)
				return -EFAULT;
			ret = dpll_msg_add_pin_on_dpll_state(msg, pin, ref,
							     NULL);
			if (ret)
				return ret;
		}
		break;
	default:
		break;
	}

	return ret;
}

static int
dpll_send_event_create(enum dplla event, struct dpll_device *dpll)
{
	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, 0, &dpll_nl_family, 0, event);
	if (!hdr)
		goto out_free_msg;

	ret = dpll_msg_add_dev_handle(msg, dpll);
	if (ret)
		goto out_cancel_msg;
	genlmsg_end(msg, hdr);
	genlmsg_multicast(&dpll_nl_family, msg, 0, 0, GFP_KERNEL);

	return 0;

out_cancel_msg:
	genlmsg_cancel(msg, hdr);
out_free_msg:
	nlmsg_free(msg);

	return ret;
}

static int
dpll_send_event_change(struct dpll_device *dpll, struct dpll_pin *pin,
		       struct dpll_pin *parent, enum dplla attr)
{
	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, 0, &dpll_nl_family, 0,
			  DPLL_CMD_DEVICE_CHANGE_NTF);
	if (!hdr)
		goto out_free_msg;

	ret = dpll_event_device_change(msg, dpll, pin, parent, attr);
	if (ret)
		goto out_cancel_msg;
	genlmsg_end(msg, hdr);
	genlmsg_multicast(&dpll_nl_family, msg, 0, 0, GFP_KERNEL);

	return 0;

out_cancel_msg:
	genlmsg_cancel(msg, hdr);
out_free_msg:
	nlmsg_free(msg);

	return ret;
}

int dpll_notify_device_create(struct dpll_device *dpll)
{
	return dpll_send_event_create(DPLL_CMD_DEVICE_CREATE_NTF, dpll);
}

int dpll_notify_device_delete(struct dpll_device *dpll)
{
	return dpll_send_event_create(DPLL_CMD_DEVICE_DELETE_NTF, dpll);
}

int dpll_device_notify(struct dpll_device *dpll, enum dplla attr)
{
	if (WARN_ON(!dpll))
		return -EINVAL;

	return dpll_send_event_change(dpll, NULL, NULL, attr);
}
EXPORT_SYMBOL_GPL(dpll_device_notify);

int dpll_pin_notify(struct dpll_device *dpll, struct dpll_pin *pin,
		    enum dplla attr)
{
	return dpll_send_event_change(dpll, pin, NULL, attr);
}
EXPORT_SYMBOL_GPL(dpll_pin_notify);

int dpll_pin_parent_notify(struct dpll_device *dpll, struct dpll_pin *pin,
			   struct dpll_pin *parent, enum dplla attr)
{
	return dpll_send_event_change(dpll, pin, parent, attr);
}

int __init dpll_netlink_init(void)
{
	return genl_register_family(&dpll_nl_family);
}

void dpll_netlink_finish(void)
{
	genl_unregister_family(&dpll_nl_family);
}

void __exit dpll_netlink_fini(void)
{
	dpll_netlink_finish();
}
