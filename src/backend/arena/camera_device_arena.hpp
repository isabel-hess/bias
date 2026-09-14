#ifdef WITH_ARENA
#ifndef BIAS_CAMERA_DEVICE_ARENA_HPP
#define BIAS_CAMERA_DEVICE_ARENA_HPP

#include <map>
#include <string>
#include <functional>
#include <cstdint>
#include <opencv2/core/core.hpp>

#include "camera_device.hpp"
#include "camera_info_arena.hpp"
#include "property.hpp"
#include "ArenaCApi.h"

namespace bias {

    class CameraDevice_arena : public CameraDevice
    {
        public:

            CameraDevice_arena();
            explicit CameraDevice_arena(Guid guid);
            virtual ~CameraDevice_arena();

            virtual CameraLib getCameraLib();

            virtual void connect();
            virtual void disconnect();

            virtual void startCapture();
            virtual void stopCapture();

            virtual cv::Mat grabImage();
            virtual void grabImage(cv::Mat &image);

            virtual bool isColor();

            virtual bool isSupported(VideoMode vidMode, FrameRate frmRate);
            virtual bool isSupported(ImageMode imgMode);
            virtual unsigned int getNumberOfImageMode();

            virtual VideoMode getVideoMode();
            virtual FrameRate getFrameRate();
            virtual ImageMode getImageMode();

            virtual VideoModeList getAllowedVideoModes();
            virtual FrameRateList getAllowedFrameRates(VideoMode vidMode);
            virtual ImageModeList getAllowedImageModes();

            virtual PropertyInfo getPropertyInfo(PropertyType propType);
            virtual Property getProperty(PropertyType propType);
            virtual void setProperty(Property prop);

            virtual Format7Settings getFormat7Settings();
            virtual Format7Info getFormat7Info(ImageMode imgMode);

            virtual bool validateFormat7Settings(Format7Settings settings);
            virtual void setFormat7Configuration(Format7Settings settings, float percentSpeed);

            virtual PixelFormatList getListOfSupportedPixelFormats(ImageMode imgMode);

            virtual void setTriggerInternal();
            virtual void setTriggerExternal();
            virtual TriggerType getTriggerType();

            virtual std::string getVendorName();
            virtual std::string getModelName();

            virtual TimeStamp getImageTimeStamp();

            virtual std::string toString();

            virtual void printGuid();
            virtual void printInfo();

            // Constants
            // --------------------------------------------------------
            // Artificial shutter limits to make the GUI more usable.
            static constexpr double MinAllowedShutterUs = 100.0;
            static constexpr double MaxAllowedShutterUs = 200000.0;

            // Timeout (ms) when waiting for the next image buffer.
            static constexpr uint64_t GetBufferTimeout = 5000;

        private:

            acSystem hSystem_ = nullptr;
            acDevice hDevice_ = nullptr;

            acNodeMap hNodeMap_ = nullptr;         // camera / device features
            acNodeMap hTLStreamNodeMap_ = nullptr; // stream engine
            acNodeMap hTLDeviceNodeMap_ = nullptr; // transport-layer device

            acBuffer hBuffer_ = nullptr;           // currently held image buffer

            CameraInfo_arena cameraInfo_;

            TimeStamp timeStamp_ = {0,0};
            int64_t timeStamp_ns_ = 0;

            bool imageOK_ = false;

            TriggerType triggerType_ = TRIGGER_TYPE_UNSPECIFIED;

            bool grabImageCommon(std::string &errMsg);
            void requeueBuffer();
            void updateTimeStamp();

            PixelFormat getPixelFormat();
            bool isSupportedPixelFormat(PixelFormat pixelFormat, ImageMode imgMode);

            // Get PropertyInfo methods
            static std::map<PropertyType, std::function<PropertyInfo(CameraDevice_arena*)>> getPropertyInfoDispatchMap_;

            PropertyInfo getPropertyInfoBrightness();
            PropertyInfo getPropertyInfoGamma();
            PropertyInfo getPropertyInfoShutter();
            PropertyInfo getPropertyInfoGain();
            PropertyInfo getPropertyInfoTriggerDelay();
            PropertyInfo getPropertyInfoFrameRate();
            PropertyInfo getPropertyInfoTemperature();
            PropertyInfo getPropertyInfoTriggerMode();

            // Get Property methods
            static std::map<PropertyType, std::function<Property(CameraDevice_arena*)>> getPropertyDispatchMap_;

            Property getPropertyBrightness();
            Property getPropertyGamma();
            Property getPropertyShutter();
            Property getPropertyGain();
            Property getPropertyTriggerDelay();
            Property getPropertyFrameRate();
            Property getPropertyTemperature();
            Property getPropertyTriggerMode();

            // Set Property methods
            bool isPropertySettable(PropertyType propType, std::string &msg);
            static std::map<PropertyType, std::function<void(CameraDevice_arena*,Property)>> setPropertyDispatchMap_;

            void setPropertyBrightness(Property prop);
            void setPropertyGamma(Property prop);
            void setPropertyShutter(Property prop);
            void setPropertyGain(Property prop);
            void setPropertyTriggerDelay(Property prop);
            void setPropertyFrameRate(Property prop);
            void setPropertyTemperature(Property prop);
            void setPropertyTriggerMode(Property prop);
    };

    typedef std::shared_ptr<CameraDevice_arena> CameraDevicePtr_arena;

}

#endif // #ifndef BIAS_CAMERA_DEVICE_ARENA_HPP
#endif // #ifdef WITH_ARENA
