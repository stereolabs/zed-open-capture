///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef VIDEOCAPTURE_HPP
#define VIDEOCAPTURE_HPP

#include "defines.hpp"
#include "videocapture_def.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <fstream>      // std::ofstream
#include <iomanip>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define LOG_SEP ","

namespace sl_oc {

namespace detail {
struct SyncLink;
}

namespace sensors {
class SensorCapture;
}

namespace video {

/*!
 * \brief The Frame struct containing the acquired video frames
 */
struct SL_OC_EXPORT Frame
{
    uint64_t frame_id = 0;          //!< Index of the frame assigned by the camera driver. It always increases, with gaps when frames are lost.
    uint64_t timestamp = 0;         //!< Timestamp in nanoseconds (see \ref sl_oc::steadyToTimestamp), 0 if no frame is available
    uint8_t* data = nullptr;        //!< Frame data in YUV 4:2:2 format
    uint16_t width = 0;             //!< Frame width
    uint16_t height = 0;            //!< Frame height
    uint8_t channels = 0;           //!< Number of channels per pixel
};

/*!
 * \brief The VideoCapture class provides image grabbing functions and settings control for all the Stereolabs camera models
 */
class SL_OC_EXPORT VideoCapture
{
public:
    /*!
     * \brief The default constructor
     *  * \param params the initialization parameters (see  VideoParams)
     */
    VideoCapture( VideoParams params = VideoParams() );

    /*!
     * \brief The class destructor
     */
    virtual ~VideoCapture();

    VideoCapture(const VideoCapture&) = delete;
    VideoCapture& operator=(const VideoCapture&) = delete;

    /*!
     * \brief Get the list of the connected Stereolabs cameras
     * \return the available cameras, sorted by device ID
     * \note The serial number is read from each camera, also if it is in use by another application
     */
    static std::vector<VideoDevice> getDeviceList();

    /*!
     * \brief Open a ZED camera using the specified ID or searching for the first available
     * \param devId Id of the camera (see `/dev/video*`). Use `-1` to open the first available camera
     * \return returns true if the camera is correctly opened
     * \note The call returns once the camera sensors accept settings (about 0.2 s after streaming starts),
     *       so gain and exposure can be set right after it.
     * \note A camera already open is closed before opening the new one
     */
    bool initializeVideo( int devId=-1 );

    /*!
     * \brief Open the ZED camera with the specified serial number
     * \param serial_number the serial number of the camera
     * \return returns true if the camera is correctly opened
     */
    bool initializeVideoBySerialNumber( int serial_number );

    /*!
     * \brief Stop the video stream and close the camera. It is safe to call it more than once.
     */
    void close();

    /*!
     * \brief Check if the camera is open and frames can still be received
     * \return false if the camera was never opened, has been closed, or has been disconnected
     */
    bool isOpened() const;

    /*!
     * \brief Get the last received camera image
     * \param timeout_msec frame grabbing timeout in millisecond.
     * \return returns a reference to the last received frame as pointer.
     *
     * \note To check if a new frame is available it is possible to compare the timestamp of the returned frame with
     * the timestamp of the previous valid frame
     *
     * \note Frame received will contains the RAW buffer from the camera, in YUV4:2:2 color format and in side by side mode.
     * Images must then be converted to RGB for proper display and will not be rectified.
     *
     * \note The returned frame, including the image pointed by `data`, stays valid and unchanged until the next call
     * of this function or until the camera is closed. The function must be called by one thread at a time.
     */
    const Frame& getLastFrame(uint64_t timeout_msec=100);

    /*!
     * \brief Get the size of the camera frame
     * \param width the frame width
     * \param height the frame height
     */
    inline void getFrameSize( int& width, int& height ){width=mWidth;height=mHeight;}

    /*!
     * \brief Get the frame rate in use
     * \return the frames per second accepted by the camera, that can differ from the requested value
     *         (e.g. when the camera is connected to a USB 2.0 port)
     */
    inline int getFps() const {return mFps;}

