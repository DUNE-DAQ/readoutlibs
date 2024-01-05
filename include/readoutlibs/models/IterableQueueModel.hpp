/** @file IterableQueueModel.hpp
 *
 * Copyright 2012-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// @author Bo Hu (bhu@fb.com)
// @author Jordan DeLong (delong.j@fb.com)

// Modification by Roland Sipos and Florian Till Groetschla
// for DUNE-DAQ software framework

#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_ITERABLEQUEUEMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_ITERABLEQUEUEMODEL_HPP_

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/concepts/LatencyBufferConcept.hpp"

//#include "readoutlibs/readoutconfig/Nljs.hpp"
//#include "readoutlibs/readoutconfig/Structs.hpp"

#include "logging/Logging.hpp"

#include <folly/lang/Align.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cxxabi.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#include <xmmintrin.h>

#ifdef WITH_LIBNUMA_SUPPORT
#include <numa.h>
#endif

namespace dunedaq {
namespace readoutlibs {

/**
 * IterableQueueModel is a one producer and one consumer queue without locks.
 * Modified version of the folly::ProducerConsumerQueue via adding a readPtr function.
 * Requires  well defined and followed constraints on the consumer side.
 *
 * Also, note that the number of usable slots in the queue at any
 * given time is actually (size-1), so if you start with an empty queue,
 * isFull() will return true after size-1 insertions.
 */
template<class T>
struct IterableQueueModel : public LatencyBufferConcept<T>
{
  typedef T value_type;

  IterableQueueModel(const IterableQueueModel&) = delete;
  IterableQueueModel& operator=(const IterableQueueModel&) = delete;

  // Default constructor
  IterableQueueModel()
    : LatencyBufferConcept<T>()
    , numa_aware_(false)
    , numa_node_(0)
    , intrinsic_allocator_(false)
    , alignment_size_(0)
    , invalid_configuration_requested_(false)
    , size_(2)
    , records_(static_cast<T*>(std::malloc(sizeof(T) * 2)))
    , readIndex_(0)
    , writeIndex_(0)
  {}

