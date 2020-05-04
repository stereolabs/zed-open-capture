#ifndef SENSORCAPTURE_HPP
#define SENSORCAPTURE_HPP

#include "defines.hpp"
#include "sensorcapture_def.hpp"

#include <thread>
#include <vector>
#include <map>
#include <mutex>

#include "hidapi.h"

namespace sl_drv {

#ifdef VIDEO_MOD_AVAILABLE
class VideoCapture;
#endif

/*!
 * \brief The struct containing the acquired IMU data
 */
struct SL_DRV_EXPORT SensImuData
{
    bool valid = false;     //!< Indicates if IMU data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float aX;               //!< Acceleration along X axis in m/s²
    float aY;               //!< Acceleration along Y axis in m/s²
    float aZ;               //!< Acceleration along Z axis in m/s²
    float gX;               //!< Angular velocity around X axis in °/s
    float gY;               //!< Angular velocity around Y axis in °/s
    float gZ;               //!< Angular velocity around > axis in °/s
    float temp;             //!< Sensor temperature in °C
    bool sync;              //!< Indicates in IMU data are synchronized with a video frame
};

/*!
 * \brief The struct containing the acquired Magnetometer data
 */
struct SL_DRV_EXPORT SensMagData
{
    // Validity of the magnetometer sensor data
    typedef enum _mag_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } MagStatus;

    MagStatus valid = NOT_PRESENT;     //!< Indicates if Magnetometer data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float mX;               //!< Acceleration along X axis in uT
    float mY;               //!< Acceleration along Y axis in uT
    float mZ;               //!< Acceleration along Z axis in uT
};

/*!
 * \brief The struct containing the acquired Environmental data
 */
struct SL_DRV_EXPORT SensEnvData
{
    // Validity of the environmental sensor data
    typedef enum _env_status {
        NOT_PRESENT = 0,
        OLD_VAL = 1,
        NEW_VAL = 2
    } EnvStatus;

    EnvStatus valid = NOT_PRESENT;     //!< Indicates if Environmental data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float temp;             //!< Sensor temperature in °C
    float press;            //!< Atmospheric pressure in hPa
    float humid;            //!< Humidity in %rH
};

/*!
 * \brief The struct containing the acquired Environmental data
 */
struct SL_DRV_EXPORT SensCamTempData
{
    bool valid = false;     //!< Indicates if camera temperature data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float temp_left;        //!< Temperature of the left CMOS camera sensor
    float temp_right;       //!< Temperature of the right CMOS camera sensor
};

/*!
 * \brief The SensorCapture class provides sensor grabbing functions
 */
class SL_DRV_EXPORT SensorCapture
{
    ZED_DRV_VERSION_ATTRIBUTE

public:    
    /*!
     * \brief SensorCapture is the default constructor
     */
    SensorCapture( bool verbose=false );

    /*!
     * \brief ~SensorCapture  destructor
     */
    virtual ~SensorCapture();

    /*!
     * \brief Get the list of the serial number of all the available devices
     * \return a vector containing the serial number of all the available devices
     */
    std::vector<int> getDeviceList();

    /*!
     * \brief Open a connection to the MCU of a ZED Mini or a ZED2 camera using the specified serial number or searching
     *        for the first available device
     * \param sn Serial Number of the camera. Use `-1` to open connect to the first available device
     * \return returns true if the connection is correctly estabilished
     */
    bool init( int sn=-1 );

    /*!
     * \brief Get the MCU firmware version in form `<fw_major>.<fw_minor>
     * \param fw_major the major firmware version number
     * \param fw_minor the minor firmware version number
     */
    void getFwVersion( uint16_t& fw_major, uint16_t& fw_minor );

    /*!
     * \brief Retrieve the serial number of the connected camera
     * \return the serial number of the connected camera
     */
    int getSerialNumber();

    /*!
     * \brief Get the last received IMU data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const SensImuData* getLastIMUData(uint64_t timeout_msec=5);

    /*!
     * \brief Get the last received Magnetometer data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const SensMagData* getLastMagData(uint64_t timeout_msec=1);

    /*!
     * \brief Get the last received Environment data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const SensEnvData* getLastEnvData(uint64_t timeout_msec=1);

    /*!
     * \brief Get the last received camera sensors temperature data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const SensCamTempData* getLastCamTempData(uint64_t timeout_msec=1);

private:
    /*!
     * \brief grabThreadFunc The sensor data grabbing thread function
     */
    void grabThreadFunc();

    bool startCapture();            //!< Start data capture thread
    void reset();                   //!< Reset  connection

    int enumerateDevices();         //!< Populates the \ref mSlDevPid map with serial number and PID of the available devices

    // ----> USB commands to MCU
    bool enableDataStream(bool enable); //!< Enable/Disable the data stream
    bool isDataStreamEnabled();         //!< Check if the data stream is enabled
    bool sendPing();                    //!< Send a ping  each second (before 6 seconds) to keep data streaming alive
    // ----> USB commands to MCU

private:
    // Flags
    bool mVerbose=false;            //!< Verbose status
    bool mNewIMUData=false;         //!< Indicates if new \ref IMU data are available
    bool mNewMagData=false;         //!< Indicates if new \ref MAG data are available
    bool mNewEnvData=false;         //!< Indicates if new \ref ENV data are available
    bool mNewCamTempData=false;     //!< Indicates if new \ref CAM_TEMP data are available

    bool mInitialized = false;  //!< Inficates if the MCU has been initialized
    bool mStopCapture = false;  //!< Indicates if the grabbing thread must be stopped
    bool mGrabRunning = false;  //!< Indicates if the grabbing thread is running

    std::map<int,uint16_t> mSlDevPid;   //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their serial number
    std::map<int,uint16_t> mSlDevFwVer; //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their firmware version

    hid_device* mDevHandle = nullptr;   //!< Hidapi device handler
    int mDevSerial = -1;                //!< Serial number of the connected device
    int mDevFwVer = -1;                 //!< FW version of the connected device

    SensImuData mLastIMUData;           //!< Contains the last received IMU data
    SensMagData mLastMagData;           //!< Contains the last received Magnetometer data
    SensEnvData mLastEnvData;           //!< Contains the last received Environmental data
    SensCamTempData mLastCamTempData;  //!< Contains the last received camera sensors temperature data

    std::thread mGrabThread;    //!< The grabbing thread

    std::mutex mIMUMutex;       //!< Mutex for safe access to IMU data buffer
    std::mutex mMagMutex;       //!< Mutex for safe access to MAG data buffer
    std::mutex mEnvMutex;       //!< Mutex for safe access to ENV data buffer
    std::mutex mCamTempMutex;   //!< Mutex for safe access to CAM_TEMP data buffer

    uint64_t mStartTs=0;        //!< Initial System Timestamp, to calculate differences [nsec]
    uint64_t mInitTs=0;         //!< Initial Device Timestamp, to calculate differences [usec]

    bool mFirstImuData=true;    //!< Used to initialize the sensor timestamp start point

#ifdef VIDEO_MOD_AVAILABLE
    friend class VideoCapture;
#endif

};

}
#endif // SENSORCAPTURE_HPP
