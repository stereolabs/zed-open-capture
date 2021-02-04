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
#include "videocapture.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
// <---- Includes

// The main function
int main(int argc, char *argv[])
{
    // ----> Create Video Capture
    sl_oc::video::VideoParams params;
    params.fps = sl_oc::video::FPS::FPS_100;
    params.res = sl_oc::video::RESOLUTION::GS_HIGH;
    params.verbose = sl_oc::VERBOSITY::ERROR;

    sl_oc::video::VideoCapture cap(params);
    if( !cap.initializeVideo() )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }
    std::cout << "Connected to camera sn: " << cap.getSerialNumber() << std::endl;
    // <---- Create Video Capture

    // Infinite video grabbing loop
    while (1)
    {
        // Get last available frame
        const sl_oc::video::Frame frame = cap.getLastFrame();

        // ----> If the frame is valid we can display it
        if(frame.data!=nullptr)
        {
            //            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            //            cv::Mat frameYUV = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
            //            cv::Mat frameBayer;
            //            cv::Mat frameBGR;
            //            cv::cvtColor(frameYUV,frameBayer,cv::COLOR_YUV2GRAY_Y422);
            //            cv::cvtColor(frameBayer,frameBGR,cv::COLOR_BayerRG2BGR);
            //            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            //            // Show frame
            //            cv::imshow( "Stream Bayer", frameBayer );
            //            cv::imshow( "Stream RGB", frameBGR );
            cv::Mat frameRAW8 = cv::Mat( frame.height, frame.width*2, CV_8UC1, frame.data );
            cv::imshow( "Stream Bayer", frameRAW8 );
            cv::Mat frameBGR;
            cv::cvtColor(frameRAW8,frameBGR,cv::COLOR_BayerRG2BGR);

            // Show frame
            cv::imshow( "Stream Bayer", frameRAW8 );
            cv::imshow( "Stream RGB", frameBGR );
        }
        // <---- If the frame is valid we can display it

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}


