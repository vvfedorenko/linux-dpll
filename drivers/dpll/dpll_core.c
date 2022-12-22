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
	char description[DPLL_PIN_DESC_LEN];
};
static DEFINE_MUTEX(dpll_device_xa_lock);

static DEFINE_XARRAY_FLAGS(dpll_device_xa, XA_FLAGS_ALLOC);
#define DPLL_REGISTERED		XA_MARK_1
#define PIN_REGISTERED		XA_MARK_1

#define ASSERT_DPLL_REGISTERED(d)                                           \
	WARN_ON_ONCE(!xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))
#define ASSERT_DPLL_NOT_REGISTERED(d)                                      \
	WARN_ON_ONCE(xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))

struct dpll_pin_ref {
	struct dpll_device *dpll;
	struct dpll_pin_ops *ops;
	void *priv;
};

/**
 * dpll_device_get_by_id - find dpll device by it's id
 * @id: id of searched dpll
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
struct dpll_device *dpll_device_get_by_id(int id)
{
	struct dpll_device *dpll = NULL;

	if (xa_get_mark(&dpll_device_xa, id, DPLL_REGISTERED))
		dpll = xa_load(&dpll_device_xa, id);

	return dpll;
}

/**
 * dpll_device_get_by_name - find dpll device by it's id
 * @name: name of searched dpll
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
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

struct dpll_device *dpll_device_get_by_clock_id(u64 clock_id,
						enum dpll_type type, u8 idx)
{
	struct dpll_device *dpll, *ret = NULL;
	unsigned long index;

	mutex_lock(&dpll_device_xa_lock);
	xa_for_each_marked(&dpll_device_xa, index, dpll, DPLL_REGISTERED) {
		if (dpll->clock_id == clock_id) {
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
EXPORT_SYMBOL_GPL(dpll_device_get_by_clock_id);

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

struct dpll_device
*dpll_device_alloc(struct dpll_device_ops *ops, enum dpll_type type,
		   const u64 clock_id, enum dpll_clock_class clock_class,
		   u8 dev_driver_idx, void *priv, struct device *parent)
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
	dpll->dev_driver_idx = dev_driver_idx;
	dpll->clock_id = clock_id;
	dpll->clock_class = clock_class;

	mutex_lock(&dpll_device_xa_lock);
	ret = xa_alloc(&dpll_device_xa, &dpll->id, dpll,
		       xa_limit_16b, GFP_KERNEL);
	if (ret)
		goto error;
	dev_set_name(&dpll->dev, "dpll_%s_%d_%d", dev_name(parent), type,
		     dev_driver_idx);
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

/**
 * dpll_device_unregister - unregister dpll device
 * @dpll: registered dpll pointer
 *
 * Note: It does not free the memory
 */
void dpll_device_unregister(struct dpll_device *dpll)
{
	ASSERT_DPLL_REGISTERED(dpll);

	mutex_lock(&dpll_device_xa_lock);
	xa_erase(&dpll_device_xa, dpll->id);
	dpll_notify_device_delete(dpll);
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_device_unregister);

/**
 * dpll_id - return dpll id
 * @dpll: registered dpll pointer
 *
 * Return: dpll id.
 */
u32 dpll_id(struct dpll_device *dpll)
{
	return dpll->id;
}

/**
 * dpll_pin_idx - return index of a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 *
 * Return: index of a pin or PIN_IDX_INVALID if not found.
 */
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