    // ----> Led Control
    /*!
     * \brief Set the status of the camera led
     * \param status true for "ON", false for "OFF"
     * \return true if the status has been set
     */
    bool setLEDstatus(bool status);

    /*!
     * \brief Get the status of the camera led
     * \param status returned status: true for "ON", false for "OFF"
     * \return true if the status has been read
     */
    bool getLEDstatus(bool *status);

    /*!
     * \brief Toggle the status of the camera led
     * \param value returned status: true for "ON", false for "OFF"
     * \return true if the status has been changed
     */
    bool toggleLED(bool *value);
    // <---- Led Control

    // ----> Camera Settings control
    /*!
     * \brief Set the Brightness value
     * \param brightness Brightness value in the range [0,8]
     * \return true if the camera accepted the setting
     */
    bool setBrightness(int brightness);

    /*!
     * \brief Get the Brightness value
     * \return the current Brightness value
     */
    int getBrightness();

    /*!
     * \brief Reset the Brightness value to default value
     * \return true if the camera accepted the setting
     */
    bool resetBrightness();

    /*!
     * \brief Set the Sharpness value
     * \param sharpness Sharpness value in the range [0,8]
     * \return true if the camera accepted the setting
     */
    bool setSharpness(int sharpness);

    /*!
     * \brief Get the Sharpness value
     * \return the current Sharpness value
     */
    int getSharpness();

    /*!
     * \brief Reset the Sharpness value to default value
     * \return true if the camera accepted the setting
     */
    bool resetSharpness();

    /*!
     * \brief Set the Contrast value
     * \param contrast Contrast value in the range [0,8]
     * \return true if the camera accepted the setting
     */
    bool setContrast(int contrast);

    /*!
     * \brief Get the Contrast value
     * \return the current Contrast value
     */
    int getContrast();

    /*!
     * \brief Reset the Contrast value to default value
     * \return true if the camera accepted the setting
     */
    bool resetContrast();

    /*!
     * \brief Set the Hue value
     * \param hue Hue value in the range [0,11]
     * \return true if the camera accepted the setting
     */
    bool setHue(int hue);

    /*!
     * \brief Get the Hue value
     * \return the current Hue value
     */
    int getHue();

    /*!
     * \brief Reset the Hue value to default value
     * \return true if the camera accepted the setting
     */
    bool resetHue();

    /*!
     * \brief Set the Saturation value
     * \param saturation Saturation value in the range [0,8]
     * \return true if the camera accepted the setting
     */
    bool setSaturation(int saturation);

    /*!
     * \brief Get the Saturation value
     * \return the current Saturation value
     */
    int getSaturation();

    /*!
     * \brief Reset the Saturation value to default value
     * \return true if the camera accepted the setting
     */
    bool resetSaturation();

    /*!
     * \brief Set the White Balance value (disable auto White Balance if active)
     * \param wb White Balance value in the range [2800,6500]
     * \return true if the camera accepted the setting
     */
    bool setWhiteBalance(int wb);

    /*!
     * \brief Get the White Balance value
     * \return the current White Balance value
     */
    int getWhiteBalance();

    /*!
     * \brief Enable/Disable the automatic White Balance control
     * \param active true to activate automatic White Balance
     * \return true if the camera accepted the setting
     */
    bool setAutoWhiteBalance(bool active);

    /*!
     * \brief Get the status of the automatic White Balance control
     * \return the status of the automatic White Balance control
     */
    bool getAutoWhiteBalance();

    /*!
     * \brief Reset the automatic White Balance control value to default value
     * \return true if the camera accepted the setting
     */
    bool resetAutoWhiteBalance();

    /*!
     * \brief Set the Gamma value
     * \param gamma Gamma value in the range [1,9]
     * \return true if the camera accepted the setting
     */
    bool setGamma(int gamma);

    /*!
     * \brief Get the Gamma value
     * \return the current Gamma value in the range [1,9], a negative value if it cannot be read
     */
    int getGamma();

    /*!
     * \brief Reset the Gamma value to default value (1)
     * \return true if the camera accepted the setting
     */
    bool resetGamma();

