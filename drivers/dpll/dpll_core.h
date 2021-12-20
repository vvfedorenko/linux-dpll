/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#ifndef __DPLL_CORE_H__
#define __DPLL_CORE_H__

#include <linux/dpll.h>

#include "dpll_netlink.h"

#define to_dpll_device(_dev) \
	container_of(_dev, struct dpll_device, dev)

#define for_each_pin_on_dpll(dpll, pin, i)			\
	for (pin = dpll_pin_first(dpll, &i); pin != NULL;	\
	     pin = dpll_pin_next(dpll, &i))

#define for_each_dpll(dpll, i)				\
	for (dpll = dpll_first(&i); dpll != NULL;	\
	     dpll = dpll_next(&i))


/**
 * dpll_device_get_by_id - find dpll device by it's id
 * @id: dpll id
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
struct dpll_device *dpll_device_get_by_id(int id);

/**
 * dpll_device_get_by_name - find dpll device by it's id
 * @name: dpll name
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
struct dpll_device *dpll_device_get_by_name(const char *name);

/**
 * dpll_pin_first - get first registered pin
 * @dpll: registered dpll pointer
 * @index: found pin index (out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_first(struct dpll_device *dpll, unsigned long *index);

/**
 * dpll_pin_next - get next registered pin to the relative pin
 * @dpll: registered dpll pointer
 * @index: relative pin index (in and out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_next(struct dpll_device *dpll, unsigned long *index);

/**
 * dpll_first - get first registered dpll device
 * @index: found dpll index (out)
 *
 * Return: dpll_device struct if found, NULL otherwise.
 */
struct dpll_device *dpll_first(unsigned long *index);

/**
 * dpll_pin_next - get next registered dpll device to the relative pin
 * @index: relative dpll index (in and out)
 *
 * Return: dpll_pin struct if found, NULL otherwise.
 */
struct dpll_device *dpll_next(unsigned long *index);

/**
 * dpll_device_unregister - unregister dpll device
 * @dpll: registered dpll pointer
 *
 * Note: It does not free the memory
 */
void dpll_device_unregister(struct dpll_device *dpll);

/**
 * dpll_id - return dpll id
 * @dpll: registered dpll pointer
 *
 * Return: dpll id.
 */
u32 dpll_id(struct dpll_device *dpll);

/**
 * dpll_pin_idx - return dpll name
 * @dpll: registered dpll pointer
 *
 * Return: dpll name.
 */
const char *dpll_dev_name(struct dpll_device *dpll);

/**
 * dpll_lock - locks the dpll using internal mutex
 * @dpll: registered dpll pointer
 */
void dpll_lock(struct dpll_device *dpll);

/**
 * dpll_unlock - unlocks the dpll using internal mutex
 * @dpll: registered dpll pointer
 */
void dpll_unlock(struct dpll_device *dpll);

/**
 * dpll_set_attr - handler for dpll subsystem: dpll set attributes
 * @dpll: registered dpll pointer
 * @attr: dpll attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_set_attr(struct dpll_device *dpll, const struct dpll_attr *attr);

/**
 * dpll_get_attr - handler for dpll subsystem: dpll get attributes
 * @dpll: registered dpll pointer
 * @attr: dpll attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_get_attr(struct dpll_device *dpll, struct dpll_attr *attr);

/**
 * dpll_pin_idx - return dpll id
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 *
 * Return: dpll id.
 */
u32 dpll_pin_idx(struct dpll_device *dpll, struct dpll_pin *pin);

/**
 * dpll_pin_get_attr - handler for dpll subsystem: dpll pin get attributes
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @attr: dpll pin attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_get_attr(struct dpll_device *dpll, struct dpll_pin *pin,
		      struct dpll_pin_attr *attr);

/**
 * dpll_pin_get_description - provide pin's description string
 * @pin: registered pin pointer
 *
 * Return: pointer to a description string.
 */
const char *dpll_pin_get_description(struct dpll_pin *pin);

/**
 * dpll_pin_get_parent - provide pin's parent pin if available
 * @pin: registered pin pointer
 *
 * Return: pointer to aparent if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_get_parent(struct dpll_pin *pin);

/**
 * dpll_pin_set_attr - handler for dpll subsystem: dpll pin get attributes
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 * @attr: dpll pin attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_set_attr(struct dpll_device *dpll, struct dpll_pin *pin,
		      const struct dpll_pin_attr *attr);

#endif
