/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  dpll_attr.h - Header for attribute handling helper class.
 *
 *  Copyright (c) 2022, Intel Corporation.
 */

#ifndef __DPLL_ATTR_H__
#define __DPLL_ATTR_H__

#include <linux/types.h>

struct dpll_attr;
struct dpll_pin_attr;

#define PIN_PRIO_HIGHEST	0
#define PIN_PRIO_LOWEST		0xff
#define PIN_ATTR_CHANGE		1

/**
 * dpll_attr_alloc - allocate a dpll attributes struct
 *
 * Return: pointer if succeeded, NULL otherwise.
 */
struct dpll_attr *dpll_attr_alloc(void);

/**
 * dpll_attr_free - frees a dpll attributes struct
 * @attr: structure with dpll attributes
 */
void dpll_attr_free(struct dpll_attr *attr);

/**
 * dpll_attr_clear - clears a dpll attributes struct
 * @attr: structure with dpll attributes
 */
void dpll_attr_clear(struct dpll_attr *attr);

/**
 * dpll_attr_valid - checks if a attribute is valid
 * @attr_id: attribute to be checked
 * @attr: structure with dpll attributes
 *
 * Checks if the attribute has been set before and if stored value is valid.
 * Return: true if valid, false otherwise.
 */
bool dpll_attr_valid(enum dplla attr_id, const struct dpll_attr *attr);

/**
 * dpll_attr_copy - create a copy of the dpll attributes structure
 * @dst: destination structure with dpll attributes
 * @src: source structure with dpll attributes
 *
 * Memory needs to be allocated before calling this function.
 * Return: 0 if succeeds, error code otherwise.
 */
int
dpll_attr_copy(struct dpll_attr *dst, const struct dpll_attr *src);

/**
 * dpll_attr_lock_status_set - set the lock status in the attributes
 * @attr: structure with dpll attributes
 * @status: dpll lock status to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_lock_status_set(struct dpll_attr *attr,
			      enum dpll_lock_status status);

/**
 * dpll_attr_lock_status_get - get the lock status from the attributes
 * @attr: structure with dpll attributes
 *
 * Return: dpll lock status
 */
enum dpll_lock_status
dpll_attr_lock_status_get(const struct dpll_attr *attr);

/**
 * dpll_attr_temp_set - set the temperature in the attributes
 * @attr: structure with dpll attributes
 * @temp: temperature to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_temp_set(struct dpll_attr *attr, s32 temp);

/**
 * dpll_attr_temp_get - get the temperature from the attributes
 * @attr: structure with dpll attributes
 * @temp: temperature (out)
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_temp_get(const struct dpll_attr *attr, s32 *temp);

/**
 * dpll_attr_source_idx_set - set the source id in the attributes
 * @attr: structure with dpll attributes
 * @source_idx: source id to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_source_idx_set(struct dpll_attr *attr, u32 source_idx);

/**
 * dpll_attr_source_idx_get - get the source id from the attributes
 * @attr: structure with dpll attributes
 * @source_idx: source id (out)
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_source_idx_get(const struct dpll_attr *attr, u32 *source_idx);

/**
 * dpll_attr_mode_set - set the dpll mode in the attributes
 * @attr: structure with dpll attributes
 * @mode: dpll mode to be set
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_mode_set(struct dpll_attr *attr, enum dpll_mode mode);

/**
 * dpll_attr_mode_get - get the dpll mode from the attributes
 * @attr: structure with dpll attributes
 *
 * Return: dpll mode.
 */
enum dpll_mode dpll_attr_mode_get(const struct dpll_attr *attr);

/**
 * dpll_attr_mode_set - set the dpll supported mode in the attributes
 * @attr: structure with dpll attributes
 * @mode: dpll mode to be set in supported modes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_mode_supported_set(struct dpll_attr *attr, enum dpll_mode mode);

/**
 * dpll_attr_mode_supported - check if the dpll mode is supported
 * @attr: structure with dpll attributes
 * @mode: dpll mode to be checked
 *
 * Return: true if mode supported, false otherwise.
 */
bool dpll_attr_mode_supported(const struct dpll_attr *attr,
			      enum dpll_mode mode);

