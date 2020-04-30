#ifndef VIDEOCAPTURE_HPP
#define VIDEOCAPTURE_HPP

#include "defines.hpp"
#include "videocapture_def.hpp"

#include <chrono>
#include <thread>
#include <mutex>

namespace sl_drv {

#ifdef SENSORS_MOD_AVAILABLE
class SensorCapture;
#endif

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
 * \brief The VideoCapture class provides image grabbing functions for Stereolabs cameras
 */
class SL_DRV_EXPORT VideoCapture
{
    ZED_DRV_VERSION_ATTRIBUTE

public:    
    /*!
     * \brief The default constructor
     *  * \param params the initialization parameters (see \ref VideoParams)
     */
    VideoCapture( VideoParams params = VideoParams() );

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
     * \brief Get the last received camera image
     * \param timeout_msec frame grabbing timeout in millisecond.
     * \return returns the last received frame as pointer.
     *
     * \note Do not delete the received frame
     */
    const Frame* getLastFrame(uint64_t timeout_msec=10);

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
    /*!
     * \brief Set the Brightness value
     * \param brightness Brightness value in the range [0,8]
     */
    void setBrightness(int brightness);

    /*!
     * \brief Get the Brightness value
     * \return the current Brightness value
     */
    int getBrightness();

    /*!
     * \brief Reset the Brightness value to default value
     */
    void resetBrightnessSetting();

    /*!
     * \brief Set the Sharpness value
     * \param sharpness Sharpness value in the range [0,8]
     */
    void setSharpness(int sharpness);

    /*!
     * \brief Get the Sharpness value
     * \return the current Sharpness value
     */
    int getSharpness();

    /*!
     * \brief Reset the Sharpness value to default value
     */
    void resetSharpness();

    /*!
     * \brief Set the Contrast value
     * \param contrast Contrast value in the range [0,8]
     */
    void setContrast(int contrast);

    /*!
     * \brief Get the Contrast value
     * \return the current Contrast value
     */
    int getContrast();

    /*!
     * \brief Reset the Contrast value to default value
     */
    void resetContrast();

    /*!
     * \brief Set the Hue value
     * \param hue Hue value in the range [0,11]
     */
    void setHue(int hue);

    /*!
     * \brief Get the Hue value
     * \return the current Hue value
     */
    int getHue();

    /*!
     * \brief Reset the Hue value to default value
     */
    void resetHue();

    /*!
     * \brief Set the Saturation value
     * \param saturation Saturation value in the range [0,8]
     */
    void setSaturation(int saturation);

    /*!
     * \brief Get the Saturation value
     * \return the current Saturation value
     */
    int getSaturation();

    /*!
     * \brief Reset the Saturation value to default value
     */
    void resetSaturation();

    /*!
     * \brief Set the White Balance value (disable auto White Balance if active)
     * \param wb White Balance value in the range [2800,6500]
     */
    void setWhiteBalance(int wb);

    /*!
     * \brief Get the White Balance value
     * \return the current White Balance value
     */
    int getWhiteBalance();

    /*!
     * \brief Enable/Disable the automatic White Balance control
     * \param active true to activate automatic White Balance
     */
    void setAutoWhiteBalance(bool active);

    /*!
     * \brief Get the status of the automatic White Balance control
     * \return the status of the automatic White Balance control
     */
    bool getAutoWhiteBalance();

    /*!
     * \brief Reset the automatic White Balance control value to default value
     */
    void resetAutoWhiteBalance();

    /*!
     * \brief Set the Gamma value
     * \param gamma Gamma value in the range [1,9]
     */
    void setGamma(int gamma);

    /*!
     * \brief Get the Gamma value
     * \return the current Gamma value
     */
    int getGamma();

    /*!
     * \brief Reset the Gamma value to default value
     */
    void resetGamma();

    /*!
     * \brief Enable/Disable the automatic Exposure and Gain control
     * \param active true to activate automatic White Balance
     */
    int setAECAGC(bool active);

    /*!
     * \brief Get the status of the automatic Exposure and Gain control
     * \return the status of the automatic Exposure and Gain control
     */
    bool getAECAGC();

    /*!
     * \brief Reset the automatic Exposure and Gain control value to default value
     */
    void resetAECAGC();

    /*!
     * \brief Set the Gain value (disable Exposure and Gain control if active)
     * \param cam position of the camera sensor (see \ref CAM_SENS_POS)
     * \param gain Gain value in the range [0,100]
     */
    void setGain(CAM_SENS_POS cam, int gain);

    /*!
     * \brief Get the current Gain value
     * \param cam position of the camera sensor (see \ref CAM_SENS_POS)
     * \return the current Gain value
     */
    int getGain(CAM_SENS_POS cam);

