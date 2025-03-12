# Copyright (c) 2024 Advanced Micro Devices, Inc.
# Copyright (c) 2025 The Regents of the University of California
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

CXX=g++
CXXFLAGS=-O3
LINKER=ld
OBJCOPY=objcopy

all: libpickledevice.so

pickle_device_manager.o: src/pickle_device_manager.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c src/pickle_device_manager.cpp -o pickle_device_manager.o -rdynamic -Wl,-E

pickle_device_low_level.o: src/pickle_device_low_level.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c src/pickle_device_low_level.cpp -o pickle_device_low_level.o -rdynamic -Wl,-E

libpickledevice.so: pickle_device_manager.o pickle_device_low_level.o
	$(CXX) $(CXXFLAGS) -fPIC pickle_device_low_level.o -shared pickle_device_manager.o -o libpickledevice.so -rdynamic -Wl,-E

clean:
	rm -f *.so *.o
