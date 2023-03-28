/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

/**
 * dpll_notify_device_create - notify that the device has been created
 * @dpll: registered dpll pointer
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_notify_device_create(struct dpll_device *dpll);


/**
 * dpll_notify_device_delete - notify that the device has been deleted
 * @dpll: registered dpll pointer
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_notify_device_delete(struct dpll_device *dpll);

int dpll_pin_parent_notify(struct dpll_device *dpll, struct dpll_pin *pin,
			   struct dpll_pin *parent, enum dplla attr);

int __init dpll_netlink_init(void);
void dpll_netlink_finish(void);
