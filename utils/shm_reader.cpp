/**
 * @file    shm_reader.cpp
 * @brief   Standalone tool that reads back an ImageStreamIO segment for diagnostics
 *
 * Prints geometry, keyword values, and basic pixel statistics for a named
 * ImageStreamIO stream. Used to verify SharedMemoryWriter's output without
 * needing a real cacao/milk installation, both interactively and in CI.
 */

#include <ImageStreamIO/ImageStreamIO.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

  void print_keywords(const IMAGE &image) {
    for (uint16_t i = 0; i < image.md->NBkw; ++i) {
      const IMAGE_KEYWORD &keyword = image.kw[i];
      if (keyword.type == 'N') continue;

      std::cout << "keyword " << keyword.name << " = ";
      switch (keyword.type) {
        case 'L': std::cout << keyword.value.numl; break;
        case 'D': std::cout << keyword.value.numf; break;
        case 'S': std::cout << keyword.value.valstr; break;
        default:  std::cout << "(unknown type '" << keyword.type << "')"; break;
      }
      std::cout << " -- " << keyword.comment << "\n";
    }
  }

  template <typename PixelType>
  void print_pixel_stats_as(const PixelType *pixels, long pixel_count) {
    PixelType min_value = pixels[0];
    PixelType max_value = pixels[0];
    unsigned long long sum = 0;
    for (long i = 0; i < pixel_count; ++i) {
      min_value = std::min(min_value, pixels[i]);
      max_value = std::max(max_value, pixels[i]);
      sum += pixels[i];
    }

    std::cout << "pixels: count=" << pixel_count << " min=" << min_value
              << " max=" << max_value
              << " mean=" << (static_cast<double>(sum) / pixel_count) << "\n";
  }

  void print_pixel_stats(const IMAGE &image) {
    const long pixel_count = static_cast<long>(image.md->size[0]) * image.md->size[1];
    switch (image.md->datatype) {
      case _DATATYPE_UINT16: print_pixel_stats_as(image.array.UI16, pixel_count); break;
      case _DATATYPE_UINT32: print_pixel_stats_as(image.array.UI32, pixel_count); break;
      default:
        std::cout << "pixel stats not printed: unsupported datatype "
                  << static_cast<int>(image.md->datatype) << "\n";
    }
  }

}

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: " << argv[0] << " <segment_name> [dir]\n";
    return 1;
  }

  if (argc == 3) {
    // ImageStreamIO's only override for its base directory is this env var
    ::setenv("MILK_SHM_DIR", argv[2], 1);
  }

  IMAGE image{};
  if (ImageStreamIO_openIm(&image, argv[1]) != IMAGESTREAMIO_SUCCESS) {
    std::cerr << "failed to open segment \"" << argv[1] << "\"\n";
    return 1;
  }

  std::cout << "segment=" << argv[1]
            << " naxis=" << static_cast<int>(image.md->naxis)
            << " size=" << image.md->size[0] << "x" << image.md->size[1]
            << " datatype=" << static_cast<int>(image.md->datatype)
            << " cnt0=" << image.md->cnt0 << "\n";

  print_keywords(image);
  print_pixel_stats(image);

  const uint64_t write_count = image.md->cnt0;
  ImageStreamIO_closeIm(&image);

  if (write_count == 0) {
    std::cerr << "segment \"" << argv[1] << "\" exists but was never written\n";
    return 1;
  }
  return 0;
}
