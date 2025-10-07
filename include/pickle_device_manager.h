// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_LIBRARY_H
#define PICKLE_LIBRARY_H

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pickle_job.h"
#include "pickle_utils.h"

enum PickleDeviceCommand { ADD_WATCH_RANGE = 1, SEND_JOB_DESCRIPTOR = 2 };
struct PickleDevicePrefetcherSpecs {
  uint64_t availability;
  uint64_t prefetch_distance;
};

class PickleDeviceManager {
 public:
  PickleDeviceManager();
  ~PickleDeviceManager();
  bool sendJob(const PickleJob& job);
  uint8_t* getUCPagePtr(const uint64_t mmap_id);
  uint8_t* getPerfPagePtr();
  PickleDevicePrefetcherSpecs getDevicePrefetcherSpecs();

 private:
  std::unordered_map<uint64_t, uint8_t*> mmap_id_to_uc_ptr_map;
  std::unordered_map<uint64_t, uint64_t> mmap_id_to_uc_paddr_map;
  uint8_t* perf_page_ptr;
  void registerUncacheablePage(const uint64_t mmap_id, uint8_t* ptr,
                               uint64_t paddr);
  void deallocateUncacheablePage(const uint64_t mmap_id);
  bool writeUncacheablePagePaddr(const uint64_t mmap_id);
  bool writeJobToPickleDevice(const std::vector<uint8_t>& job_descriptor);
};
#endif  // PICKLE_LIBRARY_H
