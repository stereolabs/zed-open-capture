// Minimal application that uses the installed library. It does not need a camera.

#include <cstdlib>
#include <iostream>

#ifdef VIDEO_MOD_AVAILABLE
#include <videocapture.hpp>
#endif

#ifdef SENSORS_MOD_AVAILABLE
#include <sensorcapture.hpp>
#endif

int main()
{
    std::cout << "ZED Open Capture v" << ZED_OC_MAJOR_VERSION << "." << ZED_OC_MINOR_VERSION << "." << ZED_OC_PATCH_VERSION << std::endl;

#ifdef VIDEO_MOD_AVAILABLE
    sl_oc::video::VideoParams params;
    params.verbose = sl_oc::VERBOSITY::NONE;
    sl_oc::video::VideoCapture cap(params);
    std::cout << "Cameras: " << sl_oc::video::VideoCapture::getDeviceList().size() << std::endl;
#endif

#ifdef SENSORS_MOD_AVAILABLE
    sl_oc::sensors::SensorCapture sens(sl_oc::VERBOSITY::NONE);
    std::cout << "Sensors modules: " << sens.getDeviceList().size() << std::endl;
#endif

    return EXIT_SUCCESS;
}
