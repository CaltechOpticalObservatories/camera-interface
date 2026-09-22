/**
 * @file    shared_memory_writer.cpp
 * @brief   FrameOutput implementation publishing frames via ImageStreamIO
 *
 */

#include "shared_memory_writer.h"
#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>

namespace {

  constexpr int NUM_KEYWORDS = 3;         // FRAMENO, TIMESTMP, SEQNUM
  constexpr mode_t SEGMENT_MODE = 0664;   // group-writable so a co-user can clean up a leftover

  void set_long_keyword(IMAGE_KEYWORD &keyword, const char *name, int64_t value,
                        const char *comment) {
    std::snprintf(keyword.name, sizeof(keyword.name), "%s", name);
    keyword.type = 'L';
    keyword.value.numl = value;
    std::snprintf(keyword.comment, sizeof(keyword.comment), "%s", comment);
    keyword.cnt++;
  }

  // Returns "path (uid=.. gid=.. mode=..)" if something exists at path, else ""
  std::string describe_path(const std::string &path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) return "";
    return path + " (uid=" + std::to_string(st.st_uid) +
           " gid=" + std::to_string(st.st_gid) +
           " mode=" + std::to_string(st.st_mode & 07777) + ")";
  }

}

namespace Camera {

  SharedMemoryWriter::SharedMemoryWriter(const std::string &segment_name,
                                         uint32_t ring_buffer_size,
                                         const std::string &shm_dir)
    : segment_name_(segment_name),
      ring_buffer_size_(ring_buffer_size),
      shm_dir_(shm_dir) {
  }

  SharedMemoryWriter::~SharedMemoryWriter() {
    this->close();
  }

  long SharedMemoryWriter::open() {
    const std::string function("Camera::SharedMemoryWriter::open");

    if (segment_name_.empty()) {
      logwrite(function, "ERROR segment name is empty");
      return ERROR;
    }
    if (ring_buffer_size_ == 0) {
      logwrite(function, "ERROR ring_buffer_size must be > 0");
      return ERROR;
    }

    if (!shm_dir_.empty()) {
      std::error_code ec;
      if (!std::filesystem::is_directory(shm_dir_, ec)) {
        logwrite(function, "ERROR shm_dir does not exist: " + shm_dir_);
        return ERROR;
      }
      // ImageStreamIO's only override for its base directory is this env var
      ::setenv("MILK_SHM_DIR", shm_dir_.c_str(), 1);
    }

    opened_ = true;

    // Geometry is fixed for a stream's whole life, so create happens in write(), not here
    logwrite(function, "ready to publish \"" + segment_name_ + "\" (" +
             std::to_string(ring_buffer_size_) + " frames, dir=" +
             (shm_dir_.empty() ? "(default)" : shm_dir_) + ")");
    return NO_ERROR;
  }

  long SharedMemoryWriter::write(const char* data, size_t size, const FrameMetadata& meta) {
    const std::string function("Camera::SharedMemoryWriter::write");

    if (!opened_) {
      logwrite(function, "ERROR shared memory not open");
      return ERROR;
    }
    if (meta.width == 0 || meta.height == 0) {
      logwrite(function, "ERROR invalid frame geometry");
      return ERROR;
    }
    if (meta.bytes_per_pixel != 2 && meta.bytes_per_pixel != 4) {
      logwrite(function, "ERROR unsupported bytes_per_pixel=" +
               std::to_string(meta.bytes_per_pixel));
      return ERROR;
    }

    const size_t frame_bytes =
      static_cast<size_t>(meta.width) * meta.height * meta.bytes_per_pixel;
    if (size < frame_bytes) {
      logwrite(function, "ERROR frame data " + std::to_string(size) +
               " < expected " + std::to_string(frame_bytes));
      return ERROR;
    }

    const auto [entry, inserted] = segments_.try_emplace(meta.stream);
    Segment &segment = entry->second;
    if (inserted) {
      segment.name = meta.stream.empty() ? segment_name_
                                         : segment_name_ + "_" + meta.stream;
    }

    if (meta.width != segment.allocated_width ||
        meta.height != segment.allocated_height ||
        meta.bytes_per_pixel != segment.allocated_bytes_per_pixel) {
      if (this->recreate(segment, meta.width, meta.height, meta.bytes_per_pixel) != NO_ERROR) {
        return ERROR;
      }
    }

    void* buffer = nullptr;
    if (ImageStreamIO_writeBuffer(&segment.image, &buffer) != IMAGESTREAMIO_SUCCESS) {
      logwrite(function, "ERROR ImageStreamIO_writeBuffer failed for \"" + segment.name + "\"");
      return ERROR;
    }
    std::memcpy(buffer, data, frame_bytes);

    write_keywords(segment.image, meta);

    ImageStreamIO_UpdateIm(&segment.image);
    frames_written_.fetch_add(1, std::memory_order_relaxed);

    return NO_ERROR;
  }

