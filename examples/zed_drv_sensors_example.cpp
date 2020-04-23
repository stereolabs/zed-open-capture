#include "sensorcapture.hpp"

#include <iostream>
#include <iomanip>

int main(int argc, char *argv[])
{
    sl_drv::SensorParams params;
    params.verbose = true;
    params.freq = 400.f;

    // Create Sensor Capture
    sl_drv::SensorCapture sens(params);

    std::vector<int> devs = sens.getDeviceList();

    if( devs.size()==0 )
        return EXIT_SUCCESS;

    sens.init( devs[0] );

    return EXIT_SUCCESS;
}
