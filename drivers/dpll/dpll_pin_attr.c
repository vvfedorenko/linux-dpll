// SPDX-License-Identifier: GPL-2.0
/*
 *  dpll_pin_attr.c - Pin attribute handling helper class.
 *
 *  Copyright (c) 2022, Intel Corporation.
 */

#include <linux/dpll.h>
#include <linux/bitops.h>
#include <linux/slab.h>

struct dpll_pin_attr {
	unsigned long valid_mask;
	enum dpll_pin_type type;
	unsigned long types_supported_mask;
	enum dpll_pin_signal_type signal_type;
	unsigned long signal_types_supported_mask;
	u32 custom_freq;
	unsigned long state_mask;
	unsigned long state_supported_mask;
	u32 prio;
	unsigned int netifindex;
};

static const int MAX_BITS = BITS_PER_TYPE(unsigned long);

struct dpll_pin_attr *dpll_pin_attr_alloc(void)
{
	return kzalloc(sizeof(struct dpll_pin_attr), GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_alloc);

void dpll_pin_attr_free(struct dpll_pin_attr *attr)
{
	kfree(attr);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_free);

void dpll_pin_attr_clear(struct dpll_pin_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_clear);

bool dpll_pin_attr_valid(enum dplla attr_id, const struct dpll_pin_attr *attr)
{
	if (!attr)
		return false;
	if (attr_id > 0 && attr_id < BITS_PER_LONG)
		return test_bit(attr_id, &attr->valid_mask);

	return false;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_valid);

int
dpll_pin_attr_copy(struct dpll_pin_attr *dst, const struct dpll_pin_attr *src)
{
	if (!src || !dst)
		return -EFAULT;
	memcpy(dst, src, sizeof(*dst));

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_copy);

static inline bool dpll_pin_type_valid(enum dpll_pin_type type)
{
	if (type >= DPLL_PIN_TYPE_UNSPEC && type <= DPLL_PIN_TYPE_MAX)
		return true;

	return false;
}

int dpll_pin_attr_type_set(struct dpll_pin_attr *attr, enum dpll_pin_type type)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_type_valid(type))
		return -EINVAL;

	attr->type = type;
	set_bit(DPLLA_PIN_TYPE, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_type_set);

enum dpll_pin_type dpll_pin_attr_type_get(const struct dpll_pin_attr *attr)
{
	if (!dpll_pin_attr_valid(DPLLA_PIN_TYPE, attr))
		return DPLL_PIN_TYPE_UNSPEC;

	return attr->type;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_type_get);

int dpll_pin_attr_type_supported_set(struct dpll_pin_attr *attr,
				     enum dpll_pin_type type)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_type_valid(type))
		return -EINVAL;

	set_bit(type, &attr->types_supported_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_type_supported_set);

bool dpll_pin_attr_type_supported(const struct dpll_pin_attr *attr,
				     enum dpll_pin_type type)
{
	if (!dpll_pin_type_valid(type))
		return false;

	return test_bit(type, &attr->types_supported_mask);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_type_supported);

static inline bool dpll_pin_signal_type_valid(enum dpll_pin_signal_type type)
{
	if (type >= DPLL_PIN_SIGNAL_TYPE_UNSPEC &&
	    type <= DPLL_PIN_SIGNAL_TYPE_MAX)
		return true;

	return false;
}

int dpll_pin_attr_signal_type_set(struct dpll_pin_attr *attr,
				  enum dpll_pin_signal_type type)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_signal_type_valid(type))
		return -EINVAL;

	attr->signal_type = type;
	set_bit(DPLLA_PIN_SIGNAL_TYPE, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_signal_type_set);

enum dpll_pin_signal_type
dpll_pin_attr_signal_type_get(const struct dpll_pin_attr *attr)
{
	if (!dpll_pin_attr_valid(DPLLA_PIN_SIGNAL_TYPE, attr))
		return DPLL_PIN_SIGNAL_TYPE_UNSPEC;

	return attr->signal_type;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_signal_type_get);

int dpll_pin_attr_signal_type_supported_set(struct dpll_pin_attr *attr,
					    enum dpll_pin_signal_type type)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_signal_type_valid(type))
		return -EINVAL;

	set_bit(type, &attr->signal_types_supported_mask);
	set_bit(DPLLA_PIN_SIGNAL_TYPE_SUPPORTED, &attr->valid_mask);

	return 0;

}
EXPORT_SYMBOL_GPL(dpll_pin_attr_signal_type_supported_set);

