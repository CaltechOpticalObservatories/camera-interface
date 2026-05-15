/**
 * @file    fits_writer.h
 * @brief   FrameOutput implementation that writes FITS files asynchronously
 *
 * Producer calls write() from the readout thread; data is memcpy'd into
 * a bounded queue (drop-oldest on overflow) and a dedicated worker thread
 * drains the queue to disk. write() never touches CCfits or blocks on
 * disk I/O.
 */
#pragma once

#include "frame_output.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Camera {

  struct FitsWriterConfig {
    std::string output_dir{"/tmp"};
    std::string basename{"tracking"};
    uint32_t    write_interval_ms{0};       // 0 = write every accepted frame
    size_t      queue_size{32};
    uint32_t    drain_timeout_ms{5000};
  };

  class FitsWriter : public FrameOutput {
    public:
      explicit FitsWriter(FitsWriterConfig cfg);
      ~FitsWriter() override;

      FitsWriter(const FitsWriter&) = delete;
      FitsWriter& operator=(const FitsWriter&) = delete;

      long open() override;
      long write(const char* data, size_t size, const FrameMetadata& meta) override;
      void close() override;

      struct Stats {
        uint64_t frames_received{0};
        uint64_t frames_written{0};
        uint64_t frames_dropped_queue{0};
        uint64_t frames_skipped_cadence{0};
        uint64_t frames_failed{0};
        uint64_t frames_dropped_shutdown{0};
      };
      Stats stats() const;

    private:
      struct QueuedFrame {
        FrameMetadata meta;
        std::vector<char> data;
      };

      void worker_loop();
      long write_fits_file(const QueuedFrame &frame);
      std::string make_filename(uint64_t frame_number) const;

      FitsWriterConfig cfg_;

      std::deque<QueuedFrame> queue_;
      mutable std::mutex mtx_;
      std::condition_variable cv_;

      std::atomic<bool> stop_{false};
      std::atomic<bool> started_{false};
      std::thread worker_;
      // Set in close() before stop_, so worker can read race-free
      std::chrono::steady_clock::time_point stop_time_;

      // Cadence-gate state — only touched on producer thread
      std::chrono::steady_clock::time_point last_accepted_{
          std::chrono::steady_clock::time_point::min()};

      std::atomic<uint64_t> n_received_{0};
      std::atomic<uint64_t> n_written_{0};
      std::atomic<uint64_t> n_dropped_queue_{0};
      std::atomic<uint64_t> n_skipped_cadence_{0};
      std::atomic<uint64_t> n_failed_{0};
      std::atomic<uint64_t> n_dropped_shutdown_{0};
  };

}
