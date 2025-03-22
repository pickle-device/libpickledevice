// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "pickle_device_manager.h"

int main() {
  std::unique_ptr<PickleDeviceManager> pdev(new PickleDeviceManager());

  PickleDevicePrefetcherSpecs specs;
  specs.availability = -1ULL;
  specs.prefetch_distance = -1ULL;
  specs = pdev->getDevicePrefetcherSpecs();

  std::cout << "Availability: " << specs.availability << std::endl;
  std::cout << "Prefetch distance: " << specs.prefetch_distance << std::endl;

  int sum = 0;
  for (int i = 0; i < 100000; i++) sum += i;

  std::cout << "Done " << sum << std::endl;
  return 0;
}
