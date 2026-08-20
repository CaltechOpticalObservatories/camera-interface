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

    // Values that vary per frame, not per exposure
    double      mjd_start{0.0};         // EXPMJDST
    std::string acq_time;               // ACQTIME, UTC
    double      exposure_time_sec{0.0}; // EXPTIME
    uint64_t    n_reads{0};              // NREADS

    // Resolved static keys, built once per exposure and shared across its frames
    std::shared_ptr<const Common::FitsKeys> header_set;
  };

  class FrameOutput {
    public:
      virtual ~FrameOutput() = default;
      virtual long open() = 0;
      virtual long write(const char* data, size_t size, const FrameMetadata& meta) = 0;
      virtual void close() = 0;
  };

}
