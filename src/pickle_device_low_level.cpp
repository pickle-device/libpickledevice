// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_DEVICE_LOW_LEVEL_H
#define PICKLE_DEVICE_LOW_LEVEL_H

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "pickle_driver.h"

bool allocate_uncacheable_page(const uint64_t mmap_id, uint8_t** ptr) {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  int err = 0;

  *ptr = nullptr;

  fd = open(pickle_driver_dev, O_RDWR | O_SYNC);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    return false;
  }

  uint8_t* mmap_ptr = (uint8_t*)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                     MAP_FILE | MAP_SHARED, fd, 0);
  if (mmap_ptr == MAP_FAILED) {
    std::cerr << "Failed to open mmap for" << pickle_driver_dev_str
              << std::endl;
    perror("Error");
    close(fd);
    return false;
  }
  *ptr = mmap_ptr;
  return true;
}

bool allocate_perf_page(uint8_t** ptr) {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  int err = 0;

  *ptr = nullptr;

  fd = open(pickle_driver_dev, O_RDWR | O_SYNC);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    return false;
  }

  // We set the length to 16 bytes to signal that we want to allocate a page
  // for performance monitoring.
  uint8_t* mmap_ptr = (uint8_t*)mmap(NULL, 16, PROT_READ | PROT_WRITE,
                                     MAP_FILE | MAP_SHARED, fd, 0);
  if (mmap_ptr == MAP_FAILED) {
    std::cerr << "Failed to open mmap for" << pickle_driver_dev_str
              << std::endl;
    perror("Error");
    close(fd);
    return false;
  }
  *ptr = mmap_ptr;
  return true;
}

bool get_mmap_paddr(const uint64_t mmap_id, uint64_t& paddr) {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  int err = 0;
  struct process_pagetable_params params;

  fd = open(pickle_driver_dev, O_RDWR | O_SYNC);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    return false;
  }

  mmap_paddr_params mmap_params;
  mmap_params.mmap_id = mmap_id;
  mmap_params.paddr = 0;

  err = ioctl(fd, ARM64_IOC_PICKLE_DRIVER_MMAP_PADDR, &mmap_params);
  if (err) {
    std::cerr << "Unsuccessfully retrieving physical address for mmap"
              << std::endl;
    perror("Error");
    close(fd);
    return false;
  }
  paddr = mmap_params.paddr;

  close(fd);
  return true;
}

bool get_perf_page_paddr(uint64_t& paddr) {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  int err = 0;
  struct process_pagetable_params params;

  fd = open(pickle_driver_dev, O_RDWR | O_SYNC);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    return false;
  }

  mmap_paddr_params mmap_params;
  mmap_params.mmap_id = 0;
  mmap_params.paddr = 0;

  err = ioctl(fd, ARM64_IOC_PICKLE_DRIVER_PERF_PAGE_PADDR, &mmap_params);
  if (err) {
    std::cerr << "Unsuccessfully retrieving physical address for mmap"
              << std::endl;
    perror("Error");
    close(fd);
    return false;
  }
  paddr = mmap_params.paddr;

  close(fd);
  return true;
}

bool write_command_to_device(uint64_t command_type, uint64_t command_length,
                             const uint8_t* command) {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  uint64_t content1[2];
  uint64_t content1_size = 16;
  uint8_t* content1_ptr8 = (uint8_t*)content1;

  content1[0] = command_type;
  content1[1] = command_length;

  fd = open(pickle_driver_dev, O_RDWR);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    return false;
  }

  if (pwrite(fd, content1_ptr8, 16, 0) != content1_size) {
    std::cerr << "error while writing control to " << pickle_driver_dev_str
              << std::endl;
    perror("Error");
    close(fd);
    return false;
  }

  uint64_t written_bytes = pwrite(fd, command, command_length, 1);
  if (written_bytes != command_length) {
    std::cerr << "error while writing command to " << pickle_driver_dev_str
              << std::endl;
    std::cerr << "written bytes: " << written_bytes << std::endl;
    perror("Error");
    close(fd);
    return false;
  }

  close(fd);

  return true;
}

struct device_specs get_device_specs() {
  const std::string pickle_driver_dev_str = "/dev/hey_pickle";
  const char* pickle_driver_dev = pickle_driver_dev_str.c_str();
  int fd;
  struct device_specs specs;

  fd = open(pickle_driver_dev, O_RDWR | O_SYNC);

  if (fd < 0) {
    std::cerr << "failed to open " << pickle_driver_dev_str << std::endl;
    perror("Error");
    exit(errno);
    return specs;
  }

  std::cout << "opened the fd" << std::endl;

  int err = ioctl(fd, IOC_PICKLE_DRIVER_GET_DEVICE_SPECS, &specs);
  if (err) {
    std::cerr << "error while IOC_PICKLE_DRIVER_GET_DEVICE_SPECS from "
              << pickle_driver_dev_str << std::endl;
    perror("Error");
    close(fd);
    exit(errno);
  }

  std::cout << "acquired the spec" << std::endl;

  return specs;
}

#endif  // PICKLE_DEVICE_LOW_LEVEL_H