  OutputStatus SharedMemoryWriter::status() const {
    OutputStatus out;
    out.name           = "shm";
    out.frames_written = frames_written_.load(std::memory_order_relaxed);
    // A consumer that falls behind loses frames to the ring, which this writer
    // cannot see, so nothing is counted as dropped here
    return out;
  }

  void SharedMemoryWriter::close() {
    const std::string function("Camera::SharedMemoryWriter::close");

    for (auto &entry : segments_) {
      ImageStreamIO_destroyIm(&entry.second.image);
      logwrite(function, "closed \"" + entry.second.name + "\"");
    }
    segments_.clear();
    opened_ = false;
  }

  long SharedMemoryWriter::recreate(Segment &segment, uint32_t width, uint32_t height,
                                    uint32_t bytes_per_pixel) {
    const std::string function("Camera::SharedMemoryWriter::recreate");

    ImageStreamIO_destroyIm(&segment.image);
    segment.allocated_width = segment.allocated_height = 0;
    segment.allocated_bytes_per_pixel = 0;

    char path[STRINGMAXLEN_FILE_NAME];
    ImageStreamIO_filename(path, sizeof(path), segment.name.c_str());

    // Diagnostic even when the library's internal unlink-and-retry self-heals a
    // same-owner crash leftover, so an operator can see it happened
    const std::string preexisting = describe_path(path);
    if (!preexisting.empty()) {
      logwrite(function, "found existing segment at " + preexisting);
    }

    const uint8_t datatype = (bytes_per_pixel == 2) ? _DATATYPE_UINT16 : _DATATYPE_UINT32;
    uint32_t size[2] = {width, height};

    const errno_t status = ImageStreamIO_createIm(
        &segment.image, segment.name.c_str(), 2, size, datatype,
        1 /* shared */, NUM_KEYWORDS, static_cast<int>(ring_buffer_size_));

    if (status != IMAGESTREAMIO_SUCCESS) {
      const std::string blocker = describe_path(path);
      logwrite(function, "ERROR ImageStreamIO_createIm failed for \"" + segment.name +
               "\" (" + std::to_string(width) + "x" + std::to_string(height) + ")" +
               (blocker.empty() ? "" : "; blocked by " + blocker));
      return ERROR;
    }

    if (::chmod(path, SEGMENT_MODE) != 0) {
      logwrite(function, "WARNING chmod failed for \"" + std::string(path) + "\"");
    }

    segment.allocated_width = width;
    segment.allocated_height = height;
    segment.allocated_bytes_per_pixel = bytes_per_pixel;

    logwrite(function, "created \"" + segment.name + "\" (" +
             std::to_string(width) + "x" + std::to_string(height) + ", " +
             std::to_string(bytes_per_pixel) + " bytes/px, " +
             std::to_string(ring_buffer_size_) + " frames)");
    return NO_ERROR;
  }

  void SharedMemoryWriter::write_keywords(IMAGE &image, const FrameMetadata &meta) {
    set_long_keyword(image.kw[0], "FRAMENO",
                     static_cast<int64_t>(meta.frame_number), "Frame number");
    set_long_keyword(image.kw[1], "TIMESTMP",
                     static_cast<int64_t>(meta.timestamp), "Archon timestamp (0.01 us units)");
    set_long_keyword(image.kw[2], "SEQNUM",
                     static_cast<int64_t>(meta.sequence_number), "Sequence number");
  }

}
