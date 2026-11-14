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

#ifndef SENSORCAPTURE_HPP
#define SENSORCAPTURE_HPP

#include "defines.hpp"
#include "sensorcapture_def.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Opaque handle of the hidapi library
struct hid_device_;

namespace sl_oc {

namespace detail {
struct SyncLink;
}

namespace video {
class VideoCapture;
}

namespace sensors {

namespace data {

/*!
 * \brief Contains the acquired Imu data
 */
struct SL_OC_EXPORT Imu
{
    // Validity of the IMU sensor data
    typedef enum _imu_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } ImuStatus;

    ImuStatus valid = NOT_PRESENT;     //!< Indicates if IMU data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float aX = 0.f;         //!< Acceleration along X axis in m/s²
    float aY = 0.f;         //!< Acceleration along Y axis in m/s²
    float aZ = 0.f;         //!< Acceleration along Z axis in m/s²
    float gX = 0.f;         //!< Angular velocity around X axis in °/s
    float gY = 0.f;         //!< Angular velocity around Y axis in °/s
    float gZ = 0.f;         //!< Angular velocity around Z axis in °/s
    float temp = 0.f;       //!< Sensor temperature in °C
    bool sync = false;      //!< Indicates if IMU data are synchronized with a video frame
};

/*!
 * \brief Contains the acquired Magnetometer data
 */
struct SL_OC_EXPORT Magnetometer
{
    // Validity of the magnetometer sensor data
    typedef enum _mag_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } MagStatus;

    MagStatus valid = NOT_PRESENT;     //!< Indicates if Magnetometer data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float mX = 0.f;         //!< Magnetic field along X axis in uT
    float mY = 0.f;         //!< Magnetic field along Y axis in uT
    float mZ = 0.f;         //!< Magnetic field along Z axis in uT
};

/*!
 * \brief Contains the acquired Environment data
 */
struct SL_OC_EXPORT Environment
{
    // Validity of the environmental sensor data
    typedef enum _env_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } EnvStatus;

    EnvStatus valid = NOT_PRESENT;     //!< Indicates if Environmental data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float temp = 0.f;       //!< Sensor temperature in °C
    float press = 0.f;      //!< Atmospheric pressure in hPa
    float humid = 0.f;      //!< Humidity in %rH
};

/*!
 * \brief Contains the acquired Camera Temperature data
 */
struct SL_OC_EXPORT Temperature
{
    typedef enum _temp_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } TempStatus;

    TempStatus valid = NOT_PRESENT;     //!< Indicates if camera temperature data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float temp_left = 0.f;  //!< Temperature of the left CMOS camera sensor
    float temp_right = 0.f; //!< Temperature of the right CMOS camera sensor
};

}

/*!
 * \brief The SensorCapture class provides sensor grabbing functions for the Stereolabs ZED Mini, ZED 2 and ZED 2i
 *        camera models
 *
 * Data are received by an internal thread. Timestamps are expressed in the same time base used by
 * \ref video::VideoCapture (see \ref sl_oc::steadyToTimestamp): the clock of the camera is continuously aligned to
 * the clock of the host, with or without a video stream.
 *
 * \note The functions that return a reference (`getLast...`) must be called by one thread at a time: the returned
 *       data stay valid and unchanged until the next call of the same function.
 */
class SL_OC_EXPORT SensorCapture
{
public:
    /*!
     * \brief The default constructor
     * \param verbose_lvl enable useful information to debug the class behaviours while running
     */
    SensorCapture( sl_oc::VERBOSITY verbose_lvl=sl_oc::VERBOSITY::ERROR );

    /*!
     * \brief The class destructor
     */
    virtual ~SensorCapture();

    SensorCapture(const SensorCapture&) = delete;
    SensorCapture& operator=(const SensorCapture&) = delete;

    /*!
     * \brief Get the list of the serial number of all the available devices
     * \param refresh if true USB device tree is parsed to search for modifications (new device connected/disconnected)
     * \return a vector containing the serial number of all the available devices
     */
    std::vector<int> getDeviceList(bool refresh=false);

    /*!
     * \brief Open a connection to the MCU of a ZED Mini, ZED 2 or ZED 2i camera using the specified serial number
     *        or searching for the first available device
     * \param sn Serial Number of the camera. Use `-1` to connect to the first available device
     * \return returns true if the connection is correctly established and the data stream is started
     * \note A connection already open is closed before opening the new one
     */
    bool initializeSensors( int sn=-1 );

