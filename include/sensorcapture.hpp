#ifndef SENSORCAPTURE_HPP
#define SENSORCAPTURE_HPP

#include "defines.hpp"
#include "sensorcapture_def.hpp"

#include <thread>
#include <vector>
#include <map>

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
    float temp;             //!< Temperature in °C
};

/*!
 * \brief The struct containing the acquired Magnetometer data
 */
struct SL_DRV_EXPORT MAG
{
    // Validity of the magnetometer sensor data
    typedef enum _mag_status {
        MAG_SENS_NOT_PRESENT = 0,
        MAG_SENS_OLD_VAL = 1,
        MAG_SENS_NEW_VAL = 2
    } MagStatus;

    MagStatus valid = MAG_SENS_NOT_PRESENT;     //!< Indicates if Magnetometer data are valid
    uint64_t timestamp = 0; //!< Timestamp in nanoseconds
    float mX;               //!< Acceleration along X axis in uT
    float mY;               //!< Acceleration along Y axis in uT
    float mZ;               //!< Acceleration along Z axis in uT
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
    SensorParams mParams; //!< Sensor capture parameters

    std::map<int,uint16_t> mSlDevPid; //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their serial number

    hid_device* mDevHandle = nullptr; //!< Hidapi device handler

    bool mInitialized = false;  //!< Inficates if the MCU has been initialized
    bool mNewData = false;      //!< Indicates if new data are available
    bool mStopCapture = false;  //!< Indicates if the grabbing thread must be stopped
    bool mGrabRunning = false;  //!< Indicates if the grabbing thread is running

    IMU mLastIMUData;           //!< Contains the last received IMU data
    MAG mLastMAGData;           //!< Contains the last received Magnetometer data

    std::thread mGrabThread;    //!< The grabbing thread
};

}
#endif // SENSORCAPTURE_HPP
