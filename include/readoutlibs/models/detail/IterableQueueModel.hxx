// Declarations for IterableQueueModel

namespace dunedaq {
namespace readoutlibs {

// Free allocated memory that is different for alignment strategies and allocation policies
template<class T>
void 
IterableQueueModel<T>::free_memory()
{
  // We need to destruct anything that may still exist in our queue.
  // (No real synchronization needed at destructor time: only one
  // thread can be doing this.)
  if (!std::is_trivially_destructible<T>::value) {
    std::size_t readIndex = readIndex_;
    std::size_t endIndex = writeIndex_;
    while (readIndex != endIndex) {
      records_[readIndex].~T();
      if (++readIndex == size_) { // NOLINT(runtime/increment_decrement)
        readIndex = 0;
      }
    }
  }
  // Different allocators require custom free functions
  if (intrinsic_allocator_) {
    _mm_free(records_);
  } else if (numa_aware_) {
#ifdef WITH_LIBNUMA_SUPPORT
      numa_free(records_, sizeof(T) * size_);
#endif
  } else {
    std::free(records_);
  }
}

// Allocate memory based on different alignment strategies and allocation policies
template<class T>
void 
IterableQueueModel<T>::allocate_memory(std::size_t size,
                                       bool numa_aware,
                                       uint8_t numa_node, // NOLINT (build/unsigned)
                                       bool intrinsic_allocator,
                                       std::size_t alignment_size)
{
  assert(size >= 2);
  // TODO: check for valid alignment sizes! | July-21-2021 | Roland Sipos | rsipos@cern.ch

  if (intrinsic_allocator && alignment_size > 0) { // _mm allocator
    records_ = static_cast<T*>(_mm_malloc(sizeof(T) * size, alignment_size));

  } else if (!intrinsic_allocator && alignment_size > 0) { // std aligned allocator
    records_ = static_cast<T*>(std::aligned_alloc(alignment_size, sizeof(T) * size));

  } else if (numa_aware && numa_node < 8) { // numa allocator from libnuma; we get "numa_node >= 0" for free, given its datatype
#ifdef WITH_LIBNUMA_SUPPORT
    numa_set_preferred((unsigned)numa_node); // https://linux.die.net/man/3/numa_set_preferred
 #ifdef WITH_LIBNUMA_BIND_POLICY
    numa_set_bind_policy(WITH_LIBNUMA_BIND_POLICY); // https://linux.die.net/man/3/numa_set_bind_policy
 #endif
 #ifdef WITH_LIBNUMA_STRICT_POLICY
    numa_set_strict(WITH_LIBNUMA_STRICT_POLICY);    // https://linux.die.net/man/3/numa_set_strict
 #endif
    records_ = static_cast<T*>(numa_alloc_onnode(sizeof(T) * size, numa_node));
#else
    throw GenericConfigurationError(ERS_HERE,
                                    "NUMA allocation was requested but program was built without USE_LIBNUMA");
#endif
  } else if (!numa_aware && !intrinsic_allocator && alignment_size == 0) {
    // Standard allocator
    records_ = static_cast<T*>(std::malloc(sizeof(T) * size));

  } else {
    // Let it fail, as expected combination might be invalid
    // records_ = static_cast<T*>(std::malloc(sizeof(T) * size_);
  }

  size_ = size;
  numa_aware_ = numa_aware;
  numa_node_ = numa_node;
  intrinsic_allocator_ = intrinsic_allocator;
  alignment_size_ = alignment_size;
}

template<class T>
void
IterableQueueModel<T>::prefill_task()
{
  // Wait until LB issues ready
  std::unique_lock lk(prefill_mutex_);
  prefill_cv_.wait(lk, [this]{ return prefill_ready_; });
  
  // After wait, we are ready to force page-fault
  for (size_t i = 0; i < size_ - 1; ++i) {
    T element = T();
    write_(std::move(element));
  }
  flush();
  
  // Preallocation done
  prefill_done_ = true;
  
  // Manual unlock is done before notify: avoid waking up the waiting thread only to block again.
  lk.unlock();
  prefill_cv_.notify_one();
}

template<class T>
void
IterableQueueModel<T>::force_pagefault()
{
  // Local prefiller thread
  std::thread prefill_thread(&IterableQueueModel<T>::prefill_task, this);

  // Tweak prefiller thread
  char tname[16];
  snprintf(tname, 16, "%s-%d", prefiller_name_.c_str(), numa_node_);
  auto handle = prefill_thread.native_handle();
  pthread_setname_np(handle, tname);

#ifdef WITH_LIBNUMA_SUPPORT
  cpu_set_t affinitymask;
  CPU_ZERO(&affinitymask);
  struct bitmask *nodecpumask = numa_allocate_cpumask();
  int ret = 0;
  // Get NODE CPU mask
  ret = numa_node_to_cpus(numa_node_, nodecpumask);
  assert(ret == 0);
  // Apply corresponding NODE CPUs to affinity mask
  for (int i=0; i< numa_num_configured_cpus(); ++i) {
    if (numa_bitmask_isbitset(nodecpumask, i)) {
      CPU_SET(i, &affinitymask);
    }
  }
  ret = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &affinitymask);
  assert(ret == 0);
  numa_free_cpumask(nodecpumask);
#endif

  // Trigger prefiller thread
  {
    std::lock_guard lk(prefill_mutex_);
    prefill_ready_ = true;
  }
  prefill_cv_.notify_one();
  // Wait for prefiller thread to finish
  {
    std::unique_lock lk(prefill_mutex_);
    prefill_cv_.wait(lk, [this]{ return prefill_done_; });
  }
  // Join with prefiller thread
  prefill_thread.join();
}

// Write element into the queue
template<class T>
bool 
IterableQueueModel<T>::write(T&& record)
{
  auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
  auto nextRecord = currentWrite + 1;
  if (nextRecord == size_) {
    nextRecord = 0;
  }

  if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
    new (&records_[currentWrite]) T(std::move(record));
    writeIndex_.store(nextRecord, std::memory_order_release);
    return true;
  }

