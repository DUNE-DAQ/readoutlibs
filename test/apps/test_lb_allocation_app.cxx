/**
 * @file test_lb_allocation_app.cxx Test application for LB allocations.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "readoutlibs/models/IterableQueueModel.hpp"
#include "readoutlibs/models/SkipListLatencyBufferModel.hpp"
#include "readoutlibs/concepts/RawDataProcessorConcept.hpp"
#include "logging/Logging.hpp"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

#ifdef WITH_LIBNUMA_SUPPORT
#include <numaif.h>
#endif

//#define REGISTER (*(volatile unsigned char*)0x1234)

using namespace dunedaq::readoutlibs;

namespace {

  struct kBlock // Dummy data type for LB test
  {
    kBlock(){};
    char data[1024];
  };

  std::size_t lb_capacity = 1000; // LB capacity
 
  bool numa_aware_test = false;
  int num_numa_nodes = 2;
  bool intrinsic_test = false;
  bool aligned_test = false;
  bool prefill = false; // Prefill the LB
  std::size_t alignment_size = 4096;
}

#include "stdio.h"
#include "unistd.h"
#include "inttypes.h"

uintptr_t vtop(uintptr_t vaddr) {
  FILE *pagemap;
  intptr_t paddr = 0;
  int offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
  uint64_t e;

  // https://www.kernel.org/doc/Documentation/vm/pagemap.txt
  if ((pagemap = fopen("/proc/self/pagemap", "r"))) {
      if (lseek(fileno(pagemap), offset, SEEK_SET) == offset) {
          if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
              if (e & (1ULL << 63)) { // page present ?
                  paddr = e & ((1ULL << 54) - 1); // pfn mask
                  paddr = paddr * sysconf(_SC_PAGESIZE);
                  // add offset within page
                  paddr = paddr | (vaddr & (sysconf(_SC_PAGESIZE) - 1));
              }   
          }   
      }   
      fclose(pagemap);
  }   

  return paddr;
}   

int
main(int argc, char** argv)
{
  int runsecs = 5;

  // Run marker
  std::atomic<bool> marker{ true };

  // Counter for ops/s
  std::atomic<int> newops = 0;

  CLI::App app{"readoutlibs_test_lb_allocation"};
  app.add_option("-c", lb_capacity, "Capacity/size of latency buffer.");
  app.add_flag("--numa_aware", numa_aware_test, "Test NUMA aware allocator.");
  app.add_option("--num_numa_nodes", num_numa_nodes, "Number of NUMA nodes to test allocation on.");
  app.add_flag("--intrinsic", intrinsic_test, "Test Intrinsic allocator.");
  app.add_flag("--aligned", aligned_test, "Test aligned allocator.");
  app.add_option("--alignment_size", alignment_size, "Set alignment size. Default: 4096");
  app.add_flag("--prefill", prefill, "Interface to init");
  CLI11_PARSE(app, argc, argv);

  if (numa_aware_test) { // check if test is issued...
#ifdef WITH_LIBNUMA_SUPPORT
    TLOG() << "NUMA aware allocator test...";

    for (int i=0; i<num_numa_nodes; ++i) { // for number of nodes...
      TLOG() << "  # Allocating NUMA aware LB on node " << i;
      IterableQueueModel<kBlock> numaIQM(lb_capacity, true, i, false, 0);

      if (prefill) { // Force page-fault issues?
        TLOG() << "  -> Prefilling LB...";
        numaIQM.force_pagefault();
      }

      for (std::size_t i=0; i<lb_capacity-1; ++i) { // Fill the LB
        numaIQM.write(kBlock());
      }
    
      // Test if the elements' pointers are on correct NUMA node.
      // Virtual addresses might be misleading between virt. and phys. addresses due to IOMMU!
      int numa_node = -1;
      get_mempolicy(&numa_node, NULL, 0, (void*)numaIQM.front(), MPOL_F_NODE | MPOL_F_ADDR);
      if (i != numa_node) { TLOG() << "Discrepancy in expected NUMA node and first element residency!"; }
      TLOG() << "  -> NUMA " << i << " IQM front virt.addr.: " << std::hex << (void*)numaIQM.front() << " is on: " << numa_node << std::dec;
      get_mempolicy(&numa_node, NULL, 0, (void*)numaIQM.back(), MPOL_F_NODE | MPOL_F_ADDR);
      if (i != numa_node) { TLOG() << "Discrepancy in expected NUMA node and last element residency!"; }
      TLOG() << "  -> NUMA " << i << " IQM back virt.addr.: " << std::hex << (void*)numaIQM.back() << " is on: " << numa_node << std::dec;
    }
    TLOG() << "  -> Done.";
#else
    TLOG() << "  -> NUMA support is turned off build-time. Passed on NUMA demo.";
#endif
  }

  if (intrinsic_test) {
    TLOG() << "Intrinsic allocator test...";
    IterableQueueModel<kBlock> intrIQM(lb_capacity, false, 0, true, alignment_size);
    if (prefill) {
      TLOG() << "  -> Prefilling LB...";
      intrIQM.force_pagefault();
    }

    for (std::size_t i=0; i<lb_capacity-1; ++i) { // Fill the LB
      intrIQM.write(kBlock());
    }

    TLOG() << "  -> Done.";
  }

  if (aligned_test) {
    TLOG() << "Aligned allocator test...";
    IterableQueueModel<kBlock> alignedIQM(lb_capacity, false, 0, false, alignment_size);
    if (prefill) {
      TLOG() << "  -> Prefilling LB...";
      alignedIQM.force_pagefault();
    }

    for (std::size_t i=0; i<lb_capacity-1; ++i) { // Fill the LB
      alignedIQM.write(kBlock());
    }

    TLOG() << "  -> Done.";
  }

  // Test app ends. Below there are general producer/consumer/stats threads.
  return 0;

  // Stats
  auto stats = std::thread([&]() {
    TLOG() << "Spawned stats thread...";
    while (marker) {
      TLOG() << "ops/s ->  " << newops.exchange(0);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  // Producer
  auto producer = std::thread([&]() {
    TLOG() << "Spawned producer thread...";
    while (marker) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  // Consumer
  auto consumer = std::thread([&]() {
    TLOG() << "Spawned consumer thread...";
    while (marker) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  // Killswitch that flips the run marker
  auto killswitch = std::thread([&]() {
    TLOG() << "Application will terminate in 5s...";
    std::this_thread::sleep_for(std::chrono::seconds(runsecs));
    marker.store(false);
  });

  // Join local threads
  TLOG() << "Flipping killswitch in order to stop...";
  if (killswitch.joinable()) {
    killswitch.join();
  }

  // Exit
  TLOG() << "Exiting.";
  return 0;
}
