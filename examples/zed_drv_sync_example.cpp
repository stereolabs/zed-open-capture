// ----> Includes
#include "videocapture.hpp"
#include "sensorcapture.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
// <---- Includes

// ----> Functions
// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_drv::RESOLUTION res );

// Sensor acquisition runs at 400Hz, so it must be executed in a different thread
void getSensorThreadFunc(sl_drv::SensorCapture* sensCap);
// <---- Functions

// ----> Global variables
std::mutex imuMutex;
std::string imuTsStr;
std::string imuAccelStr;
std::string imuGyroStr;

bool sensThreadStop=false;
uint64_t mcu_sync_ts=0;
// <---- Global variables

// The main function
int main(int argc, char *argv[])
{
    // Set the verbose level
    bool verbose = false;

    // ----> 1) Set the video parameters
    sl_drv::VideoParams params;
    params.res = sl_drv::RESOLUTION::HD720;
    params.fps = sl_drv::FPS::FPS_60;
    params.verbose = verbose;
    // <---- Video parameters

    // ----> 2) Create a Video Capture object
    sl_drv::VideoCapture videoCap(params);
    if( !videoCap.initializeVideo(-1) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "Try to enable verbose to get more info" << std::endl;

        return EXIT_FAILURE;
    }

    // Serial number of the connected camera
    int camSn = videoCap.getSerialNumber();

    std::cout << "Video Capture connected to camera sn: " << camSn << std::endl;
    // <---- Create a Video Capture object

    // ----> 4) Create a Sensors Capture object
    sl_drv::SensorCapture sensCap(verbose);
    if( !sensCap.initializeSensors(camSn) ) // Note: we use the serial number acquired by the VideoCapture object
    {
        std::cerr << "Cannot open sensors capture" << std::endl;
        std::cerr << "Try to enable verbose to get more info" << std::endl;

        return EXIT_FAILURE;
    }
    std::cout << "Sensors Capture connected to camera sn: " << sensCap.getSerialNumber() << std::endl;

    // Start the sensor capture thread. Note: since sensor data can be retrieved at 400Hz and video data frequency is
    // minor (max 100Hz), we use a separated thread for sensors.
    std::thread sensThread(getSensorThreadFunc,&sensCap);
    // <---- Create Sensors Capture

    // ----> 5) Enable video/sensors synchronization
    videoCap.enableSensorSync(&sensCap);
    // <---- Enable video/sensors synchronization

    // ----> 6) Init OpenCV RGB frame
    int w,h;
    videoCap.getFrameSize(w,h);
    cv::Mat frameBGR = cv::Mat::zeros( w, h, CV_8UC3 );
    // <---- Init OpenCV RGB frame

    // Infinite grabbing loop
    while (1)
    {
        // ----> 7) Get Video frame
        // Get last available frame
        const sl_drv::Frame* frame = videoCap.getLastFrame(1);

        // If the frame is valid we can update it
        std::stringstream videoTs;
        if(frame != nullptr)
        {
            // ----> 7.a) Video Debug information
            static uint64_t last_ts=0;

            videoTs << std::fixed << std::setprecision(9) << "Video timestamp: " << static_cast<double>(frame->timestamp)/1e9<< " sec" ;
            if( last_ts!=0 )
            {
                videoTs << std::fixed << std::setprecision(1)  << " [" << 1e9/static_cast<float>(frame->timestamp-last_ts) << " Hz]";
            }
            last_ts = frame->timestamp;
            // <---- Video Debug information

            // ----> 7.b) Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame->height, frame->width, CV_8UC2, frame->data );
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization
        }
        // <---- Get Video frame

        // ----> 8) Display frame with info
        if(frame != nullptr)
        {
            int font = cv::FONT_HERSHEY_COMPLEX;

            // Video info
            cv::putText( frameBGR, videoTs.str(), cv::Point(10,35), font, 1, cv::Scalar(200,150,150), 2 );

            // IMU info
            imuMutex.lock();
            cv::putText( frameBGR, imuTsStr, cv::Point(10,70), font, 1, cv::Scalar(150,150,200), 2 );

            // Timestamp offset info
            std::stringstream offsetStr;
            double offset = (static_cast<double>(frame->timestamp)-static_cast<double>(mcu_sync_ts))/1e9;
            offsetStr << std::fixed << std::setprecision(9) << std::showpos << "Timestamp offset: " << offset << " sec [video-sensors]";
            cv::putText( frameBGR, offsetStr.str().c_str(), cv::Point(10,105), font, 1, cv::Scalar(200,200,150), 2 );

            // Average timestamp ofset info (we wait at least 200 frames to be sure that offset is stable)
            if( frame->frame_id>200 )
            {
                static double sum=0;
                static int count=0;

                sum += offset;
                double avg_offset=sum/(++count);

                std::stringstream avgOffsetStr;
                avgOffsetStr << std::fixed << std::setprecision(9) << std::showpos << "Avg timestamp offset: " << avg_offset << " sec";
                cv::putText( frameBGR, avgOffsetStr.str().c_str(), cv::Point(10,140), font, 1, cv::Scalar(200,200,150), 2 );
            }

            // IMU values
            cv::putText( frameBGR, imuAccelStr, cv::Point(10,185), font, 1, cv::Scalar(150,150,150), 2 );
            cv::putText( frameBGR, imuGyroStr, cv::Point(10,220), font, 1, cv::Scalar(150,150,150), 2 );
            imuMutex.unlock();

            // Show frame
            showImage( "Stream RGB", frameBGR, params.res );
        }
        // <---- Display frame with info

        // ----> 9) Keyboard handling
        int key = cv::waitKey( 1 );

        if( key != -1 )
        {
            // Quit
            if(key=='q' || key=='Q')
            {
                sensThreadStop=true;
                sensThread.join();
                break;
            }
        }
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}

// Sensor acquisition runs at 400Hz, so it must be executed in a different thread
void getSensorThreadFunc(sl_drv::SensorCapture* sensCap)
{
    // Flag to stop the thread
    sensThreadStop = false;

    // Previous IMU timestamp to calculate frequency
    uint64_t last_imu_ts = 0;

    // Infinite data grabbing loop
    while(!sensThreadStop)
    {
        // ----> Get IMU data
        const sl_drv::SensImuData* imuData = sensCap->getLastIMUData(2000);

        // Process data only if valid
        if(imuData && imuData->valid /*&& imuData->sync*/) // Uncomment to use only data syncronized with the video frames
        {
            // ----> Data info to be displayed
            std::stringstream timestamp;
            std::stringstream accel;
            std::stringstream gyro;

            timestamp << std::fixed << std::setprecision(9) << "IMU timestamp:   " << static_cast<double>(imuData->timestamp)/1e9<< " sec" ;
            if(last_imu_ts!=0)
            {
                timestamp << std::fixed << std::setprecision(1)  << " [" << 1e9/static_cast<float>(imuData->timestamp-last_imu_ts) << " Hz]";
            }
            last_imu_ts = imuData->timestamp;

            accel << std::fixed << std::showpos << std::setprecision(4) << " * Accel: " << imuData->aX << " " << imuData->aY << " " << imuData->aZ << " [m/s^2]";
            gyro << std::fixed << std::showpos << std::setprecision(4) << " * Gyro: " << imuData->gX << " " << imuData->gY << " " << imuData->gZ << " [deg/s]";
            // <---- Data info to be displayed

            // Mutex to not overwrite data while diplaying them
            imuMutex.lock();

            imuTsStr = timestamp.str();
            imuAccelStr = accel.str();
            imuGyroStr = gyro.str();

            // ----> Timestamp of the synchronized data
            if(imuData->sync)
            {
                mcu_sync_ts = imuData->timestamp;
            }
            // <---- Timestamp of the synchronized data

            imuMutex.unlock();
        }
        // <---- Get IMU data
    }
}

// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_drv::RESOLUTION res )
{
    // ----> Image escaling using OpenCV
    cv::Mat resized;
    switch(res)
    {
    default:
    case sl_drv::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_drv::RESOLUTION::HD720:
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_drv::RESOLUTION::HD1080:
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    case sl_drv::RESOLUTION::HD2K:
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }
    // <---- Image rescaling using OpenCV

    // Display image
    cv::imshow( name, resized );
}
