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

/*!
 * \brief The struct containing the acquired IMU data
 */
struct SL_DRV_EXPORT IMU
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
};

/*!
 * \brief The struct containing the acquired Magnetometer data
 */
struct SL_DRV_EXPORT MAG
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
struct SL_DRV_EXPORT ENV
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
struct SL_DRV_EXPORT CAM_TEMP
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
    SensorCapture(SensorParams params=_sensor_params());

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
     * \return returns true if the camera is connection is correctly estabilished
     */
    bool init( int sn=-1 );

    /*!
     * \brief Get the last received IMU data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const IMU* getLastIMUData(uint64_t timeout_msec=5);

    /*!
     * \brief Get the last received Magnetometer data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const MAG* getLastMagData(uint64_t timeout_msec=1);

    /*!
     * \brief Get the last received Environment data
     * \param timeout_msec data grabbing timeout in milliseconds.
     * \return returns the last received data as pointer.
     *
     * \note Do not delete the received data
     */
    const ENV* getLastEnvData(uint64_t timeout_msec=1);


private:
    /*!
     * \brief grabThreadFunc The sensor data grabbing thread function
     */
    void grabThreadFunc();

    bool startCapture();
    void reset();

    int enumerateDevices();

    bool enableDataStream(bool enable);
    bool isDataStreamEnabled();
    bool sendPing(); //!< Send a ping  each second (before 6 seconds) to keep data streaming alive

private:
    // Flags
    bool mNewIMUData=false;         //!< Indicates if new IMU data are available
    bool mNewMagData=false;         //!< Indicates if new MAG data are available
    bool mNewEnvData=false;         //!< Indicates if new ENV data are available
    bool mInitialized = false;  //!< Inficates if the MCU has been initialized
    bool mStopCapture = false;  //!< Indicates if the grabbing thread must be stopped
    bool mGrabRunning = false;  //!< Indicates if the grabbing thread is running

    std::map<int,uint16_t> mSlDevPid; //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their serial number

    hid_device* mDevHandle = nullptr; //!< Hidapi device handler

    SensorParams mParams;       //!< Sensor capture parameters

    IMU mLastIMUData;           //!< Contains the last received IMU data
    MAG mLastMagData;           //!< Contains the last received Magnetometer data
    ENV mLastEnvData;           //!< Contains the last received Environmental data
    CAM_TEMP mLastCamTempData;  //!< Contains the last received camera sensors temperature data

    std::thread mGrabThread;    //!< The grabbing thread

    std::mutex mIMUMutex;       //!< Mutex for safe access to IMU data buffer
    std::mutex mMagMutex;       //!< Mutex for safe access to MAG data buffer
    std::mutex mEnvMutex;       //!< Mutex for safe access to ENV data buffer
};

}
#endif // SENSORCAPTURE_HPP
