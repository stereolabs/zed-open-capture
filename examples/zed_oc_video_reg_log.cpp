///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021, STEREOLABS.
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
#include "videocapture.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/opencv.hpp>

#include "ocv_display.hpp"
// <---- Includes

// #define TEST_FPS 1

// The main function
int main(int argc, char *argv[])
{
    // ----> Create Video Capture
    sl_oc::video::VideoParams params;
    params.res = sl_oc::video::RESOLUTION::HD720;
    params.fps = sl_oc::video::FPS::FPS_60;

    sl_oc::video::VideoCapture cap(params);
    if( !cap.initializeVideo() )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }
    std::cout << "Connected to camera sn: " << cap.getSerialNumber() << std::endl;
    // <---- Create Video Capture

#ifdef TEST_FPS
    // Timestamp to check FPS
    double lastTime = static_cast<double>(getSteadyTimestamp())/1e9;
    // Frame timestamp to check FPS
    uint64_t lastFrameTs = 0;
#endif

    // Enable AGC/AEG registers logging
    cap.enableAecAgcSensLogging(false);

    uint64_t last_ts=0;
    uint16_t not_a_new_frame = 0;
    int frame_timeout_msec = 100;

    int f_count = 0;
    // Infinite video grabbing loop
    while (1)
    {
        // Get last available frame
        const sl_oc::video::Frame frame = cap.getLastFrame(frame_timeout_msec);

        // ----> If the frame is valid we can display it
        if(frame.data!=nullptr && frame.timestamp!=last_ts)
        {
            last_ts = frame.timestamp;
            not_a_new_frame=0;
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



            if (f_count%10==0)
            {
               /* cap.
                cap.saveAllISPRegisters("ov580_lr_"+std::to_string(f_count)+".csv");
                cap.saveAllSensorsRegisters("ov4689_lr_"+std::to_string(f_count)+".csv");
                std::cout<<" Save Data for f_count "<<f_count<<std::endl;*/
                if (f_count%20==0)
                    cap.setColorBars(0,true);
                else
                    cap.setColorBars(0,false);
            }

            //            if (f_count!=0 && f_count%10==0)
//            {
//                cap.saveAllISPRegisters("ov580_lr_"+std::to_string(f_count)+".csv");
//                cap.saveAllSensorsRegisters("ov4689_lr_"+std::to_string(f_count)+".csv");
//                std::cout<<" Save Data for f_count "<<f_count<<std::endl;
//            }



            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
            cv::Mat frameBGR;
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            f_count++;
            // Show frame
            showImage( "Stream RGB", frameBGR, params.res );
        }
        else
        {
            not_a_new_frame++;
            std::cout << "Not a new frame #" << not_a_new_frame << std::endl;

            if( not_a_new_frame>=(1000/frame_timeout_msec)) // Lost connection for 1 seconds
            {
                cap.saveAllISPRegisters("ov580_lr_"+std::to_string(f_count)+"_not_a_new _frame.csv");
                cap.saveAllSensorsRegisters("ov4689_lr_"+std::to_string(f_count)+"_not_a_new _frame.csv");
                std::cout<<" Save Data for f_count "<<f_count<<std::endl;

                std::cout << "Camera connection lost. Closing..." << std::endl;
                break;
            }
        }
        // <---- If the frame is valid we can display it

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        // <---- Keyboard handling
    }

    // Disable AGC/AEG registers logging
    cap.enableAecAgcSensLogging(true);

    return EXIT_SUCCESS;
}