    /*!
     * \brief Stop the data stream and close the connection. It is safe to call it more than once.
     */
    void close();

    /*!
     * \brief Check if the connection is open and data are still received
     * \return false if the device was never opened, has been closed, or has been disconnected
     */
    bool isOpened() const;

    /*!
     * \brief Get the MCU firmware version in form [fw_major].[fw_minor]
     * \param fw_major the major firmware version number
     * \param fw_minor the minor firmware version number
     */
    void getFirmwareVersion( uint16_t& fw_major, uint16_t& fw_minor );

    /*!
     * \brief Retrieve the serial number of the connected camera
     * \return the serial number of the connected camera, -1 if not connected
     */
    int getSerialNumber();

    /*!
     * \brief Get the last received IMU data
     * \param timeout_usec maximum time to wait for new data, in microseconds
     * \return returns a reference to the last received data. `valid` is `NEW_VAL` only if new data arrived
     *         after the previous call.
     * \note Only the most recent sample is returned: use \ref getIMUData to receive every sample.
     */
    const data::Imu& getLastIMUData(uint64_t timeout_usec=1500);

    /*!
     * \brief Get all the IMU data received after the previous call of this function, without losing samples
     * \param timeout_usec maximum time to wait for at least one sample, in microseconds
     * \return the samples, oldest first. Empty if no sample arrived before the timeout.
     * \note Up to 2 seconds of data are kept if the function is not called: older samples are discarded.
     */
    std::vector<data::Imu> getIMUData(uint64_t timeout_usec=10000);

    /*!
     * \brief Get the last received Magnetometer data
     * \param timeout_usec maximum time to wait for new data, in microseconds
     * \return returns a reference to the last received data.
     */
    const data::Magnetometer& getLastMagnetometerData(uint64_t timeout_usec=100);

    /*!
     * \brief Get the last received Environment data
     * \param timeout_usec maximum time to wait for new data, in microseconds
     * \return returns a reference to the last received data.
     */
    const data::Environment& getLastEnvironmentData(uint64_t timeout_usec=100);

    /*!
     * \brief Get the last received camera sensors temperature data
     * \param timeout_usec maximum time to wait for new data, in microseconds
     * \return returns a reference to the last received data.
     */
    const data::Temperature& getLastCameraTemperatureData(uint64_t timeout_usec=100);

    /*!
     * \brief Perform a SW reset of the Sensors Module. To be called in case one of the sensors stops to work correctly.
     *
     * \param serial_number The serial number of the device to be reset (0 to reset the first available)
     * \return true if successful
     *
     * \note Only available for ZED 2 and ZED 2i
     * \note After the reset a new \ref SensorCapture connection is required
     * \note The Sensors Module reset automatically performs a reset of the Video Module, so a new \ref video::VideoCapture
     *  connection is required
     */
    static bool resetSensorModule(int serial_number=0);

    /*!
     * \brief Perform a reset of the video module without resetting the sensor module. To be called in case the Video
     * module stops to work correctly.
     *
     * \param serial_number The serial number of the device to be reset (0 to reset the first available)
     * \return true if successful
     *
     * \note Only available for ZED 2 and ZED 2i
     * \note After the reset a new \ref video::VideoCapture connection is required
     */
    static bool resetVideoModule(int serial_number=0);

private:
    friend class video::VideoCapture;

    static bool searchForConnectedDev(int* serial_number, unsigned short* found_pid); //!< Search for a device and returns its pid and serial number

    void grabThreadFunc();              //!< The sensor data grabbing thread function
    void pingThreadFunc();              //!< The thread that keeps the data stream alive

    bool open(uint16_t pid, int serial_number); //!< Open the USB connection

    int enumerateDevices();             //!< Populates the  mSlDevPid map with serial number and PID of the available devices

    // ----> USB commands to MCU
    bool enableDataStream(bool enable); //!< Enable/Disable the data stream
    bool isDataStreamEnabled();         //!< Check if the data stream is enabled
    bool sendPing();                    //!< Send a ping  each second (before 6 seconds) to keep data streaming alive
    // <---- USB commands to MCU

