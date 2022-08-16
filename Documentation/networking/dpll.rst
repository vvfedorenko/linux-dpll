.. SPDX-License-Identifier: GPL-2.0

===============================
The Linux kernel DPLL subsystem
===============================


The main purpose of DPLL subsystem is to provide general interface
to configure devices that use any kind of Digital PLL and could use
different sources of signal to synchronize to as well as different
types of outputs. The inputs and outputs could be internal components
of the device as well as external connections. The main interface is
NETLINK_GENERIC based protocol with config and monitoring groups of
commands defined.

Configuration commands group
============================

Configuration commands are used to get information about registered
DPLL devices as well as get or set configuration of each used input
or output. As DPLL device could not be abstract and reflects real
hardware, there is no way to add new DPLL device via netlink from
user space and each device should be registered by it's driver.

List of command with possible attributes
========================================

All constants identifying command types use ``DPLL_CMD_`` prefix and
suffix according to command purpose. All attributes use ``DPLLA_``
prefix and suffix according to attribute purpose:

  =====================================  =============================
  ``DEVICE_GET``                         userspace to get device info
    ``DEVICE_ID``                        attr internal device index
    ``DEVICE_NAME``                      attr DPLL device name
    ``STATUS``                           attr DPLL device status info
    ``DEVICE_SRC_SELECT_MODE``           attr DPLL source selection
                                         mode
    ``DEVICE_SRC_SELECT_MODE_SUPPORTED`` attr supported source
                                         selection modes
    ``LOCK_STATUS``                      attr internal frequency-lock
                                         status
    ``TEMP``                             attr device temperature
                                         information
  ``SET_SOURCE``                         userspace to set
                                         sources/inputs configuration
    ``DEVICE_ID``                        attr internal device index
                                         to configure source pin
    ``SOURCE_ID``                        attr index of source pin to
                                         configure
    ``SOURCE_NAME``                      attr name of source pin to
                                         configure
    ``SOURCE_TYPE``                      attr configuration value for
                                         selected source pin
  ``SET_OUTPUT``                         userspace to set outputs
                                         configuration
    ``DEVICE_ID``                        attr internal device index to
                                         configure output pin
    ``OUTPUT_ID``                        attr index of output pin to
                                         configure
    ``OUTPUT_NAME``                      attr name of output pin to
                                         configure
    ``OUTPUT_TYPE``                      attr configuration value for
                                         selected output pin
  ``SET_SRC_SELECT_MODE``                userspace to set source pin
                                         selection mode
    ``DEVICE_ID``                        attr internal device index
    ``DEVICE_SRC_SELECT_MODE``           attr source selection mode
  ``SET_SOURCE_PRIO``                    userspace to set priority of
                                         a source pin for automatic
                                         source selection mode
    ``DEVICE_ID``                        attr internal device index
                                         for source pin
    ``SOURCE_ID``                        attr index of source pin to
                                         configure
    ``SOURCE_PRIO``                      attr priority of a source pin


The pre-defined enums
=====================

These enums are used to select type values for source/input and
output pins:

  ============================= ======================================
  ``DPLL_TYPE_EXT_1PPS``        External 1PPS source
  ``DPLL_TYPE_EXT_10MHZ``       External 10MHz source
  ``DPLL_TYPE_SYNCE_ETH_PORT``  SyncE on Ethernet port
  ``DPLL_TYPE_INT_OSCILLATOR``  Internal Oscillator (i.e. Holdover
                                with Atomic Clock as a Source)
  ``DPLL_TYPE_GNSS``            GNSS 1PPS source
  ``DPLL_TYPE_CUSTOM``          Custom frequency

Values for monitoring attributes STATUS:

  ============================= ======================================
  ``DPLL_STATUS_NONE``          No information provided
  ``DPLL_STATUS_CALIBRATING``   DPLL device is not locked to the
                                source frequency
  ``DPLL_STATUS_LOCKED``        DPLL device is locked to the source
                                frequency


Possible DPLL source selection mode values:

  ============================= ======================================
  ``DPLL_SRC_SELECT_FORCED``    source pin is force-selected by
                                DPLL_CMD_SET_SOURCE_TYPE
  ``DPLL_SRC_SELECT_AUTOMATIC`` source pin ise auto selected according
                                to configured priorities and source
                                signal validity
  ``DPLL_SRC_SELECT_HOLDOVER``  force holdover mode of DPLL
  ``DPLL_SRC_SELECT_FREERUN``   DPLL is driven by supplied system
                                clock without holdover capabilities
  ``DPLL_SRC_SELECT_NCO``       similar to FREERUN, with possibility
                                to numerically control frequency offset

Notifications
================

DPLL device can provide notifications regarding status changes of the
device, i.e. lock status changes, source/output type changes or alarms.
This is the multicast group that is used to notify user-space apps via
netlink socket:

  ============================== ====================================
  ``DPLL_EVENT_DEVICE_CREATE``   New DPLL device was created
  ``DPLL_EVENT_DEVICE_DELETE``   DPLL device was deleted
  ``DPLL_EVENT_STATUS_LOCKED``   DPLL device has locked to source
  ``DPLL_EVENT_STATUS_UNLOCKED`` DPLL device is in freerun or
                                 in calibration mode
  ``DPLL_EVENT_SOURCE_CHANGE``   DPLL device source changed
  ``DPLL_EVENT_OUTPUT_CHANGE``   DPLL device output changed
  ``DPLL_EVENT_SOURCE_PRIO``     DPLL device source priority changed
  ``DPLL_EVENT_SELECT_MODE``     DPLL device source selection mode
                                 changed

Device driver implementation
============================

For device to operate as DPLL subsystem device, it should implement
set of operations and register device via ``dpll_device_alloc`` and
``dpll_device_register`` providing desired device name and set of
supported operations as well as the amount of sources/input pins and
output pins. If there is no specific name supplied, dpll subsystem
will use ``dpll%d`` template to create device name. Notifications of
adding or removing DPLL devices are created within subsystem itself,
but notifications about configurations changes or alarms should be
implemented within driver as different ways of confirmation could be
used. All the interfaces for notification messages could be found in
``<dpll.h>``, constats and enums are placed in ``<uapi/linux/dpll.h>``
to be consistent with user-space.

There is no strict requeriment to implement all the operations for
each device, every operation handler is checked for existence and
ENOTSUPP is returned in case of absence of specific handler.

