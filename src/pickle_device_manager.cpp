// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "../include/pickle_device_manager.h"

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

#include "pickle_device_low_level.h"

PickleDeviceManager::PickleDeviceManager() {
  uint64_t mmap_id = 0;
  uint8_t* mmap_ptr = getUCPagePtr(mmap_id);
  writeUncacheablePagePaddr(mmap_id);
}

PickleDeviceManager::~PickleDeviceManager() { deallocateUncacheablePage(0); }

bool PickleDeviceManager::sendJob(const PickleJob& job) {
  std::cout << "sendJob" << std::endl;
  std::vector<uint8_t> job_descriptor = job.getJobDescriptor();
  return writeJobToPickleDevice(job_descriptor);  // update the driver for this
}

uint8_t* PickleDeviceManager::getUCPagePtr(const uint64_t mmap_id) {
  if (mmap_id_to_uc_ptr_map.find(mmap_id) == mmap_id_to_uc_ptr_map.end()) {
    uint8_t* mmap_ptr = nullptr;
    bool allocate_success = allocate_uncacheable_page(mmap_id, &mmap_ptr);
    if (!allocate_success) {
      std::cout << "PickleDeviceManager: failed to allocate a new uncacheable "
                   "page for mmap_id: "
                << mmap_id << std::endl;
      exit(1);
    }
    uint64_t paddr = 0;
    bool get_paddr_success = get_mmap_paddr(mmap_id, paddr);
    if (!get_paddr_success) {
      std::cout << "PickleDeviceManager: failed to get paddr for an "
                   "uncacheable page for mmap_id: "
                << mmap_id << std::endl;
      exit(1);
    }
    registerUncacheablePage(mmap_id, mmap_ptr, paddr);
    // induce page fault
    mmap_ptr[0] = 0xDECAF;
  }
  return mmap_id_to_uc_ptr_map[mmap_id];
}

void PickleDeviceManager::registerUncacheablePage(const uint64_t mmap_id,
                                                  uint8_t* ptr,
                                                  uint64_t paddr) {
  mmap_id_to_uc_ptr_map[mmap_id] = ptr;
  mmap_id_to_uc_paddr_map[mmap_id] = paddr;
  std::cout << "PickleDeviceManager: Register Uncacheable Page: mmap_id: "
            << mmap_id << " vaddr: 0x" << std::hex << (uint64_t)ptr
            << " paddr: 0x" << paddr << std::dec << std::endl;
}

void PickleDeviceManager::deallocateUncacheablePage(const uint64_t mmap_id) {
  munmap((void*)mmap_id_to_uc_ptr_map[mmap_id], 4096);
}

bool PickleDeviceManager::writeUncacheablePagePaddr(const uint64_t mmap_id) {
  uint64_t range[2];
  range[0] = mmap_id_to_uc_paddr_map[mmap_id];
  range[1] = range[0] + 0x1000;
  uint64_t* range_ptr64 = range;
  uint8_t* range_ptr8 = (uint8_t*)(range_ptr64);
  std::cout << "writeUncacheablePagePaddr 0x" << std::hex << range[0] << " - 0x"
            << range[1] << std::dec << std::endl;
  return write_command_to_device(PickleDeviceCommand::ADD_WATCH_RANGE, 16,
                                 range_ptr8);
}

bool PickleDeviceManager::writeJobToPickleDevice(
    const std::vector<uint8_t> job_descriptor) {
  return write_command_to_device(PickleDeviceCommand::SEND_JOB_DESCRIPTOR,
                                 job_descriptor.size(), job_descriptor.data());
}
