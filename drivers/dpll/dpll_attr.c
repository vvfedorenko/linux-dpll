// SPDX-License-Identifier: GPL-2.0
/*
 *  dpll_attr.c - dpll attributes handling helper class.
 *
 *  Copyright (c) 2022, Intel Corporation.
 */

#include <linux/dpll.h>
#include <linux/bitops.h>
#include <linux/slab.h>

struct dpll_attr {
	unsigned long valid_mask;
	enum dpll_lock_status lock_status;
	s32 temp;
	u32 source_pin_idx;
	enum dpll_mode mode;
	unsigned long mode_supported_mask;
};

static const int MAX_BITS = BITS_PER_TYPE(unsigned long);

struct dpll_attr *dpll_attr_alloc(void)
{
	return kzalloc(sizeof(struct dpll_attr), GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(dpll_attr_alloc);

void dpll_attr_free(struct dpll_attr *attr)
{
	kfree(attr);
}
EXPORT_SYMBOL_GPL(dpll_attr_free);

void dpll_attr_clear(struct dpll_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
}
EXPORT_SYMBOL_GPL(dpll_attr_clear);

bool dpll_attr_valid(enum dplla attr_id, const struct dpll_attr *attr)
{
	if (!attr)
		return false;
	if (attr_id > 0 && attr_id < BITS_PER_LONG)
		return test_bit(attr_id, &attr->valid_mask);

	return false;
}
EXPORT_SYMBOL_GPL(dpll_attr_valid);

int
dpll_attr_copy(struct dpll_attr *dst, const struct dpll_attr *src)
{
	if (!src || !dst)
		return -EFAULT;
	memcpy(dst, src, sizeof(*dst));

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_copy);

static inline bool dpll_lock_status_valid(enum dpll_lock_status status)
{
	if (status >= DPLL_LOCK_STATUS_UNSPEC &&
	    status <= DPLL_LOCK_STATUS_MAX)
		return true;

	return false;
}

int dpll_attr_lock_status_set(struct dpll_attr *attr,
			      enum dpll_lock_status status)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_lock_status_valid(status))
		return -EINVAL;

	attr->lock_status = status;
	set_bit(DPLLA_LOCK_STATUS, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_lock_status_set);

enum dpll_lock_status dpll_attr_lock_status_get(const struct dpll_attr *attr)
{
	if (!dpll_attr_valid(DPLLA_LOCK_STATUS, attr))
		return DPLL_LOCK_STATUS_UNSPEC;

	return attr->lock_status;
}
EXPORT_SYMBOL_GPL(dpll_attr_lock_status_get);

int dpll_attr_temp_set(struct dpll_attr *attr, s32 temp)
{
	if (!attr)
		return -EFAULT;

	attr->temp = temp;
	set_bit(DPLLA_TEMP, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_temp_set);

int dpll_attr_temp_get(const struct dpll_attr *attr, s32 *temp)
{
	if (!attr || !temp)
		return -EFAULT;
	if (!dpll_attr_valid(DPLLA_TEMP, attr))
		return -EINVAL;

	*temp = attr->temp;

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_temp_get);

int dpll_attr_source_idx_set(struct dpll_attr *attr, u32 source_idx)
{
	if (!attr)
		return -EFAULT;

	attr->source_pin_idx = source_idx;
	set_bit(DPLLA_SOURCE_PIN_IDX, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_source_idx_set);

int dpll_attr_source_idx_get(const struct dpll_attr *attr, u32 *source_idx)
{
	if (!attr || !source_idx)
		return -EFAULT;
	if (!dpll_attr_valid(DPLLA_SOURCE_PIN_IDX, attr))
		return -EINVAL;

	*source_idx = attr->source_pin_idx;

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_source_idx_get);

static inline bool dpll_mode_valid(enum dpll_mode mode)
{
	if (mode >= DPLL_MODE_UNSPEC &&
	    mode <= DPLL_MODE_MAX)
		return true;

	return false;
}

int dpll_attr_mode_set(struct dpll_attr *attr, enum dpll_mode mode)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_mode_valid(mode))
		return -EINVAL;

	attr->mode = mode;
	set_bit(DPLLA_MODE, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_mode_set);

enum dpll_mode dpll_attr_mode_get(const struct dpll_attr *attr)
{
	if (!attr || !dpll_attr_valid(DPLLA_MODE, attr))
		return DPLL_MODE_UNSPEC;

	return attr->mode;
}
EXPORT_SYMBOL_GPL(dpll_attr_mode_get);

int dpll_attr_mode_supported_set(struct dpll_attr *attr, enum dpll_mode mode)
{
	if (!attr)
		return -EFAULT;
	if (!dpll_mode_valid(mode))
		return -EINVAL;

	set_bit(mode, &attr->mode_supported_mask);
	set_bit(DPLLA_MODE_SUPPORTED, &attr->valid_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_attr_mode_supported_set);

bool dpll_attr_mode_supported(const struct dpll_attr *attr,
			      enum dpll_mode mode)
{
	if (!dpll_mode_valid(mode))
		return false;
	if (!dpll_attr_valid(DPLLA_MODE_SUPPORTED, attr))
		return false;

	return test_bit(mode, &attr->mode_supported_mask);
}
EXPORT_SYMBOL_GPL(dpll_attr_mode_supported);

static bool dpll_attr_changed(const enum dplla attr_id,
			      struct dpll_attr *new,
			      struct dpll_attr *old)
{
	if (dpll_attr_valid(attr_id, new)) {
		if (dpll_attr_valid(attr_id, old)) {
			switch (attr_id) {
			case DPLLA_MODE:
				if (new->mode != old->mode)
					return true;
				break;
			case DPLLA_SOURCE_PIN_IDX:
				if (new->source_pin_idx != old->source_pin_idx)
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

int dpll_attr_delta(struct dpll_attr *delta, struct dpll_attr *new,
		    struct dpll_attr *old)
{
	int ret = -EINVAL;

	if (!delta || !new || !old)
		return -EFAULT;

	dpll_attr_clear(delta);

	if (dpll_attr_changed(DPLLA_MODE, new, old)) {
		ret = dpll_attr_mode_set(delta, new->mode);
		if (ret)
			return ret;
	}
	if (dpll_attr_changed(DPLLA_SOURCE_PIN_IDX, new, old)) {
		ret = dpll_attr_source_idx_set(delta, new->source_pin_idx);
		if (ret)
			return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_attr_delta);
