/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#ifndef __DPLL_H__
#define __DPLL_H__

#include "uapi/linux/dpll.h"

struct dpll_device;

struct dpll_device_ops {
	enum dpll_genl_status (*get_status)(struct dpll_device *dpll);
	int (*get_temp)(struct dpll_device *dpll);
	enum dpll_genl_signal_type (*get_source_type)(struct dpll_device *dpll, int id);
	int (*get_source_supported)(struct dpll_device *dpll, int id, enum dpll_genl_signal_type type);
	enum dpll_genl_signal_type (*get_output_type)(struct dpll_device *dpll, int id);
	int (*get_output_supported)(struct dpll_device *dpll, int id, enum dpll_genl_signal_type type);
	int (*set_source_type)(struct dpll_device *dpll, int id, enum dpll_genl_signal_type val);
	int (*set_output_type)(struct dpll_device *dpll, int id, enum dpll_genl_signal_type val);
};

struct dpll_device *dpll_device_alloc(struct dpll_device_ops *ops, const char *name,
				      int sources_count, int outputs_count, void *priv);
void dpll_device_register(struct dpll_device *dpll);
void dpll_device_unregister(struct dpll_device *dpll);
void dpll_device_free(struct dpll_device *dpll);
void *dpll_priv(struct dpll_device *dpll);

int dpll_notify_status_locked(int dpll_id);
int dpll_notify_status_unlocked(int dpll_id);
int dpll_notify_source_change(int dpll_id, int source_id, int source_type);
int dpll_notify_output_change(int dpll_id, int output_id, int output_type);
#endif