bool dpll_pin_attr_signal_type_supported(const struct dpll_pin_attr *attr,
					    enum dpll_pin_signal_type type)
{
	if (!dpll_pin_signal_type_valid(type))
		return false;
	if (!dpll_pin_attr_valid(DPLLA_PIN_SIGNAL_TYPE_SUPPORTED, attr))
		return false;

	return test_bit(type, &attr->signal_types_supported_mask);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_signal_type_supported);

int dpll_pin_attr_custom_freq_set(struct dpll_pin_attr *attr, u32 freq)
{
	if (!attr)
		return -EFAULT;

	attr->custom_freq = freq;
	set_bit(DPLLA_PIN_CUSTOM_FREQ, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_custom_freq_set);

int dpll_pin_attr_custom_freq_get(const struct dpll_pin_attr *attr, u32 *freq)
{
	if (!attr || !freq)
		return -EFAULT;
	if (!test_bit(DPLLA_PIN_CUSTOM_FREQ, &attr->valid_mask))
		return -EINVAL;

	*freq = attr->custom_freq;

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_custom_freq_get);

static inline bool dpll_pin_state_valid(enum dpll_pin_state state)
{
	if (state >= DPLL_PIN_STATE_UNSPEC &&
	    state <= DPLL_PIN_STATE_MAX)
		return true;

	return false;
}

int dpll_pin_attr_state_set(struct dpll_pin_attr *attr,
			    enum dpll_pin_state state)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_state_valid(state))
		return -EINVAL;
	if (state == DPLL_PIN_STATE_CONNECTED) {
		if (test_bit(DPLL_PIN_STATE_DISCONNECTED, &attr->state_mask))
			return -EINVAL;
	} else if (state == DPLL_PIN_STATE_DISCONNECTED) {
		if (test_bit(DPLL_PIN_STATE_CONNECTED, &attr->state_mask))
			return -EINVAL;
	}

	set_bit(state, &attr->state_mask);
	set_bit(DPLLA_PIN_STATE, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_state_set);

bool dpll_pin_attr_state_enabled(const struct dpll_pin_attr *attr,
				 enum dpll_pin_state state)
{
	if (!dpll_pin_state_valid(state))
		return false;
	if (!dpll_pin_attr_valid(DPLLA_PIN_STATE, attr))
		return false;

	return test_bit(state, &attr->state_mask);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_state_enabled);

int dpll_pin_attr_state_supported_set(struct dpll_pin_attr *attr,
				      enum dpll_pin_state state)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_pin_state_valid(state))
		return -EINVAL;

	set_bit(state, &attr->state_supported_mask);
	set_bit(DPLLA_PIN_STATE_SUPPORTED, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_state_supported_set);

bool dpll_pin_attr_state_supported(const struct dpll_pin_attr *attr,
				   enum dpll_pin_state state)
{
	if (!dpll_pin_state_valid(state))
		return false;
	if (!dpll_pin_attr_valid(DPLLA_PIN_STATE_SUPPORTED, attr))
		return false;

	return test_bit(state, &attr->state_supported_mask);
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_state_supported);