struct dpll_pin *dpll_pin_alloc(const char *description,
				const enum dpll_pin_type pin_type)
{
	struct dpll_pin *pin = kzalloc(sizeof(struct dpll_pin), GFP_KERNEL);

	if (!pin)
		return ERR_PTR(-ENOMEM);
	if (pin_type <= DPLL_PIN_TYPE_UNSPEC ||
	    pin_type > DPLL_PIN_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	strncpy(pin->description, description, DPLL_PIN_DESC_LEN);
	pin->description[DPLL_PIN_DESC_LEN - 1] = '\0';
	xa_init_flags(&pin->ref_dplls, XA_FLAGS_ALLOC);
	pin->type = pin_type;

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
		    !strncmp(pos->description, pin->description,
			     DPLL_PIN_DESC_LEN))
			return -EEXIST;
	}

	ret = xa_alloc(pins, &pin->idx, pin, xa_limit_16b, GFP_KERNEL);
	if (!ret)
		xa_set_mark(pins, pin->idx, PIN_REGISTERED);

	return ret;
}

static int dpll_pin_ref_add(struct dpll_pin *pin, struct dpll_device *dpll,
			    struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin_ref *ref, *pos;
	unsigned long index;
	u32 idx;

	ref = kzalloc(sizeof(struct dpll_pin_ref), GFP_KERNEL);
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

static void dpll_pin_ref_del(struct dpll_pin *pin, struct dpll_device *dpll)
{
	struct dpll_pin_ref *pos;
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
		ret = dpll_pin_ref_add(pin, dpll, ops, priv);
		if (ret)
			pin_deregister_from_xa(&dpll->pins, pin);
	}
	mutex_unlock(&dpll->lock);
	if (!ret)
		dpll_pin_notify(dpll, pin, DPLL_CHANGE_PIN_ADD);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_register);

struct dpll_pin *dpll_pin_get_by_idx_from_xa(struct xarray *xa_pins, int idx)
{
	struct dpll_pin *pos;
	unsigned long index;

	xa_for_each_marked(xa_pins, index, pos, PIN_REGISTERED) {
		if (pos->idx == idx)
			return pos;
	}

	return NULL;
}

/**
 * dpll_pin_get_by_idx - find a pin by its index
 * @dpll: dpll device pointer
 * @idx: index of pin
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share pin already registered with existing dpll device.
 *
 * Return: pointer if pin was found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_get_by_idx(struct dpll_device *dpll, int idx)
{
	return dpll_pin_get_by_idx_from_xa(&dpll->pins, idx);
}

	struct dpll_pin
*dpll_pin_get_by_description(struct dpll_device *dpll, const char *description)
{
	struct dpll_pin *pos, *pin = NULL;
	unsigned long index;

	xa_for_each(&dpll->pins, index, pos) {
		if (!strncmp(pos->description, description,
			     DPLL_PIN_DESC_LEN)) {
			pin = pos;
			break;
		}
	}

	return pin;
}

int
dpll_shared_pin_register(struct dpll_device *dpll_pin_owner,
			 struct dpll_device *dpll,
			 const char *shared_pin_description,
			 struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin *pin;
	int ret;

	mutex_lock(&dpll_pin_owner->lock);
	pin = dpll_pin_get_by_description(dpll_pin_owner,
					  shared_pin_description);
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
		dpll_pin_ref_del(pin, dpll);
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
			    const char *parent_pin_description,
			    struct dpll_pin *pin,
			    struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin *parent_pin;
	int ret;

	if (!parent_pin_description || !pin)
		return -EINVAL;

	mutex_lock(&dpll->lock);
	parent_pin = dpll_pin_get_by_description(dpll, parent_pin_description);
	if (!parent_pin)
		return -EINVAL;
	if (parent_pin->type != DPLL_PIN_TYPE_MUX)
		return -EPERM;
	ret = dpll_alloc_pin_on_xa(&dpll->pins, pin);
	if (!ret)
		ret = dpll_pin_ref_add(pin, dpll, ops, priv);
	if (!ret)
		pin->parent_pin = parent_pin;
	mutex_unlock(&dpll->lock);
	if (!ret)
		dpll_pin_notify(dpll, pin, DPLL_CHANGE_PIN_ADD);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_muxed_pin_register);

/**
 * dpll_pin_first - get first registered pin
 * @dpll: registered dpll pointer
 * @index: found pin index (out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_first(struct dpll_device *dpll, unsigned long *index)
{
	*index = 0;

	return xa_find(&dpll->pins, index, LONG_MAX, PIN_REGISTERED);
}

/**
 * dpll_pin_next - get next registered pin to the relative pin
 * @dpll: registered dpll pointer
 * @index: relative pin index (in and out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_next(struct dpll_device *dpll, unsigned long *index)
{
	return xa_find_after(&dpll->pins, index, LONG_MAX, PIN_REGISTERED);
}

/**
 * dpll_first - get first registered dpll device
 * @index: found dpll index (out)
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
struct dpll_device *dpll_first(unsigned long *index)
{
	*index = 0;

	return xa_find(&dpll_device_xa, index, LONG_MAX, DPLL_REGISTERED);
}

/**
 * dpll_pin_next - get next registered dpll device to the relative pin
 * @index: relative dpll index (in and out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_device *dpll_next(unsigned long *index)
{
	return xa_find_after(&dpll_device_xa, index, LONG_MAX, DPLL_REGISTERED);
}

static struct dpll_pin_ref
*dpll_pin_find_ref(const struct dpll_device *dpll, const struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;
	unsigned long index;

	xa_for_each((struct xarray *)&pin->ref_dplls, index, ref) {
		if (ref->dpll != dpll)
			continue;
		else
			return ref;
	}

	return NULL;
}

/**
 * dpll_pin_type_get - get type of a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @type: on success - configured pin type
 *
 * Return:
 * * 0 - successfully got pin's type
 * * negative - failed to get pin's type
 */
