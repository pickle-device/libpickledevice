// Copyright (c) 2024 Advanced Micro Devices, Inc.
// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PICKLE_JOB_LIBRARY_H
#define PICKLE_JOB_LIBRARY_H

#include "pickle_driver.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pickle_utils.h"

// public API
enum AccessType { SingleElement = 0, Ranged = 1 };
enum AddressingMode { Pointer = 0, Index = 1 };

class PickleArrayDescriptor
{
    private:
        const static uint64_t unassignedID; // a value indicating the ID has not been assigned yet
        static uint64_t assignNextId()
        {
            uint64_t id = PickleArrayDescriptor::nextID;
            PickleArrayDescriptor::nextID += 1;
            return id;
        }
        static uint64_t nextID;
        uint64_t array_id;
    public:
        uint64_t dst_indexing_array_id;
        uint64_t vaddr_start;
        uint64_t vaddr_end;
        uint64_t element_size;
        AccessType access_type;
        AddressingMode addressing_mode;
        PickleArrayDescriptor()
          : array_id(PickleArrayDescriptor::unassignedID),
            dst_indexing_array_id(-1ULL),
            vaddr_start(0), vaddr_end(0),
            element_size(0),
            access_type(AccessType::SingleElement),
            addressing_mode(AddressingMode::Pointer)
        {
        }
        uint64_t getArrayId()
        {
            if (this->array_id == PickleArrayDescriptor::unassignedID)
                this->array_id = PickleArrayDescriptor::assignNextId();
            return this->array_id;
        }
        void setAccessType(const AccessType& new_access_type)
        {
            access_type = new_access_type;
        }
        void setAddressingMode(const AddressingMode& new_addressing_mode)
        {
            addressing_mode = new_addressing_mode;
        }
        std::tuple<uint64_t, uint64_t, bool, bool, uint64_t, uint64_t, uint64_t> getTuple() const
        {
            return std::make_tuple(
                array_id,
                dst_indexing_array_id,
                addressing_mode == AddressingMode::Index,
                access_type == AccessType::Ranged,
                vaddr_start,
                (vaddr_end - vaddr_start) / element_size,
                element_size
            );
        }
        std::tuple<uint64_t, uint64_t, bool, bool, uint64_t, uint64_t, uint64_t> getTuple(const std::unordered_map<uint64_t, uint64_t>& rename_map) const
        {
            return std::make_tuple(
                rename_map.at(array_id),
                rename_map.at(dst_indexing_array_id),
                addressing_mode == AddressingMode::Index,
                access_type == AccessType::Ranged,
                vaddr_start,
                (vaddr_end - vaddr_start) / element_size,
                element_size
            );
        }
        void print() const
        {
            std::string am = (addressing_mode == AddressingMode::Index) ? "Index" : "Pointer";
            std::string at = (access_type == AccessType::Ranged) ? "Ranged" : "Single";
            std::cout << "array_id: " << array_id << std::endl \
                      << "- dst_array: " << dst_indexing_array_id << std::endl \
                      << "- addressing_mode: " << am << std::endl \
                      << "- access_type: " << at << std::endl \
                      << "- vaddr: 0x" << std::hex << vaddr_start << std::dec << std::endl \
                      << "- #elements: " << ((vaddr_end - vaddr_start) / element_size) << std::endl \
                      << "- element_size: " << element_size << std::endl;
        }
        void print(const std::unordered_map<uint64_t, uint64_t>& rename_map) const
        {
            std::string am = (addressing_mode == AddressingMode::Index) ? "Index" : "Pointer";
            std::string at = (access_type == AccessType::Ranged) ? "Ranged" : "Single";
            std::cout << "array_id: " << rename_map.at(array_id) << std::endl \
                      << "- dst_array: " << rename_map.at(dst_indexing_array_id) << std::endl \
                      << "- addressing_mode: " << am << std::endl \
                      << "- access_type: " << at << std::endl \
                      << "- vaddr: 0x" << std::hex << vaddr_start << std::dec << std::endl \
                      << "- #elements: " << ((vaddr_end - vaddr_start) / element_size) << std::endl \
                      << "- element_size: " << element_size << std::endl;
        }
};

