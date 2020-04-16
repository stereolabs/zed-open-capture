#ifndef VIDEOCAPTURE_HPP
#define VIDEOCAPTURE_HPP

#include "defines.hpp"

#include <chrono>
#include <thread>
#include <mutex>

namespace zed {

/*!
 * \brief The Frame struct
 */
struct SL_DRV_EXPORT Frame
{
    uint64_t frame_id = 0;          //!< Increasing index of frames
    uint64_t timestamp = 0;         //!< Timestamp in nanoseconds
    unsigned char* data = nullptr;  //!< Frame data in YUV 4:2:2 format
    uint16_t width = 0;             //!< Frame width
    uint16_t height = 0;            //!< Frame height
    uint8_t channels = 0;           //!< Number of channels per pixel
};

/*!
 * \brief The Buffer struct used by UVC to store frame data
 */
struct Buffer {
    void *start;
    size_t length;
};

/*!
 * \brief The VideoCapture class provides image and sensor grabbing functions
 */
class SL_DRV_EXPORT VideoCapture
{
    ZED_DRV_VERSION_ATTRIBUTE

public:    
    /*!
     * \brief VideoCapture is the default constructor
     *  * \param params the initialization parameters
     */
    VideoCapture(Params params = _params() );

    /*!
     * \brief ~VideoCapture  destructor
     */
    virtual ~VideoCapture();

    /*!
     * \brief init Open a ZED camera using the specified ID or searching for the first available
     * \param devId Id of the camera (see `/dev/video*`). Use `-1` to open the first free ZED camera
     * \return  true if the camera is correctly opened
     */
    bool init( int devId=-1 );

    /*!
     * \brief isVerbose Return the verbose status
     * \return true if verbose is enabled
     */
    bool isVerbose(){ return mVerbose; }

    /*!
     * \brief getLastFrame
     * \param timeout_msec frame grabbing timeout in millisecond.
     * \return returns the last new frame
     */
    zed::Frame* getLastFrame(uint64_t timeout_msec=10);

    // ----> Led Control
    int setLEDValue(bool);
    int getLEDValue(bool *value);
    int toggleLED(bool *value);
    // <---- Led Control

    // ----> Camera Settings control
    void setBrightnessSetting(int value); // 0 -> 8
    void setSharpnessSetting(int value); // 0 -> 8
    void setContrastSetting(int value); // 0 -> 8
    void setHueSetting(int value); // 0 -> 11
    void setSaturationSetting(int value); // 0 -> 8
    void setWhiteBalanceSetting(int value); // 2800 -> 6500
    void setAutoWhiteBalanceSetting(bool active);
    void setGammaSetting(int value, bool useDefault);
    void setLEDStatus(int value, bool useDefault);

    int getBrightnessSetting();
    int getSharpnessSetting();
    int getContrastSetting();
    int getHueSetting();
    int getSaturationSetting();
    int getWhiteBalanceSetting();
    bool getAutoWhiteBalanceSetting();
    int getGammaSetting();
    int getLEDStatus();

    void resetBrightnessSetting();
    void resetSharpnessSetting();
    void resetContrastSetting();
    void resetHueSetting();
    void resetSaturationSetting();
    void resetWhiteBalanceSetting();
    void resetAutoWhiteBalanceSetting();
    // <---- Camera Settings control

private:
    /*!
     * \brief grabThreadFunc The frame grabbing thread function
     */
    void grabThreadFunc();

    int linux_cbs_VendorControl(unsigned char *buf, int len, int readMode, bool safe = false);
    int linux_cbs_get_gpio_value(int gpio_number, uint8_t* value);
    int linux_cbs_set_gpio_value(int gpio_number, uint8_t value);
    int linux_cbs_set_gpio_direction(int gpio_number, int direction);

    void setCameraControlSettings(int ctrl_id, int ctrl_val);
    void resetCameraControlSettings(int ctrl_id);
    int getCameraControlSettings(int ctrl_id);

private:
    bool openCamera( uint8_t devId );
    bool startCapture();
    inline void stopCapture(){mStopCapture=true;}
    int input_set_framerate(int fps);
    int xioctl(int fd, uint64_t IOCTL_X, void *arg);
    void checkResFps( Params par );
    zed::SL_DEVICE getCameraModel(std::string dev_name);
    void reset();

    /*!
     * \brief getCurrentTs get the current system clock as steady clock, so with no jumps even if the system time changes
     * \return the current system clock
     */
    static uint64_t getCurrentTs() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}


private:
    // Flags
    bool mVerbose=false;        //!< Activate debug messages
    bool mNewFrame=false;       //!< Indicates if a new frame is available
    bool mInitialized=false;    //!< Inficates if the camera has been initialized
    bool mStopCapture=true;     //!< Indicates if the grabbing thread must be stopped
    bool mGrabRunning=false;    //!< Indicates if the grabbing thread is running

    int mDevId = 0;             //!< ID of the camera device
    std::string mDevName;       //!< The file descriptor path name (e.g. /dev/video0)
    int mFileDesc=-1;           //!< The file descriptor handler

    std::mutex mBufMutex;       //!< Mutex for safe access to data buffer
    std::mutex mComMutex;       //!< Mutex for safe access to UVC communication

    int mWidth = 0;             //!< Frame width
    int mHeight = 0;            //!< Frame height
    int mChannels = 0;          //!< Frame channels
    int mFps=0;                 //!< Frame per seconds

    zed::SL_DEVICE mCameraModel = zed::SL_DEVICE::NONE; //!< The camera model

    Frame mLastFrame;           //!< Last grabbed frame
    uint8_t mBufCount = 2;      //!< UVC buffer count
    uint8_t mCurrentIndex = 0;  //!< The index of the currect UVC buffer
    struct Buffer *mBuffers = nullptr;  //!< UVC buffers

    // ts full during init, to compute diff
    uint64_t mStartTs=0;
    // ts in us to compute diff with buffer ts
    uint64_t mInitTs=0;

    std::thread mGrabThread;    //!< The grabbing thread

};

}
#endif // VIDEOCAPTURE_HPP
