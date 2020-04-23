#ifndef SENSORCAPTURE_DEF_HPP
#define SENSORCAPTURE_DEF_HPP

namespace sl_drv {

typedef struct _sensor_params
{
    _sensor_params() {
        freq=400.0f;
        verbose=false;
    }

    float freq; // Acquisition frequency (max 400 Hz)
    bool verbose;
} SensorParams;

}

#endif // SENSORCAPTURE_DEF_HPP
