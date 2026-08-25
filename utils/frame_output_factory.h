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

  struct FrameOutputsConfig {
    bool        shm_enabled{false};
    std::string shm_segment_name{"camera"};
    size_t      shm_max_frame_bytes{0};   // required > 0 when shm_enabled
    uint32_t    shm_num_frames{4};

    bool             fits_enabled{false};
    FitsWriterConfig fits;
  };

  // Defaults set on `out` by the caller survive for keys not present in cfg
  void apply_config_overrides(FrameOutputsConfig &out, const Config &cfg);

  // open() is called on each writer; failures are logged and the writer is skipped
  std::vector<std::unique_ptr<FrameOutput>>
  make_frame_outputs(const FrameOutputsConfig &cfg);

}
