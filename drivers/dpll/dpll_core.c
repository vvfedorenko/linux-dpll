// SPDX-License-Identifier: GPL-2.0
/*
 *  dpll_core.c - Generic DPLL Management class support.
 *
 *  Copyright (c) 2023 Meta Platforms, Inc. and affiliates
 *  Copyright (c) 2023 Intel Corporation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dpll_core.h"

DEFINE_MUTEX(dpll_device_xa_lock);
DEFINE_MUTEX(dpll_pin_xa_lock);

DEFINE_XARRAY_FLAGS(dpll_device_xa, XA_FLAGS_ALLOC);
DEFINE_XARRAY_FLAGS(dpll_pin_xa, XA_FLAGS_ALLOC);

#define ASSERT_DPLL_REGISTERED(d)                                          \
	WARN_ON_ONCE(!xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))
#define ASSERT_DPLL_NOT_REGISTERED(d)                                      \
	WARN_ON_ONCE(xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))

/**
 * dpll_device_get_by_id - find dpll device by it's id
 * @id: id of searched dpll
 *
 * Return:
 * * dpll_device struct if found
 * * NULL otherwise
 */
struct dpll_device *dpll_device_get_by_id(int id)
{
	if (xa_get_mark(&dpll_device_xa, id, DPLL_REGISTERED))
		return xa_load(&dpll_device_xa, id);

	return NULL;
}

/**
 * dpll_device_get_by_name - find dpll device by it's id
 * @bus_name: bus name of searched dpll
 * @dev_name: dev name of searched dpll
 *
 * Return:
 * * dpll_device struct if found
 * * NULL otherwise
 */
struct dpll_device *
dpll_device_get_by_name(const char *bus_name, const char *device_name)
{
	struct dpll_device *dpll, *ret = NULL;
	unsigned long i;

	xa_for_each_marked(&dpll_device_xa, i, dpll, DPLL_REGISTERED) {
		if (!strcmp(dev_bus_name(&dpll->dev), bus_name) &&
		    !strcmp(dev_name(&dpll->dev), device_name)) {
			ret = dpll;
			break;
		}
	}

	return ret;
}

/**
 * dpll_xa_ref_pin_add - add pin reference to a given xarray
 * @xa_pins: dpll_pin_ref xarray holding pins
 * @pin: pin being added
 * @ops: ops for a pin
 * @priv: pointer to private data of owner
 *
 * Allocate and create reference of a pin or increase refcount on existing pin
 * reference on given xarray.
 *
 * Return:
 * * 0 on success
 * * -ENOMEM on failed allocation
 */
static int
dpll_xa_ref_pin_add(struct xarray *xa_pins, struct dpll_pin *pin,
		    const struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin_ref *ref;
	unsigned long i;
	u32 idx;
	int ret;

	xa_for_each(xa_pins, i, ref) {
		if (ref->pin == pin) {
			refcount_inc(&ref->refcount);
			return 0;
		}
	}

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return -ENOMEM;
	ref->pin = pin;
	ref->ops = ops;
	ref->priv = priv;
	ret = xa_alloc(xa_pins, &idx, ref, xa_limit_16b, GFP_KERNEL);
	if (!ret)
		refcount_set(&ref->refcount, 1);
	else
		kfree(ref);

	return ret;
}

/**
 * dpll_xa_ref_pin_del - remove reference of a pin from xarray
 * @xa_pins: dpll_pin_ref xarray holding pins
 * @pin: pointer to a pin
 *
 * Decrement refcount of existing pin reference on given xarray.
 * If all references are dropped, delete the reference and free its memory.
 *
 * Return:
 * * 0 on success
 * * -EINVAL if reference to a pin was not found
 */
