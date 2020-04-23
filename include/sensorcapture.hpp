#ifndef SENSORCAPTURE_HPP
#define SENSORCAPTURE_HPP

#include "defines.hpp"
#include "sensorcapture_def.hpp"

#include <vector>
#include <map>

#include "hidapi.h"

namespace sl_drv {

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
    int enumerateDevices();

private:
    SensorParams mParams; //!< Sensor capture parameters

    std::map<int,uint16_t> mSlDevPid; //!< All the available Stereolabs MCU (ZED-M and ZED2) product IDs associated to their serial number

    hid_device* mDevHandle = nullptr; //!< Hidapi device handler
};

}
#endif // SENSORCAPTURE_HPP
