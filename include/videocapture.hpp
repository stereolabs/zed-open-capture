#ifndef VIDEOCAPTURE_HPP
#define VIDEOCAPTURE_HPP

#include "defines.hpp"
#include "videocapture_def.hpp"

#include <chrono>
#include <thread>
#include <mutex>

namespace sl_drv {

/*!
 * \brief The Frame struct containing the acquired video frames
 */
struct SL_DRV_EXPORT Frame
{
    uint64_t frame_id = 0;          //!< Increasing index of frames
    uint64_t timestamp = 0;         //!< Timestamp in nanoseconds
    uint8_t* data = nullptr;  //!< Frame data in YUV 4:2:2 format
    uint16_t width = 0;             //!< Frame width
    uint16_t height = 0;            //!< Frame height
    uint8_t channels = 0;           //!< Number of channels per pixel
};

/*!
 * \brief The VideoCapture class provides image and sensor grabbing functions
 */
class SL_DRV_EXPORT VideoCapture
{
    ZED_DRV_VERSION_ATTRIBUTE

public:    
    /*!
     * \brief The default constructor
     *  * \param params the initialization parameters (see \ref VideoParams)
     */
    VideoCapture( VideoParams params = _video_params() );

    /*!
     * \brief Destructor
     */
    virtual ~VideoCapture();

    /*!
     * \brief Open a ZED camera using the specified ID or searching for the first available
     * \param devId Id of the camera (see `/dev/video*`). Use `-1` to open the first available ZED camera
     * \return returns true if the camera is correctly opened
     */
    bool init( int devId=-1 );

    /*!
     * \brief Return the verbose status
     * \return returns true if verbose is enabled
     */
    bool isVerbose(){ return mVerbose; }

    /*!
     * \brief Get the last received camera image
     * \param timeout_msec frame grabbing timeout in millisecond.
     * \return returns the last received frame as pointer.
     *
     * \note Do not delete the received frame
     */
    const sl_drv::Frame* getLastFrame(uint64_t timeout_msec=10);

    // ----> Led Control
    /*!
     * \brief Set the status of the camera led
     * \param status true for "ON", false for "OFF"
     * \return returns a negative value if an error occurred
     */
    int setLEDstatus(bool status);

    /*!
     * \brief Get the status of the camera led
     * \param value returned status: true for "ON", false for "OFF"
     * \return returns a negative value if an error occurred
     */
    int getLEDstatus(bool *status);

    /*!
     * \brief Toggle the status of the camera led
     * \param value returned status: true for "ON", false for "OFF"
     * \return returns a negative value if an error occurred
     */
    int toggleLED(bool *value);
    // <---- Led Control

    // ----> Camera Settings control
    void setBrightness(int brightness); // 0 -> 8
    int getBrightness();
    void resetBrightnessSetting();

    void setSharpness(int sharpness); // 0 -> 8
    int getSharpness();
    void resetSharpness();

    void setContrast(int contrast); // 0 -> 8
    int getContrast();
    void resetContrast();

    void setHue(int hue); // 0 -> 11
    int getHue();
    void resetHue();

    void setSaturation(int saturation); // 0 -> 8
    int getSaturation();
    void resetSaturation();

    void setWhiteBalance(int wb); // 2800 -> 6500
    int getWhiteBalance();

    void setAutoWhiteBalance(bool active);
    bool getAutoWhiteBalance();
    void resetAutoWhiteBalance();

    void setGamma(int gamma); // 1 -> 9
    int getGamma();
    void resetGamma();

    int setAECAGC(bool active);
    bool getAECAGC();
    void resetAECAGC();

    void setGain(CAM_SENS_POS cam, int gain); // 0 -> 100
    int getGain(CAM_SENS_POS cam);

    void setExposure(CAM_SENS_POS cam, int exposure); // 0 -> 100
    int getExposure(CAM_SENS_POS cam);
    // <---- Camera Settings control