class PickleJob
{
    private:
        std::string kernel_name; // this will be used to determine which prefetch generator to use
        std::vector<std::shared_ptr<PickleArrayDescriptor>> arrays;
        std::unordered_map<uint64_t, uint64_t> array_rename_map;
        uint64_t renameCount = 0;
        uint64_t root = -1;
        void addToJobDescriptor(
            std::vector<uint8_t>& job_descriptor, const uint64_t& value
        ) const
        {
            // little-endian
            const uint64_t* ptr64 = &value;
            const uint8_t* ptr8 = (const uint8_t*) ptr64;
            const size_t n_bytes = 8;
            for (size_t i = 0; i < n_bytes; i++)
                job_descriptor.push_back(ptr8[i]);
        }
        void addKernelNameToJobDescriptor(std::vector<uint8_t>& job_descriptor) const
        {
            for (uint64_t i = 0; i < kernel_name.size(); i++) {
                job_descriptor.push_back(kernel_name[i]);
            }
        }
    public:
        PickleJob()
        {
            kernel_name = "";
            array_rename_map[-1ULL] = -1ULL;
            arrays.reserve(5);
        }
        PickleJob(std::string _kernel_name)
        {
            kernel_name = _kernel_name;
            array_rename_map[-1ULL] = -1ULL;
            arrays.reserve(5);
        }
        void changeAccessTypeByArrayId(const uint64_t& arrayId, const AccessType& accessType)
        {
            for (auto& array: arrays)
            {
                if (array->getArrayId() == arrayId)
                {
                    array->setAccessType(accessType);
                    return;
                }
            }
            std::cout << "Warn: changeAccessTypeByArrayId cannot find arrayId = " << arrayId << std::endl;
        }
        void changeAddressingModeByArrayId(const uint64_t& arrayId, const AddressingMode& addressingMode)
        {
            for (auto& array: arrays)
            {
                if (array->getArrayId() == arrayId)
                {
                    array->setAddressingMode(addressingMode);
                    return;
                }
            }
            std::cout << "Warn: changeAddressingModeByArrayId cannot find arrayId = " << arrayId << std::endl;
        }
        void addArrayDescriptor(const std::shared_ptr<PickleArrayDescriptor>& array)
        {
            // The prefetcher assumes contiguous array ids, i.e., 0, 1, 2, etc.
            // While the library assumes arbitrary ids for arrays, e.g., 4 -> 5 -> 1 -> -1
            // So, the library needs to rename the array ids so it will work with the prefetcher
            const uint64_t array_id = array->getArrayId();
            array_rename_map[array_id] = renameCount;
            renameCount++;
            arrays.push_back(array);
        }
        std::vector<uint8_t> getJobDescriptor() const
        {
            // layout: 8 bits for the number of arrays + number_of_arrays * (7 * 64 bits) for the array description
            std::vector<uint8_t> job_descriptor;
            const uint8_t n_arrays = arrays.size();
            job_descriptor.reserve(1 + 7 * 8 * n_arrays + kernel_name.size());
            job_descriptor.push_back(n_arrays);
            for (const auto & arr: arrays)
            {
                this->addToJobDescriptor(job_descriptor, array_rename_map.at(arr->getArrayId()));
                this->addToJobDescriptor(job_descriptor, array_rename_map.at(arr->dst_indexing_array_id));
                this->addToJobDescriptor(job_descriptor, arr->vaddr_start);
                this->addToJobDescriptor(job_descriptor, arr->vaddr_end);
                this->addToJobDescriptor(job_descriptor, arr->element_size);
                this->addToJobDescriptor(job_descriptor, arr->access_type);
                this->addToJobDescriptor(job_descriptor, arr->addressing_mode);
            }
            this->addKernelNameToJobDescriptor(job_descriptor);
            return job_descriptor;
        }
        void print() const
        {
            std::cout << "-----" << std::endl;
            std::cout << "kernel_name: " << kernel_name << std::endl;
            for (const auto& arr: arrays)
            {
                if (renameCount > 0)
                    arr->print(array_rename_map);
                else
                    arr->print();
            }
            std::cout << "-----" << std::endl;
        }
};

uint64_t PickleArrayDescriptor::nextID = 1;
const uint64_t PickleArrayDescriptor::unassignedID = 0;

#endif // PICKLE_JOB_LIBRARY_H
