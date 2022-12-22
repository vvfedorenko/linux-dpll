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

#define for_each_dpll(dpll, i)                         \
	for (dpll = dpll_first(&i); dpll != NULL; dpll = dpll_next(&i))

struct dpll_device *dpll_device_get_by_id(int id);

struct dpll_device *dpll_device_get_by_name(const char *name);
struct dpll_pin *dpll_pin_first(struct dpll_device *dpll, unsigned long *index);
struct dpll_pin *dpll_pin_next(struct dpll_device *dpll, unsigned long *index);
struct dpll_device *dpll_first(unsigned long *index);
struct dpll_device *dpll_next(unsigned long *index);
void dpll_device_unregister(struct dpll_device *dpll);
u32 dpll_id(struct dpll_device *dpll);
const char *dpll_dev_name(struct dpll_device *dpll);
void dpll_lock(struct dpll_device *dpll);
void dpll_unlock(struct dpll_device *dpll);
u32 dpll_pin_idx(struct dpll_device *dpll, struct dpll_pin *pin);
int dpll_pin_type_get(const struct dpll_device *dpll,
		      const struct dpll_pin *pin,
		      enum dpll_pin_type *type);
int dpll_pin_signal_type_get(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     enum dpll_pin_signal_type *type);
int dpll_pin_signal_type_set(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     const enum dpll_pin_signal_type type);
int dpll_pin_signal_type_supported(const struct dpll_device *dpll,
				   const struct dpll_pin *pin,
				   const enum dpll_pin_signal_type type,
				   bool *supported);
int dpll_pin_state_active(const struct dpll_device *dpll,
			  const struct dpll_pin *pin,
			  const enum dpll_pin_state state,
			  bool *active);
int dpll_pin_state_supported(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     const enum dpll_pin_state state,
			     bool *supported);
int dpll_pin_state_set(const struct dpll_device *dpll,
		       const struct dpll_pin *pin,
		       const enum dpll_pin_state state);
int dpll_pin_custom_freq_get(const struct dpll_device *dpll,
			     const struct dpll_pin *pin, u32 *freq);
int dpll_pin_custom_freq_set(const struct dpll_device *dpll,
			     const struct dpll_pin *pin, const u32 freq);
int dpll_pin_prio_get(const struct dpll_device *dpll,
		      const struct dpll_pin *pin, u32 *prio);
struct dpll_pin *dpll_pin_get_by_idx(struct dpll_device *dpll, int idx);
int dpll_pin_prio_set(const struct dpll_device *dpll,
		      const struct dpll_pin *pin, const u32 prio);
int dpll_pin_netifindex_get(const struct dpll_device *dpll,
			    const struct dpll_pin *pin,
			    int *netifindex);
const char *dpll_pin_description(struct dpll_pin *pin);
struct dpll_pin *dpll_pin_parent(struct dpll_pin *pin);
int dpll_mode_set(struct dpll_device *dpll, const enum dpll_mode mode);
int dpll_source_idx_set(struct dpll_device *dpll, const u32 source_pin_idx);

#endif
