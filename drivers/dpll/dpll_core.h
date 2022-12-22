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
 * @clock_id:	unique identifier (clock_id) of a dpll
 * @clock_class	quality class of a DPLL clock
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
	u64 clock_id;
	enum dpll_clock_class clock_class;
	u8 dev_driver_idx;
};

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
 * dpll_pin_idx - return dpll id
 * @dpll: registered dpll pointer
 * @pin: registered pin pointer
 *
 * Return: dpll id.
 */
u32 dpll_pin_idx(struct dpll_device *dpll, struct dpll_pin *pin);

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
		      enum dpll_pin_type *type);

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
			     enum dpll_pin_signal_type *type);

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
			     const enum dpll_pin_signal_type type);

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
				   bool *supported);

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
			  bool *active);

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
			     bool *supported);

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
		       const enum dpll_pin_state state);

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
			     const struct dpll_pin *pin, u32 *freq);

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
			     const struct dpll_pin *pin, const u32 freq);

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
		      const struct dpll_pin *pin, u32 *prio);

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
struct dpll_pin *dpll_pin_get_by_idx(struct dpll_device *dpll, int idx);

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
		      const struct dpll_pin *pin, const u32 prio);

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
			    int *netifindex);

/**
 * dpll_pin_description - provide pin's description string
 * @pin: registered pin pointer
 *
 * Return: pointer to a description string.
 */
const char *dpll_pin_description(struct dpll_pin *pin);

/**
 * dpll_pin_parent - provide pin's parent pin if available
 * @pin: registered pin pointer
 *
 * Return: pointer to aparent if found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_parent(struct dpll_pin *pin);

/**
 * dpll_mode_set - handler for dpll mode set
 * @dpll: registered dpll pointer
 * @mode: mode to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_mode_set(struct dpll_device *dpll, const enum dpll_mode mode);

/**
 * dpll_source_idx_set - handler for selecting a dpll's source
 * @dpll: registered dpll pointer
 * @source_pin_idx: index of a source pin to e selected
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_source_idx_set(struct dpll_device *dpll, const u32 source_pin_idx);

#endif