    void setSyncLink(std::shared_ptr<detail::SyncLink> link); //!< Called by VideoCapture to share the frame sync signals

    uint64_t updateTimestamp(uint64_t mcu_ts_nsec, uint64_t arrival_ts); //!< Convert a MCU timestamp to the host time base

private:
    int mVerbose=0;                     //!< Verbose status

    std::atomic<bool> mInitialized{false};  //!< Indicates if the MCU has been initialized
    std::atomic<bool> mStopCapture{false};  //!< Indicates if the grabbing thread must be stopped
    std::atomic<bool> mDeviceLost{false};   //!< Indicates if the device stopped to reply

    std::map<int,uint16_t> mSlDevPid;   //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their serial number
    std::map<int,uint16_t> mSlDevFwVer; //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their firmware version

    hid_device_* mDevHandle = nullptr;  //!< Hidapi device handler
    int mDevSerial = -1;                //!< Serial number of the connected device
    int mDevFwVer = -1;                 //!< FW version of the connected device
    unsigned short mDevPid = 0;         //!< Product ID of the connected device

    std::thread mGrabThread;            //!< The grabbing thread
    std::thread mPingThread;            //!< The thread that keeps the data stream alive
    std::mutex mPingMutex;              //!< Used to stop the ping thread
    std::condition_variable mPingCond;  //!< Used to stop the ping thread

    // ----> Data shared with the grabbing thread, protected by mDataMutex
    std::mutex mDataMutex;
    std::condition_variable mDataCond;

    data::Imu mLastIMUData;             //!< Contains the last received IMU data
    data::Magnetometer mLastMagData;    //!< Contains the last received Magnetometer data
    data::Environment mLastEnvData;     //!< Contains the last received Environmental data
    data::Temperature mLastCamTempData; //!< Contains the last received camera sensors temperature data

    bool mNewIMUData=false;             //!< Indicates if new  IMU data are available
    bool mNewMagData=false;             //!< Indicates if new  MAG data are available
    bool mNewEnvData=false;             //!< Indicates if new  ENV data are available
    bool mNewCamTempData=false;         //!< Indicates if new  CAM_TEMP data are available

    std::deque<data::Imu> mImuQueue;    //!< IMU data not yet retrieved with \ref getIMUData

    std::shared_ptr<detail::SyncLink> mSyncLink; //!< Shared with the synchronized VideoCapture object
    // <---- Data shared with the grabbing thread

    // ----> Data returned to the user, only modified by the `getLast...` functions
    data::Imu mUserIMUData;
    data::Magnetometer mUserMagData;
    data::Environment mUserEnvData;
    data::Temperature mUserCamTempData;
    // <---- Data returned to the user

    // ----> Conversion of the MCU timestamps to the host time base (grabbing thread only)
    bool mTsValid=false;                //!< False until the first sample, or after a discontinuity
    uint64_t mTsAnchor=0;               //!< Host timestamp of the reference sample [nsec]
    double mTsRel=0.0;                  //!< Time elapsed from the reference sample [nsec]
    uint64_t mTsLastMcu=0;              //!< MCU timestamp of the previous sample [nsec]
    uint64_t mTsLastOut=0;              //!< Timestamp of the previous sample [nsec]
    double mTsScale=1.0;                //!< Ratio between the host clock and the MCU clock
    double mTsSlew=0.0;                 //!< Temporary rate correction used to remove the measured offset
    uint64_t mTsWinStart=0;             //!< Start of the current measurement window [nsec]
    int64_t mTsWinMin=0;                //!< Smallest delay between timestamp and arrival in the current window [nsec]
    bool mTsWinHasMin=false;            //!< Indicates if the current window contains at least one measure
    int64_t mTsPrevMin=0;               //!< Result of the previous window [nsec]
    int64_t mTsPrevCorr=0;              //!< Correction applied during the current window [nsec]
    int mTsWinCount=0;                  //!< Number of completed windows from the last reference sample
    // <---- Conversion of the MCU timestamps
};

}
}

/** \example zed_oc_sensors_example.cpp
 * Example of how to use the SensorCapture class to get the raw sensors data at the maximum available
 * frequency.
 */

/** \example zed_oc_sync_example.cpp
 * Example of how to get synchronized video and sensors data from
 * the VideoCapture class and the SensorCapture class.
 */

#endif // SENSORCAPTURE_HPP
