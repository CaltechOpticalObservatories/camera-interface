/**
 * @file    frame_output.h
 * @brief   abstract base class for frame output destinations
 *
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Common { class FitsKeys; }

namespace Camera {

  struct FrameMetadata {
    uint64_t frame_number{0};
    uint64_t timestamp{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t bytes_per_pixel{0};
    uint64_t sequence_number{0};   // monotonic per-stream counter

    // FITS keys that vary per frame (e.g. per read within one exposure); rebuilt each frame
    std::shared_ptr<const Common::FitsKeys> frame_keys;

    // FITS keys resolved once per exposure and shared across its frames
    std::shared_ptr<const Common::FitsKeys> header_set;
  };

  class FrameOutput {
    public:
      virtual ~FrameOutput() = default;
      virtual long open() = 0;
      virtual long write(const char* data, size_t size, const FrameMetadata& meta) = 0;
      virtual void close() = 0;

      // Runtime option toggle (e.g. "datacube"/"true"); false if key is unrecognized
      virtual bool set_option(const std::string &key, const std::string &value) { return false; }
  };

}