int dpll_pin_type_get(const struct dpll_device *dpll,
		      const struct dpll_pin *pin,
		      enum dpll_pin_type *type)
{
	if (!pin)
		return -ENODEV;
	*type = pin->type;

	return 0;
}

/**
 * dpll_pin_signal_type_get - get signal type of a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @type: on success - configured signal type
 *
 * Return:
 * * 0 - successfully got signal type
 * * negative - failed to obtain signal type
 */
int dpll_pin_signal_type_get(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     enum dpll_pin_signal_type *type)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->signal_type_get)
		return -EOPNOTSUPP;
	ret = ref->ops->signal_type_get(ref->dpll, pin, type);

	return ret;
}

/**
 * dpll_pin_signal_type_set - set signal type of a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @type: type to be set
 *
 * Return:
 * * 0 - signal type set
 * * negative - failed to set signal type
 */
int dpll_pin_signal_type_set(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     const enum dpll_pin_signal_type type)
{
	struct dpll_pin_ref *ref;
	unsigned long index;
	int ret;

	xa_for_each((struct xarray *)&pin->ref_dplls, index, ref) {
		if (!ref->dpll)
			return -EFAULT;
		if (!ref || !ref->ops || !ref->ops->signal_type_set)
			return -EOPNOTSUPP;
		if (ref->dpll != dpll)
			mutex_lock(&ref->dpll->lock);
		ret = ref->ops->signal_type_set(ref->dpll, pin, type);
		if (ref->dpll != dpll)
			mutex_unlock(&ref->dpll->lock);
		if (ret)
			return ret;
	}

	return ret;
}

/**
 * dpll_pin_signal_type_supported - check if signal type is supported on a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @type: type being checked
 * @supported: on success - if given signal type is supported
 *
 * Return:
 * * 0 - successfully got supported signal type
 * * negative - failed to obtain supported signal type
 */
int dpll_pin_signal_type_supported(const struct dpll_device *dpll,
				   const struct dpll_pin *pin,
				   const enum dpll_pin_signal_type type,
				   bool *supported)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->signal_type_supported)
		return -EOPNOTSUPP;
	*supported = ref->ops->signal_type_supported(ref->dpll, pin, type);

	return 0;
}

