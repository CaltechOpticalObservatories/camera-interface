/**
 * @file    cadence_gate.h
 * @brief   FrameOutput decorator that rate-limits frames to a wrapped output
 *
 * Forwards a frame only when at least write_interval_ms has elapsed since the
 * last forwarded frame; otherwise the frame is dropped. Keeps the cadence
 * policy out of the concrete writers, which then just do their I/O.
 */
#pragma once

#include "frame_output.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace Camera {

  /// FrameOutput that thins frames by minimum interval before delegating
  class CadenceGate : public FrameOutput {
    public:
      CadenceGate(std::unique_ptr<FrameOutput> inner, uint32_t write_interval_ms);
      ~CadenceGate() override = default;

      CadenceGate(const CadenceGate&) = delete;
      CadenceGate& operator=(const CadenceGate&) = delete;

      long open() override;
      long write(const char* data, size_t size, const FrameMetadata& meta) override;
      void close() override;

      uint64_t frames_skipped() const { return n_skipped_.load(); }

    private:
      std::unique_ptr<FrameOutput> inner_;
      std::chrono::milliseconds interval_;

      // Only touched on the producer thread
      std::chrono::steady_clock::time_point last_forwarded_{
          std::chrono::steady_clock::time_point::min()};

      std::atomic<uint64_t> n_skipped_{0};
  };

}