    /*!
     * \brief Set the Exposure value (disable Exposure and Gain control if active)
     * \param cam position of the camera sensor (see \ref CAM_SENS_POS)
     * \param exposure Exposure value in the range [0,100]
     */
    void setExposure(CAM_SENS_POS cam, int exposure);

    /*!
     * \brief Get the current Exposure value
     * \param cam position of the camera sensor (see \ref CAM_SENS_POS)
     * \return the current Exposure value
     */
    int getExposure(CAM_SENS_POS cam);
    // <---- Camera Settings control

    /*!
     * \brief Retrieve the serial number of the connected camera
     * \return the serial number of the connected camera
     */
    int getSerialNumber();

#ifdef SENSORS_MOD_AVAILABLE
    /*!
     * \brief Enable synchronizations between Camera frame and Sensors timestamps
     * \param sensCap pointer to \ref SensorCapture object
     * \return true if synchronization has been correctly started
     */
    bool enableSensorSync( SensorCapture* sensCap=nullptr );
#endif

private:
    /*!
     * \brief grabThreadFunc The frame grabbing thread function
     */
    void grabThreadFunc();

    // ----> Low level functions
    int ll_VendorControl(uint8_t *buf, int len, int readMode, bool safe = false);
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

    void ll_activate_sync(); // Activate low level sync signal between Camera and MCU
    // <---- Low level functions

    // ----> Mid level functions
    void setCameraControlSettings(int ctrl_id, int ctrl_val);
    void resetCameraControlSettings(int ctrl_id);
    int getCameraControlSettings(int ctrl_id);

    int setGammaPreset(int side, int value);

    int calcRawGainValue(int gain); // Convert "user gain" to "ISP gain"
    int calcGainValue(int rawGain); // Convert "ISP Gain" to "User gain"
    // <---- Mid level functions

    // ----> Connection control functions
    bool openCamera( uint8_t devId );                   //!< Open camera
    bool startCapture();                                //!< Start video capture thread
    void reset();                                       //!< Reset camera connection
    inline void stopCapture(){mStopCapture=true;}       //!< Stop video cpture thread
    int input_set_framerate(int fps);                   //!< Set UVC framerate
    int xioctl(int fd, uint64_t IOCTL_X, void *arg);    //!< Send ioctl command
    void checkResFps();                                 //!< Check if the Framerate is correct for the selected resolution
    sl_drv::SL_DEVICE getCameraModel(std::string dev_name); //!< Get the connected camera model
    // <---- Connection control functions

    /*!
     * \brief Get the current system clock as steady clock, so with no jumps even if the system time changes
     * \return the current system clock in nanoseconds
     */
    static uint64_t getSysTs() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}

private:
    // Flags
    bool mNewFrame=false;       //!< Indicates if a new frame is available
    bool mInitialized=false;    //!< Inficates if the camera has been initialized
    bool mStopCapture=true;     //!< Indicates if the grabbing thread must be stopped
    bool mGrabRunning=false;    //!< Indicates if the grabbing thread is running

    VideoParams mParams;        //!< Grabbing parameters

    int mDevId = 0;             //!< ID of the camera device
    std::string mDevName;       //!< The file descriptor path name (e.g. /dev/video0)
    int mFileDesc=-1;           //!< The file descriptor handler

    std::mutex mBufMutex;       //!< Mutex for safe access to data buffer
    std::mutex mComMutex;       //!< Mutex for safe access to UVC communication

    int mWidth = 0;             //!< Frame width
    int mHeight = 0;            //!< Frame height
    int mChannels = 0;          //!< Frame channels
    int mFps=0;                 //!< Frames per seconds

    sl_drv::SL_DEVICE mCameraModel = sl_drv::SL_DEVICE::NONE; //!< The camera model

    Frame mLastFrame;           //!< Last grabbed frame
    uint8_t mBufCount = 2;      //!< UVC buffer count
    uint8_t mCurrentIndex = 0;  //!< The index of the currect UVC buffer
    struct UVCBuffer *mBuffers = nullptr;  //!< UVC buffers

    uint64_t mStartTs=0;        //!< Initial System Timestamp, to calculate differences [nsec]
    uint64_t mInitTs=0;         //!< Initial Device Timestamp, to calculate differences [usec]

    int mGainSegMax=0;          //!< Maximum value of the raw gain to be used for conversion
    int mExpoureRawMax;         //!< Maximum value of the raw exposure to be used for conversion

    std::thread mGrabThread;    //!< The video grabbing thread

#ifdef SENSORS_MOD_AVAILABLE
    friend class sl_drv::SensorCapture;
#endif
};

}
#endif // VIDEOCAPTURE_HPP
