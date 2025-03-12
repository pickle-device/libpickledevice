// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_DEVICE_LOW_LEVEL_H
#define PICKLE_DEVICE_LOW_LEVEL_H

bool allocate_uncacheable_page(const uint64_t mmap_id, uint8_t** ptr);
bool get_mmap_paddr(const uint64_t mmap_id, uint64_t& paddr);
bool write_command_to_device(uint64_t command_type, uint64_t command_length,
                             const uint8_t* command);

#endif  // PICKLE_DEVICE_LOW_LEVEL_H
