/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (c) 2021 Meta Platforms, Inc. and affiliates
 */

#ifndef __DPLL_H__
#define __DPLL_H__

#include <uapi/linux/dpll.h>
#include <linux/device.h>

struct dpll_device;
struct dpll_pin;

#define DPLL_COOKIE_LEN		10
#define PIN_IDX_INVALID		((u32)ULONG_MAX)

struct dpll_device_ops {
	int (*mode_get)(const struct dpll_device *dpll, enum dpll_mode *mode);
	int (*mode_set)(const struct dpll_device *dpll,
			const enum dpll_mode mode);
	bool (*mode_supported)(const struct dpll_device *dpll,
			       const enum dpll_mode mode);
	int (*source_pin_idx_get)(const struct dpll_device *dpll,
				  u32 *pin_idx);
	int (*lock_status_get)(const struct dpll_device *dpll,
			       enum dpll_lock_status *status);
	int (*temp_get)(const struct dpll_device *dpll, s32 *temp);
};

struct dpll_pin_ops {
	int (*signal_type_get)(const struct dpll_device *dpll,
			       const struct dpll_pin *pin,
			       enum dpll_pin_signal_type *type);
	int (*signal_type_set)(const struct dpll_device *dpll,
			       const struct dpll_pin *pin,
			       const enum dpll_pin_signal_type type);
	bool (*signal_type_supported)(const struct dpll_device *dpll,
				      const struct dpll_pin *pin,
				      const enum dpll_pin_signal_type type);
	int (*custom_freq_set)(const struct dpll_device *dpll,
			       const struct dpll_pin *pin,
			       const u32 custom_freq);
	int (*custom_freq_get)(const struct dpll_device *dpll,
			       const struct dpll_pin *pin,
			       u32 *custom_freq);
	bool (*state_active)(const struct dpll_device *dpll,
			     const struct dpll_pin *pin,
			     const enum dpll_pin_state state);
	int (*state_enable)(const struct dpll_device *dpll,
			    const struct dpll_pin *pin,
			    const enum dpll_pin_state state);
	bool (*state_supported)(const struct dpll_device *dpll,
				const struct dpll_pin *pin,
				const enum dpll_pin_state state);
	int (*prio_get)(const struct dpll_device *dpll,
			const struct dpll_pin *pin,
			u32 *prio);
	int (*prio_set)(const struct dpll_device *dpll,
			const struct dpll_pin *pin,
			const u32 prio);
	int (*net_if_idx_get)(const struct dpll_device *dpll,
			      const struct dpll_pin *pin,
			      int *net_if_idx);
	int (*select)(const struct dpll_device *dpll,
		      const struct dpll_pin *pin);
};

enum dpll_type {
	DPLL_TYPE_UNSPEC,
	DPLL_TYPE_PPS,
	DPLL_TYPE_EEC,

	__DPLL_TYPE_MAX
};
#define DPLL_TYPE_MAX	(__DPLL_TYPE_MAX - 1)

/**
 * dpll_device_alloc - allocate memory for a new dpll_device object
 * @ops: pointer to dpll operations structure
 * @type: type of a dpll being allocated
 * @cookie: a system unique number for a device
 * @dev_driver_idx: index of dpll device on parent device
 * @priv: private data of a registerer
 * @parent: device structure of a module registering dpll device
 *
 * Allocate memory for a new dpll and initialize it with its type, name,
 * callbacks and private data pointer.
 *
 * Name is generated based on: cookie, type and dev_driver_idx.
 * Finding allocated and registered dpll device is also possible with
 * the: cookie, type and dev_driver_idx. This way dpll device can be
 * shared by multiple instances of a device driver.
 *
 * Returns:
 * * pointer to initialized dpll - success
 * * NULL - memory allocation fail
 */
struct dpll_device
*dpll_device_alloc(struct dpll_device_ops *ops, enum dpll_type type,
		   const u8 cookie[DPLL_COOKIE_LEN], u8 dev_driver_idx,
		   void *priv, struct device *parent);

/**
 * dpll_device_register - registers allocated dpll
 * @dpll: pointer to dpll
 *
 * Register the dpll on the dpll subsystem, make it available for netlink
 * API users.
 */
void dpll_device_register(struct dpll_device *dpll);

/**
 * dpll_device_unregister - unregister registered dpll
 * @dpll: pointer to dpll
 *
 * Unregister the dpll from the subsystem, make it unavailable for netlink
 * API users.
 */
void dpll_device_unregister(struct dpll_device *dpll);

/**
 * dpll_device_free - free dpll memory
 * @dpll: pointer to dpll
 *
 * Free memory allocated with ``dpll_device_alloc(..)``
 */
void dpll_device_free(struct dpll_device *dpll);

/**
 * dpll_priv - get private data
 * @dpll: pointer to dpll
 *
 * Obtain private data pointer passed to dpll subsystem when allocating
 * device with ``dpll_device_alloc(..)``
 */
void *dpll_priv(const struct dpll_device *dpll);

/**
 * dpll_pin_priv - get private data
 * @dpll: pointer to dpll
 *
 * Obtain private pin data pointer passed to dpll subsystem when pin
 * was registered with dpll.
 */
void *dpll_pin_priv(const struct dpll_device *dpll, const struct dpll_pin *pin);

