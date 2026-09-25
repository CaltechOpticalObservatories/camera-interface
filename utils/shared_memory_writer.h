/**
 * @file    shared_memory_writer.h
 * @brief   FrameOutput implementation publishing frames via ImageStreamIO
 *
 */
#pragma once

#include "frame_output.h"

#include <ImageStreamIO/ImageStreamIO.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace Camera {

  /// FrameOutput that publishes frames as ImageStreamIO shared-memory image streams
  class SharedMemoryWriter : public FrameOutput {
    public:
      SharedMemoryWriter(const std::string &segment_name,
                         uint32_t ring_buffer_size = 4,
                         const std::string &shm_dir = "");
      ~SharedMemoryWriter();

      long open() override;
      long write(const char* data, size_t size, const FrameMetadata& meta) override;
      void close() override;
      OutputStatus status() const override;

    private:
      /// One ImageStreamIO stream, created on the first frame that names it
      struct Segment {
        IMAGE image{};
        std::string name;
        uint32_t allocated_width{0};
        uint32_t allocated_height{0};
        uint32_t allocated_bytes_per_pixel{0};
      };

      std::atomic<uint64_t> frames_written_{0};

      std::string segment_name_;
      uint32_t ring_buffer_size_;
      std::string shm_dir_;
      bool opened_{false};

      // Keyed by FrameMetadata::stream, so a RAW capture gets its own segment
      // instead of tearing down and resizing the image stream
      std::map<std::string, Segment> segments_;

      // Destroys any existing stream and creates one for the given frame shape
      long recreate(Segment &segment, uint32_t width, uint32_t height,
                    uint32_t bytes_per_pixel);

      // Writes FRAMENO/TIMESTMP/SEQNUM into image.kw[]
      static void write_keywords(IMAGE &image, const FrameMetadata &meta);
  };

}