/**
 * dpll_attr_delta - calculate the difference between two dpll attribute sets
 * @delta: structure with delta of dpll attributes
 * @new: structure with new dpll attributes
 * @old: structure with old dpll attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_attr_delta(struct dpll_attr *delta, struct dpll_attr *new,
		    struct dpll_attr *old);

/**
 * dpll_pin_attr_alloc - allocate a dpll pin attributes struct
 *
 * Return: pointer if succeeded, NULL otherwise.
 */
struct dpll_pin_attr *dpll_pin_attr_alloc(void);

/**
 * dpll_pin_attr_free - frees a dpll pin attributes struct
 * @attr: structure with dpll pin attributes
 */
void dpll_pin_attr_free(struct dpll_pin_attr *attr);

/**
 * dpll_pin_attr_clear - clears a dpll pin attributes struct
 * @attr: structure with dpll attributes
 */
void dpll_pin_attr_clear(struct dpll_pin_attr *attr);

/**
 * dpll_pin_attr_valid - checks if a pin attribute is valid
 * @attr_id: attribute to be checked
 * @attr: structure with dpll pin attributes
 *
 * Checks if the attribute has been set before and if stored value is valid.
 * Return: true if valid, false otherwise.
 */
bool dpll_pin_attr_valid(enum dplla attr_id, const struct dpll_pin_attr *attr);

/**
 * dpll_pin_attr_copy - create a copy of the dpll pin attributes structure
 * @dst: destination structure with dpll pin attributes
 * @src: source structure with dpll pin attributes
 *
 * Memory needs to be allocated before calling this function.
 * Return: 0 if succeeds, error code otherwise.
 */
int
dpll_pin_attr_copy(struct dpll_pin_attr *dst, const struct dpll_pin_attr *src);

/**
 * dpll_pin_attr_type_set - set the pin type in the attributes
 * @attr: structure with dpll pin attributes
 * @type: parameter to be set in attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_type_set(struct dpll_pin_attr *attr, enum dpll_pin_type type);

/**
 * dpll_pin_attr_type_get - get the pin type from the attributes
 * @attr: structure with dpll pin attributes
 *
 * Return: dpll pin type.
 */
enum dpll_pin_type dpll_pin_attr_type_get(const struct dpll_pin_attr *attr);

/**
 * dpll_pin_attr_type_supported_set - set the dpll pin supported type in the
 * attributes
 * @attr: structure with dpll attributes
 * @type: pin type to be set in supported modes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_type_supported_set(struct dpll_pin_attr *attr,
				     enum dpll_pin_type type);

/**
 * dpll_pin_attr_type_supported - check if the pin type is supported
 * @attr: structure with dpll attributes
 * @type: dpll mode to be checked
 *
 * Return: true if type supported, false otherwise.
 */
bool dpll_pin_attr_type_supported(const struct dpll_pin_attr *attr,
				     enum dpll_pin_type type);

/**
 * dpll_pin_attr_signal_type_set - set the pin signal type in the attributes
 * @attr: structure with dpll attributes
 * @type: pin signal type to be set in supported modes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_signal_type_set(struct dpll_pin_attr *attr,
				  enum dpll_pin_signal_type type);

/**
 * dpll_pin_attr_signal_type_get - get the pin signal type from the attributes
 * @attr: structure with dpll pin attributes
 *
 * Return: pin signal type.
 */
enum dpll_pin_signal_type
dpll_pin_attr_signal_type_get(const struct dpll_pin_attr *attr);

/**
 * dpll_pin_attr_signal_type_supported_set - set the dpll pin supported signal
 * type in the attributes
 * @attr: structure with dpll attributes
 * @type: pin signal type to be set in supported types
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_signal_type_supported_set(struct dpll_pin_attr *attr,
					    enum dpll_pin_signal_type type);

/**
 * dpll_pin_attr_signal_type_supported - check if the pin signal type is
 * supported
 * @attr: structure with dpll attributes
 * @type: pin signal type to be checked
 *
 * Return: true if type supported, false otherwise.
 */
bool dpll_pin_attr_signal_type_supported(const struct dpll_pin_attr *attr,
					    enum dpll_pin_signal_type type);