    int getSerialNumber();

private:
    /*!
     * \brief grabThreadFunc The frame grabbing thread function
     */
    void grabThreadFunc();

    int ll_VendorControl(unsigned char *buf, int len, int readMode, bool safe = false);
    int ll_get_gpio_value(int gpio_number, uint8_t* value);
    int ll_set_gpio_value(int gpio_number, uint8_t value);
    int ll_set_gpio_direction(int gpio_number, int direction);
    int ll_read_system_register(uint64_t address, uint8_t* value);
    int ll_write_system_register(uint64_t address, uint8_t value);
    int ll_read_sensor_register(int side, int sscb_id, uint64_t address, uint8_t *value);
    int ll_write_sensor_register(int side, int sscb_id, uint64_t address, uint8_t value);

    int ll_SPI_FlashProgramRead(uint8_t *pBuf, int Adr, int len);

    int ll_isp_aecagc_enable(int side, bool enable);
    int ll_isp_is_aecagc(int side);

    int ll_isp_get_gain(uint8_t *val, uint8_t sensorID);
    int ll_isp_set_gain(unsigned char ucGainH, unsigned char ucGainM, unsigned char ucGainL, int sensorID);
    int ll_isp_get_exposure(unsigned char *val, unsigned char sensorID);
    int ll_isp_set_exposure(unsigned char ucExpH, unsigned char ucExpM, unsigned char ucExpL, int sensorID);


    void setCameraControlSettings(int ctrl_id, int ctrl_val);
    void resetCameraControlSettings(int ctrl_id);
    int getCameraControlSettings(int ctrl_id);

    int setGammaPreset(int side, int value);

    int calcRawGainValue(int gain); // Convert "user gain" to "ISP gain"
    int calcGainValue(int rawGain); // Convert "ISP Gain" to "User gain"

    bool openCamera( uint8_t devId );
    bool startCapture();
    inline void stopCapture(){mStopCapture=true;}
    int input_set_framerate(int fps);
    int xioctl(int fd, uint64_t IOCTL_X, void *arg);
    void checkResFps( VideoParams par );
    sl_drv::SL_DEVICE getCameraModel(std::string dev_name);
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

    sl_drv::SL_DEVICE mCameraModel = sl_drv::SL_DEVICE::NONE; //!< The camera model

    Frame mLastFrame;           //!< Last grabbed frame
    uint8_t mBufCount = 2;      //!< UVC buffer count
    uint8_t mCurrentIndex = 0;  //!< The index of the currect UVC buffer
    struct Buffer *mBuffers = nullptr;  //!< UVC buffers

    // ts full during init, to compute diff
    uint64_t mStartTs=0;
    // ts in us to compute diff with buffer ts
    uint64_t mInitTs=0;

    int mGainSegMax=0;
    int mExpoureRawMax;

    std::thread mGrabThread;    //!< The grabbing thread

};

namespace cbs {

    enum class ERASE_MODE {
        ERASE_MODE_FULL,
        ERASE_MODE_SECTOR
    };



    const unsigned char PRESET_GAMMA[9][16] = {
        {7,14,29,54,66,78,89,103,114,123,139,154,183,206,228,254}, //FW 1142
        {9,17,34,58,71,83,89,108,118,127,143,158,186,208,229,254},
        {10,20,38,63,75,88,99,112,123,132,147,162,189,210,230,254},
        {12,23,43,67,80,92,103,117,127,136,151,165,192,212,231,254},
        {13,26,47,71,84,97,108,121,131,140,155,169,195,214,232,255}, //ECT
        {18,32,54,80,93,106,117,130,140,149,164,177,202,219,236,255},
        {24,38,61,88,102,115,127,139,148,157,172,186,209,225,240,255},
        {29,44,68,97,111,124,136,147,157,166,181,194,215,230,243,255},
        {34,50,75,105,120,133,145,156,165,174,189,202,222,235,247,255} //FW904
    };

}

}
#endif // VIDEOCAPTURE_HPP
