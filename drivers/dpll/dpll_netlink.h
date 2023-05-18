/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

/**
 * dpll_device_create_ntf - notify that the device has been created
 * @dpll: registered dpll pointer
 *
 * Context: caller shall hold dpll_xa_lock.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_device_create_ntf(struct dpll_device *dpll);

/**
 * dpll_device_delete_ntf - notify that the device has been deleted
 * @dpll: registered dpll pointer
 *
 * Context: caller shall hold dpll_xa_lock.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_device_delete_ntf(struct dpll_device *dpll);

/**
 * dpll_pin_delete_ntf - notify that the pin has been deleted
 * @pin: registered pin pointer
 *
 * Context: caller shall hold dpll_xa_lock.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_create_ntf(struct dpll_pin *pin);

/**
 * dpll_pin_delete_ntf - notify that the pin has been deleted
 * @pin: registered pin pointer
 *
 * Context: caller shall hold dpll_xa_lock.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_delete_ntf(struct dpll_pin *pin);

int __init dpll_netlink_init(void);
void dpll_netlink_finish(void);
