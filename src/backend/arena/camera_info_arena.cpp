#ifdef WITH_ARENA

#include "camera_info_arena.hpp"
#include "node_map_utils_arena.hpp"
#include "exception.hpp"
#include <sstream>
#include <iostream>

namespace bias {

    CameraInfo_arena::CameraInfo_arena()
    {
        guid_ = std::string("");
        serialNumber_ = std::string("");
        modelName_ = std::string("");
        vendorName_ = std::string("");
        deviceVersion_ = std::string("");
        ipAddress_ = std::string("");
        macAddress_ = std::string("");
    }


    void CameraInfo_arena::populate(acNodeMap hNodeMapDevice, acNodeMap hNodeMapTLDevice)
    {
        // Read what is available from the device node map; guard each node so a
        // model missing any of these does not cause a failure. The IP address
        // is normally set separately from the guid by the camera device.
        if (arena_node::isReadable(hNodeMapDevice, "DeviceVendorName"))
        {
            vendorName_ = arena_node::getStringValue(hNodeMapDevice, "DeviceVendorName");
        }
        if (arena_node::isReadable(hNodeMapDevice, "DeviceModelName"))
        {
            modelName_ = arena_node::getStringValue(hNodeMapDevice, "DeviceModelName");
        }
        if (arena_node::isReadable(hNodeMapDevice, "DeviceSerialNumber"))
        {
            serialNumber_ = arena_node::getStringValue(hNodeMapDevice, "DeviceSerialNumber");
        }
        if (arena_node::isReadable(hNodeMapDevice, "DeviceVersion"))
        {
            deviceVersion_ = arena_node::getStringValue(hNodeMapDevice, "DeviceVersion");
        }

        // Fall back to the transport-layer node map for GigE network info.
        // Guarded with try/catch so an unexpected node type never aborts the
        // (optional) info population during connect().
        if (hNodeMapTLDevice != nullptr)
        {
            if (macAddress_.empty() && arena_node::isReadable(hNodeMapTLDevice, "GevDeviceMACAddress"))
            {
                try
                {
                    std::stringstream ss;
                    ss << arena_node::getIntegerValue(hNodeMapTLDevice, "GevDeviceMACAddress");
                    macAddress_ = ss.str();
                }
                catch (RuntimeError &err)
                {
                    // MAC address is optional - ignore if it cannot be read.
                }
            }
        }
    }


    std::string CameraInfo_arena::toString()
    {
        std::stringstream ss;
        ss << std::endl;
        ss << "Camera Information (Arena)" << std::endl;
        ss << "--------------------------" << std::endl;
        ss << "guid:          " << guid_ << std::endl;
        ss << "vendor name:   " << vendorName_ << std::endl;
        ss << "model name:    " << modelName_ << std::endl;
        ss << "serial number: " << serialNumber_ << std::endl;
        ss << "device version:" << deviceVersion_ << std::endl;
        ss << "ip address:    " << ipAddress_ << std::endl;
        ss << "mac address:   " << macAddress_ << std::endl;
        ss << std::endl;
        return ss.str();
    }


    void CameraInfo_arena::print()
    {
        std::cout << toString();
    }


    std::string CameraInfo_arena::guid() { return guid_; }
    void CameraInfo_arena::setGuid(std::string guid) { guid_ = guid; }

    std::string CameraInfo_arena::serialNumber() { return serialNumber_; }
    void CameraInfo_arena::setSerialNumber(std::string serialNumber) { serialNumber_ = serialNumber; }

    std::string CameraInfo_arena::modelName() { return modelName_; }
    void CameraInfo_arena::setModelName(std::string modelName) { modelName_ = modelName; }

    std::string CameraInfo_arena::vendorName() { return vendorName_; }
    void CameraInfo_arena::setVendorName(std::string vendorName) { vendorName_ = vendorName; }

    std::string CameraInfo_arena::deviceVersion() { return deviceVersion_; }
    void CameraInfo_arena::setDeviceVersion(std::string deviceVersion) { deviceVersion_ = deviceVersion; }

    std::string CameraInfo_arena::ipAddress() { return ipAddress_; }
    void CameraInfo_arena::setIpAddress(std::string ipAddress) { ipAddress_ = ipAddress; }

    std::string CameraInfo_arena::macAddress() { return macAddress_; }
    void CameraInfo_arena::setMacAddress(std::string macAddress) { macAddress_ = macAddress; }

}

#endif // #ifdef WITH_ARENA