int dpll_pin_attr_prio_set(struct dpll_pin_attr *attr, u32 prio)
{
	if (!attr)
		return -EFAULT;
	if (prio > PIN_PRIO_LOWEST)
		return -EINVAL;

	attr->prio = prio;
	set_bit(DPLLA_PIN_PRIO, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_prio_set);

int dpll_pin_attr_prio_get(const struct dpll_pin_attr *attr, u32 *prio)
{
	if (!attr || !prio)
		return -EFAULT;
	if (!dpll_pin_attr_valid(DPLLA_PIN_PRIO, attr))
		return -EINVAL;

	*prio = attr->prio;

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_prio_get);

int dpll_pin_attr_netifindex_set(struct dpll_pin_attr *attr, unsigned int netifindex)
{
	if (!attr)
		return -EFAULT;

	attr->netifindex = netifindex;
	set_bit(DPLLA_PIN_NETIFINDEX, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_netifindex_set);

int dpll_pin_attr_netifindex_get(const struct dpll_pin_attr *attr,
				 unsigned int *netifindex)
{
	if (!dpll_pin_attr_valid(DPLLA_PIN_NETIFINDEX, attr))
		return -EINVAL;

	*netifindex = attr->netifindex;
	return true;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_netifindex_get);

static bool dpll_pin_attr_changed(const enum dplla attr_id,
				  struct dpll_pin_attr *new,
				  struct dpll_pin_attr *old)
{
	if (dpll_pin_attr_valid(attr_id, new)) {
		if (dpll_pin_attr_valid(attr_id, old)) {
			switch (attr_id) {
			case DPLLA_PIN_TYPE:
				if (new->type != old->type)
					return true;
				break;
			case DPLLA_PIN_SIGNAL_TYPE:
				if (new->signal_type != old->signal_type)
					return true;
				break;
			case DPLLA_PIN_CUSTOM_FREQ:
				if (new->custom_freq != old->custom_freq)
					return true;
				break;
			case DPLLA_PIN_STATE:
				if (new->state_mask != old->state_mask)
					return true;
				break;
			case DPLLA_PIN_PRIO:
				if (new->prio != old->prio)
					return true;
				break;
			default:
				return false;
			}
		} else {
			return true;
		}
	}

	return false;
}

int dpll_pin_attr_delta(struct dpll_pin_attr *delta, struct dpll_pin_attr *new,
			struct dpll_pin_attr *old)
{
	int ret = -EINVAL;

	if (!delta || !new || !old)
		return -EFAULT;

	dpll_pin_attr_clear(delta);

	if (dpll_pin_attr_changed(DPLLA_PIN_TYPE, new, old)) {
		ret = dpll_pin_attr_type_set(delta, new->type);
		if (ret)
			return ret;
	}
	if (dpll_pin_attr_changed(DPLLA_PIN_SIGNAL_TYPE, new, old)) {
		ret = dpll_pin_attr_signal_type_set(delta, new->signal_type);
		if (ret)
			return ret;
	}
	if (dpll_pin_attr_changed(DPLLA_PIN_CUSTOM_FREQ, new, old)) {
		ret = dpll_pin_attr_custom_freq_set(delta, new->custom_freq);
		if (ret)
			return ret;
	}
	if (dpll_pin_attr_changed(DPLLA_PIN_STATE, new, old)) {
		enum dpll_pin_state i;

		for (i = DPLL_PIN_STATE_UNSPEC + 1;
		     i <= DPLL_PIN_STATE_MAX; i++)
			if (test_bit(i, &new->state_mask)) {
				ret = dpll_pin_attr_state_set(delta, i);
				if (ret)
					return ret;
			}
	}
	if (dpll_pin_attr_changed(DPLLA_PIN_PRIO, new, old)) {
		ret = dpll_pin_attr_prio_set(delta, new->prio);
		if (ret)
			return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_delta);

int dpll_pin_attr_prep_common(struct dpll_pin_attr *common,
			      const struct dpll_pin_attr *reference)
{
	if (!common || !reference)
		return -EFAULT;
	dpll_pin_attr_clear(common);
	if (dpll_pin_attr_valid(DPLLA_PIN_TYPE, reference)) {
		common->type = reference->type;
		set_bit(DPLLA_PIN_TYPE, &common->valid_mask);
	}
	if (dpll_pin_attr_valid(DPLLA_PIN_SIGNAL_TYPE, reference)) {
		common->signal_type = reference->signal_type;
		set_bit(DPLLA_PIN_SIGNAL_TYPE, &common->valid_mask);
	}
	if (dpll_pin_attr_valid(DPLLA_PIN_CUSTOM_FREQ, reference)) {
		common->custom_freq = reference->custom_freq;
		set_bit(DPLLA_PIN_CUSTOM_FREQ, &common->valid_mask);
	}
	if (dpll_pin_attr_valid(DPLLA_PIN_STATE, reference)) {
		common->state_mask = reference->state_mask;
		set_bit(DPLLA_PIN_STATE, &common->valid_mask);
	}
	return common->valid_mask ? PIN_ATTR_CHANGE : 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_prep_common);


int dpll_pin_attr_prep_exclusive(struct dpll_pin_attr *exclusive,
				 const struct dpll_pin_attr *reference)
{
	if (!exclusive || !reference)
		return -EFAULT;
	dpll_pin_attr_clear(exclusive);
	if (dpll_pin_attr_valid(DPLLA_PIN_PRIO, reference)) {
		exclusive->prio = reference->prio;
		set_bit(DPLLA_PIN_PRIO, &exclusive->valid_mask);
	}
	return exclusive->valid_mask ? PIN_ATTR_CHANGE : 0;
}
EXPORT_SYMBOL_GPL(dpll_pin_attr_prep_exclusive);

