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
                                         size_t max_frame_bytes,
                                         uint32_t ring_buffer_size,
                                         const std::string &shm_dir)
    : segment_name_(segment_name),
      max_frame_bytes_(max_frame_bytes),
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
    if (max_frame_bytes_ == 0) {
      logwrite(function, "ERROR max_frame_bytes must be > 0");
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
    logwrite(function, "ready to publish \"" + segment_name_ + "\" (max " +
             std::to_string(max_frame_bytes_) + " bytes/frame, " +
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
    if (frame_bytes > max_frame_bytes_) {
      logwrite(function, "ERROR frame size " + std::to_string(frame_bytes) +
               " exceeds max " + std::to_string(max_frame_bytes_));
      return ERROR;
    }
    if (size < frame_bytes) {
      logwrite(function, "ERROR frame data " + std::to_string(size) +
               " < expected " + std::to_string(frame_bytes));
      return ERROR;
    }

    if (meta.width != allocated_width_ ||
        meta.height != allocated_height_ ||
        meta.bytes_per_pixel != allocated_bytes_per_pixel_) {
      if (this->recreate(meta.width, meta.height, meta.bytes_per_pixel) != NO_ERROR) {
        return ERROR;
      }
    }

    void* buffer = nullptr;
    if (ImageStreamIO_writeBuffer(&image_, &buffer) != IMAGESTREAMIO_SUCCESS) {
      logwrite(function, "ERROR ImageStreamIO_writeBuffer failed");
      return ERROR;
    }
    std::memcpy(buffer, data, frame_bytes);

    this->write_keywords(meta);

    ImageStreamIO_UpdateIm(&image_);

    return NO_ERROR;
  }

  void SharedMemoryWriter::close() {
    const std::string function("Camera::SharedMemoryWriter::close");

    ImageStreamIO_destroyIm(&image_);
    allocated_width_ = allocated_height_ = allocated_bytes_per_pixel_ = 0;

    if (opened_) {
      logwrite(function, "closed \"" + segment_name_ + "\"");
    }
    opened_ = false;
  }

  long SharedMemoryWriter::recreate(uint32_t width, uint32_t height, uint32_t bytes_per_pixel) {
    const std::string function("Camera::SharedMemoryWriter::recreate");

    ImageStreamIO_destroyIm(&image_);
    allocated_width_ = allocated_height_ = allocated_bytes_per_pixel_ = 0;

    char path[STRINGMAXLEN_FILE_NAME];
    ImageStreamIO_filename(path, sizeof(path), segment_name_.c_str());

    // Diagnostic even when the library's internal unlink-and-retry self-heals a
    // same-owner crash leftover, so an operator can see it happened
    const std::string preexisting = describe_path(path);
    if (!preexisting.empty()) {
      logwrite(function, "found existing segment at " + preexisting);
    }

    const uint8_t datatype = (bytes_per_pixel == 2) ? _DATATYPE_UINT16 : _DATATYPE_UINT32;
    uint32_t size[2] = {width, height};

    const errno_t status = ImageStreamIO_createIm(
        &image_, segment_name_.c_str(), 2, size, datatype,
        1 /* shared */, NUM_KEYWORDS, static_cast<int>(ring_buffer_size_));

    if (status != IMAGESTREAMIO_SUCCESS) {
      const std::string blocker = describe_path(path);
      logwrite(function, "ERROR ImageStreamIO_createIm failed for \"" + segment_name_ +
               "\" (" + std::to_string(width) + "x" + std::to_string(height) + ")" +
               (blocker.empty() ? "" : "; blocked by " + blocker));
      return ERROR;
    }

    if (::chmod(path, SEGMENT_MODE) != 0) {
      logwrite(function, "WARNING chmod failed for \"" + std::string(path) + "\"");
    }

    allocated_width_ = width;
    allocated_height_ = height;
    allocated_bytes_per_pixel_ = bytes_per_pixel;

    logwrite(function, "created \"" + segment_name_ + "\" (" +
             std::to_string(width) + "x" + std::to_string(height) + ", " +
             std::to_string(bytes_per_pixel) + " bytes/px, " +
             std::to_string(ring_buffer_size_) + " frames)");
    return NO_ERROR;
  }

  void SharedMemoryWriter::write_keywords(const FrameMetadata &meta) {
    set_long_keyword(image_.kw[0], "FRAMENO",
                     static_cast<int64_t>(meta.frame_number), "Frame number");
    set_long_keyword(image_.kw[1], "TIMESTMP",
                     static_cast<int64_t>(meta.timestamp), "Archon timestamp (0.01 us units)");
    set_long_keyword(image_.kw[2], "SEQNUM",
                     static_cast<int64_t>(meta.sequence_number), "Sequence number");
  }

}
