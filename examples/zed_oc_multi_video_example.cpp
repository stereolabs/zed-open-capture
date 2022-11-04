////////////////////////////////////////////////////////////////////////////
////
//// Copyright (c) 2021, STEREOLABS.
////
//// All rights reserved.
////
//// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////
/////////////////////////////////////////////////////////////////////////////

//// ----> Includes
#include "videocapture.hpp"
#include "ocv_display.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/opencv.hpp>
// <---- Includes

// #define TEST_FPS 1

// The main function
int main(int argc, char *argv[])
{
    // ----> Silence unused warning
    (void)argc;
    (void)argv;
    // <---- Silence unused warning

    sl_oc::video::VideoParams params;
    params.res = sl_oc::video::RESOLUTION::HD720;
    params.fps = sl_oc::video::FPS::FPS_60;

    // ----> Create Video Capture 0
    sl_oc::video::VideoCapture cap_0(params);
    if( !cap_0.initializeVideo(0) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }

    std::cout << "Connected to camera sn: " << cap_0.getSerialNumber() << " [" << cap_0.getDeviceName() << "]" << std::endl;
    // <---- Create Video Capture 0

    // ----> Create Video Capture 1
    sl_oc::video::VideoCapture cap_1(params);
    if( !cap_1.initializeVideo(2) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }

    std::cout << "Connected to camera sn: " << cap_1.getSerialNumber() << " [" << cap_1.getDeviceName() << "]" << std::endl;
    // <---- Create Video Capture 1

    // Set video parameters
    bool autoSettingEnable = true;
    cap_0.setAutoWhiteBalance(autoSettingEnable);
    cap_0.setAECAGC(autoSettingEnable);

    cap_1.setAutoWhiteBalance(autoSettingEnable);
    cap_1.setAECAGC(autoSettingEnable);

#ifdef TEST_FPS
    // Timestamp to check FPS
    double lastTime = static_cast<double>(getSteadyTimestamp())/1e9;
    // Frame timestamp to check FPS
    uint64_t lastFrameTs = 0;
#endif

    // Infinite video grabbing loop
    while (1)
    {
        // Get last available frame
        const sl_oc::video::Frame frame_0 = cap_0.getLastFrame();
        const sl_oc::video::Frame frame_1 = cap_1.getLastFrame();

        // ----> If the frame is valid we can display it
        if(frame_0.data!=nullptr && frame_1.data!=nullptr)
        {
#ifdef TEST_FPS
            if(lastFrameTs!=0)
            {
                // ----> System time
                double now = static_cast<double>(getSteadyTimestamp())/1e9;
                double elapsed_sec = now - lastTime;
                lastTime = now;
                std::cout << "[System] Frame period: " << elapsed_sec << "sec - Freq: " << 1./elapsed_sec << " Hz" << std::endl;
                // <---- System time

                // ----> Frame time
                double frame_dT = static_cast<double>(frame.timestamp-lastFrameTs)/1e9;
                std::cout << "[Camera] Frame period: " << frame_dT << "sec - Freq: " << 1./frame_dT << " Hz" << std::endl;
                // <---- Frame time
            }
            lastFrameTs = frame.timestamp;
#endif

            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV_0 = cv::Mat( frame_0.height, frame_0.width, CV_8UC2, frame_0.data );
            cv::Mat frameBGR_0;
            cv::Mat frameYUV_1 = cv::Mat( frame_1.height, frame_1.width, CV_8UC2, frame_1.data );
            cv::Mat frameBGR_1;
            cv::cvtColor(frameYUV_0,frameBGR_0,cv::COLOR_YUV2BGR_YUYV);
            cv::cvtColor(frameYUV_1,frameBGR_1,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // Show frame
            sl_oc::tools::showImage( "Stream RGB #0", frameBGR_0, params.res  );
            sl_oc::tools::showImage( "Stream RGB #1", frameBGR_1, params.res  );
        }
        // <---- If the frame is valid we can display it

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        if(key=='a' || key=='A')
        {
            autoSettingEnable = !autoSettingEnable;
            cap_0.setAutoWhiteBalance(autoSettingEnable);
            cap_0.setAECAGC(autoSettingEnable);

            cap_1.setAutoWhiteBalance(autoSettingEnable);
            cap_1.setAECAGC(autoSettingEnable);

            std::cout << "Auto GAIN/EXPOSURE and Auto White Balance: " << (autoSettingEnable?"ENABLED":"DISABLED") << std::endl;
        }
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}


