#ifdef WITH_ARENA

#include "guid_device_arena.hpp"
#include <sstream>
#include <iostream>
#include <vector>

namespace bias {

    uint32_t ipStringToUint_arena(const std::string &ipStr)
    {
        // Parse a dotted IPv4 string into a 32-bit integer. On any parse
        // failure return 0 so that ordering degrades gracefully rather than
        // throwing (non-Arena / non-IP guids compare by string elsewhere).
        uint32_t result = 0;
        std::stringstream ss(ipStr);
        std::string octetStr;
        int count = 0;
        while (std::getline(ss, octetStr, '.'))
        {
            if (count >= 4)
            {
                return 0;
            }
            if (octetStr.empty())
            {
                return 0;
            }
            for (char c : octetStr)
            {
                if (c < '0' || c > '9')
                {
                    return 0;
                }
            }
            long octet = std::stol(octetStr);
            if (octet < 0 || octet > 255)
            {
                return 0;
            }
            result = (result << 8) | (uint32_t)(octet);
            count++;
        }
        if (count != 4)
        {
            return 0;
        }
        return result;
    }


    GuidDevice_arena::GuidDevice_arena()
    {
        value_ = std::string("0.0.0.0");
        ipNumeric_ = 0;
    }

    GuidDevice_arena::GuidDevice_arena(std::string guid_arena)
    {
        value_ = guid_arena;
        ipNumeric_ = ipStringToUint_arena(guid_arena);
    }

    CameraLib GuidDevice_arena::getCameraLib()
    {
        return CAMERA_LIB_ARENA;
    }

    std::string GuidDevice_arena::toString()
    {
        return value_;
    }

    void GuidDevice_arena::printValue()
    {
        std::cout << "guid: " << toString() << std::endl;
    }

    std::string GuidDevice_arena::getValue()
    {
        return value_;
    }

    uint32_t GuidDevice_arena::getIpNumeric()
    {
        return ipNumeric_;
    }

    bool GuidDevice_arena::isEqual(GuidDevice &guid)
    {
        bool rval = false;
        if (value_.compare(guid.toString()) == 0)
        {
            rval = true;
        }
        return rval;
    }

    bool GuidDevice_arena::lessThan(GuidDevice &guid)
    {
        // Order numerically by IP when the other guid is also an Arena guid so
        // that "10.0.0.9" sorts before "10.0.0.10" (lexical string order would
        // get this wrong). Fall back to string compare for a total order in
        // mixed-backend guid sets.
        if (guid.getCameraLib() == CAMERA_LIB_ARENA)
        {
            uint32_t otherIpNumeric = ipStringToUint_arena(guid.toString());
            return ipNumeric_ < otherIpNumeric;
        }
        return value_.compare(guid.toString()) < 0;
    }

    bool GuidDevice_arena::lessThanEqual(GuidDevice &guid)
    {
        if (isEqual(guid))
        {
            return true;
        }
        else
        {
            return lessThan(guid);
        }
    }
}

#endif // #ifdef WITH_ARENA
