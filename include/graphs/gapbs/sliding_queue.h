// Copyright (c) 2015, The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#ifndef SLIDING_QUEUE_H_
#define SLIDING_QUEUE_H_

#include <algorithm>
#include <fstream>
#include <memory>
#include <utility>

#include "platform_atomics.h"
#include "pickle_job.h"

/*
GAP Benchmark Suite
Class:  SlidingQueue
Author: Scott Beamer

Double-buffered queue so appends aren't seen until SlideWindow() called
 - Use QueueBuffer when used in parallel to avoid false sharing by doing
   bulk appends from thread-local storage
*/


template <typename T>
class QueueBuffer;

template <typename T>
class SlidingQueue {
  T *shared;
  size_t alloc_size;
  size_t shared_in;
  size_t shared_out_start;
  size_t shared_out_end;
  friend class QueueBuffer<T>;
  // Array Description
  std::shared_ptr<PickleArrayDescriptor> array_descriptor;

 public:
  explicit SlidingQueue(size_t shared_size) {
    shared = new T[shared_size];
    alloc_size = shared_size;
    reset();

    // Initializing array descriptor
    array_descriptor = std::shared_ptr<PickleArrayDescriptor>(new PickleArrayDescriptor());
    AddressRange addr_range = getAddressRange();
    array_descriptor->vaddr_start = addr_range.start;
    array_descriptor->vaddr_end = addr_range.end;
    array_descriptor->element_size = getElementSize();
  }

  ~SlidingQueue() {
    delete[] shared;
    array_descriptor = nullptr;
  }

  void push_back(T to_add) {
    shared[shared_in++] = to_add;
  }

  bool empty() const {
    return shared_out_start == shared_out_end;
  }

  void reset() {
    shared_out_start = 0;
    shared_out_end = 0;
    shared_in = 0;
  }

  void slide_window() {
    shared_out_start = shared_out_end;
    shared_out_end = shared_in;
  }

  typedef T* iterator;

  iterator begin() const {
    return shared + shared_out_start;
  }

  iterator end() const {
    return shared + shared_out_end;
  }

  size_t size() const {
    return end() - begin();
  }

  void write(std::ofstream &f, const std::string& msg) const {
    f << msg << " " << alloc_size << "\n";
    for (size_t i = 0; i < alloc_size; ++i) {
        f << shared[i] << "\n";
    }
  }

  // -------------------- Array descriptor interface --------------------
  AddressRange getAddressRange() const {
    return AddressRange((uint64_t)(&(*shared)), (uint64_t)(&(*shared) + alloc_size));
  }

  uint64_t getElementSize() const {
    return sizeof(T);
  }

  std::shared_ptr<PickleArrayDescriptor> getArrayDescriptor() const {
    return this->array_descriptor;
  }

  void indexedBy(const std::shared_ptr<PickleArrayDescriptor>& descriptor) {
    descriptor->dst_indexing_array_id = this->getArrayId();
  }
  // -------------------------- Interface END ---------------------------

};


template <typename T>
class QueueBuffer {
  size_t in;
  T *local_queue;
  SlidingQueue<T> &sq;
  const size_t local_size;

 public:
  explicit QueueBuffer(SlidingQueue<T> &master, size_t given_size = 16384)
      : sq(master), local_size(given_size) {
    in = 0;
    local_queue = new T[local_size];
  }

  ~QueueBuffer() {
    delete[] local_queue;
  }

  void push_back(T to_add) {
    if (in == local_size)
      flush();
    local_queue[in++] = to_add;
  }

  void flush() {
    T *shared_queue = sq.shared;
    size_t copy_start = fetch_and_add(sq.shared_in, in);
    std::copy(local_queue, local_queue+in, shared_queue+copy_start);
    in = 0;
  }
};

#endif  // PICKLE_SLIDING_QUEUE_H_
