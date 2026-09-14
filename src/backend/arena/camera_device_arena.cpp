#ifdef WITH_ARENA

#include "camera_device_arena.hpp"
#include "utils_arena.hpp"
#include "node_map_utils_arena.hpp"
#include "guid_device_arena.hpp"
#include "system_arena.hpp"
#include "exception.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>

namespace bias {

    namespace nm = arena_node;


    CameraDevice_arena::CameraDevice_arena() : CameraDevice() {}


    CameraDevice_arena::CameraDevice_arena(Guid guid) : CameraDevice(guid)
    {
        // The Arena system is opened in connect() so that the device lifecycle
        // (open system -> create device -> destroy device -> close system) is
        // fully contained in connect()/disconnect().
    }


    CameraDevice_arena::~CameraDevice_arena()
    {
        if (capturing_)
        {
            stopCapture();
        }

        if (connected_)
        {
            disconnect();
        }
    }


    CameraLib CameraDevice_arena::getCameraLib()
    {
        return guid_.getCameraLib();
    }


    void CameraDevice_arena::connect()
    {
        if (connected_)
        {
            return;
        }

        AC_ERROR err = AC_ERR_SUCCESS;

        // Acquire the shared process-wide Arena system (see system_arena).
        hSystem_ = acquireArenaSystem();

        // Refresh the device list
        err = acSystemUpdateDevices(hSystem_, 100);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to update Arena devices, error=" << err;
            throw RuntimeError(ERROR_ARENA_UPDATE_DEVICES, ssError.str());
        }

