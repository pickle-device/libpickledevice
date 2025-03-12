// Copyright (c) 2024 Advanced Micro Devices, Inc.
// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_UTILS_H
#define PICKLE_UTILS_H

#include <stdint.h>
#include <unistd.h>

class AddressRange
{
    public:
        uint64_t start = 0;
        uint64_t end = 0;
        AddressRange() {}
        AddressRange(const uint64_t& start, const uint64_t& end)
            : start(start), end(end)
        {
        }
};

#endif // PICKLE_UTILS_H
