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
    // ----> 1) Create Video Capture
    sl_oc::VideoCapture cap;
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
        // 2) Get last available frame
        const sl_oc::Frame* frame = cap.getLastFrame();

        // ----> 3) If the frame is valid we can display it
        if(frame != nullptr)
        {
            // ----> 3.a)Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame->height, frame->width, CV_8UC2, frame->data );
            cv::Mat frameBGR;
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // 3.b) Show frame
            cv::imshow( "Stream RGB", frameBGR );
        }
        // <---- If the frame is valid we can display it

        // ----> 4) Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}


