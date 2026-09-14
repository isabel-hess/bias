#ifdef WITH_ARENA
#ifndef BIAS_UTILS_ARENA_HPP
#define BIAS_UTILS_ARENA_HPP

// ---------------------------------------------------------------------------
// Utility functions for the Lucid Arena backend - mainly pixel-format mapping
// between the Arena PFNC (Pixel Format Naming Convention) uint64_t codes, the
// OpenCV Mat types, and the BIAS PixelFormat enum.
//
// The PFNC codes themselves live in ArenaCDefs.h (pulled in via ArenaCApi.h);
// this header only deals in uint64_t so it stays free of the Arena headers.
// ---------------------------------------------------------------------------

#include "basic_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace bias
{
    // Choose the conversion target PFNC format (Mono8 / Mono16 / BGR8 / BGR16)
    // for a given source PFNC pixel format, so the image can be wrapped in an
    // OpenCV Mat with a compatible layout.
    uint64_t getSuitablePixelFormat_arena(uint64_t pixelFormat);

    // OpenCV Mat type (CV_8UC1, CV_16UC1, CV_8UC3, CV_16UC3) for a (converted)
    // PFNC pixel format.
    int getCompatibleOpencvFormat_arena(uint64_t pixelFormat);

    // Mapping between Arena PFNC formats and the BIAS PixelFormat enum.
    uint64_t convertPixelFormat_to_arena(PixelFormat pixFormat);
    PixelFormat convertPixelFormat_from_arena(uint64_t pixFormat);

    // Color detection.
    std::vector<uint64_t> getAllowedColorPixelFormats_arena();
    bool isColorPixelFormat_arena(uint64_t pixFormat);

    // Supported property types for the Arena backend.
    std::vector<PropertyType> getArenaSupportedPropertyTypes();
    bool isArenaSupportedPropertyType(PropertyType propType);

    std::string getPixelFormatString_arena(uint64_t pixFormat);

} // namespace bias

#endif // #ifndef BIAS_UTILS_ARENA_HPP
#endif // #ifdef WITH_ARENA