/**
 * dpll_pin_attr_custom_freq_set - set the custom frequency in the attributes
 * @attr: structure with dpll pin attributes
 * @freq: parameter to be set in attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_custom_freq_set(struct dpll_pin_attr *attr, u32 freq);

/**
 * dpll_pin_attr_custom_freq_get - get the pin type from the attributes
 * @attr: structure with dpll pin attributes
 * @freq: parameter to be retrieved
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_custom_freq_get(const struct dpll_pin_attr *attr, u32 *freq);

/**
 * dpll_pin_attr_state_set - set the pin state the attributes
 * @attr: structure with dpll pin attributes
 * @state: parameter to be set in attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_state_set(struct dpll_pin_attr *attr,
			    enum dpll_pin_state state);

/**
 * dpll_pin_attr_state_enabled - check if state is enabled
 * @attr: structure with dpll pin attributes
 * @state: parameter to be checked in attributes
 *
 * Return: true if enabled, false otherwise.
 */
bool dpll_pin_attr_state_enabled(const struct dpll_pin_attr *attr,
				 enum dpll_pin_state state);

/**
 * dpll_pin_attr_state_supported_set - set the supported pin state in the
 * attributes
 * @attr: structure with dpll attributes
 * @state: pin state to be set in supported types
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_state_supported_set(struct dpll_pin_attr *attr,
				      enum dpll_pin_state state);

/**
 * dpll_pin_attr_state_supported - check if the pin state is supported
 * @attr: structure with dpll attributes
 * @state: pin signal type to be checked
 *
 * Return: true if state supported, false otherwise.
 */
bool dpll_pin_attr_state_supported(const struct dpll_pin_attr *attr,
				   enum dpll_pin_state state);

/**
 * dpll_pin_attr_prio_set - set the pin priority in the attributes
 * @attr: structure with dpll pin attributes
 * @prio: parameter to be set in attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_prio_set(struct dpll_pin_attr *attr, u32 prio);

/**
 * dpll_pin_attr_prio_get - get the pin priority from the attributes
 * @attr: structure with dpll pin attributes
 * @prio: parameter to be retrieved
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_prio_get(const struct dpll_pin_attr *attr, u32 *prio);

/**
 * dpll_pin_attr_netifindex_set - set the pin netifindex in the attributes
 * @attr: structure with dpll pin attributes
 * @netifindex: parameter to be set in attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_netifindex_set(struct dpll_pin_attr *attr,
				 unsigned int netifindex);

/**
 * dpll_pin_attr_netifindex_get - get the pin netifindex from the attributes
 * @attr: structure with dpll pin attributes
 * @netifindex: parameter to be retrieved
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_netifindex_get(const struct dpll_pin_attr *attr,
				 unsigned int *netifindex);

/**
 * dpll_attr_delta - calculate the difference between two dpll pin attribute sets
 * @delta: structure with delta of dpll pin attributes
 * @new: structure with new dpll pin attributes
 * @old: structure with old dpll pin attributes
 *
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_delta(struct dpll_pin_attr *delta, struct dpll_pin_attr *new,
			struct dpll_pin_attr *old);

/**
 * dpll_pin_attr_prep_common - calculate the common dpll pin attributes
 * @common: structure with common dpll pin attributes
 * @reference: referenced structure with dpll pin attributes
 *
 * Some of the pin attributes applies to all DPLLs and other are exclusive.
 * This function calculates if any of the common pin attributes are set.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_prep_common(struct dpll_pin_attr *common,
			      const struct dpll_pin_attr *reference);

/**
 * dpll_pin_attr_prep_exclusive - calculate the exclusive dpll pin attributes
 * @exclusive: structure with common dpll pin attributes
 * @reference: referenced structure with dpll pin attributes
 *
 * Some of the pin attributes applies to all DPLLs and other are exclusive.
 * This function calculates if any of the exclusive pin attributes are set.
 * Return: 0 if succeeds, error code otherwise.
 */
int dpll_pin_attr_prep_exclusive(struct dpll_pin_attr *exclusive,
				 const struct dpll_pin_attr *reference);


#endif /* __DPLL_ATTR_H__ */