/**
 * dpll_pin_state_active - check if given state is active on a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @state: state being checked
 * @active: on success - if state is active
 *
 * Return:
 * * 0 - successfully checked if state is active
 * * negative - failed to check for active state
 */
int dpll_pin_state_active(const struct dpll_device *dpll,
			  const struct dpll_pin *pin,
			  const enum dpll_pin_state state,
			  bool *active)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->state_active)
		return -EOPNOTSUPP;
	*active = ref->ops->state_active(ref->dpll, pin, state);

	return 0;
}

/**
 * dpll_pin_state_supported - check if given state is supported on a pin
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @state: state being checked
 * @supported: on success - if state is supported
 *
 * Return:
 * * 0 - successfully checked if state is supported
 * * negative - failed to check for supported state
 */
int dpll_pin_state_supported(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     const enum dpll_pin_state state,
			     bool *supported)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->state_supported)
		return -EOPNOTSUPP;
	*supported = ref->ops->state_supported(ref->dpll, pin, state);

	return 0;
}

/**
 * dpll_pin_state_set - set pin's state
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @state: state being set
 *
 * Return:
 * * 0 - successfully set the state
 * * negative - failed to set the state
 */
int dpll_pin_state_set(const struct dpll_device *dpll,
		       const struct dpll_pin *pin,
		       const enum dpll_pin_state state)
{
	struct dpll_pin_ref *ref;
	unsigned long index;
	int ret;

	xa_for_each((struct xarray *)&pin->ref_dplls, index, ref) {
		if (!ref)
			return -ENODEV;
		if (!ref->ops || !ref->ops->state_enable)
			return -EOPNOTSUPP;
		if (ref->dpll != dpll)
			mutex_lock(&ref->dpll->lock);
		ret = ref->ops->state_enable(ref->dpll, pin, state);
		if (ref->dpll != dpll)
			mutex_unlock(&ref->dpll->lock);
		if (ret)
			return ret;
	}

	return ret;
}

/**
 * dpll_pin_custom_freq_get - get pin's custom frequency
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @freq: on success - custom frequency of a pin
 *
 * Return:
 * * 0 - successfully got custom frequency
 * * negative - failed to obtain custom frequency
 */
int dpll_pin_custom_freq_get(const struct dpll_device *dpll,
			     const struct dpll_pin *pin, u32 *freq)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->custom_freq_get)
		return -EOPNOTSUPP;
	ret = ref->ops->custom_freq_get(ref->dpll, pin, freq);

	return ret;
}

/**
 * dpll_pin_custom_freq_set - set pin's custom frequency
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @freq: custom frequency to be set
 *
 * Return:
 * * 0 - successfully set custom frequency
 * * negative - failed to set custom frequency
 */
int dpll_pin_custom_freq_set(const struct dpll_device *dpll,
			     const struct dpll_pin *pin, const u32 freq)
{
	enum dpll_pin_signal_type type;
	struct dpll_pin_ref *ref;
	unsigned long index;
	int ret;

	xa_for_each((struct xarray *)&pin->ref_dplls, index, ref) {
		if (!ref)
			return -ENODEV;
		if (!ref->ops || !ref->ops->custom_freq_set ||
		    !ref->ops->signal_type_get)
			return -EOPNOTSUPP;
		if (dpll != ref->dpll)
			mutex_lock(&ref->dpll->lock);
		ret = ref->ops->signal_type_get(dpll, pin, &type);
		if (!ret && type == DPLL_PIN_SIGNAL_TYPE_CUSTOM_FREQ)
			ret = ref->ops->custom_freq_set(ref->dpll, pin, freq);
		if (dpll != ref->dpll)
			mutex_unlock(&ref->dpll->lock);
		if (ret)
			return ret;
	}

	return ret;
}

/**
 * dpll_pin_prio_get - get pin's prio on dpll
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @prio: on success - priority of a pin on a dpll
 *
 * Return:
 * * 0 - successfully got priority
 * * negative - failed to obtain priority
 */
