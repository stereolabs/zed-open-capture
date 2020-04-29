#include "sensorcapture.hpp"

#include <iostream>
#include <iomanip>

int main(int argc, char *argv[])
{
    // Create Sensor Capture
    sl_drv::SensorCapture sens(true);

    std::vector<int> devs = sens.getDeviceList();

    if( devs.size()==0 )
    {
        std::cerr << "No available ZED Mini or ZED2 cameras" << std::endl;
        return EXIT_FAILURE;
    }

    if( !sens.init( devs[0] ) )
    {
        std::cerr << "Connection failed" << std::endl;
        return EXIT_FAILURE;
    }

    uint16_t fw_maior;
    uint16_t fw_minor;

    sens.getFwVersion( fw_maior, fw_minor );

    std::cout << " * Firmware version: " << std::to_string(fw_maior) << "." << std::to_string(fw_minor) << std::endl;

    uint64_t last_imu_ts = 0;
    uint64_t last_mag_ts = 0;
    uint64_t last_env_ts = 0;
    uint64_t last_cam_temp_ts = 0;

    while(1)
    {
        const sl_drv::SensImuData* imuData = sens.getLastIMUData(10);
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

        const sl_drv::SensMagData* magData = sens.getLastMagData(1);
        if( magData && magData->valid == sl_drv::SensMagData::NEW_VAL )
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

        const sl_drv::SensEnvData* envData = sens.getLastEnvData(1);
        if( envData && envData->valid == sl_drv::SensEnvData::NEW_VAL )
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

        const sl_drv::SensCamTempData* tempData = sens.getLastCamTempData(1);
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
