// Copyright (c) 2015, The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#ifndef PVECTOR_H_
#define PVECTOR_H_

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#include "pickle_job.h"

/*
GAP Benchmark Suite
Class:  pvector
Author: Scott Beamer

Vector class with ability to not initialize or do initialize in parallel
 - std::vector (when resizing) will always initialize, and does it serially
 - When pvector is resized, new elements are uninitialized
 - Resizing is not thread-safe
*/


template <typename T_>
class pvector {
 public:
  typedef T_* iterator;

  pvector() : start_(nullptr), end_size_(nullptr), end_capacity_(nullptr)
  {
    // Initializing array descriptor
    array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
  }

  explicit pvector(size_t num_elements) {
    num = num_elements;
    start_ = new T_[num_elements];
    end_size_ = start_ + num_elements;
    end_capacity_ = end_size_;
    
    // Initializing array descriptor
    array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    AddressRange addr_range = getAddressRange();
    array_descriptor->vaddr_start = addr_range.start;
    array_descriptor->vaddr_end = addr_range.end;
    array_descriptor->element_size = getElementSize();
  }

  pvector(size_t num_elements, T_ init_val) : pvector(num_elements) {
    fill(init_val);
  }

  pvector(iterator copy_begin, iterator copy_end)
      : pvector(copy_end - copy_begin) {
    #pragma omp parallel for
    for (size_t i=0; i < capacity(); i++)
      start_[i] = copy_begin[i];
  }

  // don't want this to be copied, too much data to move
  pvector(const pvector &other) = delete;

  // prefer move because too much data to copy
  pvector(pvector &&other)
      : start_(other.start_), end_size_(other.end_size_),
        end_capacity_(other.end_capacity_),
        array_descriptor(other.getArrayDescriptor()) {
    other.start_ = nullptr;
    other.end_size_ = nullptr;
    other.end_capacity_ = nullptr;
  }

  // want move assignment
  pvector& operator= (pvector &&other) {
    if (this != &other) {
      ReleaseResources();
      start_ = other.start_;
      end_size_ = other.end_size_;
      end_capacity_ = other.end_capacity_;
      array_descriptor = other.getArrayDescriptor();
      other.start_ = nullptr;
      other.end_size_ = nullptr;
      other.end_capacity_ = nullptr;
      other.array_descriptor = nullptr;
    }
    return *this;
  }

  void ReleaseResources(){
    if (start_ != nullptr) {
      delete[] start_;
    }
    array_descriptor = nullptr;
  }

  ~pvector() {
    ReleaseResources();
  }

  // not thread-safe
  void reserve(size_t num_elements) {
    num = num_elements;
    if (num_elements > capacity()) {
      T_ *new_range = new T_[num_elements];
      #pragma omp parallel for
      for (size_t i=0; i < size(); i++)
        new_range[i] = start_[i];
      end_size_ = new_range + size();
      delete[] start_;
      start_ = new_range;
      end_capacity_ = start_ + num_elements;
    }
  }

  // prevents internal storage from being freed when this pvector is desctructed
  // - used by Builder to reuse an EdgeList's space for in-place graph building
  void leak() {
    start_ = nullptr;
  }

  bool empty() {
    return end_size_ == start_;
  }

  void clear() {
    end_size_ = start_;
  }

  void resize(size_t num_elements) {
    reserve(num_elements);
    end_size_ = start_ + num_elements;
    num = num_elements;
  }

  T_& operator[](size_t n) {
    return start_[n];
  }

  const T_& operator[](size_t n) const {
    return start_[n];
  }

  void push_back(T_ val) {
    if (size() == capacity()) {
      size_t new_size = capacity() == 0 ? 1 : capacity() * growth_factor;
      reserve(new_size);
    }
    *end_size_ = val;
    end_size_++;
  }

  void fill(T_ init_val) {
    #pragma omp parallel for
    for (T_* ptr=start_; ptr < end_size_; ptr++)
      *ptr = init_val;
  }

  size_t capacity() const {
    return end_capacity_ - start_;
  }

  size_t size() const {
    return end_size_ - start_;
  }

  iterator begin() const {
    return start_;
  }

  iterator end() const {
    return end_size_;
  }

  T_* data() const {
    return start_;
  }

  void swap(pvector &other) {
    std::swap(start_, other.start_);
    std::swap(end_size_, other.end_size_);
    std::swap(end_capacity_, other.end_capacity_);
  }

  // -------------------- Array descriptor interface --------------------
  AddressRange getAddressRange() const {
    return AddressRange((uint64_t)start_, (uint64_t)end_size_);
  }

  uint64_t getElementSize() const {
    return sizeof(T_);
  }

  std::shared_ptr<PickleArrayDescriptor> getArrayDescriptor() const {
    return this->array_descriptor;
  }

  void indexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) {
    descriptor->dst_indexing_array_id = this->array_descriptor->getArrayId();
  }
  // -------------------------- Interface END ---------------------------

 private:
  T_* start_;
  T_* end_size_;
  T_* end_capacity_;
  static const size_t growth_factor = 2;
  size_t num;

  // Array Description
  std::shared_ptr<PickleArrayDescriptor> array_descriptor;
};

#endif  // PICKLE_PVECTOR_H_
