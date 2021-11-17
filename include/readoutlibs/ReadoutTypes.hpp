/**
 * @file ReadoutTypes.hpp Common types in udaq-readoutlibs
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_READOUTTYPES_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_READOUTTYPES_HPP_

#include <cstdint> // uint_t types
#include <memory>  // unique_ptr

namespace dunedaq {
namespace readoutlibs {
namespace types {

const constexpr std::size_t DUMMY_FRAME_SIZE = 1024;
struct DUMMY_FRAME_STRUCT
{
  using FrameType = DUMMY_FRAME_STRUCT;

  // header
  uint64_t timestamp; // NOLINT(build/unsigned)

  // data
  char data[DUMMY_FRAME_SIZE];

  // comparable based on start timestamp
  bool operator<(const DUMMY_FRAME_STRUCT& other) const
  {
    return this->get_timestamp() < other.get_timestamp() ? true : false;
  }

  uint64_t get_timestamp() const // NOLINT(build/unsigned)
  {
    return timestamp;
  }

  void set_timestamp(uint64_t ts) // NOLINT(build/unsigned)
  {
    timestamp = ts;
  }

  void fake_timestamp(uint64_t /*first_timestamp*/, uint64_t /*offset = 25*/) // NOLINT(build/unsigned)
  {
    // tp.time_start = first_timestamp;
  }

  FrameType* begin() { return this; }

  FrameType* end() { return (this + 1); } // NOLINT

  static const constexpr size_t frame_size = DUMMY_FRAME_SIZE;
  static const constexpr uint8_t frames_per_element = 1; // NOLINT(build/unsigned)
  static const constexpr size_t element_size = DUMMY_FRAME_SIZE;
};

} // namespace types
} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_READOUTTYPES_HPP_