/**
 * dpll_pin_idx - get pin idx
 * @dpll: pointer to dpll
 * @pin: pointer to a pin
 *
 * Obtain pin index of given pin on given dpll.
 *
 * Return: PIN_IDX_INVALID - if failed to find pin, otherwise pin index
 */
u32 dpll_pin_idx(struct dpll_device *dpll, struct dpll_pin *pin);


/**
 * dpll_shared_pin_register - share a pin between dpll devices
 * @dpll_pin_owner: a dpll already registered with a pin
 * @dpll: a dpll being registered with a pin
 * @pin_idx: index of a pin on dpll device (@dpll_pin_owner)
 *	     that is being registered on new dpll (@dpll)
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops
 *
 * Register a pin already registered with different dpll device.
 * Allow to share a single pin within multiple dpll instances.
 *
 * Returns:
 * * 0 - success
 * * negative - failure
 */
int
dpll_shared_pin_register(struct dpll_device *dpll_pin_owner,
			 struct dpll_device *dpll, u32 pin_idx,
			 struct dpll_pin_ops *ops, void *priv);

/**
 * dpll_pin_alloc - allocate memory for a new dpll_pin object
 * @description: pointer to string description of a pin with max length
 * equal to PIN_DESC_LEN
 * @desc_len: number of chars in description
 * @type: type of allocated pin
 *
 * Allocate memory for a new pin and initialize its resources.
 *
 * Returns:
 * * pointer to initialized pin - success
 * * NULL - memory allocation fail
 */
struct dpll_pin *dpll_pin_alloc(const char *description, size_t desc_len,
				const enum dpll_pin_type type);


/**
 * dpll_pin_register - register pin with a dpll device
 * @dpll: pointer to dpll object to register pin with
 * @pin: pointer to allocated pin object being registered with dpll
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops
 *
 * Register previously allocated pin object with a dpll device.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this dpll,
 * * -EBUSY - couldn't allocate id for a pin.
 */
int dpll_pin_register(struct dpll_device *dpll, struct dpll_pin *pin,
		      struct dpll_pin_ops *ops, void *priv);

/**
 * dpll_pin_deregister - deregister pin from a dpll device
 * @dpll: pointer to dpll object to deregister pin from
 * @pin: pointer to allocated pin object being deregistered from dpll
 *
 * Deregister previously registered pin object from a dpll device.
 *
 * Return:
 * * 0 - pin was successfully deregistered from this dpll device,
 * * -ENXIO - given pin was not registered with this dpll device,
 * * -EINVAL - pin pointer is not valid.
 */
int dpll_pin_deregister(struct dpll_device *dpll, struct dpll_pin *pin);

/**
 * dpll_pin_free - free memory allocated for a pin
 * @pin: pointer to allocated pin object being freed
 *
 * Shared pins must be deregistered from all dpll devices before freeing them,
 * otherwise the memory won't be freed.
 */
void dpll_pin_free(struct dpll_pin *pin);

/**
 * dpll_muxed_pin_register - register a pin to a muxed-type pin
 * @parent_pin: pointer to object to register pin with
 * @pin: pointer to allocated pin object being deregistered from dpll
 * @ops: struct with pin ops callbacks
 * @priv: private data pointer passed when calling callback ops*
 *
 * In case of multiplexed pins, allows registring them under a single
 * parent pin.
 *
 * Return:
 * * 0 - if pin was registered with a parent pin,
 * * -ENOMEM - failed to allocate memory,
 * * -EEXIST - pin already registered with this parent pin,
 * * -EBUSY - couldn't assign id for a pin.
 */
int dpll_muxed_pin_register(struct dpll_device *dpll,
			    struct dpll_pin *parent_pin, struct dpll_pin *pin,
			    struct dpll_pin_ops *ops, void *priv);
/**
 * dpll_device_get_by_cookie - find a dpll by its cookie
 * @cookie: cookie of dpll to search for, as given by driver on
 *	    ``dpll_device_alloc``
 * @type: type of dpll, as given by driver on ``dpll_device_alloc``
 * @idx: index of dpll, as given by driver on ``dpll_device_alloc``
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share already registered DPLL device.
 *
 * Return: pointer if device was found, NULL otherwise.
 */
struct dpll_device *dpll_device_get_by_cookie(u8 cookie[DPLL_COOKIE_LEN],
					      enum dpll_type type, u8 idx);

/**
 * dpll_pin_get_by_description - find a pin by its description
 * @dpll: dpll device pointer
 * @description: string description of pin
 *
 * Allows multiple driver instances using one physical DPLL to find
 * and share pin already registered with existing dpll device.
 *
 * Return: pointer if pin was found, NULL otherwise.
 */
struct dpll_pin *dpll_pin_get_by_description(struct dpll_device *dpll,
					     const char *description);

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
 * dpll_device_notify - notify on dpll device change
 * @dpll: dpll device pointer
 * @event: type of change
 *
 * Broadcast event to the netlink multicast registered listeners.
 *
 * Return:
 * * 0 - success
 * * negative - error
 */
int dpll_device_notify(struct dpll_device *dpll, enum dpll_event_change event);

/**
 * dpll_pin_notify - notify on dpll pin change
 * @dpll: dpll device pointer
 * @pin: dpll pin pointer
 * @event: type of change
 *
 * Broadcast event to the netlink multicast registered listeners.
 *
 * Return:
 * * 0 - success
 * * negative - error
 */
int dpll_pin_notify(struct dpll_device *dpll, struct dpll_pin *pin,
		    enum dpll_event_change event);

#endif
