/**
 * @file    frame_output_factory.h
 * @brief   factory that constructs configured FrameOutput implementations
 *
 * Lets instruments obtain their frame_outputs without depending on
 * concrete writer subclasses. Adding a new writer touches only this
 * factory and its .cpp.
 */
#pragma once

#include "config.h"
#include "frame_output.h"
#include "fits_writer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Camera {

  // Safe fallback SHM frame-size ceiling when neither a .cfg file nor an
  // instrument specifies one: comfortably covers any detector geometry
  // realistic for this codebase (e.g. HISPEC's 2048x2048x4B ~16 MiB) without
  // being an arbitrarily large "accept anything" bound.
  constexpr size_t DEFAULT_SHM_MAX_FRAME_BYTES = 4096ULL * 4096ULL * 4ULL;  // 64 MiB

  struct FrameOutputsConfig {
    bool        shm_enabled{false};
    std::string shm_segment_name{"camera"};
    size_t      shm_max_frame_bytes{DEFAULT_SHM_MAX_FRAME_BYTES};
    uint32_t    shm_ring_buffer_size{4};   // depth of ImageStreamIO's internal history ring buffer
    std::string shm_dir{};                 // ImageStreamIO base directory; empty uses its own default resolution

    bool             fits_enabled{false};
    FitsWriterConfig fits;
  };

  // Defaults set on `out` by the caller survive for keys not present in cfg
  void apply_config_overrides(FrameOutputsConfig &out, const Config &cfg);

  // open() is called on each writer; failures are logged and the writer is skipped
  std::vector<std::unique_ptr<FrameOutput>>
  make_frame_outputs(const FrameOutputsConfig &cfg);

}
