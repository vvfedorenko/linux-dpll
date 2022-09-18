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

static DEFINE_MUTEX(dpll_device_xa_lock);

static DEFINE_XARRAY_FLAGS(dpll_device_xa, XA_FLAGS_ALLOC);
#define DPLL_REGISTERED XA_MARK_1

#define ASSERT_DPLL_REGISTERED(d)                                           \
	WARN_ON_ONCE(!xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))
#define ASSERT_DPLL_NOT_REGISTERED(d)                                      \
	WARN_ON_ONCE(xa_get_mark(&dpll_device_xa, (d)->id, DPLL_REGISTERED))

#define IS_TYPE_SOURCE(t) (t == DPLL_PIN_TYPE_SOURCE)

static LIST_HEAD(pins);
static DEFINE_MUTEX(pins_lock);

int for_each_dpll_device(int id, int (*cb)(struct dpll_device *, void *),
			 void *data)
{
	struct dpll_device *dpll;
	unsigned long index;
	int ret = 0;

	mutex_lock(&dpll_device_xa_lock);
	xa_for_each_start(&dpll_device_xa, index, dpll, id) {
		if (!xa_get_mark(&dpll_device_xa, index, DPLL_REGISTERED))
			continue;
		ret = cb(dpll, data);
		if (ret)
			break;
	}
	mutex_unlock(&dpll_device_xa_lock);

	return ret;
}

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
EXPORT_SYMBOL_GPL(dpll_device_get_by_name);

void *dpll_priv(struct dpll_device *dpll)
{
	return dpll->priv;
}
EXPORT_SYMBOL_GPL(dpll_priv);

void *pin_priv(struct dpll_pin *pin)
{
	return pin->priv;
}
EXPORT_SYMBOL_GPL(pin_priv);

int pin_id(struct dpll_pin *pin)
{
	return pin->id;
}
EXPORT_SYMBOL_GPL(pin_id);

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

struct dpll_device *dpll_device_alloc(struct dpll_device_ops *ops, const char *name,
				      void *priv)
{
	struct dpll_device *dpll;
	int ret;

	dpll = kzalloc(sizeof(*dpll), GFP_KERNEL);
	if (!dpll)
		return ERR_PTR(-ENOMEM);

	mutex_init(&dpll->lock);
	dpll->ops = ops;
	dpll->dev.class = &dpll_class;

	mutex_lock(&dpll_device_xa_lock);
	ret = xa_alloc(&dpll_device_xa, &dpll->id, dpll, xa_limit_16b, GFP_KERNEL);
	if (ret)
		goto error;
	if (name)
		dev_set_name(&dpll->dev, "%s", name);
	else
		dev_set_name(&dpll->dev, "%s%d", "dpll", dpll->id);
	mutex_unlock(&dpll_device_xa_lock);
	dpll->priv = priv;
	xa_init_flags(&dpll->pins, XA_FLAGS_ALLOC);

	dpll_notify_device_create(dpll->id, dev_name(&dpll->dev));

	return dpll;

error:
	mutex_unlock(&dpll_device_xa_lock);
	kfree(dpll);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(dpll_device_alloc);

void dpll_device_free(struct dpll_device *dpll)
{
	if (!dpll)
		return;

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
	mutex_unlock(&dpll_device_xa_lock);
}
EXPORT_SYMBOL_GPL(dpll_device_unregister);

struct dpll_pin *dpll_pin_alloc(struct dpll_pin_ops *ops, int id,
				enum dpll_pin_type type,
				const char *name, void *priv)
{
	struct dpll_pin *pin;

	pin = kzalloc(sizeof(*pin), GFP_KERNEL);
	if (!pin)
		return ERR_PTR(-ENOMEM);

	if (type <= DPLL_PIN_TYPE_INVALID || type > DPLL_PIN_TYPE_MAX)
		return ERR_PTR(-EINVAL);

	mutex_lock(&pins_lock);
	mutex_init(&pin->lock);
	pin->ops = ops;
	pin->id = id;
	pin->type = type;
	if (name)
		snprintf(&pin->name[0], PIN_NAME_LENGTH, "%s", name);
	else
		snprintf(&pin->name[0], PIN_NAME_LENGTH, "%s%d",
			 IS_TYPE_SOURCE(pin->type) ? "source" : "output",
			 pin->id);
	pin->priv = priv;
	xa_init_flags(&pin->muxed_pins, XA_FLAGS_ALLOC);
	list_add(&pin->node, &pins);
	mutex_unlock(&pins_lock);

	return pin;
}
EXPORT_SYMBOL_GPL(dpll_pin_alloc);

static int pin_register(struct xarray *pins, struct dpll_pin *pin)
{
	struct dpll_pin *pos;
	unsigned long index;
	int ret;
	u32 id;

	xa_for_each(pins, index, pos) {
		if (pos == pin)
			return -EEXIST;
	}

	ret = xa_alloc(pins, &id, pin, xa_limit_16b, GFP_KERNEL);
	if (!ret)
		xa_set_mark(pins, id, PIN_TYPE_TO_MARK(pin->type));

	return ret;
}

static void change_pin_count(struct dpll_device *dpll, struct dpll_pin *pin,
			     bool increment)
{
	if (IS_TYPE_SOURCE(pin->type)) {
		if (increment)
			dpll->sources_count++;
		else
			dpll->sources_count--;
	} else {
		if (increment)
			dpll->outputs_count++;
		else
			dpll->outputs_count--;
	}
}

int dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin)
{
	int ret;

	mutex_lock(&pins_lock);
	mutex_lock(&dpll->lock);
	ret = pin_register(&dpll->pins, pin);
	if (!ret) {
		pin->ref_count++;
		change_pin_count(dpll, pin, true);
		xa_set_mark(&dpll->pins, dpll->id, DPLL_REGISTERED);
	}
	mutex_unlock(&dpll->lock);
	mutex_unlock(&pins_lock);
	if (!ret)
		dpll_notify_pin_register(dpll->id, pin->id);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_register);

