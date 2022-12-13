// SPDX-License-Identifier: GPL-2.0
/*
 *  dpll_core.c - Generic DPLL Management class support.
 *
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dpll_core.h"

/**
 * struct dpll_pin - structure for a dpll pin
 * @idx:		unique id number for each pin
 * @parent_pin:		parent pin
 * @type:		type of the pin
 * @ops:		operations this &dpll_pin supports
 * @priv:		pointer to private information of owner
 * @ref_dplls:		array of registered dplls
 * @description:	name to distinguish the pin
 */
struct dpll_pin {
	u32 idx;
	struct dpll_pin *parent_pin;
	enum dpll_pin_type type;
	struct dpll_pin_ops *ops;
	void *priv;
	struct xarray ref_dplls;
	char description[PIN_DESC_LEN];
};

/**
 * struct dpll_device - structure for a DPLL device
 * @id:		unique id number for each device
 * @dev:	struct device for this dpll device
 * @parent:	parent device
 * @ops:	operations this &dpll_device supports
 * @lock:	mutex to serialize operations
 * @type:	type of a dpll
 * @priv:	pointer to private information of owner
 * @pins:	list of pointers to pins registered with this dpll
 * @cookie:	unique identifier (cookie) of a dpll
 * @dev_driver_idx: provided by driver for
 */
struct dpll_device {
	u32 id;
	struct device dev;
	struct device *parent;
	struct dpll_device_ops *ops;
	struct mutex lock;
	enum dpll_type type;
	void *priv;
	struct xarray pins;
	u8 cookie[DPLL_COOKIE_LEN];
	u8 dev_driver_idx;
};

static DEFINE_MUTEX(dpll_device_xa_lock);

static DEFINE_XARRAY_FLAGS(dpll_device_xa, XA_FLAGS_ALLOC);
#define DPLL_REGISTERED		XA_MARK_1
#define PIN_REGISTERED		XA_MARK_1

#define ASSERT_DPLL_REGISTERED(d)                                           \
	WARN_ON_ONCE(!xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))
#define ASSERT_DPLL_NOT_REGISTERED(d)                                      \
	WARN_ON_ONCE(xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))

struct pin_ref_dpll {
	struct dpll_device *dpll;
	struct dpll_pin_ops *ops;
	void *priv;
};

struct dpll_device *dpll_device_get_by_id(int id)
{
	struct dpll_device *dpll = NULL;

	if (xa_get_mark(&dpll_device_xa, id, DPLL_REGISTERED))
		dpll = xa_load(&dpll_device_xa, id);

	return dpll;
}

struct dpll_device *dpll_device_get_by_name(const char *name)
{
	struct dpll_device *dpll, *ret = NULL;
	unsigned long index;

	mutex_lock(&dpll_device_xa_lock);
	xa_for_each_marked(&dpll_device_xa, index, dpll, DPLL_REGISTERED) {
		if (!strcmp(dev_name(&dpll->dev), name)) {
			ret = dpll;
			break;
		}
	}
	mutex_unlock(&dpll_device_xa_lock);

	return ret;
}

struct dpll_device *dpll_device_get_by_cookie(u8 cookie[DPLL_COOKIE_LEN],
					      enum dpll_type type, u8 idx)
{
	struct dpll_device *dpll, *ret = NULL;
	unsigned long index;

