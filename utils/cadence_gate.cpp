/**
 * @file    cadence_gate.cpp
 * @brief   FrameOutput decorator that rate-limits frames to a wrapped output
 */

#include "cadence_gate.h"
#include "common.h"

#include <utility>

namespace Camera {

  CadenceGate::CadenceGate(std::unique_ptr<FrameOutput> inner, uint32_t write_interval_ms)
    : inner_(std::move(inner)),
      interval_(std::chrono::milliseconds(write_interval_ms)) {
  }

  long CadenceGate::open() {
    return inner_->open();
  }

  long CadenceGate::write(const char* data, size_t size, const FrameMetadata& meta) {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_forwarded_ < interval_) {
      n_skipped_.fetch_add(1, std::memory_order_relaxed);
      return NO_ERROR;
    }
    last_forwarded_ = now;
    return inner_->write(data, size, meta);
  }

  void CadenceGate::close() {
    inner_->close();
  }

}