static int pin_deregister(struct xarray *pins, struct dpll_pin *pin)
{
	struct dpll_pin *pos;
	unsigned long index;

	xa_for_each(pins, index, pos) {
		if (pos == pin) {
			if (pin == xa_erase(pins, index))
				return 0;
			break;
		}
	}

	return -ENOENT;
}

int dpll_pin_deregister(struct dpll_device *dpll, struct dpll_pin *pin)
{
	int ret = 0;

	if (xa_empty(&dpll->pins))
		return -ENOENT;

	mutex_lock(&pins_lock);
	mutex_lock(&dpll->lock);
	ret = pin_deregister(&dpll->pins, pin);
	if (!ret) {
		change_pin_count(dpll, pin, false);
		pin->ref_count--;
	}
	mutex_unlock(&dpll->lock);
	mutex_unlock(&pins_lock);
	if (!ret)
		dpll_notify_pin_deregister(dpll->id, pin->id);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_pin_deregister);

void dpll_pin_free(struct dpll_pin *pin)
{
	if (pin->ref_count)
		return;

	mutex_lock(&pins_lock);
	list_del(&pin->node);
	xa_destroy(&pin->muxed_pins);
	mutex_unlock(&pins_lock);
	kfree(pin);
}
EXPORT_SYMBOL_GPL(dpll_pin_free);

int dpll_muxed_pin_register(struct dpll_pin *parent_pin, struct dpll_pin *pin)
{
	int ret;

	mutex_lock(&pins_lock);
	mutex_lock(&pin->lock);
	ret = pin_register(&parent_pin->muxed_pins, pin);
	if (!ret)
		pin->ref_count++;
	mutex_unlock(&pin->lock);
	mutex_unlock(&pins_lock);
	if (!ret)
		dpll_notify_muxed_pin_register(parent_pin, pin->id);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_muxed_pin_register);

int dpll_muxed_pin_deregister(struct dpll_pin *parent_pin, struct dpll_pin *pin)
{
	int ret = 0;

	if (xa_empty(&parent_pin->muxed_pins))
		return -ENOENT;

	mutex_lock(&pins_lock);
	mutex_lock(&parent_pin->lock);
	ret = pin_deregister(&parent_pin->muxed_pins, pin);
	if (!ret)
		pin->ref_count--;
	mutex_unlock(&parent_pin->lock);
	mutex_unlock(&pins_lock);
	if (!ret)
		dpll_notify_muxed_pin_deregister(parent_pin, pin->id);

	return ret;
}
EXPORT_SYMBOL_GPL(dpll_muxed_pin_deregister);

struct dpll_pin *dpll_pin_get_by_name(struct dpll_device *dpll,
				      const char *name)
{
	struct dpll_pin *pos, *pin = NULL;
	unsigned long index;

	mutex_lock(&dpll->lock);
	xa_for_each(&dpll->pins, index, pos) {
		if (!strncmp(pos->name, name, PIN_NAME_LENGTH)) {
			pin = pos;
			break;
		}
	}
	mutex_unlock(&dpll->lock);

	return pin;
}
EXPORT_SYMBOL_GPL(dpll_pin_get_by_name);

struct dpll_pin *dpll_pin_get_by_id(struct dpll_device *dpll, int id)
{
	struct dpll_pin *pos, *pin = NULL;
	unsigned long index;

	xa_for_each(&dpll->pins, index, pos) {
		if (pos->id == id) {
			pin = pos;
			break;
		}
	}

	return pin;
}
EXPORT_SYMBOL_GPL(dpll_pin_get_by_id);

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