	mutex_lock(&dpll_device_xa_lock);
	xa_for_each_marked(&dpll_device_xa, index, dpll, DPLL_REGISTERED) {
		if (!memcmp(dpll->cookie, cookie, DPLL_COOKIE_LEN)) {
			if (dpll->type == type) {
				if (dpll->dev_driver_idx == idx) {
					ret = dpll;
					break;
				}
			}
		}
	}
	mutex_unlock(&dpll_device_xa_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_device_get_by_cookie);

static void dpll_device_release(struct device *dev)
{
	struct dpll_device *dpll;

	dpll = to_dpll_device(dev);

	dpll_device_unregister(dpll);
	dpll_device_free(dpll);
}

static struct class dpll_class = {
	.name = "dpll",
	.dev_release = dpll_device_release,
};

static const char *dpll_type_name[__DPLL_TYPE_MAX] = {
	[DPLL_TYPE_UNSPEC] = "",
	[DPLL_TYPE_PPS] = "PPS",
	[DPLL_TYPE_EEC] = "EEC",
};

static const char *dpll_type_str(enum dpll_type type)
{
	if (type >= DPLL_TYPE_UNSPEC && type <= DPLL_TYPE_MAX)
		return dpll_type_name[type];
	else
		return "";
}

struct dpll_device
*dpll_device_alloc(struct dpll_device_ops *ops, enum dpll_type type,
		   const u8 cookie[DPLL_COOKIE_LEN], u8 idx,
		   void *priv, struct device *parent)
{
	struct dpll_device *dpll;
	int ret;

	dpll = kzalloc(sizeof(*dpll), GFP_KERNEL);
	if (!dpll)
		return ERR_PTR(-ENOMEM);

	mutex_init(&dpll->lock);
	dpll->ops = ops;
	dpll->dev.class = &dpll_class;
	dpll->parent = parent;
	dpll->type = type;
	dpll->dev_driver_idx = idx;
	memcpy(dpll->cookie, cookie, sizeof(dpll->cookie));

	mutex_lock(&dpll_device_xa_lock);
	ret = xa_alloc(&dpll_device_xa, &dpll->id, dpll,
		       xa_limit_16b, GFP_KERNEL);
	if (ret)
		goto error;
	dev_set_name(&dpll->dev, "dpll-%s-%s-%s%d", dev_driver_string(parent),
		     dev_name(parent), type ? dpll_type_str(type) : "", idx);
	dpll->priv = priv;
	xa_init_flags(&dpll->pins, XA_FLAGS_ALLOC);
	mutex_unlock(&dpll_device_xa_lock);
	dpll_notify_device_create(dpll);

	return dpll;

error:
	mutex_unlock(&dpll_device_xa_lock);
	kfree(dpll);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(dpll_device_alloc);

void dpll_device_free(struct dpll_device *dpll)
{
	WARN_ON_ONCE(!dpll);
	WARN_ON_ONCE(!xa_empty(&dpll->pins));
	xa_destroy(&dpll->pins);
	mutex_destroy(&dpll->lock);
	kfree(dpll);
}
EXPORT_SYMBOL_GPL(dpll_device_free);

void dpll_device_register(struct dpll_device *dpll)
{
	ASSERT_DPLL_NOT_REGISTERED(dpll);

	mutex_lock(&dpll_device_xa_lock);
	xa_set_mark(&dpll_device_xa, dpll->id, DPLL_REGISTERED);
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_device_register);

void dpll_device_unregister(struct dpll_device *dpll)
{
	ASSERT_DPLL_REGISTERED(dpll);

	mutex_lock(&dpll_device_xa_lock);
	xa_erase(&dpll_device_xa, dpll->id);
	dpll_notify_device_delete(dpll);
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_device_unregister);

u32 dpll_id(struct dpll_device *dpll)
{
	return dpll->id;
}
EXPORT_SYMBOL_GPL(dpll_id);

u32 dpll_pin_idx(struct dpll_device *dpll, struct dpll_pin *pin)
{
	struct dpll_pin *pos;
	unsigned long index;

	xa_for_each_marked(&dpll->pins, index, pos, PIN_REGISTERED) {
		if (pos == pin)
			return pin->idx;
	}

	return PIN_IDX_INVALID;
}
EXPORT_SYMBOL_GPL(dpll_pin_idx);

const char *dpll_dev_name(struct dpll_device *dpll)
{
	return dev_name(&dpll->dev);
}
EXPORT_SYMBOL_GPL(dpll_dev_name);

struct dpll_pin *dpll_pin_alloc(const char *description, size_t desc_len)
{
	struct dpll_pin *pin = kzalloc(sizeof(struct dpll_pin), GFP_KERNEL);

	if (!pin)
		return ERR_PTR(-ENOMEM);
	if (desc_len > PIN_DESC_LEN)
		return ERR_PTR(-EINVAL);

	strncpy(pin->description, description, PIN_DESC_LEN);
	if (desc_len == PIN_DESC_LEN)
		pin->description[PIN_DESC_LEN - 1] = '\0';
	xa_init_flags(&pin->ref_dplls, XA_FLAGS_ALLOC);

	return pin;
}
EXPORT_SYMBOL_GPL(dpll_pin_alloc);

static int dpll_alloc_pin_on_xa(struct xarray *pins, struct dpll_pin *pin)
{
	struct dpll_pin *pos;
	unsigned long index;
	int ret;

	xa_for_each(pins, index, pos) {
		if (pos == pin ||
		    !strncmp(pos->description, pin->description, PIN_DESC_LEN))
			return -EEXIST;
	}

	ret = xa_alloc(pins, &pin->idx, pin, xa_limit_16b, GFP_KERNEL);
	if (!ret)
		xa_set_mark(pins, pin->idx, PIN_REGISTERED);

	return ret;
}

static int pin_ref_dpll_add(struct dpll_pin *pin, struct dpll_device *dpll,
			    struct dpll_pin_ops *ops, void *priv)
{
	struct pin_ref_dpll *ref, *pos;
	unsigned long index;
	u32 idx;

	ref = kzalloc(sizeof(struct pin_ref_dpll), GFP_KERNEL);
	if (!ref)
		return -ENOMEM;
	ref->dpll = dpll;
	ref->ops = ops;
	ref->priv = priv;
	if (!xa_empty(&pin->ref_dplls)) {
		xa_for_each(&pin->ref_dplls, index, pos) {
			if (pos->dpll == ref->dpll)
				return -EEXIST;
		}
	}

	return xa_alloc(&pin->ref_dplls, &idx, ref, xa_limit_16b, GFP_KERNEL);
}

static void pin_ref_dpll_del(struct dpll_pin *pin, struct dpll_device *dpll)
{
	struct pin_ref_dpll *pos;
	unsigned long index;

	xa_for_each(&pin->ref_dplls, index, pos) {
		if (pos->dpll == dpll) {
			WARN_ON_ONCE(pos != xa_erase(&pin->ref_dplls, index));
			break;
		}
	}
}

static int pin_deregister_from_xa(struct xarray *xa_pins, struct dpll_pin *pin)
{
	struct dpll_pin *pos;
	unsigned long index;

	xa_for_each(xa_pins, index, pos) {
		if (pos == pin) {
			WARN_ON_ONCE(pos != xa_erase(xa_pins, index));
			return 0;
		}
	}

	return -ENXIO;
}

int dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin,
		      struct dpll_pin_ops *ops, void *priv)
{
	int ret;

