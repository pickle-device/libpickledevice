// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_DEVICE_LOW_LEVEL_H
#define PICKLE_DEVICE_LOW_LEVEL_H

#include "pickle_driver.h"

// Return an uncacheable page for device's type 1 communication
bool allocate_uncacheable_page(const uint64_t mmap_id, uint8_t** ptr);
// Return an uncacheable page for device's type 2 communication
// (performance monitoring related communication)
bool allocate_perf_page(uint8_t** ptr);
// Return the type 1 communication page paddr
bool get_mmap_paddr(const uint64_t mmap_id, uint64_t& paddr);
// Return the type 2 commnucation page paddr
bool get_perf_page_paddr(uint64_t& paddr);
// Write command to the device
bool write_command_to_device(uint64_t command_type, uint64_t command_length,
                             const uint8_t* command);
// Get device specification
struct device_specs get_device_specs();
#endif  // PICKLE_DEVICE_LOW_LEVEL_H
