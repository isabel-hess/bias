#ifdef WITH_ARENA
#ifndef BIAS_CAMERA_INFO_ARENA_HPP
#define BIAS_CAMERA_INFO_ARENA_HPP

#include <string>
#include "ArenaCApi.h"

namespace bias {

    // Holds identifying information read from a Lucid Arena device's node maps.
    class CameraInfo_arena
    {
        public:

            CameraInfo_arena();

            // Populate all fields from the device / transport-layer node maps.
            void populate(acNodeMap hNodeMapDevice, acNodeMap hNodeMapTLDevice);

            std::string toString();
            void print();

            std::string guid();
            void setGuid(std::string guid);

            std::string serialNumber();
            void setSerialNumber(std::string serialNumber);

            std::string modelName();
            void setModelName(std::string modelName);

            std::string vendorName();
            void setVendorName(std::string vendorName);

            std::string deviceVersion();
            void setDeviceVersion(std::string deviceVersion);

            std::string ipAddress();
            void setIpAddress(std::string ipAddress);

            std::string macAddress();
            void setMacAddress(std::string macAddress);

        protected:

            std::string guid_;
            std::string serialNumber_;
            std::string modelName_;
            std::string vendorName_;
            std::string deviceVersion_;
            std::string ipAddress_;
            std::string macAddress_;
    };

}

#endif // #ifndef BIAS_CAMERA_INFO_ARENA_HPP
#endif // #ifdef WITH_ARENA
