/**
 * @file BufferCopy.hpp Copy arbitrary amount of bytes from
 * source to destination buffer.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_UTILS_BUFFERCOPY_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_UTILS_BUFFERCOPY_HPP_

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace dunedaq {
namespace readoutlibs {

/* 
 * Most common use-case:
 *
 *   size_t msg_size = 1024; // source buffer size
 *   char message[msg_size]; // source buffer
 *   readoutlibs::types::DUMMY_FRAME_STRUCT target_payload; // target object
 *   uint32_t bytes_copied = 0; // book-keeping of copied bytes
 *   buffer_copy(&message[0], msg_size, static_cast<void*>(&target_payload), bytes_copied, sizeof(target_payload));
 *
 *
 * Copy multiple char*, size_t pairs into destination buffer (serialize them together)
 *
 *   const char* subchunk_data[NUMBER_OF_CHUNKS]; // source payloads
 *   size_t subchunk_length[NUMBER_OF_CHUNKS];    // source payloads' sizes
 *   TargetStruct payload; // target payload
 *   size_t target_size = sizeof(payload); // target payload size
 *   uint32_t bytes_copied_chunk = 0; 
 *   for (unsigned i = 0; i < n_subchunks; i++) {
 *     buffer_copy(subchunk_data[i], subchunk_sizes[i], static_cast<void*>(&payload.data), bytes_copied_chunk, target_size);
 *     bytes_copied_chunk += subchunk_sizes[i];
 *   }
 *
 * */
inline void
buffer_copy(const char* data,
            std::size_t size,
            void* buffer,
            std::uint32_t buffer_pos, // NOLINT
            const std::size_t& buffer_size)
{
  auto bytes_to_copy = size; // NOLINT
  while (bytes_to_copy > 0) {
    auto n = std::min(bytes_to_copy, buffer_size - buffer_pos); // NOLINT
    std::memcpy(static_cast<char*>(buffer) + buffer_pos, data, n);
    buffer_pos += n; 
    bytes_to_copy -= n;
    if (buffer_pos == buffer_size) {
      buffer_pos = 0;
    }
  }
}

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_UTILS_BUFFERCOPY_HPP_
