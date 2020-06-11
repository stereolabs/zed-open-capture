///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2020, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// ----> Includes
#include "sensorcapture.hpp"

#include <unistd.h> // for usleep
#include <iostream>
#include <iomanip>
// <---- Includes

// The main function
int main(int argc, char *argv[])
{
    // Set the verbose level
    sl_oc::VERBOSITY verbose = sl_oc::VERBOSITY::INFO;

    // Create a SensorCapture object
    sl_oc::sensors::SensorCapture sens(verbose);

    // ----> Get a list of available camera with sensor
    std::vector<int> devs = sens.getDeviceList();

    if( devs.size()==0 )
    {
        std::cerr << "No available ZED Mini or ZED2 cameras" << std::endl;
        return EXIT_FAILURE;
    }
    // <---- Get a list of available camera with sensor

    // ----> Inizialize the sensors
    if( !sens.initializeSensors( devs[0] ) )
    {
        std::cerr << "Connection failed" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Video Capture connected to camera sn: " << sens.getSerialNumber() << std::endl;
    // <---- Inizialize the sensors

    // ----> Get FW version information
    uint16_t fw_maior;
    uint16_t fw_minor;

    sens.getFirmwareVersion( fw_maior, fw_minor );

    std::cout << " * Firmware version: " << std::to_string(fw_maior) << "." << std::to_string(fw_minor) << std::endl;
    // <---- Get FW version information

    // ----> Variables to calculate sensors frequencies
    uint64_t last_imu_ts = 0;
    uint64_t last_mag_ts = 0;
    uint64_t last_env_ts = 0;
    uint64_t last_cam_temp_ts = 0;
    // <---- Variables to calculate sensors frequencies

    // Lets grab 4000 sensors data (about 10 seconds
    int count = 0;
    while(++count<4000)
    {
        // ----> Get IMU data with a timeout of 5 millisecods
        const sl_oc::sensors::data::Imu imuData = sens.getLastIMUData(5000);
        if( imuData.valid == sl_oc::sensors::data::Imu::NEW_VAL  )
        {
            std::cout << "**** New IMU data ****" << std::endl;
            std::cout << " * Timestamp: " << imuData.timestamp << " nsec" << std::endl;
            if(last_imu_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(imuData.timestamp-last_imu_ts) << " Hz" << std::endl;
            }
            last_imu_ts = imuData.timestamp;
            std::cout << " * Accelerations [m/s²]: " << imuData.aX << " " << imuData.aY << " " << imuData.aZ << std::endl;
            std::cout << " * Angular Velocities [°/s]: " << imuData.gX << " " << imuData.gY << " " << imuData.gZ << std::endl;
        }
        // <---- Get IMU data with a timeout of 5 millisecods

        // ----> Get Magnetometer data with a timeout of 100 microseconds to not slow down fastest data (IMU)
        const sl_oc::sensors::data::Magnetometer magData = sens.getLastMagnetometerData(100);
        if( magData.valid == sl_oc::sensors::data::Magnetometer::NEW_VAL )
        {
            std::cout << "**** New Magnetometer data ****" << std::endl;
            std::cout << " * Timestamp: " << magData.timestamp << " nsec" << std::endl;
            if(last_mag_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(magData.timestamp-last_mag_ts) << " Hz" << std::endl;
            }
            last_mag_ts = magData.timestamp;
            std::cout << " * Magnetic field [uT]: " << magData.mX << " " << magData.mY << " " << magData.mZ << std::endl;
        }
        // <---- Get Magnetometer data with a timeout of 100 microseconds to not slow down fastest data (IMU)

        // ----> Get Environment data with a timeout of 100 microseconds to not slow down fastest data (IMU)
        const sl_oc::sensors::data::Environment envData = sens.getLastEnvironmentData(100);
        if( envData.valid == sl_oc::sensors::data::Environment::NEW_VAL )
        {
            std::cout << "**** New Environment data ****" << std::endl;
            std::cout << " * Timestamp: " << envData.timestamp << " nsec" << std::endl;
            if(last_env_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(envData.timestamp-last_env_ts) << " Hz" << std::endl;
            }
            last_env_ts = envData.timestamp;
            std::cout << " * Pressure [hPa]: " << envData.press << std::endl;
            std::cout << " * Temperature [°C]: " << envData.temp << std::endl;
            std::cout << " * Relative Humidity [%rH]: " << envData.humid << std::endl;
        }
        // <---- Get Environment data with a timeout of 100 microseconds to not slow down fastest data (IMU)

        // ----> Get Temperature data with a timeout of 100 microseconds to not slow down fastest data (IMU)
        const sl_oc::sensors::data::Temperature tempData = sens.getLastCameraTemperatureData(100);
        if( tempData.valid == sl_oc::sensors::data::Temperature::NEW_VAL )
        {
            std::cout << "**** New Camera Sensors Temperature data ****" << std::endl;
            std::cout << " * Timestamp: " << tempData.timestamp << " nsec" << std::endl;
            if(last_cam_temp_ts!=0)
            {
                std::cout << " * Frequency: " << 1e9/static_cast<float>(tempData.timestamp-last_cam_temp_ts) << " Hz" << std::endl;
            }
            last_cam_temp_ts = tempData.timestamp;
            std::cout << " * Left Camera [°C]: " << tempData.temp_left << std::endl;
            std::cout << " * Right Camera [°C]: " << tempData.temp_right << std::endl;
        }
        // <---- Get Temperature data with a timeout of 100 microseconds to not slow down fastest data (IMU)
    }

    return EXIT_SUCCESS;
}
