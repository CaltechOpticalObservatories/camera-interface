/**
 * @file    frame_output_factory.cpp
 * @brief   factory that constructs configured FrameOutput implementations
 */

#include "frame_output_factory.h"
#include "fits_writer.h"
#include "common.h"

#ifdef CAMERAD_HAVE_SHM
#include "shared_memory_writer.h"
#endif

#include <stdexcept>
#include <utility>

namespace {
  bool parse_bool(const std::string &v) {
    return v == "yes" || v == "YES" || v == "true" || v == "TRUE" || v == "1";
  }
}

namespace Camera {

  void apply_config_overrides(FrameOutputsConfig &out, const Config &cfg) {
    const std::string function("Camera::apply_config_overrides");
    for (int row = 0; row < cfg.n_rows; ++row) {
      const auto &key = cfg.param[row];
      const auto &val = cfg.arg[row];
      try {
        if      (key == "SHM_ENABLED")            out.shm_enabled            = parse_bool(val);
        else if (key == "SHM_SEGMENT_NAME")       out.shm_segment_name       = val;
        else if (key == "SHM_RING_BUFFER_SIZE")   out.shm_ring_buffer_size   = static_cast<uint32_t>(std::stoul(val));
        else if (key == "SHM_DIR")                out.shm_dir                = val;
        else if (key == "FITS_ENABLED")           out.fits_enabled           = parse_bool(val);
        else if (key == "FITS_OUTPUT_DIR")        out.fits.output_dir        = val;
        else if (key == "FITS_AUTODIR")           out.fits.autodir           = parse_bool(val);
        else if (key == "FITS_BASENAME")          out.fits.basename          = val;
        else if (key == "FITS_QUEUE_SIZE")        out.fits.queue_size        = static_cast<size_t>(std::stoul(val));
        else if (key == "FITS_DRAIN_TIMEOUT_MS")  out.fits.drain_timeout_ms  = static_cast<uint32_t>(std::stoul(val));
      }
      catch (const std::exception &e) {
        logwrite(function, "WARNING bad value for " + key + "=" + val + ": " + e.what());
      }
    }
  }

  std::vector<std::unique_ptr<FrameOutput>>
  make_frame_outputs(const FrameOutputsConfig &cfg) {
    const std::string function("Camera::make_frame_outputs");
    std::vector<std::unique_ptr<FrameOutput>> outputs;

    if (cfg.shm_enabled) {
#ifdef CAMERAD_HAVE_SHM
      auto shm = std::make_unique<SharedMemoryWriter>(
          cfg.shm_segment_name, cfg.shm_ring_buffer_size, cfg.shm_dir);
      if (shm->open() == NO_ERROR) {
        logwrite(function, "SHM output enabled: segment=" + cfg.shm_segment_name +
                 " ring_buffer_size=" + std::to_string(cfg.shm_ring_buffer_size) +
                 " dir=" + (cfg.shm_dir.empty() ? "(default)" : cfg.shm_dir));
        outputs.push_back(std::move(shm));
      }
      else {
        logwrite(function, "WARNING SHM output failed to open; skipped");
      }
#else
      logwrite(function, "WARNING shm_enabled but this build was compiled without SHM support (ENABLE_SHM_OUTPUT=OFF)");
#endif
    }

    if (cfg.fits_enabled) {
      auto fits = std::make_unique<FitsWriter>(cfg.fits);
      if (fits->open() == NO_ERROR) {
        logwrite(function, "FITS output enabled: dir=" + cfg.fits.output_dir +
                 " basename=" + cfg.fits.basename +
                 " queue=" + std::to_string(cfg.fits.queue_size));
        outputs.push_back(std::move(fits));
      }
      else {
        logwrite(function, "WARNING FITS output failed to open; skipped");
      }
    }

    if (outputs.empty()) {
      logwrite(function, "no frame outputs configured");
    }
    return outputs;
  }

}