  // queue is full
  ++overflow_ctr;
  return false;
}

// Read element from a queue (move or copy the value at the front of the queue to given variable)
template<class T>
bool
IterableQueueModel<T>::read(T& record)
{
  auto const currentRead = readIndex_.load(std::memory_order_relaxed);
  if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
    // queue is empty
    return false;
  }

  auto nextRecord = currentRead + 1;
  if (nextRecord == size_) {
    nextRecord = 0;
  }
  record = std::move(records_[currentRead]);
  records_[currentRead].~T();
  readIndex_.store(nextRecord, std::memory_order_release);
  return true;
}

// Pop element on front of queue
template<class T>
void 
IterableQueueModel<T>::popFront()
{
  auto const currentRead = readIndex_.load(std::memory_order_relaxed);
  assert(currentRead != writeIndex_.load(std::memory_order_acquire));

  auto nextRecord = currentRead + 1;
  if (nextRecord == size_) {
    nextRecord = 0;
  }

  records_[currentRead].~T();
  readIndex_.store(nextRecord, std::memory_order_release);
}

// Pop number of elements (X) from the front of the queue
template<class T>
void 
IterableQueueModel<T>::pop(std::size_t x)
{
  for (std::size_t i = 0; i < x; i++) {
    popFront();
  }
}

// Returns true if the queue is empty
template<class T>
bool 
IterableQueueModel<T>::isEmpty() const
{
  return readIndex_.load(std::memory_order_acquire) == writeIndex_.load(std::memory_order_acquire);
}

// Returns true if write index reached read index
template<class T>
bool 
IterableQueueModel<T>::isFull() const
{
  auto nextRecord = writeIndex_.load(std::memory_order_acquire) + 1;
  if (nextRecord == size_) {
    nextRecord = 0;
  }
  if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
    return false;
  }
  // queue is full
  return true;
}

// Returns a good-enough guess on current occupancy:
// * If called by consumer, then true size may be more (because producer may
//   be adding items concurrently).
// * If called by producer, then true size may be less (because consumer may
//   be removing items concurrently).
// * It is undefined to call this from any other thread.
template<class T>
std::size_t 
IterableQueueModel<T>::occupancy() const
{
  int ret = static_cast<int>(writeIndex_.load(std::memory_order_acquire)) -
            static_cast<int>(readIndex_.load(std::memory_order_acquire));
  if (ret < 0) {
    ret += static_cast<int>(size_);
  }
  return static_cast<std::size_t>(ret);
}

// Gives a pointer to the current read index
template<class T>
const T* 
IterableQueueModel<T>::front()
{
  auto const currentRead = readIndex_.load(std::memory_order_relaxed);
  if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
    return nullptr;
  }
  return &records_[currentRead];
}

// Gives a pointer to the current write index
template<class T>
const T* 
IterableQueueModel<T>::back()
{
  auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
  if (currentWrite == readIndex_.load(std::memory_order_acquire)) {
    return nullptr;
  }
  int currentLast = currentWrite;
  if (currentLast == 0) {
    currentLast = size_ - 1;
  } else {
    currentLast--;
  }
  return &records_[currentLast];
}

// Configures the model
template<class T>
void 
IterableQueueModel<T>::conf(const appdal::LatencyBuffer* cfg)
{
  assert(cfg->get_size() >= 2);
  free_memory();

  allocate_memory(cfg->get_size(),
                  cfg->get_numa_aware(),
                  cfg->get_numa_node(),
                  cfg->get_intrinsic_allocator(),
                  cfg->get_alignment_size());
  readIndex_ = 0;
  writeIndex_ = 0;

  if (!records_) {
    throw std::bad_alloc();
  }

  if (fg->get_preallocation()) {
    force_pagefault();
  }
}

// Unconfigures the model
template<class T>
void 
IterableQueueModel<T>::scrap(const nlohmann::json& /*cfg*/)
{
  free_memory();
  numa_aware_ = false;
  numa_node_ = 0;
  intrinsic_allocator_ = false;
  alignment_size_ = 0;
  invalid_configuration_requested_ = false;
  prefill_ready_ = false;
  prefill_done_ = false;
  size_ = 2;
  records_ = static_cast<T*>(std::malloc(sizeof(T) * 2));
  readIndex_ = 0;
  writeIndex_ = 0;
}

// Hidden original write implementation with signature difference. Only used for pre-allocation
template<class T>
template<class... Args>
bool 
IterableQueueModel<T>::write_(Args&&... recordArgs)
{
  // const std::lock_guard<std::mutex> lock(m_mutex);
  auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
  auto nextRecord = currentWrite + 1;
  if (nextRecord == size_) {
    nextRecord = 0;
  }
  // if (nextRecord == readIndex_.load(std::memory_order_acquire)) {
  // std::cout << "SPSC WARNING -> Queue is full! WRITE PASSES READ!!! \n";
  //}
  //    new (&records_[currentWrite]) T(std::forward<Args>(recordArgs)...);
  // writeIndex_.store(nextRecord, std::memory_order_release);
  // return true;

  // ORIGINAL:
  if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
    new (&records_[currentWrite]) T(std::forward<Args>(recordArgs)...);
    writeIndex_.store(nextRecord, std::memory_order_release);
    return true;
  }
  // queue is full

  ++overflow_ctr;

  return false;
}


} // namespace readoutlibs
} // namespace dunedaq
