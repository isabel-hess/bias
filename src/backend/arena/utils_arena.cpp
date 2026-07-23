#ifdef WITH_ARENA

#include "utils_arena.hpp"
#include "exception.hpp"
#include "ArenaCApi.h"
#include <opencv2/core/core.hpp>
#include <map>
#include <algorithm>
#include <sstream>

namespace bias
{

    // Choose a conversion target format for wrapping into an OpenCV Mat.
    // Monochrome sources -> Mono8/Mono16, color/bayer sources -> BGR8/BGR16.
    // ------------------------------------------------------------------------
    uint64_t getSuitablePixelFormat_arena(uint64_t pixelFormat)
    {
        switch (pixelFormat)
        {
            // 8-bit mono
            case PFNC_Mono8:
            case PFNC_Mono8s:
                return PFNC_Mono8;

            // >8-bit mono -> 16-bit mono
            case PFNC_Mono10:
            case PFNC_Mono10p:
            case PFNC_Mono12:
            case PFNC_Mono12p:
            case PFNC_Mono14:
            case PFNC_Mono16:
                return PFNC_Mono16;

            // 8-bit color / bayer -> BGR8
            case PFNC_RGB8:
            case PFNC_BGR8:
            case PFNC_BayerRG8:
            case PFNC_BayerGB8:
            case PFNC_BayerGR8:
            case PFNC_BayerBG8:
            case PFNC_YCbCr422_8:
            case PFNC_YCbCr8:
                return PFNC_BGR8;

            // >8-bit color / bayer -> BGR16
            case PFNC_RGB16:
            case PFNC_BGR16:
            case PFNC_BayerRG10:
            case PFNC_BayerGB10:
            case PFNC_BayerGR10:
            case PFNC_BayerBG10:
            case PFNC_BayerRG12:
            case PFNC_BayerGB12:
            case PFNC_BayerGR12:
            case PFNC_BayerBG12:
            case PFNC_BayerRG16:
            case PFNC_BayerGB16:
            case PFNC_BayerGR16:
            case PFNC_BayerBG16:
                return PFNC_BGR16;

            default:
                // Fall back to a safe color format; the Arena image factory
                // will debayer / convert as needed.
                return PFNC_BGR8;
        }
    }


    int getCompatibleOpencvFormat_arena(uint64_t pixelFormat)
    {
        switch (pixelFormat)
        {
            case PFNC_Mono8:
                return CV_8UC1;

            case PFNC_Mono16:
                return CV_16UC1;

            case PFNC_BGR8:
                return CV_8UC3;

            case PFNC_BGR16:
                return CV_16UC3;

            default:
            {
                std::stringstream ssError;
                ssError << __PRETTY_FUNCTION__;
                ssError << ": no compatible opencv format for PFNC format " << pixelFormat;
                throw RuntimeError(ERROR_ARENA_NO_COMPATIBLE_OPENCV_FORMAT, ssError.str());
            }
        }
    }


    static std::map<PixelFormat, uint64_t> pixelFormatMap_to_arena =
    {
        {PIXEL_FORMAT_MONO8,   PFNC_Mono8},
        {PIXEL_FORMAT_MONO16,  PFNC_Mono16},
        {PIXEL_FORMAT_MONO12,  PFNC_Mono12},
        {PIXEL_FORMAT_RGB8,    PFNC_RGB8},
        {PIXEL_FORMAT_RGB16,   PFNC_RGB16},
        {PIXEL_FORMAT_BGR8,    PFNC_BGR8},
        {PIXEL_FORMAT_BGR16,   PFNC_BGR16},
        {PIXEL_FORMAT_RAW8,    PFNC_BayerRG8},
        {PIXEL_FORMAT_422YUV8, PFNC_YCbCr422_8},
    };


