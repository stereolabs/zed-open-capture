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

    uint64_t last_imu_ts = 0;
    uint64_t last_mag_ts = 0;
    uint64_t last_env_ts = 0;
    uint64_t last_cam_temp_ts = 0;

    while(1)
    {
        const sl_drv::IMU* imuData = sens.getLastIMUData(10);
        if( imuData && imuData->valid )
        {
            std::cout << "**** New IMU data ****" << std::endl;
            std::cout << " * Timestamp: " << imuData->timestamp << " nsec" << std::endl;
            if(last_imu_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(imuData->timestamp-last_imu_ts) << " Hz" << std::endl;
            }
            last_imu_ts = imuData->timestamp;
            std::cout << " * Accelerations [m/s²]: " << imuData->aX << " " << imuData->aY << " " << imuData->aZ << std::endl;
            std::cout << " * Angular Velocities [°/s]: " << imuData->gX << " " << imuData->gY << " " << imuData->gZ << std::endl;
        }

        const sl_drv::MAG* magData = sens.getLastMagData(1);
        if( magData && magData->valid == sl_drv::MAG::NEW_VAL )
        {
            std::cout << "**** New Magnetometer data ****" << std::endl;
            std::cout << " * Timestamp: " << magData->timestamp << " nsec" << std::endl;
            if(last_mag_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(magData->timestamp-last_mag_ts) << " Hz" << std::endl;
            }
            last_mag_ts = magData->timestamp;
            std::cout << " * Magnetic field [uT]: " << magData->mX << " " << magData->mY << " " << magData->mZ << std::endl;
        }

        const sl_drv::ENV* envData = sens.getLastEnvData(1);
        if( envData && envData->valid == sl_drv::ENV::NEW_VAL )
        {
            std::cout << "**** New Environment data ****" << std::endl;
            std::cout << " * Timestamp: " << envData->timestamp << " nsec" << std::endl;
            if(last_env_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(envData->timestamp-last_env_ts) << " Hz" << std::endl;
            }
            last_env_ts = envData->timestamp;
            std::cout << " * Pressure [hPa]: " << envData->press << std::endl;
            std::cout << " * Temperature [°C]: " << envData->temp << std::endl;
            std::cout << " * Relative Humidity [%rH]: " << envData->humid << std::endl;
        }

        const sl_drv::CAM_TEMP* tempData = sens.getLastCamTempData(1);
        if( tempData && tempData->valid )
        {
            std::cout << "**** New Camera Sensors Temperature data ****" << std::endl;
            std::cout << " * Timestamp: " << tempData->timestamp << " nsec" << std::endl;
            if(last_cam_temp_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(tempData->timestamp-last_cam_temp_ts) << " Hz" << std::endl;
            }
            last_cam_temp_ts = tempData->timestamp;
            std::cout << " * Left Camera [°C]: " << tempData->temp_left << std::endl;
            std::cout << " * Right Camera [°C]: " << tempData->temp_right << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
