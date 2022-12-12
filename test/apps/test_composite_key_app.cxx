/**
 * @file test_ratelimiter_app.cxx Test application for
 * ratelimiter implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "readoutlibs/utils/RateLimiter.hpp"

#include "logging/Logging.hpp"

#include "readoutlibs/ReadoutTypes.hpp"

#include "folly/ConcurrentSkipList.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace dunedaq::readoutlibs;
using namespace folly;

int
main(int /*argc*/, char** /*argv[]*/)
{

  // ConcurrentSkipList from Folly
  typedef ConcurrentSkipList<types::DUMMY_FRAME_STRUCT> SkipListT;
  typedef SkipListT::Accessor SkipListTAcc;
  typedef SkipListT::iterator SkipListTIter;
  // typedef SkipListT::Skipper SkipListTSkip; //Skipper accessor to test

  // Skiplist instance
  auto head_height = 2;
  std::shared_ptr<SkipListT> skl(SkipListT::createInstance(head_height));

  TLOG() << "Composite key test...";
  types::DUMMY_FRAME_STRUCT payload1;
  payload1.set_timestamp(12340);
  payload1.set_another_key(1);

  types::DUMMY_FRAME_STRUCT payload2;
  payload2.set_timestamp(12342);
  payload2.set_another_key(2);

  types::DUMMY_FRAME_STRUCT payload3; // equivalent to payload 2
  payload3.set_timestamp(12342);
  payload3.set_another_key(2);

  types::DUMMY_FRAME_STRUCT payload4;
  payload4.set_timestamp(12342);
  payload4.set_another_key(3);

  types::DUMMY_FRAME_STRUCT payload5;
  payload5.set_timestamp(12345);
  payload5.set_another_key(4);

  types::DUMMY_FRAME_STRUCT payload6;
  payload6.set_timestamp(12342);
  payload6.set_another_key(1);

  {
    SkipListTAcc acc(skl);
    auto ret = acc.insert(std::move(payload4));
    TLOG() << "Payload4 insertion success? -> " << ret.second;
    ret = acc.insert(std::move(payload2));
    TLOG() << "Payload2 insertion success? -> " << ret.second;
    ret = acc.insert(std::move(payload3)); // This payload is equivalent with payload2. Insert fails!
    TLOG() << "Payload3 (equivalent with payload2) insertion success? (Should fail) -> " << ret.second;
    ret = acc.insert(std::move(payload1));
    TLOG() << "Payload1 insertion success? -> " << ret.second;
    ret = acc.insert(std::move(payload5));
    TLOG() << "Payload5 insertion success? -> " << ret.second;
    ret = acc.insert(std::move(payload6));
    TLOG() << "Payload6 insertion success? -> " << ret.second;

    TLOG() << "SkipList size: " << acc.size();
  }

  {
    types::DUMMY_FRAME_STRUCT findpayload;
    findpayload.set_timestamp(12342);
    findpayload.set_another_key(2);

    SkipListTAcc acc(skl);
    auto lb = acc.lower_bound(findpayload);
    auto foundptr = reinterpret_cast<const types::DUMMY_FRAME_STRUCT*>(&(*lb)); // NOLINT
    auto foundts = foundptr->get_timestamp();
    TLOG() << "Found element 1 lower bound to 12342 in skiplist with timestamp --> " << foundts;

    lb++;
    foundptr = reinterpret_cast<const types::DUMMY_FRAME_STRUCT*>(&(*lb)); // NOLINT
    foundts = foundptr->get_timestamp();
    TLOG() << "Found element 2 lower bound to 12342 in skiplist with timestamp --> " << foundts;

    TLOG() << "Dumping SkipList content:";
    SkipListTIter node = acc.find(payload1);
    while (node != acc.end()) {
      auto nodeptr = reinterpret_cast<const types::DUMMY_FRAME_STRUCT*>(&(*node)); // NOLINT
      auto nodets = nodeptr->get_timestamp();
      auto nodeck = nodeptr->another_key;
      TLOG() << "  --> Element with timestamp: " << (unsigned)nodets << " other key: " << (unsigned)nodeck;
      node++;
    }
  }

  return 0;
} // NOLINT(readability/fn_size)
