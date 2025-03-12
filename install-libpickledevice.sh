#!/bin/bash

# Copyright (c) 2024 Advanced Micro Devices, Inc.
# Copyright (c) 2025 The Regents of the University of California
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

sudo cp libpickledevice.so /usr/lib/
sudo chmod a+rwX /usr/lib/libpickledevice.so

sudo cp libpickledevice.so /usr/local/lib/
sudo chmod a+rwX /usr/local/lib/libpickledevice.so

sudo cp include/pickle_job.h /usr/include/
sudo chmod a+rwX /usr/include/pickle_job.h

sudo cp include/pickle_device_manager.h /usr/include/
sudo chmod a+rwX /usr/include/pickle_device_manager.h

sudo cp include/pickle_graph.h /usr/include/
sudo chmod a+rwX /usr/include/pickle_graph.h

sudo cp include/pickle_utils.h /usr/include/
sudo chmod a+rwX /usr/include/pickle_utils.h

sudo cp -rf include/graphs/ /usr/include/
sudo chmod a+rwX -R /usr/include/graphs/
