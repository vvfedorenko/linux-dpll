/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#ifndef __DPLL_CORE_H__
#define __DPLL_CORE_H__

#include <linux/dpll.h>

#include "dpll_netlink.h"

#define PIN_SOURCE	XA_MARK_1
#define PIN_OUTPUT	XA_MARK_2
#define PIN_TYPE_TO_MARK(t) (t == DPLL_PIN_TYPE_SOURCE ? PIN_SOURCE : PIN_OUTPUT)
#define __FOR_EACH_PIN(dpll, index, count, pin, type) \
	count = type == DPLL_PIN_TYPE_SOURCE ? \
		dpll->sources_count : dpll->outputs_count; \
	xa_for_each_marked(&dpll->pins, index, pin, PIN_TYPE_TO_MARK(type))
#define FOR_EACH_SOURCE(dpll, index, count, pin) \
	__FOR_EACH_PIN(dpll, index, count, pin, DPLL_PIN_TYPE_SOURCE)
#define FOR_EACH_OUTPUT(dpll, index, count, pin) \
	__FOR_EACH_PIN(dpll, index, count, pin, DPLL_PIN_TYPE_OUTPUT)

/**
 * struct dpll_pin - structure for a dpll pin
 * @ref_count:	count number of dpll's that registered this pin
 * @ops:	operations this &dpll_pin supports
 * @lock:	mutex to serialize operations
 * @priv:	pointer to private information of owner
 * @name:	name to distinguish the pin
 */
struct dpll_pin {
	struct list_head node;
	int id;
	enum dpll_pin_type type;
	int ref_count;
	struct dpll_pin_ops *ops;
	struct mutex lock;
	void *priv;
	struct xarray muxed_pins;
	char name[PIN_NAME_LENGTH];
};

/**
 * struct dpll_device - structure for a DPLL device
 * @id:		unique id number for each edvice
 * @dev:	&struct device for this dpll device
 * @sources_count:     amount of input sources this dpll_device supports
 * @outputs_count:     amount of outputs this dpll_device supports
 * @ops:	operations this &dpll_device supports
 * @lock:	mutex to serialize operations
 * @priv:	pointer to private information of owner
 * @pins:	list of pointers to pins registered with this dpll
 */
struct dpll_device {
	int id;
	struct device dev;
	int sources_count;
	int outputs_count;
	struct dpll_device_ops *ops;
	struct mutex lock;
	void *priv;
	struct xarray pins;
};

#define to_dpll_device(_dev) \
	container_of(_dev, struct dpll_device, dev)

int for_each_dpll_device(int id, int (*cb)(struct dpll_device *, void *),
			  void *data);
struct dpll_device *dpll_device_get_by_id(int id);
struct dpll_device *dpll_device_get_by_name(const char *name);
void dpll_device_unregister(struct dpll_device *dpll);
#endif