    uint64_t convertPixelFormat_to_arena(PixelFormat pixFormat)
    {
        if (pixelFormatMap_to_arena.count(pixFormat) != 0)
        {
            return pixelFormatMap_to_arena[pixFormat];
        }
        else
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to convert pixel format to Arena PFNC format";
            throw RuntimeError(ERROR_ARENA_CONVERT_PIXEL_FORMAT, ssError.str());
        }
    }


    static std::map<uint64_t, PixelFormat> pixelFormatMap_from_arena =
    {
        {PFNC_Mono8,      PIXEL_FORMAT_MONO8},
        {PFNC_Mono16,     PIXEL_FORMAT_MONO16},
        {PFNC_Mono12,     PIXEL_FORMAT_MONO12},
        {PFNC_RGB8,       PIXEL_FORMAT_RGB8},
        {PFNC_RGB16,      PIXEL_FORMAT_RGB16},
        {PFNC_BGR8,       PIXEL_FORMAT_BGR8},
        {PFNC_BGR16,      PIXEL_FORMAT_BGR16},
        {PFNC_YCbCr422_8, PIXEL_FORMAT_422YUV8},
    };


    PixelFormat convertPixelFormat_from_arena(uint64_t pixFormat)
    {
        if (pixelFormatMap_from_arena.count(pixFormat) != 0)
        {
            return pixelFormatMap_from_arena[pixFormat];
        }
        else
        {
            // Bayer and other formats do not map cleanly onto the BIAS enum;
            // report the closest sensible default rather than throwing.
            if (isColorPixelFormat_arena(pixFormat))
            {
                return PIXEL_FORMAT_RAW8;
            }
            return PIXEL_FORMAT_MONO8;
        }
    }


    std::vector<uint64_t> getAllowedColorPixelFormats_arena()
    {
        std::vector<uint64_t> formatsVec =
        {
            PFNC_RGB8, PFNC_BGR8, PFNC_RGB16, PFNC_BGR16,
            PFNC_YCbCr422_8, PFNC_YCbCr8,
            PFNC_BayerRG8, PFNC_BayerGB8, PFNC_BayerGR8, PFNC_BayerBG8,
            PFNC_BayerRG10, PFNC_BayerGB10, PFNC_BayerGR10, PFNC_BayerBG10,
            PFNC_BayerRG12, PFNC_BayerGB12, PFNC_BayerGR12, PFNC_BayerBG12,
            PFNC_BayerRG16, PFNC_BayerGB16, PFNC_BayerGR16, PFNC_BayerBG16,
        };
        return formatsVec;
    }


    bool isColorPixelFormat_arena(uint64_t pixFormat)
    {
        std::vector<uint64_t> colorFormats = getAllowedColorPixelFormats_arena();
        return std::find(colorFormats.begin(), colorFormats.end(), pixFormat) != colorFormats.end();
    }


    std::vector<PropertyType> getArenaSupportedPropertyTypes()
    {
        std::vector<PropertyType> arenaSupportedVec =
        {
            PROPERTY_TYPE_BRIGHTNESS,
            PROPERTY_TYPE_GAMMA,
            PROPERTY_TYPE_SHUTTER,
            PROPERTY_TYPE_GAIN,
            PROPERTY_TYPE_TRIGGER_MODE,
            PROPERTY_TYPE_TRIGGER_DELAY,
            PROPERTY_TYPE_FRAME_RATE,
            PROPERTY_TYPE_TEMPERATURE,
        };
        return arenaSupportedVec;
    }


    bool isArenaSupportedPropertyType(PropertyType propType)
    {
        std::vector<PropertyType> supportedTypeVec = getArenaSupportedPropertyTypes();
        return (std::find(supportedTypeVec.begin(), supportedTypeVec.end(), propType) != supportedTypeVec.end());
    }


    static std::map<uint64_t, std::string> pixelFormatToStringMap_arena =
    {
        {PFNC_Mono8,      std::string("Monochrome 8-bit")},
        {PFNC_Mono10,     std::string("Monochrome 10-bit unpacked")},
        {PFNC_Mono10p,    std::string("Monochrome 10-bit packed")},
        {PFNC_Mono12,     std::string("Monochrome 12-bit unpacked")},
        {PFNC_Mono12p,    std::string("Monochrome 12-bit packed")},
        {PFNC_Mono16,     std::string("Monochrome 16-bit")},
        {PFNC_RGB8,       std::string("Red-Green-Blue 8-bit")},
        {PFNC_RGB16,      std::string("Red-Green-Blue 16-bit")},
        {PFNC_BGR8,       std::string("Blue-Green-Red 8-bit")},
        {PFNC_BGR16,      std::string("Blue-Green-Red 16-bit")},
        {PFNC_YCbCr422_8, std::string("YCbCr 4:2:2 8-bit")},
        {PFNC_YCbCr8,     std::string("YCbCr 4:4:4 8-bit")},
        {PFNC_BayerRG8,   std::string("Bayer Red-Green 8-bit")},
        {PFNC_BayerGB8,   std::string("Bayer Green-Blue 8-bit")},
        {PFNC_BayerGR8,   std::string("Bayer Green-Red 8-bit")},
        {PFNC_BayerBG8,   std::string("Bayer Blue-Green 8-bit")},
    };


    std::string getPixelFormatString_arena(uint64_t pixFormat)
    {
        if (pixelFormatToStringMap_arena.count(pixFormat) != 0)
        {
            return pixelFormatToStringMap_arena[pixFormat];
        }
        std::stringstream ss;
        ss << "PFNC 0x" << std::hex << pixFormat;
        return ss.str();
    }

} // namespace bias

#endif // #ifdef WITH_ARENA