	if (!pin || !ops)
		return -EINVAL;

	mutex_lock(&dpll->lock);
	ret = dpll_alloc_pin_on_xa(&dpll->pins, pin);
	if (!ret) {
		ret = pin_ref_dpll_add(pin, dpll, ops, priv);
		if (ret)
			pin_deregister_from_xa(&dpll->pins, pin);
	}
	mutex_unlock(&dpll->lock);
	if (!ret)
		dpll_pin_notify(dpll, pin, DPLL_CHANGE_PIN_ADD);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_register);

int
dpll_shared_pin_register(struct dpll_device *dpll_pin_owner,
			 struct dpll_device *dpll, u32 pin_idx,
			 struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin *pin;
	int ret;

	mutex_lock(&dpll_pin_owner->lock);
	pin = dpll_pin_get_by_idx(dpll_pin_owner, pin_idx);
	if (!pin) {
		ret = -EINVAL;
		goto unlock;
	}
	ret = dpll_pin_register(dpll, pin, ops, priv);
unlock:
	mutex_unlock(&dpll_pin_owner->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_shared_pin_register);

int dpll_pin_deregister(struct dpll_device *dpll, struct dpll_pin *pin)
{
	int ret = 0;

	if (xa_empty(&dpll->pins))
		return -ENOENT;

	mutex_lock(&dpll->lock);
	ret = pin_deregister_from_xa(&dpll->pins, pin);
	if (!ret)
		pin_ref_dpll_del(pin, dpll);
	mutex_unlock(&dpll->lock);
	if (!ret)
		dpll_pin_notify(dpll, pin, DPLL_CHANGE_PIN_DEL);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_deregister);

void dpll_pin_free(struct dpll_pin *pin)
{
	if (!xa_empty(&pin->ref_dplls))
		return;

	xa_destroy(&pin->ref_dplls);
	kfree(pin);
}
EXPORT_SYMBOL_GPL(dpll_pin_free);

int dpll_muxed_pin_register(struct dpll_device *dpll,
			    struct dpll_pin *parent_pin, struct dpll_pin *pin,
			    struct dpll_pin_ops *ops, void *priv)
{
	int ret;

	if (!parent_pin || !pin)
		return -EINVAL;

	mutex_lock(&dpll->lock);
	ret = dpll_alloc_pin_on_xa(&dpll->pins, pin);
	if (!ret)
		ret = pin_ref_dpll_add(pin, dpll, ops, priv);
	if (!ret)
		pin->parent_pin = parent_pin;
	mutex_unlock(&dpll->lock);
	if (!ret)
		dpll_pin_notify(dpll, pin, DPLL_CHANGE_PIN_ADD);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_muxed_pin_register);

struct dpll_pin
*dpll_pin_get_by_description(struct dpll_device *dpll, const char *description)
{
	struct dpll_pin *pos, *pin = NULL;
	unsigned long index;

	mutex_lock(&dpll->lock);
	xa_for_each(&dpll->pins, index, pos) {
		if (!strncmp(pos->description, description, PIN_DESC_LEN)) {
			pin = pos;
			break;
		}
	}
	mutex_unlock(&dpll->lock);

	return pin;
}
EXPORT_SYMBOL_GPL(dpll_pin_get_by_description);

static struct dpll_pin
*dpll_pin_get_by_idx_from_xa(struct xarray *xa_pins, int idx)
{
	struct dpll_pin *pos;
	unsigned long index;

	xa_for_each_marked(xa_pins, index, pos, PIN_REGISTERED) {
		if (pos->idx == idx)
			return pos;
	}

	return NULL;
}

struct dpll_pin *dpll_pin_get_by_idx(struct dpll_device *dpll, int idx)
{
	return dpll_pin_get_by_idx_from_xa(&dpll->pins, idx);
}
EXPORT_SYMBOL_GPL(dpll_pin_get_by_idx);

struct dpll_pin *dpll_pin_first(struct dpll_device *dpll, unsigned long *index)
{
	*index = 0;

	return xa_find(&dpll->pins, index, LONG_MAX, PIN_REGISTERED);
}

struct dpll_pin *dpll_pin_next(struct dpll_device *dpll, unsigned long *index)
{
	return xa_find_after(&dpll->pins, index, LONG_MAX, PIN_REGISTERED);
}

struct dpll_device *dpll_first(unsigned long *index)
{
	*index = 0;

	return xa_find(&dpll_device_xa, index, LONG_MAX, DPLL_REGISTERED);
}

struct dpll_device *dpll_next(unsigned long *index)
{
	return xa_find_after(&dpll_device_xa, index, LONG_MAX, DPLL_REGISTERED);
}

static int
dpll_notify_pin_change_attr(struct dpll_device *dpll, struct dpll_pin *pin,
			    const struct dpll_pin_attr *attr)
{
	enum dpll_event_change change;
	int ret = 0;

	if (dpll_pin_attr_valid(DPLLA_PIN_TYPE, attr)) {
		change = DPLL_CHANGE_PIN_TYPE;
		ret = dpll_pin_notify(dpll, pin, change);
	}
	if (!ret && dpll_pin_attr_valid(DPLLA_PIN_SIGNAL_TYPE, attr)) {
		change = DPLL_CHANGE_PIN_SIGNAL_TYPE;
		ret = dpll_pin_notify(dpll, pin, change);
	}
	if (!ret && dpll_pin_attr_valid(DPLLA_PIN_CUSTOM_FREQ, attr)) {
		change = DPLL_CHANGE_PIN_CUSTOM_FREQ;
		ret = dpll_pin_notify(dpll, pin, change);
	}
	if (!ret && dpll_pin_attr_valid(DPLLA_PIN_STATE, attr)) {
		change = DPLL_CHANGE_PIN_STATE;
		ret = dpll_pin_notify(dpll, pin, change);
	}
	if (!ret && dpll_pin_attr_valid(DPLLA_PIN_PRIO, attr)) {
		change = DPLL_CHANGE_PIN_PRIO;
		ret = dpll_pin_notify(dpll, pin, change);
	}

	return ret;
}

static int dpll_notify_device_change_attr(struct dpll_device *dpll,
					  const struct dpll_attr *attr)
{
	int ret = 0;

	if (dpll_attr_valid(DPLLA_MODE, attr))
		ret = dpll_device_notify(dpll, DPLL_CHANGE_MODE);
	if (!ret && dpll_attr_valid(DPLLA_SOURCE_PIN_IDX, attr))
		ret = dpll_device_notify(dpll, DPLL_CHANGE_SOURCE_PIN);

	return ret;
}

static struct pin_ref_dpll
*dpll_pin_find_ref(struct dpll_device *dpll, struct dpll_pin *pin)
{
	struct pin_ref_dpll *ref;
	unsigned long index;

	xa_for_each(&pin->ref_dplls, index, ref) {
		if (ref->dpll != dpll)
			continue;
		else
			return ref;
	}

	return NULL;
}

static int
dpll_pin_set_attr_single_ref(struct dpll_device *dpll, struct dpll_pin *pin,
			     const struct dpll_pin_attr *attr)
{
	struct pin_ref_dpll *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	mutex_lock(&ref->dpll->lock);
	ret = ref->ops->set(ref->dpll, pin, attr);
	if (!ret)
		dpll_notify_pin_change_attr(dpll, pin, attr);
	mutex_unlock(&ref->dpll->lock);

	return ret;
}

static int
dpll_pin_set_attr_all_refs(struct dpll_pin *pin,
			   const struct dpll_pin_attr *attr)
{
	struct pin_ref_dpll *ref;
	unsigned long index;
	int ret;

	xa_for_each(&pin->ref_dplls, index, ref) {
		if (!ref->dpll)
			return -EFAULT;
		if (!ref || !ref->ops || !ref->ops->set)
			return -EOPNOTSUPP;
		mutex_lock(&ref->dpll->lock);
		ret = ref->ops->set(ref->dpll, pin, attr);
		mutex_unlock(&ref->dpll->lock);
		if (!ret)
			dpll_notify_pin_change_attr(ref->dpll, pin, attr);
	}

	return ret;
}

int dpll_pin_set_attr(struct dpll_device *dpll, struct dpll_pin *pin,
		      const struct dpll_pin_attr *attr)
{
	struct dpll_pin_attr *tmp_attr;
	int ret;

	tmp_attr = dpll_pin_attr_alloc();
	if (!tmp_attr)
		return -ENOMEM;
	ret = dpll_pin_attr_prep_common(tmp_attr, attr);
	if (ret < 0)
		goto tmp_free;
	if (ret == PIN_ATTR_CHANGE) {
		ret = dpll_pin_set_attr_all_refs(pin, tmp_attr);
		if (ret)
			goto tmp_free;
	}

	ret = dpll_pin_attr_prep_exclusive(tmp_attr, attr);
	if (ret < 0)
		goto tmp_free;
	if (ret == PIN_ATTR_CHANGE)
		ret = dpll_pin_set_attr_single_ref(dpll, pin, tmp_attr);

tmp_free:
	dpll_pin_attr_free(tmp_attr);
	return ret;
}

int dpll_pin_get_attr(struct dpll_device *dpll, struct dpll_pin *pin,
		      struct dpll_pin_attr *attr)
{
	struct pin_ref_dpll *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->get)
		return -EOPNOTSUPP;

	ret = ref->ops->get(dpll, pin, attr);
	if (ret)
		return -EAGAIN;

	return ret;
}

const char *dpll_pin_get_description(struct dpll_pin *pin)
{
	return pin->description;
}

struct dpll_pin *dpll_pin_get_parent(struct dpll_pin *pin)
{
	return pin->parent_pin;
}

int dpll_set_attr(struct dpll_device *dpll, const struct dpll_attr *attr)
{
	int ret;

	if (dpll_attr_valid(DPLLA_SOURCE_PIN_IDX, attr)) {
		struct pin_ref_dpll *ref;
		struct dpll_pin *pin;
		u32 source_idx;

		ret = dpll_attr_source_idx_get(attr, &source_idx);
		if (ret)
			return -EINVAL;
		pin = dpll_pin_get_by_idx(dpll, source_idx);
		if (!pin)
			return -ENXIO;
		ref = dpll_pin_find_ref(dpll, pin);
		if (!ref || !ref->ops)
			return -EFAULT;
		if (!ref->ops->select)
			return -ENODEV;
		dpll_lock(ref->dpll);
		ret = ref->ops->select(ref->dpll, pin);
		dpll_unlock(ref->dpll);
		if (ret)
			return -EINVAL;
		dpll_notify_device_change_attr(dpll, attr);
	}

	if (dpll_attr_valid(DPLLA_MODE, attr)) {
		dpll_lock(dpll);
		ret = dpll->ops->set(dpll, attr);
		dpll_unlock(dpll);
		if (ret)
			return -EINVAL;
	}
	dpll_notify_device_change_attr(dpll, attr);

	return ret;
}

int dpll_get_attr(struct dpll_device *dpll, struct dpll_attr *attr)
{
	if (!dpll)
		return -ENODEV;
	if (!dpll->ops || !dpll->ops->get)
		return -EOPNOTSUPP;
	if (dpll->ops->get(dpll, attr))
		return -EAGAIN;

	return 0;
}

void dpll_lock(struct dpll_device *dpll)
{
	mutex_lock(&dpll->lock);
}

void dpll_unlock(struct dpll_device *dpll)
{
	mutex_unlock(&dpll->lock);
}

void *dpll_priv(struct dpll_device *dpll)
{
	return dpll->priv;
}
EXPORT_SYMBOL_GPL(dpll_priv);

void *dpll_pin_priv(struct dpll_device *dpll, struct dpll_pin *pin)
{
	struct pin_ref_dpll *ref = dpll_pin_find_ref(dpll, pin);

	if (!ref)
		return NULL;

	return ref->priv;
}
EXPORT_SYMBOL_GPL(dpll_pin_priv);

static int __init dpll_init(void)
{
	int ret;

	ret = dpll_netlink_init();
	if (ret)
		goto error;

	ret = class_register(&dpll_class);
	if (ret)
		goto unregister_netlink;

	return 0;

unregister_netlink:
	dpll_netlink_finish();
error:
	mutex_destroy(&dpll_device_xa_lock);
	return ret;
}
subsys_initcall(dpll_init);
