/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#ifndef __DPLL_H__
#define __DPLL_H__

#define PIN_NAME_LENGTH 20

struct dpll_device;
struct dpll_pin;

struct dpll_device_ops {
	int (*get_status)(struct dpll_device *dpll);
	int (*get_temp)(struct dpll_device *dpll);
	int (*get_lock_status)(struct dpll_device *dpll);
	int (*get_source_select_mode)(struct dpll_device *dpll);
	int (*get_source_select_mode_supported)(struct dpll_device *dpll, int type);
	int (*set_source_select_mode)(struct dpll_device *dpll, int mode);
};

struct dpll_pin_ops {
	int (*get_type)(struct dpll_pin *pin);
	int (*is_type_supported)(struct dpll_pin *pin, int type);
	int (*set_type)(struct dpll_pin *pin, int type);
	int (*set_source)(struct dpll_pin *pin, struct dpll_device *dpll, int id);
	int (*set_flags)(struct dpll_pin *pin, struct dpll_device *dpll, int flags);
	int (*get_prio)(struct dpll_pin *pin, struct dpll_device *dpll);
	int (*set_prio)(struct dpll_pin *pin, struct dpll_device *dpll, int prio);
};

enum dpll_pin_type {
	DPLL_PIN_TYPE_INVALID,
	DPLL_PIN_TYPE_SOURCE,
	DPLL_PIN_TYPE_OUTPUT,
	DPLL_PIN_TYPE_MUX,
	__DPLL_PIN_TYPE_MAX,
};
#define DPLL_PIN_TYPE_MAX (__DPLL_PIN_TYPE_MAX - 1)

struct dpll_device *dpll_device_alloc(struct dpll_device_ops *ops,
				      const char *name, void *priv);
void dpll_device_register(struct dpll_device *dpll);
void dpll_device_unregister(struct dpll_device *dpll);
void dpll_device_free(struct dpll_device *dpll);
void *dpll_priv(struct dpll_device *dpll);
void *pin_priv(struct dpll_pin *pin);
int pin_id(struct dpll_pin *pin);

/*
 * dpll_pin_alloc - allocate memory for a new dpll_pin object
 * @ops: pointer to pin operations structure
 * @id: id given by the owner, used to find pin by id
 * @dpll_pin_type: type of a pin being allocated
 * @name: human readable pin name of allocated pin
 * @priv: private data of a registerer
 *
 * Allocate memory for a new pin and initialize it with its type, name,
 * callbacks and private data pointer.
 *
 * Returns:
 * * pointer to initialized pin - success,
 * * ERR_PTR(-ENOMEM) - memory failure,
 * * ERR_PTR(-EINVAL) - given pin type is invalid.
 **/
struct dpll_pin *dpll_pin_alloc(struct dpll_pin_ops *ops, int id,
				enum dpll_pin_type type,
				const char *name, void *priv);

/*
 * dpll_pin_register - register pin with a dpll device
 * @dpll: pointer to dpll object to register pin with
 * @pin: pointer to allocated pin object being registered with dpll
 *
 * Register previously allocated pin object with a dpll device.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this dpll,
 * * -EBUSY - couldn't allocate id for a pin.
 **/
int dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin);

/*
 * dpll_pin_deregister - deregister pin from a dpll device
 * @dpll: pointer to dpll object to deregister pin from
 * @pin: pointer to allocated pin object being deregistered from dpll
 *
 * Deregister previously registered pin object from a dpll device.
 *
 * Return:
 * * 0 - pin was successfully deregistered from this dpll device,
 * * -ENOENT - given pin was not registered with this dpll device,
 * * -EINVAL - pin pointer is not valid.
 **/
int dpll_pin_deregister(struct dpll_device *dpll, struct dpll_pin *pin);

/*
 * dpll_pin_free - free memory allocated for a pin
 * @pin: pointer to allocated pin object being freed
 *
 * Free memory of deregistered pin object.
 * Shared pins must be deregistered from all dpll devices before freeing them,
 * otherwise the memory won't be freed.
 **/
void dpll_pin_free(struct dpll_pin *pin);

/*
 * dpll_pin_add_muxed_pin - add a pin to a mux type pin
 * @parent_pin: pointer to  object to deregister pin from
 * @pin: pointer to allocated pin object being deregistered from dpll
 *
 * In case of multiplexed pins, register them under a single parent pin.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this parent pin,
 * * -EBUSY - couldn't assign id for a pin.
 **/
int dpll_muxed_pin_register(struct dpll_pin *parent_pin, struct dpll_pin *pin);

/*
 * dpll_muxed_pin_deregister - deregister a pin from a muxed-type pin
 * @parent_pin: pointer to  object to deregister pin from
 * @pin: pointer to allocated pin object being deregistered from dpll
 *
 * In case of multiplexed pins, deregister a pin from a parent pin.
 *
 * Return:
 * * 0 - pin was successfully deregistered from a parent pin,
 * * -ENOENT - given pin was not registered with this parent pin,
 * * -EINVAL - pin pointer is not valid.
 **/
int dpll_muxed_pin_deregister(struct dpll_pin *parent_pin, struct dpll_pin *pin);

/*
 * dpll_device_get_by_name - find a dpll by its name
 * @name: name of device
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share already registered DPLL device.
 *
 * Return: pointer if device was found, NULL otherwise.
 **/
struct dpll_device *dpll_device_get_by_name(const char *name);

/*
 * dpll_pin_get_by_name - find a pin belonging to dpll by its name
 * @name: name of device
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share pin already registered with existing dpll device.
 *
 * Return:
 * * pointer to existing pin if pin was found,
 * * NULL if pin was not found.
 **/
struct dpll_pin *dpll_pin_get_by_name(struct dpll_device *dpll, const char *name);

/*
 * dpll_pin_get_by_id - find a pin by its id
 * @name: name of device
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share pin already registered with existing dpll device.
 *
 * Return: pointer if pin was found, NULL otherwise.
 **/
struct dpll_pin *dpll_pin_get_by_id(struct dpll_device *dpll, int id);

int dpll_notify_status_locked(int dpll_id);
int dpll_notify_status_unlocked(int dpll_id);
int dpll_notify_source_change(int dpll_id, int pin_id);
int dpll_notify_pin_type_change(int dpll_id, int pin_id, int type);
int dpll_notify_pin_flags_change(int dpll_id, int pin_id, int flags);
int dpll_notify_source_select_mode_change(int dpll_id, int source_select_mode);
int dpll_notify_source_prio_change(int dpll_id, int src_id, int prio);
int dpll_notify_pin_register(int dpll_id, int pin_id);
int dpll_notify_pin_deregister(int dpll_id, int pin_id);
int dpll_notify_muxed_pin_register(struct dpll_pin *parent_pin, int pin_id);
int dpll_notify_muxed_pin_deregister(struct dpll_pin *parent_pin, int pin_id);
#endif
