/**
 * @file    fits_writer.cpp
 * @brief   FrameOutput implementation that writes FITS files asynchronously
 *
 * Producer enqueues from the readout thread; a dedicated worker thread
 * drains the queue and writes one FITS file per frame via CCfits.
 */

#include "fits_writer.h"
#include "common.h"
#include "utilities.h"

#include <CCfits/CCfits>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <valarray>

namespace Camera {

  FitsWriter::FitsWriter(FitsWriterConfig cfg)
    : cfg_(std::move(cfg)) {
  }

  FitsWriter::~FitsWriter() {
    this->close();
  }

  long FitsWriter::open() {
    const std::string function("Camera::FitsWriter::open");

    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
      logwrite(function, "ERROR already opened");
      return ERROR;
    }

    if (cfg_.queue_size == 0) {
      logwrite(function, "ERROR queue_size must be > 0");
      started_.store(false);
      return ERROR;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(cfg_.output_dir, ec)) {
      logwrite(function, "ERROR output_dir does not exist: " + cfg_.output_dir);
      started_.store(false);
      return ERROR;
    }

    stop_.store(false);
    worker_ = std::thread(&FitsWriter::worker_loop, this);

    logwrite(function, "started: dir=" + cfg_.output_dir +
             " basename=" + cfg_.basename +
             " queue_size=" + std::to_string(cfg_.queue_size) +
             " write_interval_ms=" + std::to_string(cfg_.write_interval_ms));
    return NO_ERROR;
  }

  long FitsWriter::write(const char* data, size_t size, const FrameMetadata& meta) {
    if (!started_.load()) return ERROR;

    // Cadence gate — skipped frames never enter the queue
    if (cfg_.write_interval_ms > 0) {
      const auto now = std::chrono::steady_clock::now();
      const auto interval = std::chrono::milliseconds(cfg_.write_interval_ms);
      if (now - last_accepted_ < interval) {
        n_skipped_cadence_.fetch_add(1, std::memory_order_relaxed);
        return NO_ERROR;
      }
      last_accepted_ = now;
    }

    QueuedFrame frame;
    frame.meta = meta;
    frame.data.assign(data, data + size);   // the one memcpy — outside the lock

    {
      std::lock_guard lock(mtx_);
      if (queue_.size() >= cfg_.queue_size) {
        queue_.pop_front();
        n_dropped_queue_.fetch_add(1, std::memory_order_relaxed);
      }
      queue_.push_back(std::move(frame));
    }
    n_received_.fetch_add(1, std::memory_order_relaxed);
    cv_.notify_one();
    return NO_ERROR;
  }

  void FitsWriter::close() {
    if (!started_.load()) return;

    stop_time_ = std::chrono::steady_clock::now();
    stop_.store(true);
    cv_.notify_all();

    if (worker_.joinable()) worker_.join();
    started_.store(false);

    const std::string function("Camera::FitsWriter::close");
    logwrite(function, "stopped: received=" + std::to_string(n_received_.load()) +
             " written=" + std::to_string(n_written_.load()) +
             " dropped_queue=" + std::to_string(n_dropped_queue_.load()) +
             " skipped_cadence=" + std::to_string(n_skipped_cadence_.load()) +
             " failed=" + std::to_string(n_failed_.load()) +
             " dropped_shutdown=" + std::to_string(n_dropped_shutdown_.load()));
  }

  FitsWriter::Stats FitsWriter::stats() const {
    Stats s;
    s.frames_received        = n_received_.load();
    s.frames_written         = n_written_.load();
    s.frames_dropped_queue   = n_dropped_queue_.load();
    s.frames_skipped_cadence = n_skipped_cadence_.load();
    s.frames_failed          = n_failed_.load();
    s.frames_dropped_shutdown= n_dropped_shutdown_.load();
    return s;
  }

  void FitsWriter::worker_loop() {
    const auto drain_timeout = std::chrono::milliseconds(cfg_.drain_timeout_ms);

    while (true) {
      QueuedFrame frame;
      {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty() || stop_.load(); });

        if (queue_.empty()) return;     // stop_ is set and we're drained

        if (stop_.load()) {
          const auto elapsed = std::chrono::steady_clock::now() - stop_time_;
          if (elapsed > drain_timeout) {
            n_dropped_shutdown_.fetch_add(queue_.size(), std::memory_order_relaxed);
            queue_.clear();
            return;
          }
        }

        frame = std::move(queue_.front());
        queue_.pop_front();
      }

      if (write_fits_file(frame) == NO_ERROR) {
        n_written_.fetch_add(1, std::memory_order_relaxed);
      } else {
        n_failed_.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  long FitsWriter::write_fits_file(const QueuedFrame &frame) {
    const std::string function("Camera::FitsWriter::write_fits_file");
    const auto &meta = frame.meta;

    if (meta.width == 0 || meta.height == 0 || meta.bytes_per_pixel == 0) {
      logwrite(function, "ERROR invalid frame metadata");
      return ERROR;
    }
    if (meta.bytes_per_pixel != 2 && meta.bytes_per_pixel != 4) {
      logwrite(function, "ERROR unsupported bytes_per_pixel=" +
               std::to_string(meta.bytes_per_pixel));
      return ERROR;
    }

    const size_t npixels = static_cast<size_t>(meta.width) * meta.height;
    const size_t expected_bytes = npixels * meta.bytes_per_pixel;
    if (frame.data.size() < expected_bytes) {
      logwrite(function, "ERROR frame data " + std::to_string(frame.data.size()) +
               " < expected " + std::to_string(expected_bytes));
      return ERROR;
    }

    const std::string filename = make_filename(meta.frame_number);
    // CCfits requires a non-existing path; "!" prefix would overwrite,
    // but make_filename() already resolved any conflict
    const int bitpix = (meta.bytes_per_pixel == 2) ? USHORT_IMG : ULONG_IMG;
    long axes[2] = { static_cast<long>(meta.width),
                     static_cast<long>(meta.height) };

    try {
      auto pFits = std::make_unique<CCfits::FITS>(filename, bitpix, 2, axes);
      auto &phdu = pFits->pHDU();

      phdu.addKey("FRAMENO", static_cast<long>(meta.frame_number),
                  "Frame number");
      phdu.addKey("TIMESTMP", static_cast<long>(meta.timestamp),
                  "Archon timestamp (0.01 us units)");
      phdu.addKey("DATE", get_timestamp(), "FITS file write time");

      const long first_pixel = 1;
      if (meta.bytes_per_pixel == 2) {
        const auto *src = reinterpret_cast<const uint16_t*>(frame.data.data());
        std::valarray<uint16_t> data(src, npixels);
        phdu.write(first_pixel, npixels, data);
      } else {
        const auto *src = reinterpret_cast<const uint32_t*>(frame.data.data());
        std::valarray<uint32_t> data(src, npixels);
        phdu.write(first_pixel, npixels, data);
      }
    }
    catch (const CCfits::FitsException &e) {
      logwrite(function, "ERROR FITS exception writing " + filename + ": " + e.message());
      return ERROR;
    }
    catch (const std::exception &e) {
      logwrite(function, "ERROR exception writing " + filename + ": " + e.what());
      return ERROR;
    }

    return NO_ERROR;
  }

  std::string FitsWriter::make_filename(uint64_t frame_number) const {
    char num[32];
    std::snprintf(num, sizeof(num), "%08llu",
                  static_cast<unsigned long long>(frame_number));

    const std::string prefix = cfg_.output_dir + "/" + cfg_.basename + "_" + num;
    std::string filename = prefix + ".fits";

    int suffix = 1;
    while (std::filesystem::exists(filename)) {
      filename = prefix + "_" + std::to_string(suffix) + ".fits";
      suffix++;
    }
    return filename;
  }

}
