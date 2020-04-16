#ifndef SENSORCAPTURE_HPP
#define SENSORCAPTURE_HPP

#include "defines.hpp"

namespace zed {

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
    SensorCapture();

    /*!
     * \brief ~SensorCapture  destructor
     */
    virtual ~SensorCapture();

private:

};

}
#endif // SENSORCAPTURE_HPP
