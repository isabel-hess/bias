#ifdef WITH_ARENA
#ifndef BIAS_GUID_DEVICE_ARENA_HPP
#define BIAS_GUID_DEVICE_ARENA_HPP

#include <string>
#include <memory>
#include <cstdint>
#include "basic_types.hpp"
#include "guid_device.hpp"

namespace bias {

    class GuidDevice_arena : public GuidDevice
    {
        // ---------------------------------------------------------------------
        // Provides representation of Lucid Arena specific camera guids.
        //
        // For GigE cameras the guid value is the camera's IP address (dotted
        // string). Ordering is done numerically on the IP so that a given
        // physical camera keeps a stable position in the enumerated camera set
        // (and therefore a stable camera number / http port / window) across
        // runs - unlike USB enumeration order.
        // ---------------------------------------------------------------------
        public:
            GuidDevice_arena();
            explicit GuidDevice_arena(std::string guid_arena);
            virtual ~GuidDevice_arena() {};
            virtual CameraLib getCameraLib();
            virtual void printValue();
            virtual std::string toString();
            std::string getValue();
            uint32_t getIpNumeric();

        private:
            std::string value_;
            uint32_t ipNumeric_;
            virtual bool isEqual(GuidDevice &guid);
            virtual bool lessThan(GuidDevice &guid);
            virtual bool lessThanEqual(GuidDevice &guid);
    };

    typedef std::shared_ptr<GuidDevice_arena> GuidDevicePtr_arena;

    // Parse a dotted IPv4 string ("10.0.1.7") into a 32-bit integer for
    // numeric ordering. Returns 0 if the string is not a valid dotted IP.
    uint32_t ipStringToUint_arena(const std::string &ipStr);
}

#endif // #ifndef BIAS_GUID_DEVICE_ARENA_HPP
#endif // #ifdef WITH_ARENA
