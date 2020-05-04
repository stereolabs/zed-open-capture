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

// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_drv::RESOLUTION res );

// Sensor acquisition runs at 400Hz, so it must be executed in a different thread
void getSensorThreadFunc(sl_drv::SensorCapture* sensCap);

// ----> Shared data
std::mutex imuMutex;
std::string imuTsStr;
std::string imuAccelStr;
std::string imuGyroStr;
std::string imuSyncStr;
bool sensThreadStop=false;
// <---- Shared data

int main(int argc, char *argv[])
{
    bool verbose = false;

    // ----> Video parameters
    sl_drv::VideoParams params;
    params.res = sl_drv::RESOLUTION::HD720;
    params.fps = sl_drv::FPS::FPS_60;
    params.verbose = verbose;
    // <---- Video parameters

    // ----> Create Video Capture
    sl_drv::VideoCapture videoCap(params);
    if( !videoCap.init(-1) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "Try to enable verbose to get more info" << std::endl;

        return EXIT_FAILURE;
    }
    int camSn = videoCap.getSerialNumber();

    std::cout << "Video Capture connected to camera sn: " << camSn << std::endl;
    // <---- Create Video Capture

    // ----> Create Sensors Capture
    sl_drv::SensorCapture sensCap(verbose);
    if( !sensCap.init(camSn) )
    {
        std::cerr << "Cannot open sensors capture" << std::endl;
        std::cerr << "Try to enable verbose to get more info" << std::endl;

        return EXIT_FAILURE;
    }
    std::cout << "Sensors Capture connected to camera sn: " << sensCap.getSerialNumber() << std::endl;

    std::thread sensThread(getSensorThreadFunc,&sensCap);
    // <---- Create Sensors Capture

    // ----> Enable synchronization
    videoCap.enableSensorSync(&sensCap);
    // <---- Enable synchronization

    // ----> Init RGB frame
    int w,h;
    videoCap.getFrameSize(w,h);
    cv::Mat frameBGR = cv::Mat::zeros( w, h, CV_8UC3 );
    // <---- Init RGB frame

    while (1)
    {
        // ----> Get Video frame
        // Get last available frame
        const sl_drv::Frame* frame = videoCap.getLastFrame(1);

        // If the frame is valid we can update it
        std::stringstream videoTs;
        if(frame != nullptr)
        {
            // ----> Video Debug information
            static uint64_t last_ts=0;

            videoTs << std::fixed << std::setprecision(9) << "Video timestamp: " << static_cast<double>(frame->timestamp)/1e9<< " sec" ;
            if( last_ts!=0 )
            {
                videoTs << std::fixed << std::setprecision(1)  << " [" << 1e9/static_cast<float>(frame->timestamp-last_ts) << " Hz]";
            }
            last_ts = frame->timestamp;
            // <---- Video Debug information

            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame->height, frame->width, CV_8UC2, frame->data );
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization
        }
        // <---- Get Video frame

        // ----> Display frame with info

        int font = cv::FONT_HERSHEY_COMPLEX;

        // Video info
        cv::putText( frameBGR, videoTs.str(), cv::Point(10,35), font, 1, cv::Scalar(150,150,150), 2 );

        // IMU info
        imuMutex.lock();
        cv::putText( frameBGR, imuTsStr, cv::Point(10,70), font, 1, cv::Scalar(150,150,150), 2 );
        cv::putText( frameBGR, imuAccelStr, cv::Point(10,105), font, 1, cv::Scalar(150,150,150), 2 );
        cv::putText( frameBGR, imuGyroStr, cv::Point(10,140), font, 1, cv::Scalar(150,150,150), 2 );
        cv::putText( frameBGR, imuSyncStr, cv::Point(10,175), font, 1, cv::Scalar(150,150,150), 2 );
        imuMutex.unlock();

        // Show frame
        showImage( "Stream RGB", frameBGR, params.res );
        // <---- Display frame with info

        // ----> Keyboard handling
        int key = cv::waitKey( 1 );

        if( key != -1 )
        {
            //std::cout << key << std::endl;

            if(key=='q' || key=='Q') // Quit
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

void showImage( std::string name, cv::Mat& img, sl_drv::RESOLUTION res )
{
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

    cv::imshow( name, resized );
}

void getSensorThreadFunc(sl_drv::SensorCapture* sensCap)
{
    sensThreadStop = false;
    uint64_t last_imu_ts = 0;

    while(!sensThreadStop)
    {
        // ----> Get IMU data
        const sl_drv::SensImuData* imuData = sensCap->getLastIMUData(1);

        if(imuData && imuData->valid)
        {
            std::stringstream timestamp;
            std::stringstream accel;
            std::stringstream gyro;
            std::stringstream sync;

            timestamp << std::fixed << std::setprecision(9) << "IMU timestamp:   " << static_cast<double>(imuData->timestamp)/1e9<< " sec" ;
            if(last_imu_ts!=0)
            {
                timestamp << std::fixed << std::setprecision(1)  << " [" << 1e9/static_cast<float>(imuData->timestamp-last_imu_ts) << " Hz]";
            }
            last_imu_ts = imuData->timestamp;

            accel << std::fixed << std::showpos << std::setprecision(4) << " * Accel: " << imuData->aX << " " << imuData->aY << " " << imuData->aZ << " [m/s^2]";
            gyro << std::fixed << std::showpos << std::setprecision(4) << " * Gyro: " << imuData->gX << " " << imuData->gY << " " << imuData->gZ << " [deg/s]";
            sync << " * Frame sync: " << (imuData->sync?"YES":"NO");

            imuMutex.lock();
            imuTsStr = timestamp.str();
            imuAccelStr = accel.str();
            imuGyroStr = gyro.str();
            imuSyncStr = sync.str();
            imuMutex.unlock();
        }
        // <---- Get IMU data
    }
}