  // Explicit constructor with size
  explicit IterableQueueModel(std::size_t size) // size must be >= 2
    : LatencyBufferConcept<T>() // NOLINT(build/unsigned)
    , numa_aware_(false)
    , numa_node_(0)
    , intrinsic_allocator_(false)
    , alignment_size_(0)
    , invalid_configuration_requested_(false)
    , size_(size)
    , records_(static_cast<T*>(std::malloc(sizeof(T) * size)))
    , readIndex_(0)
    , writeIndex_(0)
  {
    assert(size >= 2);
    if (!records_) {
      throw std::bad_alloc();
    }
#if 0
    ptrlogger = std::thread([&](){
      while(true) {
        auto const currentRead = readIndex_.load(std::memory_order_relaxed);
        auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
        TLOG() << "BEG:" << std::hex << &records_[0] << " END:" << &records_[size] << std::dec
                  << " R:" << currentRead << " - W:" << currentWrite
                  << " OFLOW:" << overflow_ctr;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });
#endif
  }

  // Constructor with alignment strategies
  IterableQueueModel(std::size_t size, // size must be >= 2
                     bool numa_aware = false,
                     uint8_t numa_node = 0, // NOLINT (build/unsigned)
                     bool intrinsic_allocator = false,
                     std::size_t alignment_size = 0)
    : LatencyBufferConcept<T>() // NOLINT(build/unsigned)
    , numa_aware_(numa_aware)
    , numa_node_(numa_node)
    , intrinsic_allocator_(intrinsic_allocator)
    , alignment_size_(alignment_size)
    , invalid_configuration_requested_(false)
    , size_(size)
    , readIndex_(0)
    , writeIndex_(0)
  {
    assert(size >= 2);
    allocate_memory(size, numa_aware, numa_node, intrinsic_allocator, alignment_size);

    if (!records_) {
      throw std::bad_alloc();
    }
#if 0
    ptrlogger = std::thread([&](){
      while(true) {
        auto const currentRead = readIndex_.load(std::memory_order_relaxed);
        auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
        TLOG() << "BEG:" << std::hex << &records_[0] << " END:" << &records_[size] << std::dec
                  << " R:" << currentRead << " - W:" << currentWrite
                  << " OFLOW:" << overflow_ctr;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });
#endif
  }

  // Destructor
  ~IterableQueueModel() { free_memory(); }

  // Free allocated memory that is different for alignment strategies and allocation policies
  void free_memory();

  // Allocate memory based on different alignment strategies and allocation policies
  void allocate_memory(std::size_t size,
                       bool numa_aware = false,
                       uint8_t numa_node = 0, // NOLINT (build/unsigned)
                       bool intrinsic_allocator = false,
                       std::size_t alignment_size = 0);

  // Write element into the queue
  bool write(T&& record) override;

  // Read element from a queue (move or copy the value at the front of the queue to given variable)
  bool read(T& record) override;

  // Pop element on front of queue
  void popFront();

  // Pop number of elements (X) from the front of the queue
  void pop(std::size_t x);

  // Returns true if the queue is empty
  bool isEmpty() const;

  // Returns true if write index reached read index
  bool isFull() const;

  // Returns a good-enough guess on current occupancy:
  // * If called by consumer, then true size may be more (because producer may
  //   be adding items concurrently).
  // * If called by producer, then true size may be less (because consumer may
  //   be removing items concurrently).
  // * It is undefined to call this from any other thread.
  std::size_t occupancy() const override;

  // The size of the underlying buffer, not the amount of usable slots
  std::size_t size() const { return size_; }

  // Maximum number of items in the queue.
  std::size_t capacity() const { return size_ - 1; }

  // Gives a pointer to the current read index
  const T* front() override;

  // Gives a pointer to the current write index
  const T* back() override;

  // Gives a pointer to the first available slot of the queue
  T* start_of_buffer() { return &records_[0]; }

  // Gives a pointer to the last available slot of the queue
  T* end_of_buffer() { return &records_[size_]; }

  // Configures the model
  void conf(const appdal::LatencyBuffer* cfg) override;

  // Unconfigures the model
  void scrap(const nlohmann::json& /*cfg*/) override;

  // Flushes the elements from the queue
  void flush() override { pop(occupancy()); }

  // Returns the current memory alignment size
  std::size_t get_alignment_size() { return alignment_size_; }

  // Iterator for elements in the queue
  struct Iterator
  {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    Iterator(IterableQueueModel<T>& queue, uint32_t index) // NOLINT(build/unsigned)
      : m_queue(queue)
      , m_index(index)
    {}

    reference operator*() const { return m_queue.records_[m_index]; }
    pointer operator->() { return &m_queue.records_[m_index]; }
    Iterator& operator++() // NOLINT(runtime/increment_decrement) :)
    {
      if (good()) {
        m_index++;
        if (m_index == m_queue.size_) {
          m_index = 0;
        }
      }
      if (!good()) {
        m_index = std::numeric_limits<uint32_t>::max(); // NOLINT(build/unsigned)
      }
      return *this;
    }
    Iterator operator++(int amount) // NOLINT(runtime/increment_decrement) :)
    {
      Iterator tmp = *this;
      for (int i = 0; i < amount; ++i) {
        ++(*this);
      }
      return tmp;
    }
    friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_index == b.m_index; }
    friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_index != b.m_index; }

    bool good()
    {
      auto const currentRead = m_queue.readIndex_.load(std::memory_order_relaxed);
      auto const currentWrite = m_queue.writeIndex_.load(std::memory_order_relaxed);
      return (*this != m_queue.end()) &&
             ((m_index >= currentRead && m_index < currentWrite) ||
              (m_index >= currentRead && currentWrite < currentRead) ||
              (currentWrite < currentRead && m_index < currentRead && m_index < currentWrite));
    }

    uint32_t get_index() { return m_index; } // NOLINT(build/unsigned)

  private:
    IterableQueueModel<T>& m_queue;
    uint32_t m_index; // NOLINT(build/unsigned)
  };

  Iterator begin()
  {
    auto const currentRead = readIndex_.load(std::memory_order_relaxed);
    if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
      // queue is empty
      return end();
    }
    return Iterator(*this, currentRead);
  }

  Iterator end()
  {
    return Iterator(*this, std::numeric_limits<uint32_t>::max()); // NOLINT(build/unsigned)
  }

protected:
  // Hidden original write implementation with signature difference. Only used for pre-allocation
  template<class... Args>
  bool write_(Args&&... recordArgs);

  // Counter for failed writes, due to the fact the queue is full
  std::atomic<int> overflow_ctr{ 0 };

  // NUMA awareness and aligned allocator usage configuration
  bool numa_aware_;
  uint8_t numa_node_; // NOLINT (build/unsigned)
  bool intrinsic_allocator_;
  std::size_t alignment_size_;
  bool invalid_configuration_requested_;

  // Ptr logger for debugging
  std::thread ptrlogger;

  // Underlying buffer with padding:
  //  * hardware_destructive_interference_size is set to 128.
  //  * (Assuming cache line size of 64, so we use a cache line pair size of 128)
  char pad0_[folly::hardware_destructive_interference_size]; // NOLINT(runtime/arrays)
  uint32_t size_;                                            // NOLINT(build/unsigned)
  T* records_;
  alignas(
    folly::hardware_destructive_interference_size) std::atomic<unsigned int> readIndex_; // NOLINT(build/unsigned)
  alignas(
    folly::hardware_destructive_interference_size) std::atomic<unsigned int> writeIndex_; // NOLINT(build/unsigned)
  char pad1_[folly::hardware_destructive_interference_size - sizeof(writeIndex_)]; // NOLINT(runtime/arrays)
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/IterableQueueModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_ITERABLEQUEUEMODEL_HPP_