    /*!
     * \brief Enable/Disable the automatic Exposure and Gain control
     * \param active true to activate automatic Exposure and Gain control
     * \note When disabling an active control, the call blocks for 5 frame periods
     *       (333 ms at 15 fps) so that a pending automatic update cannot overwrite
     *       the gain and exposure values set right after it.
     * \return true if the camera accepted the setting
     */
    bool setAECAGC(bool active);

    /*!
     * \brief Get the status of the automatic Exposure and Gain control
     * \return the status of the automatic Exposure and Gain control
     */
    bool getAECAGC();

    /*!
     * \brief Reset the automatic Exposure and Gain control value to default value
     * \return true if the camera accepted the setting
     */
    bool resetAECAGC();

    /*!
     * \brief Set Region Of Interest (ROI) for AECAGC control
     * \param side position of the camera sensor (see  CAM_SENS_POS)
     * \param x top left ROI rectangle X coordinate
     * \param y top left ROI rectangle Y coordinate
     * \param w ROI rectangle width
     * \param h ROI rectangle width
     * \return returns true if the ROI has been correctly set
     */
    bool setROIforAECAGC(CAM_SENS_POS side, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    /*!
     * \brief Reset the ROI for AECAGC control
     * \param side position of the camera sensor (see  CAM_SENS_POS)
     * \return returns true if the ROI has been correctly reset
     */
    bool resetROIforAECAGC(CAM_SENS_POS side);

    /*!
     * \brief Get the coordinates of the current ROI for AECAGC control
     * \param side position of the camera sensor (see  CAM_SENS_POS)
     * \param x top left ROI rectangle X coordinate
     * \param y top left ROI rectangle Y coordinate
     * \param w ROI rectangle width
     * \param h ROI rectangle width
     * \return returns true if no errors occurred
     */
    bool getROIforAECAGC(CAM_SENS_POS side,uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h);

    /*!
     * \brief Set the Gain value (disable Exposure and Gain control if active)
     * \note If the automatic control was active, the call blocks for 5 frame periods (see \ref setAECAGC)
     * \param cam position of the camera sensor (see  CAM_SENS_POS)
     * \param gain Gain value in the range [0,100]
     * \return true if the camera accepted the setting
     */
    bool setGain(CAM_SENS_POS cam, int gain);

    /*!
     * \brief Get the current Gain value
     * \param cam position of the camera sensor (see  CAM_SENS_POS)
     * \return the current Gain value
     */
    int getGain(CAM_SENS_POS cam);

    /*!
     * \brief Set the Exposure value (disable Exposure and Gain control if active)
     * \note If the automatic control was active, the call blocks for 5 frame periods (see \ref setAECAGC)
     * \param cam position of the camera sensor (see  CAM_SENS_POS)
     * \param exposure Exposure value in the range [0,100]
     * \return true if the camera accepted the setting
     */
    bool setExposure(CAM_SENS_POS cam, int exposure);

    /*!
     * \brief Get the current Exposure value
     * \param cam position of the camera sensor (see  CAM_SENS_POS)
     * \return the current Exposure value
     */
    int getExposure(CAM_SENS_POS cam);
    // <---- Camera Settings control

    /*!
     * \brief Retrieve the serial number of the connected camera
     * \return the serial number of the connected camera, -1 if not available
     */
    int getSerialNumber();

    /*!
     * \brief Retrieve the model of the connected camera
     * \return the camera model, `SL_DEVICE::NONE` if not connected
     */
    inline SL_DEVICE getCameraModel() const {return mCameraModel;}

    /*!
     * \brief Retrieve the OS device name
     * \return the OS device name
     */
    inline std::string getDeviceName(){return mDevName;}

    /*!
     * \brief Retrieve the OS device index
     * \return the OS device index
     */
    inline int getDeviceId(){return mDevId;}

    /*!
     * \brief Enable synchronizations between Camera frame and Sensors timestamps
     *
     * Frames and sensors data always share the same time base. When the synchronization is enabled the camera
     * also sends a hardware signal to the sensors module at each frame, and the frame timestamp is replaced by
     * the timestamp of the sensor data acquired with that signal (see \ref sensors::data::Imu::sync).
     *
     * \param sensCap pointer to the SensorCapture object connected to the same camera
     * \return true if synchronization has been correctly started
     * \note To be called again if the camera or the sensors module are closed and opened again
     */
    bool enableSensorSync( sensors::SensorCapture* sensCap=nullptr );

    // ----> Debug functions
    /*!
     * \brief Utils fct to set Color Bars on Image
     */
    void setColorBars(int side, bool c);

    /*!
     * \brief Restore the automatic exposure and gain registers of the CMOS sensors
     * \return true if the registers have been written
     */
    bool resetAGCAECregisters();

    /*!
     * \brief Start logging to file of AEG/AGC camera registers
     * \param enable set to true to enable logging
     * \param frame_skip number of frames to skip when logging to file
     * \return true if log file can be correctly created/closed
     */
    bool enableAecAgcSensLogging(bool enable, int frame_skip=10);

    /*!
     * \brief Save all ISP camera registers into a file
     * \param filename csv filename
     * \note CSV file will contain Adress , L value, R value
     */
    void saveAllISPRegisters(std::string filename);

    /*!
     * \brief Save all sensors ctrl register
     * \param filename csv filename
     * \note CSV file will contain Adress , L value, R value
     */
    void saveAllSensorsRegisters(std::string filename);
    // <---- Debug functions

private:
    void grabThreadFunc();  //!< The frame grabbing thread function

    // ----> Low level functions
    int ll_VendorControl(uint8_t *buf, int readMode, bool safe = false, bool force=false); //!< `buf` must contain 384 bytes
    int ll_get_gpio_value(int gpio_number, uint8_t* value);
    int ll_set_gpio_value(int gpio_number, uint8_t value);
    int ll_set_gpio_direction(int gpio_number, int direction);
    int ll_read_system_register(uint64_t address, uint8_t* value);
    int ll_write_system_register(uint64_t address, uint8_t value);
    int ll_read_sensor_register(int side, int sscb_id, uint64_t address, uint8_t *value);
    int ll_write_sensor_register(int side, int sscb_id, uint64_t address, uint8_t value);
    bool waitSensorsReady(int timeout_msec);

    int ll_isp_aecagc_enable(int side, bool enable);
    int ll_isp_is_aecagc(int side);

    int ll_isp_get_gain(uint8_t *val, uint8_t sensorID);
    int ll_isp_set_gain(unsigned char ucGainH, unsigned char ucGainM, unsigned char ucGainL, int sensorID);
    int ll_isp_get_exposure(unsigned char *val, unsigned char sensorID);
    int ll_isp_set_exposure(unsigned char ucExpH, unsigned char ucExpM, unsigned char ucExpL, int sensorID);

    void ll_activate_sync(); // Activate low level sync signal between Camera and MCU
    // <---- Low level functions

    // ----> Mid level functions
    bool setCameraControlSettings(int ctrl_id, int ctrl_val);
    bool resetCameraControlSettings(int ctrl_id);
    int getCameraControlSettings(int ctrl_id);

    int setGammaPreset(int side, int value);

    int calcRawGainValue(int gain); // Convert "user gain" to "ISP gain"
    int calcGainValue(int rawGain); // Convert "ISP Gain" to "User gain"
    // <---- Mid level functions

    // ----> Connection control functions
    bool openCamera( int devId );                               //!< Open camera
    void closeDevice();                                         //!< Release the device and the buffers
    bool startCapture();                                        //!< Start video capture thread
    bool finalizeInit();                                        //!< Complete the initialization of an opened camera
    int xioctl(int fd, uint64_t IOCTL_X, void *arg);            //!< Send ioctl command
    void checkResFps();                                         //!< Check if the Framerate is correct for the selected resolution
    // <---- Connection control functions

    typedef enum _date_time
    {
        FULL,
        DATE,
        TIME
    } DateTime;

    static std::string getCurrentDateTime( DateTime type );

    void saveLogDataLeft(uint64_t timestamp);
    void saveLogDataRight(uint64_t timestamp);

private:
    // Flags
    std::atomic<bool> mInitialized{false};  //!< Indicates if the camera has been initialized
    std::atomic<bool> mStopCapture{true};   //!< Indicates if the grabbing thread must be stopped
    std::atomic<bool> mDeviceLost{false};   //!< Indicates if the camera has been disconnected

    VideoParams mParams;                //!< Grabbing parameters

    int mDevId = -1;                    //!< ID of the camera device
    std::string mDevName;               //!< The file descriptor path name (e.g. /dev/video0)
    int mFileDesc=-1;                   //!< The file descriptor handler
    int mSerialNumber=-1;               //!< Serial number of the opened camera

    std::mutex mComMutex;               //!< Mutex for safe access to UVC communication

    int mWidth = 0;                     //!< Frame width
    int mHeight = 0;                    //!< Frame height
    int mChannels = 0;                  //!< Frame channels
    int mFps=0;                         //!< Frames per seconds

    SL_DEVICE mCameraModel = SL_DEVICE::NONE; //!< The camera model

    // ----> Frames shared with the grabbing thread, protected by mBufMutex
    std::mutex mBufMutex;               //!< Mutex for safe access to data buffer
    std::condition_variable mFrameCond; //!< Signals a new frame
    bool mNewFrame=false;               //!< Indicates if a new frame is available
    std::array<std::vector<uint8_t>,3> mFrameData; //!< Image being written, last complete image, image owned by the user
    int mWriteIdx=0;                    //!< Index of the image being written by the grabbing thread
    int mReadyIdx=1;                    //!< Index of the last complete image
    int mUserIdx=2;                     //!< Index of the image returned to the user
    uint64_t mReadyFrameId=0;           //!< ID of the last complete frame
    uint64_t mReadyTimestamp=0;         //!< Timestamp of the last complete frame
    std::shared_ptr<detail::SyncLink> mSyncLink; //!< Shared with the synchronized SensorCapture object
    // <---- Frames shared with the grabbing thread

    Frame mLastFrame;                   //!< Frame returned to the user
    size_t mFrameSize = 0;              //!< Size of an image in bytes

    uint8_t mBufCount = 0;              //!< UVC buffer count
    struct UVCBuffer *mBuffers = nullptr;  //!< UVC buffers

    int mGainSegMax=0;                  //!< Maximum value of the raw gain to be used for conversion
    int mExpoureRawMax=0;               //!< Maximum value of the raw exposure to be used for conversion

    std::thread mGrabThread;            //!< The video grabbing thread

    // ----> Registers logging
    std::atomic<bool> mLogEnable{false};
    std::string mLogFilenameLeft;
    std::string mLogFilenameRight;
    std::ofstream mLogFileLeft;
    std::ofstream mLogFileRight;
    int mLogFrameSkip=10;
    // <---- Registers logging
};

}

}

/** \example zed_oc_video_example.cpp
 * Example of how to use the VideoCapture class to get raw video frames and show the stream on screen using the
 * OpenCV library.
 */

/** \example zed_oc_control_example.cpp
 * Example of how to use the VideoCapture class to get raw video frames and control the camera
 * parameters.
 */

/** \example zed_oc_rectify_example.cpp
 * Example of how to use the VideoCapture class to get and rectify raw video frames downloading
 * calibration parameters from Stereolabs servers.
 */

/** \example zed_oc_depth_example.cpp
 * Example of how to use the VideoCapture class to get and rectify raw video frames downloading
 * calibration parameters from Stereolabs servers and then use OpenCV and T-API to extract the
 * disparity map, the depth map, and finally generate the RGB point cloud.
 */

/** \example zed_oc_tune_stereo_sgbm.cpp
 * Example of how to use the VideoCapture class and OpenCV GUI controls to tune the depth
 * extraction process.
 */

#endif // VIDEOCAPTURE_HPP