static int dpll_xa_ref_pin_del(struct xarray *xa_pins, struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;
	unsigned long i;

	xa_for_each(xa_pins, i, ref) {
		if (ref->pin == pin) {
			if (refcount_dec_and_test(&ref->refcount)) {
				xa_erase(xa_pins, i);
				kfree(ref);
			}
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * dpll_xa_ref_pin_find - find pin reference on xarray
 * @xa_pins: dpll_pin_ref xarray holding pins
 * @pin: pointer to a pin
 *
 * Search for pin reference struct of a given pin on given xarray.
 *
 * Return:
 * * pin reference struct pointer on success
 * * NULL - reference to a pin was not found
 */
struct dpll_pin_ref *
dpll_xa_ref_pin_find(struct xarray *xa_pins, const struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;
	unsigned long i;

	xa_for_each(xa_pins, i, ref) {
		if (ref->pin == pin)
			return ref;
	}

	return NULL;
}

/**
 * dpll_xa_ref_dpll_add - add dpll reference to a given xarray
 * @xa_dplls: dpll_pin_ref xarray holding dplls
 * @dpll: dpll being added
 * @ops: pin-reference ops for a dpll
 * @priv: pointer to private data of owner
 *
 * Allocate and create reference of a dpll-pin ops or increase refcount
 * on existing dpll reference on given xarray.
 *
 * Return:
 * * 0 on success
 * * -ENOMEM on failed allocation
 */
static int
dpll_xa_ref_dpll_add(struct xarray *xa_dplls, struct dpll_device *dpll,
		     const struct dpll_pin_ops *ops, void *priv)
{
	struct dpll_pin_ref *ref;
	unsigned long i;
	u32 idx;
	int ret;

	xa_for_each(xa_dplls, i, ref) {
		if (ref->dpll == dpll) {
			refcount_inc(&ref->refcount);
			return 0;
		}
	}
	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return -ENOMEM;
	ref->dpll = dpll;
	ref->ops = ops;
	ref->priv = priv;
	ret = xa_alloc(xa_dplls, &idx, ref, xa_limit_16b, GFP_KERNEL);
	if (!ret)
		refcount_set(&ref->refcount, 1);
	else
		kfree(ref);

	return ret;
}

/**
 * dpll_xa_ref_dpll_del - remove reference of a dpll from xarray
 * @xa_dplls: dpll_pin_ref xarray holding dplls
 * @dpll: pointer to a dpll to remove
 *
 * Decrement refcount of existing dpll reference on given xarray.
 * If all references are dropped, delete the reference and free its memory.
 */
static void
dpll_xa_ref_dpll_del(struct xarray *xa_dplls, struct dpll_device *dpll)
{
	struct dpll_pin_ref *ref;
	unsigned long i;

	xa_for_each(xa_dplls, i, ref) {
		if (ref->dpll == dpll) {
			if (refcount_dec_and_test(&ref->refcount)) {
				xa_erase(xa_dplls, i);
				kfree(ref);
			}
			break;
		}
	}
}

/**
 * dpll_xa_ref_dpll_find - find dpll reference on xarray
 * @xa_dplls: dpll_pin_ref xarray holding dplls
 * @dpll: pointer to a dpll
 *
 * Search for dpll-pin ops reference struct of a given dpll on given xarray.
 *
 * Return:
 * * pin reference struct pointer on success
 * * NULL - reference to a pin was not found
 */
struct dpll_pin_ref *
dpll_xa_ref_dpll_find(struct xarray *xa_refs, const struct dpll_device *dpll)
{
	struct dpll_pin_ref *ref;
	unsigned long i;

	xa_for_each(xa_refs, i, ref) {
		if (ref->dpll == dpll)
			return ref;
	}

	return NULL;
}


/**
 * dpll_device_alloc - allocate the memory for dpll device
 * @clock_id: clock_id of creator
 * @dev_driver_id: id given by dev driver
 * @module: reference to registering module
 *
 * Allocates memory and initialize dpll device, hold its reference on global
 * xarray.
 *
 * Return:
 * * dpll_device struct pointer if succeeded
 * * ERR_PTR(X) - failed allocation
 */
static struct dpll_device *
dpll_device_alloc(const u64 clock_id, u32 dev_driver_id, struct module *module)
{
	struct dpll_device *dpll;
	int ret;

	dpll = kzalloc(sizeof(*dpll), GFP_KERNEL);
	if (!dpll)
		return ERR_PTR(-ENOMEM);
	refcount_set(&dpll->refcount, 1);
	dpll->dev_driver_id = dev_driver_id;
	dpll->clock_id = clock_id;
	dpll->module = module;
	ret = xa_alloc(&dpll_device_xa, &dpll->id, dpll,
		       xa_limit_16b, GFP_KERNEL);
	if (ret) {
		kfree(dpll);
		return ERR_PTR(ret);
	}
	xa_init_flags(&dpll->pin_refs, XA_FLAGS_ALLOC);

	return dpll;
}

/**
 * dpll_device_get - find existing or create new dpll device
 * @clock_id: clock_id of creator
 * @dev_driver_id: id given by dev driver
 * @module: reference to registering module
 *
 * Get existing object of a dpll device, unique for given arguments.
 * Create new if doesn't exist yet.
 *
 * Return:
 * * valid dpll_device struct pointer if succeeded
 * * ERR_PTR of an error
 */
struct dpll_device *
dpll_device_get(u64 clock_id, u32 dev_driver_id, struct module *module)
{
	struct dpll_device *dpll, *ret = NULL;
	unsigned long index;

	mutex_lock(&dpll_device_xa_lock);
	xa_for_each(&dpll_device_xa, index, dpll) {
		if (dpll->clock_id == clock_id &&
		    dpll->dev_driver_id == dev_driver_id &&
		    dpll->module == module) {
			ret = dpll;
			refcount_inc(&ret->refcount);
			break;
		}
	}
	if (!ret)
		ret = dpll_device_alloc(clock_id, dev_driver_id, module);
	mutex_unlock(&dpll_device_xa_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_device_get);

/**
 * dpll_device_put - decrease the refcount and free memory if possible
 * @dpll: dpll_device struct pointer
 *
 * Drop reference for a dpll device, if all references are gone, delete
 * dpll device object.
 */
void dpll_device_put(struct dpll_device *dpll)
{
	if (!dpll)
		return;
	mutex_lock(&dpll_device_xa_lock);
	if (refcount_dec_and_test(&dpll->refcount)) {
		ASSERT_DPLL_NOT_REGISTERED(dpll);
		WARN_ON_ONCE(!xa_empty(&dpll->pin_refs));
		xa_destroy(&dpll->pin_refs);
		xa_erase(&dpll_device_xa, dpll->id);
		kfree(dpll);
	}
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_device_put);

/**
 * dpll_device_register - register the dpll device in the subsystem
 * @dpll: pointer to a dpll
 * @type: type of a dpll
 * @ops: ops for a dpll device
 * @priv: pointer to private information of owner
 * @owner: pointer to owner device
 *
 * Make dpll device available for user space.
 *
 * Return:
 * * 0 on success
 * * -EINVAL on failure
 */
int dpll_device_register(struct dpll_device *dpll, enum dpll_type type,
			 const struct dpll_device_ops *ops, void *priv,
			 struct device *owner)
{
	if (WARN_ON(!ops || !owner))
		return -EINVAL;
	if (WARN_ON(type <= DPLL_TYPE_UNSPEC || type > DPLL_TYPE_MAX))
		return -EINVAL;
	mutex_lock(&dpll_device_xa_lock);
	if (ASSERT_DPLL_NOT_REGISTERED(dpll)) {
		mutex_unlock(&dpll_device_xa_lock);
		return -EEXIST;
	}
	dpll->dev.bus = owner->bus;
	dpll->parent = owner;
	dpll->type = type;
	dpll->ops = ops;
	dev_set_name(&dpll->dev, "%s/%llx/%d", module_name(dpll->module),
		     dpll->clock_id, dpll->dev_driver_id);
	dpll->priv = priv;
	xa_set_mark(&dpll_device_xa, dpll->id, DPLL_REGISTERED);
	mutex_unlock(&dpll_device_xa_lock);
	dpll_notify_device_create(dpll);

	return 0;
}
EXPORT_SYMBOL_GPL(dpll_device_register);

/**
 * dpll_device_unregister - deregister dpll device
 * @dpll: registered dpll pointer
 *
 * Deregister device, make it unavailable for userspace.
 * Note: It does not free the memory
 */
void dpll_device_unregister(struct dpll_device *dpll)
{
	mutex_lock(&dpll_device_xa_lock);
	ASSERT_DPLL_REGISTERED(dpll);
	xa_clear_mark(&dpll_device_xa, dpll->id, DPLL_REGISTERED);
	mutex_unlock(&dpll_device_xa_lock);
	dpll_notify_device_delete(dpll);
}
EXPORT_SYMBOL_GPL(dpll_device_unregister);

/**
 * dpll_pin_alloc - allocate the memory for dpll pin
 * @clock_id: clock_id of creator
 * @dev_driver_id: id given by dev driver
 * @module: reference to registering module
 * @prop: dpll pin properties
 *
 * Return:
 * valid allocated dpll_pin struct pointer if succeeded
 * ERR_PTR of an error
 */
static struct dpll_pin *
dpll_pin_alloc(u64 clock_id, u8 dev_driver_id, struct module *module,
	       const struct dpll_pin_properties *prop)
{
	struct dpll_pin *pin;
	int ret, fs_size;

	pin = kzalloc(sizeof(*pin), GFP_KERNEL);
	if (!pin)
		return ERR_PTR(-ENOMEM);
	pin->dev_driver_id = dev_driver_id;
	pin->clock_id = clock_id;
	pin->module = module;
	refcount_set(&pin->refcount, 1);
	if (WARN_ON(!prop->label)) {
		ret = -EINVAL;
		goto err;
	}
	pin->prop.label = kstrdup(prop->label, GFP_KERNEL);
	if (!pin->prop.label) {
		ret = -ENOMEM;
		goto err;
	}
	if (WARN_ON(prop->type <= DPLL_PIN_TYPE_UNSPEC ||
		    prop->type > DPLL_PIN_TYPE_MAX)) {
		ret = -EINVAL;
		goto err;
	}
	pin->prop.type = prop->type;
	pin->prop.capabilities = prop->capabilities;
	if (prop->freq_supported_num) {
		fs_size = sizeof(*pin->prop.freq_supported) *
			  prop->freq_supported_num;
		pin->prop.freq_supported = kzalloc(fs_size, GFP_KERNEL);
		if (!pin->prop.freq_supported) {
			ret = -ENOMEM;
			goto err;
		}
		memcpy(pin->prop.freq_supported, prop->freq_supported, fs_size);
		pin->prop.freq_supported_num = prop->freq_supported_num;
	}
	xa_init_flags(&pin->dpll_refs, XA_FLAGS_ALLOC);
	xa_init_flags(&pin->parent_refs, XA_FLAGS_ALLOC);
	ret = xa_alloc(&dpll_pin_xa, &pin->idx, pin,
		       xa_limit_16b, GFP_KERNEL);
	if (ret)
		goto err;
	return pin;
err:
	xa_destroy(&pin->dpll_refs);
	xa_destroy(&pin->parent_refs);
	kfree(pin->prop.label);
	kfree(pin->rclk_dev_name);
	kfree(pin);
	return ERR_PTR(ret);
}

/**
 * dpll_pin_get - find existing or create new dpll pin
 * @clock_id: clock_id of creator
 * @dev_driver_id: id given by dev driver
 * @module: reference to registering module
 * @prop: dpll pin properties
 *
 * Get existing object of a pin (unique for given arguments) or create new
 * if doesn't exist yet.
 *
 * Return:
 * * valid allocated dpll_pin struct pointer if succeeded
 * * ERR_PTR of an error
 */
struct dpll_pin *
dpll_pin_get(u64 clock_id, u32 dev_driver_id, struct module *module,
	     const struct dpll_pin_properties *prop)
{
	struct dpll_pin *pos, *ret = NULL;
	unsigned long i;

	mutex_lock(&dpll_pin_xa_lock);
	xa_for_each(&dpll_pin_xa, i, pos) {
		if (pos->clock_id == clock_id &&
		    pos->dev_driver_id == dev_driver_id &&
		    pos->module == module) {
			ret = pos;
			refcount_inc(&ret->refcount);
			break;
		}
	}
	if (!ret)
		ret = dpll_pin_alloc(clock_id, dev_driver_id, module, prop);
	mutex_unlock(&dpll_pin_xa_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_get);

/**
 * dpll_pin_put - decrease the refcount and free memory if possible
 * @dpll: dpll_device struct pointer
 *
 * Drop reference for a pin, if all references are gone, delete pin object.
 */
void dpll_pin_put(struct dpll_pin *pin)
{
	if (!pin)
		return;
	mutex_lock(&dpll_pin_xa_lock);
	if (refcount_dec_and_test(&pin->refcount)) {
		xa_destroy(&pin->dpll_refs);
		xa_destroy(&pin->parent_refs);
		xa_erase(&dpll_pin_xa, pin->idx);
		kfree(pin->prop.label);
		kfree(pin->prop.freq_supported);
		kfree(pin->rclk_dev_name);
		kfree(pin);
	}
	mutex_unlock(&dpll_pin_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_pin_put);

static int
__dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin,
		    const struct dpll_pin_ops *ops, void *priv,
		    const char *rclk_device_name)
{
	int ret;

	if (rclk_device_name && !pin->rclk_dev_name) {
		pin->rclk_dev_name = kstrdup(rclk_device_name, GFP_KERNEL);
		if (!pin->rclk_dev_name)
			return -ENOMEM;
	}
	ret = dpll_xa_ref_pin_add(&dpll->pin_refs, pin, ops, priv);
	if (ret)
		goto rclk_free;
	ret = dpll_xa_ref_dpll_add(&pin->dpll_refs, dpll, ops, priv);
	if (ret)
		goto ref_pin_del;
	else
		dpll_pin_notify(dpll, pin, DPLL_A_PIN_IDX);

	return ret;

ref_pin_del:
	dpll_xa_ref_pin_del(&dpll->pin_refs, pin);
rclk_free:
	kfree(pin->rclk_dev_name);
	return ret;
}

/**
 * dpll_pin_register - register the dpll pin in the subsystem
 * @dpll: pointer to a dpll
 * @pin: pointer to a dpll pin
 * @ops: ops for a dpll pin ops
 * @priv: pointer to private information of owner
 * @rclk_device: pointer to recovered clock device
 *
 * Return:
 * * 0 on success
 * * -EINVAL - missing dpll or pin
 * * -ENOMEM - failed to allocate memory
 */
int
dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin,
		  const struct dpll_pin_ops *ops, void *priv,
		  struct device *rclk_device)
{
	const char *rclk_name = rclk_device ? dev_name(rclk_device) : NULL;
	int ret;

	mutex_lock(&dpll_device_xa_lock);
	mutex_lock(&dpll_pin_xa_lock);
	ret = __dpll_pin_register(dpll, pin, ops, priv, rclk_name);
	mutex_unlock(&dpll_pin_xa_lock);
	mutex_unlock(&dpll_device_xa_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_register);

static void
__dpll_pin_unregister(struct dpll_device *dpll, struct dpll_pin *pin)
{
	dpll_xa_ref_pin_del(&dpll->pin_refs, pin);
	dpll_xa_ref_dpll_del(&pin->dpll_refs, dpll);
}

/**
 * dpll_pin_unregister - deregister dpll pin from dpll device
 * @dpll: registered dpll pointer
 * @pin: pointer to a pin
 *
 * Note: It does not free the memory
 */
void dpll_pin_unregister(struct dpll_device *dpll, struct dpll_pin *pin)
{
	if (WARN_ON(xa_empty(&dpll->pin_refs)))
		return;

	mutex_lock(&dpll_device_xa_lock);
	mutex_lock(&dpll_pin_xa_lock);
	__dpll_pin_unregister(dpll, pin);
	mutex_unlock(&dpll_pin_xa_lock);
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_pin_unregister);

/**
 * dpll_pin_on_pin_register - register a pin with a parent pin
 * @parent: pointer to a parent pin
 * @pin: pointer to a pin
 * @ops: ops for a dpll pin
 * @priv: pointer to private information of owner
 * @rclk_device: pointer to recovered clock device
 *
 * Register a pin with a parent pin, create references between them and
 * between newly registered pin and dplls connected with a parent pin.
 *
 * Return:
 * * 0 on success
 * * -EINVAL missing pin or parent
 * * -ENOMEM failed allocation
 * * -EPERM if parent is not allowed
 */
int dpll_pin_on_pin_register(struct dpll_pin *parent, struct dpll_pin *pin,
			     const struct dpll_pin_ops *ops, void *priv,
			     struct device *rclk_device)
{
	struct dpll_pin_ref *ref;
	unsigned long i, stop;
	int ret;

	if (WARN_ON(parent->prop.type != DPLL_PIN_TYPE_MUX))
		return -EINVAL;
	mutex_lock(&dpll_pin_xa_lock);
	ret = dpll_xa_ref_pin_add(&pin->parent_refs, parent, ops, priv);
	if (ret)
		goto unlock;
	refcount_inc(&pin->refcount);
	xa_for_each(&parent->dpll_refs, i, ref) {
		mutex_lock(&dpll_device_xa_lock);
		ret = __dpll_pin_register(ref->dpll, pin, ops, priv,
					  rclk_device ?
					  dev_name(rclk_device) : NULL);
		mutex_unlock(&dpll_device_xa_lock);
		if (ret) {
			stop = i;
			goto dpll_unregister;
		}
		dpll_pin_parent_notify(ref->dpll, pin, parent, DPLL_A_PIN_IDX);
	}
	mutex_unlock(&dpll_pin_xa_lock);

	return ret;

dpll_unregister:
	xa_for_each(&parent->dpll_refs, i, ref) {
		if (i < stop) {
			mutex_lock(&dpll_device_xa_lock);
			__dpll_pin_unregister(ref->dpll, pin);
			mutex_unlock(&dpll_device_xa_lock);
		}
	}
	refcount_dec(&pin->refcount);
	dpll_xa_ref_pin_del(&pin->parent_refs, parent);
unlock:
	mutex_unlock(&dpll_pin_xa_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_on_pin_register);

/**
 * dpll_pin_on_pin_unregister - deregister dpll pin from a parent pin
 * @parent: pointer to a parent pin
 * @pin: pointer to a pin
 *
 * Note: It does not free the memory
 */
void dpll_pin_on_pin_unregister(struct dpll_pin *parent, struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;
	unsigned long i;

	mutex_lock(&dpll_device_xa_lock);
	mutex_lock(&dpll_pin_xa_lock);
	dpll_xa_ref_pin_del(&pin->parent_refs, parent);
	refcount_dec(&pin->refcount);
	xa_for_each(&pin->dpll_refs, i, ref) {
		__dpll_pin_unregister(ref->dpll, pin);
		dpll_pin_parent_notify(ref->dpll, pin, parent,
				       DPLL_A_PIN_IDX);
	}
	mutex_unlock(&dpll_pin_xa_lock);
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_pin_on_pin_unregister);

/**
 * dpll_pin_get_by_idx - find a pin ref on dpll by pin index
 * @dpll: dpll device pointer
 * @idx: index of pin
 *
 * Find a reference to a pin registered with given dpll and return its pointer.
 *
 * Return:
 * * valid pointer if pin was found
 * * NULL if not found
 */
struct dpll_pin *dpll_pin_get_by_idx(struct dpll_device *dpll, u32 idx)
{
	struct dpll_pin_ref *pos;
	unsigned long i;

	xa_for_each(&dpll->pin_refs, i, pos) {
		if (pos && pos->pin && pos->pin->dev_driver_id == idx)
			return pos->pin;
	}

	return NULL;
}

/**
 * dpll_priv - get the dpll device private owner data
 * @dpll:	registered dpll pointer
 *
 * Return: pointer to the data
 */
void *dpll_priv(const struct dpll_device *dpll)
{
	return dpll->priv;
}
EXPORT_SYMBOL_GPL(dpll_priv);

/**
 * dpll_pin_on_dpll_priv - get the dpll device private owner data
 * @dpll:	registered dpll pointer
 * @pin:	pointer to a pin
 *
 * Return: pointer to the data
 */
void *dpll_pin_on_dpll_priv(const struct dpll_device *dpll,
			    const struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;

	ref = dpll_xa_ref_pin_find((struct xarray *)&dpll->pin_refs, pin);
	if (!ref)
		return NULL;

	return ref->priv;
}
EXPORT_SYMBOL_GPL(dpll_pin_on_dpll_priv);

/**
 * dpll_pin_on_pin_priv - get the dpll pin private owner data
 * @parent: pointer to a parent pin
 * @pin: pointer to a pin
 *
 * Return: pointer to the data
 */
void *dpll_pin_on_pin_priv(const struct dpll_pin *parent,
			   const struct dpll_pin *pin)
{
	struct dpll_pin_ref *ref;

	ref = dpll_xa_ref_pin_find((struct xarray *)&pin->parent_refs, parent);
	if (!ref)
		return NULL;

	return ref->priv;
}
EXPORT_SYMBOL_GPL(dpll_pin_on_pin_priv);

static int __init dpll_init(void)
{
	int ret;

	ret = dpll_netlink_init();
	if (ret)
		goto error;

	return 0;

error:
	mutex_destroy(&dpll_device_xa_lock);
	mutex_destroy(&dpll_pin_xa_lock);
	return ret;
}
subsys_initcall(dpll_init);
