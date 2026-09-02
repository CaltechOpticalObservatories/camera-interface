/**
 * @file    shared_memory_writer.h
 * @brief   FrameOutput implementation publishing frames via ImageStreamIO
 *
 */
#pragma once

#include "frame_output.h"

#include <ImageStreamIO/ImageStreamIO.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Camera {

  /// FrameOutput that publishes frames as an ImageStreamIO shared-memory image stream
  class SharedMemoryWriter : public FrameOutput {
    public:
      SharedMemoryWriter(const std::string &segment_name,
                         uint32_t ring_buffer_size = 4,
                         const std::string &shm_dir = "");
      ~SharedMemoryWriter();

      long open() override;
      long write(const char* data, size_t size, const FrameMetadata& meta) override;
      void close() override;

    private:
      std::string segment_name_;
      uint32_t ring_buffer_size_;
      std::string shm_dir_;
      bool opened_{false};

      IMAGE image_{};
      uint32_t allocated_width_{0};
      uint32_t allocated_height_{0};
      uint32_t allocated_bytes_per_pixel_{0};

      // Destroys any existing stream and creates one for the given frame shape
      long recreate(uint32_t width, uint32_t height, uint32_t bytes_per_pixel);

      // Writes FRAMENO/TIMESTMP/SEQNUM into image_.kw[]
      void write_keywords(const FrameMetadata &meta);
  };

}