        size_t numDevices = 0;
        err = acSystemGetNumDevices(hSystem_, &numDevices);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get number of Arena devices, error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_NUM_DEVICES, ssError.str());
        }

        // Match the device whose IP address equals this guid (the guid value is
        // the camera IP). acSystemCreateDevice takes an index, so we resolve the
        // IP to an index within the current enumeration snapshot.
        std::string targetIp = guid_.toString();
        bool found = false;
        size_t foundIndex = 0;
        for (size_t i=0; i<numDevices; i++)
        {
            size_t bufLen = 64;
            std::vector<char> bufVec(bufLen);
            err = acSystemGetDeviceIpAddressStr(hSystem_, i, bufVec.data(), &bufLen);
            if (err != AC_ERR_SUCCESS)
            {
                continue;
            }
            if (targetIp == std::string(bufVec.data()))
            {
                found = true;
                foundIndex = i;
                break;
            }
        }

        if (!found)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to find Arena camera with IP " << targetIp;
            throw RuntimeError(ERROR_ARENA_CAMERA_NOT_FOUND, ssError.str());
        }

        err = acSystemCreateDevice(hSystem_, foundIndex, &hDevice_);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to create Arena device, error=" << err;
            throw RuntimeError(ERROR_ARENA_CREATE_DEVICE, ssError.str());
        }
        connected_ = true;

        // Retrieve node maps
        err = acDeviceGetNodeMap(hDevice_, &hNodeMap_);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena device node map, error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_NODEMAP, ssError.str());
        }

        err = acDeviceGetTLStreamNodeMap(hDevice_, &hTLStreamNodeMap_);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena TL stream node map, error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_TLSTREAM_NODEMAP, ssError.str());
        }

        err = acDeviceGetTLDeviceNodeMap(hDevice_, &hTLDeviceNodeMap_);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena TL device node map, error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_TLDEVICE_NODEMAP, ssError.str());
        }

        // Populate camera info
        cameraInfo_.populate(hNodeMap_, hTLDeviceNodeMap_);
        cameraInfo_.setGuid(targetIp);
        cameraInfo_.setIpAddress(targetIp);

        // Default settings - guard each so a model missing a node does not fail.
        if (nm::isWritable(hNodeMap_, "ExposureAuto"))
        {
            nm::setEnumValue(hNodeMap_, "ExposureAuto", "Off");
        }
        if (nm::isWritable(hNodeMap_, "GainAuto"))
        {
            nm::setEnumValue(hNodeMap_, "GainAuto", "Off");
        }
        if (nm::isWritable(hNodeMap_, "TriggerSelector"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerSelector", "FrameStart");
        }
        if (nm::isWritable(hNodeMap_, "AcquisitionFrameRateEnable"))
        {
            nm::setBooleanValue(hNodeMap_, "AcquisitionFrameRateEnable", true);
        }

        // Start in free-run (internal trigger) mode.
        setTriggerInternal();
        triggerType_ = getTriggerType();
    }


    void CameraDevice_arena::disconnect()
    {
        if (capturing_)
        {
            stopCapture();
        }

        if (connected_)
        {
            if (hDevice_ != nullptr)
            {
                AC_ERROR err = acSystemDestroyDevice(hSystem_, hDevice_);
                hDevice_ = nullptr;
                if (err != AC_ERR_SUCCESS)
                {
                    std::stringstream ssError;
                    ssError << __PRETTY_FUNCTION__;
                    ssError << ": unable to destroy Arena device, error=" << err;
                    throw RuntimeError(ERROR_ARENA_DESTROY_DEVICE, ssError.str());
                }
            }
            connected_ = false;
        }

        if (hSystem_ != nullptr)
        {
            hSystem_ = nullptr;
            hNodeMap_ = nullptr;
            hTLStreamNodeMap_ = nullptr;
            hTLDeviceNodeMap_ = nullptr;
            // Release our hold on the shared Arena system (closes it only when
            // the last holder - finder or device - releases it).
            releaseArenaSystem();
        }
    }


    void CameraDevice_arena::startCapture()
    {
        if (!connected_)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to start Arena capture - not connected";
            throw RuntimeError(ERROR_ARENA_START_CAPTURE, ssError.str());
        }

        if (!capturing_)
        {
            // Configure acquisition and stream engine before streaming.
            if (nm::isWritable(hNodeMap_, "AcquisitionMode"))
            {
                nm::setEnumValue(hNodeMap_, "AcquisitionMode", "Continuous");
            }
            if (nm::isWritable(hTLStreamNodeMap_, "StreamBufferHandlingMode"))
            {
                nm::setEnumValue(hTLStreamNodeMap_, "StreamBufferHandlingMode", "NewestOnly");
            }
            if (nm::isWritable(hTLStreamNodeMap_, "StreamAutoNegotiatePacketSize"))
            {
                nm::setBooleanValue(hTLStreamNodeMap_, "StreamAutoNegotiatePacketSize", true);
            }
            if (nm::isWritable(hTLStreamNodeMap_, "StreamPacketResendEnable"))
            {
                nm::setBooleanValue(hTLStreamNodeMap_, "StreamPacketResendEnable", true);
            }

            AC_ERROR err = acDeviceStartStream(hDevice_);
            if (err != AC_ERR_SUCCESS)
            {
                std::stringstream ssError;
                ssError << __PRETTY_FUNCTION__;
                ssError << ": unable to start Arena stream, error=" << err;
                throw RuntimeError(ERROR_ARENA_START_CAPTURE, ssError.str());
            }
            capturing_ = true;
        }
    }


    void CameraDevice_arena::stopCapture()
    {
        if (capturing_)
        {
            // Requeue any buffer we are still holding before stopping.
            requeueBuffer();

            AC_ERROR err = acDeviceStopStream(hDevice_);
            if (err != AC_ERR_SUCCESS)
            {
                std::stringstream ssError;
                ssError << __PRETTY_FUNCTION__;
                ssError << ": unable to stop Arena stream, error=" << err;
                throw RuntimeError(ERROR_ARENA_STOP_CAPTURE, ssError.str());
            }
            capturing_ = false;
        }
    }


    cv::Mat CameraDevice_arena::grabImage()
    {
        cv::Mat image;
        grabImage(image);
        return image;
    }


    void CameraDevice_arena::grabImage(cv::Mat &image)
    {
        std::string errMsg;
        bool ok = grabImageCommon(errMsg);
        if (!ok)
        {
            image.release();
            return;
        }

        // Convert into an OpenCV-friendly layout (Mono8 / Mono16 / BGR8 / BGR16)
        uint64_t srcFormat = 0;
        AC_ERROR err = acImageGetPixelFormat(hBuffer_, &srcFormat);
        if (err != AC_ERR_SUCCESS)
        {
            requeueBuffer();
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena image pixel format, error=" << err;
            throw RuntimeError(ERROR_ARENA_IMAGE_GET_PIXEL_FORMAT, ssError.str());
        }

        uint64_t dstFormat = getSuitablePixelFormat_arena(srcFormat);

        acBuffer hConv = nullptr;
        err = acImageFactoryConvert(hBuffer_, dstFormat, &hConv);
        if (err != AC_ERR_SUCCESS)
        {
            requeueBuffer();
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to convert Arena image, error=" << err;
            throw RuntimeError(ERROR_ARENA_IMAGE_CONVERT, ssError.str());
        }

        size_t width = 0;
        size_t height = 0;
        uint8_t *pData = nullptr;
        acImageGetWidth(hConv, &width);
        acImageGetHeight(hConv, &height);
        acImageGetData(hConv, &pData);

        int opencvFormat = getCompatibleOpencvFormat_arena(dstFormat);

        // The image factory produces a tightly-packed (unpadded) buffer, so the
        // default row stride is correct here.
        cv::Mat imageTmp = cv::Mat(
                (int)(height),
                (int)(width),
                opencvFormat,
                pData
                );
        imageTmp.copyTo(image);

        // Destroy the converted image and requeue the original device buffer.
        err = acImageFactoryDestroy(hConv);
        if (err != AC_ERR_SUCCESS)
        {
            requeueBuffer();
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to destroy converted Arena image, error=" << err;
            throw RuntimeError(ERROR_ARENA_IMAGE_DESTROY, ssError.str());
        }

        requeueBuffer();
    }


    bool CameraDevice_arena::grabImageCommon(std::string &errMsg)
    {
        if (!capturing_)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to grab image - not capturing";
            errMsg = ssError.str();
            return false;
        }

        // Requeue any previously held buffer (defensive - grabImage requeues).
        requeueBuffer();

        imageOK_ = false;

        AC_ERROR err = acDeviceGetBuffer(hDevice_, GetBufferTimeout, &hBuffer_);
        if (err != AC_ERR_SUCCESS)
        {
            hBuffer_ = nullptr;
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena image buffer, error=" << err;
            errMsg = ssError.str();
            return false;
        }

        // Check for incomplete image
        bool8_t isIncomplete = 0;
        err = acBufferIsIncomplete(hBuffer_, &isIncomplete);
        if (err != AC_ERR_SUCCESS)
        {
            requeueBuffer();
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to determine if Arena image is complete, error=" << err;
            errMsg = ssError.str();
            return false;
        }

        if (isIncomplete != 0)
        {
            requeueBuffer();
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": Arena image is incomplete";
            errMsg = ssError.str();
            return false;
        }

        imageOK_ = true;
        updateTimeStamp();
        return true;
    }


    void CameraDevice_arena::requeueBuffer()
    {
        if (hBuffer_ != nullptr)
        {
            AC_ERROR err = acDeviceRequeueBuffer(hDevice_, hBuffer_);
            hBuffer_ = nullptr;
            if (err != AC_ERR_SUCCESS)
            {
                std::stringstream ssError;
                ssError << __PRETTY_FUNCTION__;
                ssError << ": unable to requeue Arena buffer, error=" << err;
                throw RuntimeError(ERROR_ARENA_REQUEUE_BUFFER, ssError.str());
            }
        }
    }


    void CameraDevice_arena::updateTimeStamp()
    {
        timeStamp_ns_ = 0;
        timeStamp_.seconds = 0;
        timeStamp_.microSeconds = 0;

        uint64_t ts = 0;
        AC_ERROR err = acImageGetTimestampNs(hBuffer_, &ts);
        if (err == AC_ERR_SUCCESS)
        {
            timeStamp_ns_ = (int64_t)(ts);
            timeStamp_.seconds = (unsigned long long)(ts / 1000000000ULL);
            timeStamp_.microSeconds = (unsigned int)((ts % 1000000000ULL) / 1000ULL);
        }
    }


    bool CameraDevice_arena::isColor()
    {
        if (nm::isReadable(hNodeMap_, "PixelFormat"))
        {
            PixelFormatList formatList = getListOfSupportedPixelFormats(IMAGEMODE_0);
            // getListOfSupportedPixelFormats maps color formats to RAW8/RGB/BGR.
            for (auto fmt : formatList)
            {
                if (fmt == PIXEL_FORMAT_RAW8 || fmt == PIXEL_FORMAT_RGB8 ||
                    fmt == PIXEL_FORMAT_BGR8 || fmt == PIXEL_FORMAT_RGB16 ||
                    fmt == PIXEL_FORMAT_BGR16 || fmt == PIXEL_FORMAT_422YUV8)
                {
                    return true;
                }
            }
        }
        return false;
    }


    VideoMode CameraDevice_arena::getVideoMode()
    {
        return VIDEOMODE_FORMAT7;
    }


    FrameRate CameraDevice_arena::getFrameRate()
    {
        return FRAMERATE_FORMAT7;
    }


    ImageMode CameraDevice_arena::getImageMode()
    {
        return IMAGEMODE_0;
    }


    VideoModeList CameraDevice_arena::getAllowedVideoModes()
    {
        // Arena (GenICam) cameras do not have IIDC-style video modes, so we
        // present a single Format7-style mode as the other backends do.
        VideoModeList allowedVideoModes = {VIDEOMODE_FORMAT7};
        return allowedVideoModes;
    }


    FrameRateList CameraDevice_arena::getAllowedFrameRates(VideoMode vidMode)
    {
        FrameRateList allowedFrameRates = {};
        if (vidMode == VIDEOMODE_FORMAT7)
        {
            allowedFrameRates.push_back(FRAMERATE_FORMAT7);
        }
        return allowedFrameRates;
    }


    ImageModeList CameraDevice_arena::getAllowedImageModes()
    {
        ImageModeList allImageModes = {IMAGEMODE_0};
        return allImageModes;
    }


    bool CameraDevice_arena::isSupported(VideoMode vidMode, FrameRate frmRate)
    {
        VideoModeList allowedVideoModes = getAllowedVideoModes();
        FrameRateList allowedFrameRates = getAllowedFrameRates(vidMode);
        bool videoModeFound = (std::find(allowedVideoModes.begin(), allowedVideoModes.end(), vidMode) != allowedVideoModes.end());
        bool frameRateFound = (std::find(allowedFrameRates.begin(), allowedFrameRates.end(), frmRate) != allowedFrameRates.end());
        return (videoModeFound && frameRateFound);
    }


    bool CameraDevice_arena::isSupported(ImageMode imgMode)
    {
        ImageModeList allowedModes = getAllowedImageModes();
        return (std::find(allowedModes.begin(), allowedModes.end(), imgMode) != allowedModes.end());
    }


    unsigned int CameraDevice_arena::getNumberOfImageMode()
    {
        return getAllowedImageModes().size();
    }


    // Property methods
    // -----------------------------------------------------------------------

    PropertyInfo CameraDevice_arena::getPropertyInfo(PropertyType propType)
    {
        PropertyInfo propInfo;
        propInfo.type = propType;

        if (isArenaSupportedPropertyType(propType))
        {
            if (getPropertyInfoDispatchMap_.count(propType) > 0)
            {
                propInfo = getPropertyInfoDispatchMap_[propType](this);
            }
        }
        return propInfo;
    }


    Property CameraDevice_arena::getProperty(PropertyType propType)
    {
        Property prop;
        prop.type = propType;

        if (isArenaSupportedPropertyType(propType))
        {
            if (getPropertyDispatchMap_.count(propType) > 0)
            {
                prop = getPropertyDispatchMap_[propType](this);
            }
        }
        return prop;
    }


    void CameraDevice_arena::setProperty(Property prop)
    {
        std::string settableMsg("");
        bool isSettable = isPropertySettable(prop.type, settableMsg);
        if (!isSettable)
        {
            throw RuntimeError(ERROR_ARENA_PROPERTY_NOT_SETTABLE, settableMsg);
        }
        setPropertyDispatchMap_[prop.type](this, prop);
    }


    bool CameraDevice_arena::isPropertySettable(PropertyType propType, std::string &msg)
    {
        if (!isArenaSupportedPropertyType(propType))
        {
            msg = std::string("PropertyType is not supported by Arena Backend");
            return false;
        }
        if (setPropertyDispatchMap_.count(propType) <= 0)
        {
            msg = std::string("PropertyType is not in setter dispatch map");
            return false;
        }
        PropertyInfo propInfo = getPropertyInfo(propType);
        if (!propInfo.present)
        {
            msg = std::string("PropertyType is not present");
            return false;
        }
        return true;
    }


    // PropertyInfo methods
    // -----------------------------------------------------------------------

    PropertyInfo CameraDevice_arena::getPropertyInfoBrightness()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_BRIGHTNESS;
        propInfo.present = nm::isAvailable(hNodeMap_, "BlackLevel");
        if (propInfo.present)
        {
            propInfo.autoCapable = false;
            propInfo.manualCapable = true;
            propInfo.absoluteCapable = true;
            propInfo.onePushCapable = false;
            propInfo.onOffCapable = false;
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "BlackLevel"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "BlackLevel"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "BlackLevel"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "BlackLevel");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoGamma()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_GAMMA;
        propInfo.present = nm::isAvailable(hNodeMap_, "Gamma");
        if (propInfo.present)
        {
            propInfo.autoCapable = false;
            propInfo.manualCapable = true;
            propInfo.absoluteCapable = true;
            propInfo.onePushCapable = false;
            propInfo.onOffCapable = nm::isAvailable(hNodeMap_, "GammaEnable");
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "Gamma"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "Gamma"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "Gamma"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "Gamma");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoShutter()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_SHUTTER;
        propInfo.present = nm::isAvailable(hNodeMap_, "ExposureTime");
        if (propInfo.present)
        {
            if (nm::isReadable(hNodeMap_, "ExposureAuto"))
            {
                propInfo.autoCapable = nm::hasEnumEntry(hNodeMap_, "ExposureAuto", "Continuous");
                propInfo.manualCapable = nm::hasEnumEntry(hNodeMap_, "ExposureAuto", "Off");
                propInfo.onePushCapable = nm::hasEnumEntry(hNodeMap_, "ExposureAuto", "Once");
            }
            propInfo.absoluteCapable = true;
            propInfo.onOffCapable = false;
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "ExposureTime"))
            {
                double minVal = std::max(nm::getFloatMin(hNodeMap_, "ExposureTime"), MinAllowedShutterUs);
                double maxVal = std::min(nm::getFloatMax(hNodeMap_, "ExposureTime"), MaxAllowedShutterUs);
                propInfo.minAbsoluteValue = (float)(minVal);
                propInfo.maxAbsoluteValue = (float)(maxVal);
                propInfo.minValue = (unsigned int)(minVal);
                propInfo.maxValue = (unsigned int)(maxVal);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "ExposureTime");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoGain()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_GAIN;
        propInfo.present = nm::isAvailable(hNodeMap_, "Gain");
        if (propInfo.present)
        {
            if (nm::isReadable(hNodeMap_, "GainAuto"))
            {
                propInfo.autoCapable = nm::hasEnumEntry(hNodeMap_, "GainAuto", "Continuous");
                propInfo.manualCapable = nm::hasEnumEntry(hNodeMap_, "GainAuto", "Off");
                propInfo.onePushCapable = nm::hasEnumEntry(hNodeMap_, "GainAuto", "Once");
            }
            propInfo.absoluteCapable = true;
            propInfo.onOffCapable = false;
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "Gain"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "Gain"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "Gain"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "Gain");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoTriggerDelay()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_TRIGGER_DELAY;
        propInfo.present = nm::isAvailable(hNodeMap_, "TriggerDelay");
        if (propInfo.present)
        {
            propInfo.autoCapable = false;
            propInfo.manualCapable = true;
            propInfo.absoluteCapable = true;
            propInfo.onePushCapable = false;
            propInfo.onOffCapable = false;
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "TriggerDelay"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "TriggerDelay"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "TriggerDelay"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "TriggerDelay");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoFrameRate()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_FRAME_RATE;
        propInfo.present = nm::isAvailable(hNodeMap_, "AcquisitionFrameRate");
        if (propInfo.present)
        {
            propInfo.autoCapable = false;
            propInfo.manualCapable = true;
            propInfo.absoluteCapable = true;
            propInfo.onePushCapable = false;
            propInfo.onOffCapable = nm::isAvailable(hNodeMap_, "AcquisitionFrameRateEnable");
            propInfo.readOutCapable = false;
            if (nm::isReadable(hNodeMap_, "AcquisitionFrameRate"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "AcquisitionFrameRate"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "AcquisitionFrameRate"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "AcquisitionFrameRate");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoTemperature()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_TEMPERATURE;
        propInfo.present = nm::isAvailable(hNodeMap_, "DeviceTemperature");
        if (propInfo.present)
        {
            propInfo.autoCapable = false;
            propInfo.manualCapable = false;
            propInfo.absoluteCapable = true;
            propInfo.onePushCapable = false;
            propInfo.onOffCapable = false;
            propInfo.readOutCapable = true;
            if (nm::isReadable(hNodeMap_, "DeviceTemperature"))
            {
                propInfo.minAbsoluteValue = (float)(nm::getFloatMin(hNodeMap_, "DeviceTemperature"));
                propInfo.maxAbsoluteValue = (float)(nm::getFloatMax(hNodeMap_, "DeviceTemperature"));
                propInfo.minValue = (unsigned int)(propInfo.minAbsoluteValue);
                propInfo.maxValue = (unsigned int)(propInfo.maxAbsoluteValue);
                propInfo.units = nm::getFloatUnit(hNodeMap_, "DeviceTemperature");
                propInfo.unitsAbbr = propInfo.units;
                propInfo.haveUnits = !propInfo.units.empty();
            }
        }
        return propInfo;
    }


    PropertyInfo CameraDevice_arena::getPropertyInfoTriggerMode()
    {
        PropertyInfo propInfo;
        propInfo.type = PROPERTY_TYPE_TRIGGER_MODE;
        propInfo.present = true;
        return propInfo;
    }


    // Property get methods
    // -----------------------------------------------------------------------

    Property CameraDevice_arena::getPropertyBrightness()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_BRIGHTNESS;
        prop.present = nm::isAvailable(hNodeMap_, "BlackLevel");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            if (nm::isReadable(hNodeMap_, "BlackLevel"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "BlackLevel"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyGamma()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_GAMMA;
        prop.present = nm::isAvailable(hNodeMap_, "Gamma");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            prop.on = true;
            if (nm::isReadable(hNodeMap_, "GammaEnable"))
            {
                prop.on = nm::getBooleanValue(hNodeMap_, "GammaEnable");
            }
            if (nm::isReadable(hNodeMap_, "Gamma"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "Gamma"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyShutter()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_SHUTTER;
        prop.present = nm::isAvailable(hNodeMap_, "ExposureTime");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            if (nm::isReadable(hNodeMap_, "ExposureAuto"))
            {
                prop.autoActive = (nm::getEnumValue(hNodeMap_, "ExposureAuto") == std::string("Continuous"));
            }
            if (nm::isReadable(hNodeMap_, "ExposureTime"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "ExposureTime"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyGain()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_GAIN;
        prop.present = nm::isAvailable(hNodeMap_, "Gain");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            if (nm::isReadable(hNodeMap_, "GainAuto"))
            {
                prop.autoActive = (nm::getEnumValue(hNodeMap_, "GainAuto") == std::string("Continuous"));
            }
            if (nm::isReadable(hNodeMap_, "Gain"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "Gain"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyTriggerDelay()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_TRIGGER_DELAY;
        prop.present = nm::isAvailable(hNodeMap_, "TriggerDelay");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            if (nm::isReadable(hNodeMap_, "TriggerDelay"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "TriggerDelay"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyFrameRate()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_FRAME_RATE;
        prop.present = nm::isAvailable(hNodeMap_, "AcquisitionFrameRate");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            prop.on = true;
            if (nm::isReadable(hNodeMap_, "AcquisitionFrameRateEnable"))
            {
                prop.on = nm::getBooleanValue(hNodeMap_, "AcquisitionFrameRateEnable");
            }
            if (nm::isReadable(hNodeMap_, "AcquisitionFrameRate"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "AcquisitionFrameRate"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyTemperature()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_TEMPERATURE;
        prop.present = nm::isAvailable(hNodeMap_, "DeviceTemperature");
        if (prop.present)
        {
            prop.absoluteControl = true;
            prop.onePush = false;
            prop.autoActive = false;
            if (nm::isReadable(hNodeMap_, "DeviceTemperature"))
            {
                prop.absoluteValue = (float)(nm::getFloatValue(hNodeMap_, "DeviceTemperature"));
                prop.value = (unsigned int)(prop.absoluteValue);
            }
        }
        return prop;
    }


    Property CameraDevice_arena::getPropertyTriggerMode()
    {
        Property prop;
        prop.type = PROPERTY_TYPE_TRIGGER_MODE;
        prop.present = true;
        return prop;
    }


    // Property set methods
    // -----------------------------------------------------------------------

    void CameraDevice_arena::setPropertyBrightness(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "BlackLevel"))
        {
            nm::setFloatValue(hNodeMap_, "BlackLevel", (double)(prop.absoluteValue));
        }
    }


    void CameraDevice_arena::setPropertyGamma(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "GammaEnable"))
        {
            nm::setBooleanValue(hNodeMap_, "GammaEnable", prop.on);
        }
        if (nm::isWritable(hNodeMap_, "Gamma"))
        {
            nm::setFloatValue(hNodeMap_, "Gamma", (double)(prop.absoluteValue));
        }
    }


    void CameraDevice_arena::setPropertyShutter(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "ExposureAuto"))
        {
            if (prop.autoActive && nm::hasEnumEntry(hNodeMap_, "ExposureAuto", "Continuous"))
            {
                nm::setEnumValue(hNodeMap_, "ExposureAuto", "Continuous");
                return;
            }
            else
            {
                nm::setEnumValue(hNodeMap_, "ExposureAuto", "Off");
            }
        }
        if (nm::isWritable(hNodeMap_, "ExposureTime"))
        {
            double value = (double)(prop.absoluteValue);
            value = std::max(value, MinAllowedShutterUs);
            value = std::min(value, MaxAllowedShutterUs);
            double nodeMin = nm::getFloatMin(hNodeMap_, "ExposureTime");
            double nodeMax = nm::getFloatMax(hNodeMap_, "ExposureTime");
            value = std::max(value, nodeMin);
            value = std::min(value, nodeMax);
            nm::setFloatValue(hNodeMap_, "ExposureTime", value);
        }
    }


    void CameraDevice_arena::setPropertyGain(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "GainAuto"))
        {
            if (prop.autoActive && nm::hasEnumEntry(hNodeMap_, "GainAuto", "Continuous"))
            {
                nm::setEnumValue(hNodeMap_, "GainAuto", "Continuous");
                return;
            }
            else
            {
                nm::setEnumValue(hNodeMap_, "GainAuto", "Off");
            }
        }
        if (nm::isWritable(hNodeMap_, "Gain"))
        {
            double value = (double)(prop.absoluteValue);
            double nodeMin = nm::getFloatMin(hNodeMap_, "Gain");
            double nodeMax = nm::getFloatMax(hNodeMap_, "Gain");
            value = std::max(value, nodeMin);
            value = std::min(value, nodeMax);
            nm::setFloatValue(hNodeMap_, "Gain", value);
        }
    }


    void CameraDevice_arena::setPropertyTriggerDelay(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "TriggerDelay"))
        {
            nm::setFloatValue(hNodeMap_, "TriggerDelay", (double)(prop.absoluteValue));
        }
    }


    void CameraDevice_arena::setPropertyFrameRate(Property prop)
    {
        if (nm::isWritable(hNodeMap_, "AcquisitionFrameRateEnable"))
        {
            nm::setBooleanValue(hNodeMap_, "AcquisitionFrameRateEnable", true);
        }
        if (nm::isWritable(hNodeMap_, "AcquisitionFrameRate"))
        {
            double value = (double)(prop.absoluteValue);
            double nodeMin = nm::getFloatMin(hNodeMap_, "AcquisitionFrameRate");
            double nodeMax = nm::getFloatMax(hNodeMap_, "AcquisitionFrameRate");
            value = std::max(value, nodeMin);
            value = std::min(value, nodeMax);
            nm::setFloatValue(hNodeMap_, "AcquisitionFrameRate", value);
        }
    }


    void CameraDevice_arena::setPropertyTemperature(Property prop)
    {
        // Read-only property - do nothing.
    }


    void CameraDevice_arena::setPropertyTriggerMode(Property prop)
    {
        // Handled via setTriggerInternal / setTriggerExternal - do nothing.
    }


    // Format7 methods
    // -----------------------------------------------------------------------

    Format7Settings CameraDevice_arena::getFormat7Settings()
    {
        Format7Settings settings;
        settings.mode = getImageMode();
        if (nm::isReadable(hNodeMap_, "OffsetX"))
        {
            settings.offsetX = (unsigned int)(nm::getIntegerValue(hNodeMap_, "OffsetX"));
        }
        if (nm::isReadable(hNodeMap_, "OffsetY"))
        {
            settings.offsetY = (unsigned int)(nm::getIntegerValue(hNodeMap_, "OffsetY"));
        }
        if (nm::isReadable(hNodeMap_, "Width"))
        {
            settings.width = (unsigned int)(nm::getIntegerValue(hNodeMap_, "Width"));
        }
        if (nm::isReadable(hNodeMap_, "Height"))
        {
            settings.height = (unsigned int)(nm::getIntegerValue(hNodeMap_, "Height"));
        }
        settings.pixelFormat = getPixelFormat();
        return settings;
    }


    Format7Info CameraDevice_arena::getFormat7Info(ImageMode imgMode)
    {
        Format7Info format7Info;
        format7Info.mode = imgMode;
        format7Info.supported = isSupported(imgMode);
        if (format7Info.supported)
        {
            if (nm::isReadable(hNodeMap_, "Width"))
            {
                format7Info.maxWidth = (unsigned int)(nm::getIntegerMax(hNodeMap_, "Width"));
                format7Info.imageHStepSize = (unsigned int)(nm::getIntegerInc(hNodeMap_, "Width"));
            }
            if (nm::isReadable(hNodeMap_, "Height"))
            {
                format7Info.maxHeight = (unsigned int)(nm::getIntegerMax(hNodeMap_, "Height"));
                format7Info.imageVStepSize = (unsigned int)(nm::getIntegerInc(hNodeMap_, "Height"));
            }
            if (nm::isReadable(hNodeMap_, "OffsetX"))
            {
                format7Info.offsetHStepSize = (unsigned int)(nm::getIntegerInc(hNodeMap_, "OffsetX"));
            }
            if (nm::isReadable(hNodeMap_, "OffsetY"))
            {
                format7Info.offsetVStepSize = (unsigned int)(nm::getIntegerInc(hNodeMap_, "OffsetY"));
            }
        }
        return format7Info;
    }


    bool CameraDevice_arena::validateFormat7Settings(Format7Settings settings)
    {
        bool ok = true;
        if (!isSupported(settings.mode))
        {
            ok = false;
        }
        Format7Info info = getFormat7Info(settings.mode);
        if ((settings.width + settings.offsetX) > info.maxWidth) { ok = false; }
        if ((settings.height + settings.offsetY) > info.maxHeight) { ok = false; }
        if (info.imageHStepSize != 0 && settings.width % info.imageHStepSize != 0) { ok = false; }
        if (info.imageVStepSize != 0 && settings.height % info.imageVStepSize != 0) { ok = false; }
        if (info.offsetHStepSize != 0 && settings.offsetX % info.offsetHStepSize != 0) { ok = false; }
        if (info.offsetVStepSize != 0 && settings.offsetY % info.offsetVStepSize != 0) { ok = false; }
        return ok;
    }


    void CameraDevice_arena::setFormat7Configuration(Format7Settings settings, float percentSpeed)
    {
        bool ok = validateFormat7Settings(settings);
        if (!ok)
        {
            return;
        }

        // Note: pixel-format changes via Format7 are deferred (the GenICam
        // symbolic naming differs from the BIAS enum); only the ROI geometry
        // is applied here. The current pixel format is left unchanged.
        if (nm::isWritable(hNodeMap_, "Width"))
        {
            nm::setIntegerValue(hNodeMap_, "Width", (int64_t)(settings.width));
        }
        if (nm::isWritable(hNodeMap_, "Height"))
        {
            nm::setIntegerValue(hNodeMap_, "Height", (int64_t)(settings.height));
        }
        if (nm::isWritable(hNodeMap_, "OffsetX"))
        {
            nm::setIntegerValue(hNodeMap_, "OffsetX", (int64_t)(settings.offsetX));
        }
        if (nm::isWritable(hNodeMap_, "OffsetY"))
        {
            nm::setIntegerValue(hNodeMap_, "OffsetY", (int64_t)(settings.offsetY));
        }
    }


    PixelFormat CameraDevice_arena::getPixelFormat()
    {
        // Read the current PixelFormat entry's PFNC integer value: get the
        // current symbolic, then look up that entry and read its int value.
        if (nm::isReadable(hNodeMap_, "PixelFormat"))
        {
            acNode hNode = nullptr;
            AC_ERROR err = acNodeMapGetNode(hNodeMap_, "PixelFormat", &hNode);
            if (err == AC_ERR_SUCCESS && hNode != nullptr)
            {
                std::string symbolic = nm::getEnumValue(hNodeMap_, "PixelFormat");
                acNode hEntry = nullptr;
                if (acEnumerationGetEntryByName(hNode, symbolic.c_str(), &hEntry) == AC_ERR_SUCCESS
                        && hEntry != nullptr)
                {
                    int64_t pfncValue = 0;
                    if (acEnumEntryGetIntValue(hEntry, &pfncValue) == AC_ERR_SUCCESS)
                    {
                        return convertPixelFormat_from_arena((uint64_t)(pfncValue));
                    }
                }
            }
        }
        return PIXEL_FORMAT_MONO8;
    }


    bool CameraDevice_arena::isSupportedPixelFormat(PixelFormat pixelFormat, ImageMode imgMode)
    {
        PixelFormatList formatList = getListOfSupportedPixelFormats(imgMode);
        return std::find(std::begin(formatList), std::end(formatList), pixelFormat) != std::end(formatList);
    }


    PixelFormatList CameraDevice_arena::getListOfSupportedPixelFormats(ImageMode imgMode)
    {
        PixelFormatList pixelFormatList;
        if (!isSupported(imgMode))
        {
            return pixelFormatList;
        }
        if (!nm::isReadable(hNodeMap_, "PixelFormat"))
        {
            return pixelFormatList;
        }

        try
        {
            acNode hNode = nullptr;
            AC_ERROR err = acNodeMapGetNode(hNodeMap_, "PixelFormat", &hNode);
            if (err != AC_ERR_SUCCESS || hNode == nullptr)
            {
                return pixelFormatList;
            }

            size_t numSymbolics = 0;
            if (acEnumerationGetNumSymbolics(hNode, &numSymbolics) != AC_ERR_SUCCESS)
            {
                return pixelFormatList;
            }

            for (size_t i=0; i<numSymbolics; i++)
            {
                acNode hEntry = nullptr;
                if (acEnumerationGetEntryByIndex(hNode, i, &hEntry) != AC_ERR_SUCCESS || hEntry == nullptr)
                {
                    continue;
                }
                int64_t pfncValue = 0;
                if (acEnumEntryGetIntValue(hEntry, &pfncValue) != AC_ERR_SUCCESS)
                {
                    continue;
                }
                PixelFormat biasFormat = convertPixelFormat_from_arena((uint64_t)(pfncValue));
                if (std::find(pixelFormatList.begin(), pixelFormatList.end(), biasFormat) == pixelFormatList.end())
                {
                    pixelFormatList.push_back(biasFormat);
                }
            }
        }
        catch (RuntimeError &err)
        {
            // Degrade gracefully - return whatever was collected.
        }

        return pixelFormatList;
    }


    // Trigger methods
    // -----------------------------------------------------------------------

    void CameraDevice_arena::setTriggerInternal()
    {
        if (nm::isWritable(hNodeMap_, "TriggerMode"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerMode", "Off");
        }
        triggerType_ = TRIGGER_INTERNAL;
    }


    void CameraDevice_arena::setTriggerExternal()
    {
        if (nm::isWritable(hNodeMap_, "TriggerSelector"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerSelector", "FrameStart");
        }
        if (nm::isWritable(hNodeMap_, "TriggerSource"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerSource", "Line0");
        }
        if (nm::isWritable(hNodeMap_, "TriggerActivation"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerActivation", "RisingEdge");
        }
        if (nm::isWritable(hNodeMap_, "TriggerMode"))
        {
            nm::setEnumValue(hNodeMap_, "TriggerMode", "On");
        }
        else
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": TriggerMode node is not writable";
            throw RuntimeError(ERROR_ARENA_SET_TRIGGER_EXTERNAL, ssError.str());
        }
        triggerType_ = TRIGGER_EXTERNAL;
    }


    TriggerType CameraDevice_arena::getTriggerType()
    {
        if (nm::isReadable(hNodeMap_, "TriggerMode"))
        {
            std::string modeSymb = nm::getEnumValue(hNodeMap_, "TriggerMode");
            return (modeSymb == std::string("On")) ? TRIGGER_EXTERNAL : TRIGGER_INTERNAL;
        }
        return TRIGGER_INTERNAL;
    }


    // Info methods
    // -----------------------------------------------------------------------

    std::string CameraDevice_arena::getVendorName()
    {
        return cameraInfo_.vendorName();
    }


    std::string CameraDevice_arena::getModelName()
    {
        return cameraInfo_.modelName();
    }


    TimeStamp CameraDevice_arena::getImageTimeStamp()
    {
        return timeStamp_;
    }


    std::string CameraDevice_arena::toString()
    {
        return cameraInfo_.toString();
    }


    void CameraDevice_arena::printGuid()
    {
        guid_.printValue();
    }


    void CameraDevice_arena::printInfo()
    {
        std::cout << toString();
    }


    // Static property dispatch maps
    // -----------------------------------------------------------------------

    std::map<PropertyType, std::function<PropertyInfo(CameraDevice_arena*)>>
        CameraDevice_arena::getPropertyInfoDispatchMap_ =
    {
        {PROPERTY_TYPE_BRIGHTNESS,    &CameraDevice_arena::getPropertyInfoBrightness},
        {PROPERTY_TYPE_GAMMA,         &CameraDevice_arena::getPropertyInfoGamma},
        {PROPERTY_TYPE_SHUTTER,       &CameraDevice_arena::getPropertyInfoShutter},
        {PROPERTY_TYPE_GAIN,          &CameraDevice_arena::getPropertyInfoGain},
        {PROPERTY_TYPE_TRIGGER_DELAY, &CameraDevice_arena::getPropertyInfoTriggerDelay},
        {PROPERTY_TYPE_FRAME_RATE,    &CameraDevice_arena::getPropertyInfoFrameRate},
        {PROPERTY_TYPE_TEMPERATURE,   &CameraDevice_arena::getPropertyInfoTemperature},
        {PROPERTY_TYPE_TRIGGER_MODE,  &CameraDevice_arena::getPropertyInfoTriggerMode},
    };


    std::map<PropertyType, std::function<Property(CameraDevice_arena*)>>
        CameraDevice_arena::getPropertyDispatchMap_ =
    {
        {PROPERTY_TYPE_BRIGHTNESS,    &CameraDevice_arena::getPropertyBrightness},
        {PROPERTY_TYPE_GAMMA,         &CameraDevice_arena::getPropertyGamma},
        {PROPERTY_TYPE_SHUTTER,       &CameraDevice_arena::getPropertyShutter},
        {PROPERTY_TYPE_GAIN,          &CameraDevice_arena::getPropertyGain},
        {PROPERTY_TYPE_TRIGGER_DELAY, &CameraDevice_arena::getPropertyTriggerDelay},
        {PROPERTY_TYPE_FRAME_RATE,    &CameraDevice_arena::getPropertyFrameRate},
        {PROPERTY_TYPE_TEMPERATURE,   &CameraDevice_arena::getPropertyTemperature},
        {PROPERTY_TYPE_TRIGGER_MODE,  &CameraDevice_arena::getPropertyTriggerMode},
    };


    std::map<PropertyType, std::function<void(CameraDevice_arena*,Property)>>
        CameraDevice_arena::setPropertyDispatchMap_ =
    {
        {PROPERTY_TYPE_BRIGHTNESS,    &CameraDevice_arena::setPropertyBrightness},
        {PROPERTY_TYPE_GAMMA,         &CameraDevice_arena::setPropertyGamma},
        {PROPERTY_TYPE_SHUTTER,       &CameraDevice_arena::setPropertyShutter},
        {PROPERTY_TYPE_GAIN,          &CameraDevice_arena::setPropertyGain},
        {PROPERTY_TYPE_TRIGGER_DELAY, &CameraDevice_arena::setPropertyTriggerDelay},
        {PROPERTY_TYPE_FRAME_RATE,    &CameraDevice_arena::setPropertyFrameRate},
        {PROPERTY_TYPE_TEMPERATURE,   &CameraDevice_arena::setPropertyTemperature},
        {PROPERTY_TYPE_TRIGGER_MODE,  &CameraDevice_arena::setPropertyTriggerMode},
    };

}

#endif // #ifdef WITH_ARENA