int dpll_pin_prio_get(const struct dpll_device *dpll,
		      const struct dpll_pin *pin, u32 *prio)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->prio_get)
		return -EOPNOTSUPP;
	ret = ref->ops->prio_get(ref->dpll, pin, prio);

	return ret;
}

/**
 * dpll_pin_prio_set - set pin's prio on dpll
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @prio: priority of a pin to be set on a dpll
 *
 * Return:
 * * 0 - successfully set priority
 * * negative - failed to set the priority
 */
int dpll_pin_prio_set(const struct dpll_device *dpll,
		      const struct dpll_pin *pin, const u32 prio)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->prio_set)
		return -EOPNOTSUPP;
	ret = ref->ops->prio_set(ref->dpll, pin, prio);

	return ret;
}

/**
 * dpll_pin_netifindex_get - get pin's netdev iterface index
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @netifindex: on success - index of a netdevice associated with pin
 *
 * Return:
 * * 0 - successfully got netdev interface index
 * * negative - failed to obtain netdev interface index
 */
int dpll_pin_netifindex_get(const struct dpll_device *dpll,
			    const struct dpll_pin *pin,
			    int *netifindex)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);
	int ret;

	if (!ref)
		return -ENODEV;
	if (!ref->ops || !ref->ops->net_if_idx_get)
		return -EOPNOTSUPP;
	ret = ref->ops->net_if_idx_get(ref->dpll, pin, netifindex);

	return ret;
}

/**
 * dpll_pin_description - provide pin's description string
 * @pin: registered pin pointer
 *
 * Return: pointer to a description string.
 */
const char *dpll_pin_description(struct dpll_pin *pin)
{
	return pin->description;
}

/**
 * dpll_pin_parent - provide pin's parent pin if available
 * @pin: registered pin pointer
 *
 * Return: pointer to aparent if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_parent(struct dpll_pin *pin)
{
	return pin->parent_pin;
}

/**
 * dpll_mode_set - handler for dpll mode set
 * @dpll: registered dpll pointer
 * @mode: mode to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_mode_set(struct dpll_device *dpll, const enum dpll_mode mode)
{
	int ret;

	if (!dpll->ops || !dpll->ops)
		return -EOPNOTSUPP;

	ret = dpll->ops->mode_set(dpll, mode);

	return ret;
}

/**
 * dpll_source_idx_set - handler for selecting a dpll's source
 * @dpll: registered dpll pointer
 * @source_pin_idx: index of a source pin to e selected
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_source_idx_set(struct dpll_device *dpll, const u32 source_pin_idx)
{
	struct dpll_pin_ref *ref;
	struct dpll_pin *pin;
	int ret;

	pin = dpll_pin_get_by_idx_from_xa(&dpll->pins, source_pin_idx);
	if (!pin)
		return -ENXIO;
	ref = dpll_pin_find_ref(dpll, pin);
	if (!ref || !ref->ops)
		return -EFAULT;
	if (!ref->ops->select)
		return -ENODEV;
	ret = ref->ops->select(ref->dpll, pin);

	return ret;
}

/**
 * dpll_lock - locks the dpll using internal mutex
 * @dpll: registered dpll pointer
 */
void dpll_lock(struct dpll_device *dpll)
{
	mutex_lock(&dpll->lock);
}

/**
 * dpll_unlock - unlocks the dpll using internal mutex
 * @dpll: registered dpll pointer
 */
void dpll_unlock(struct dpll_device *dpll)
{
	mutex_unlock(&dpll->lock);
}

enum dpll_pin_type dpll_pin_type(const struct dpll_pin *pin)
{
	return pin->type;
}

void *dpll_priv(const struct dpll_device *dpll)
{
	return dpll->priv;
}
EXPORT_SYMBOL_GPL(dpll_priv);

void *dpll_pin_priv(const struct dpll_device *dpll, const struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref = dpll_pin_find_ref(dpll, pin);

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
