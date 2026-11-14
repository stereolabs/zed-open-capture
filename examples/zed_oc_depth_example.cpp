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
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "videocapture.hpp"

// OpenCV includes
#include <opencv2/opencv.hpp>

// Sample includes
#include "calibration.hpp"
#include "stereo.hpp"
#include "depth.hpp"
#include "cloud_viewer.hpp"
#include "ocv_display.hpp"
// <---- Includes

int main(int argc, char *argv[])
{
    // ----> Silence unused warning
    (void)argc;
    (void)argv;
    // <---- Silence unused warning

    sl_oc::VERBOSITY verbose = sl_oc::VERBOSITY::INFO;

    // ----> Set Video parameters
    sl_oc::video::VideoParams params;
#ifdef EMBEDDED_ARM
    params.res = sl_oc::video::RESOLUTION::VGA;
#else
    params.res = sl_oc::video::RESOLUTION::HD720;
#endif
    params.fps = sl_oc::video::FPS::FPS_30;
    params.verbose = verbose;
    // <---- Set Video parameters

    // ----> Create Video Capture
    sl_oc::video::VideoCapture cap(params);
    if( !cap.initializeVideo(-1) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }
    int sn = cap.getSerialNumber();
    std::cout << "Connected to camera sn: " << sn << std::endl;
    // <---- Create Video Capture

    // ----> Retrieve calibration file from Stereolabs server
    std::string calibration_file;
    // Download camera calibration file
    if( sn<=0 || !sl_oc::tools::downloadCalibrationFile(static_cast<unsigned int>(sn), calibration_file) )
    {
        std::cerr << "Could not load calibration file from Stereolabs servers" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Calibration file found. Loading..." << std::endl;
    // <---- Retrieve calibration file from Stereolabs server

    // ----> Frame size
    int w,h;
    cap.getFrameSize(w,h);
    // <---- Frame size

    // ----> Stereo matching parameters
    sl_oc::tools::StereoSgbmPar stereoPar;

    //Note: you can use the tool 'zed_open_capture_depth_tune_stereo' to tune the parameters and save them to YAML
    if(!stereoPar.load())
    {
        stereoPar.save(); // Save default parameters.
    }

    stereoPar.print();
    // <---- Stereo matching parameters

    // ----> Initialize calibration, rectification and stereo matching
    sl_oc::tools::StereoDepth depth;
    if( !depth.init(calibration_file, cv::Size(w/2,h), stereoPar) )
    {
        std::cerr << "Could not initialize the depth processing: invalid calibration file" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Depth map size: " << depth.depthSize().width << "x" << depth.depthSize().height << std::endl;
    std::cout << "Baseline: " << depth.baseline() << " mm - Focal: " << depth.fx() << " px" << std::endl << std::endl;
    // <---- Initialize calibration, rectification and stereo matching

    cv::Mat left_rect;      // Left rectified color image
    cv::Mat depth_image;    // Color representation of the depth map
    cv::Mat cloudMat;       // Point cloud
    cv::Mat cloudColors;    // Color of each point

    // The point cloud is drawn in a standard window: drag with the mouse to rotate the view
    sl_oc::tools::CloudViewer pc_viewer( "Point Cloud", cv::Size(800, 450) );

    uint64_t last_ts=0; // Used to check new frame arrival
    uint64_t last_print_ts=0;

    // Infinite video grabbing loop
    while (1)
    {
        // Get a new frame from camera
        const sl_oc::video::Frame& frame = cap.getLastFrame();

        // ----> If the frame is valid we can extract the depth and display it
        if(frame.data!=nullptr && frame.timestamp!=last_ts)
        {
            last_ts = frame.timestamp;

            // ----> Rectification, stereo matching and depth extraction
            // The DISPARITY MAP is transformed in DEPTH MAP using the formula
            // depth = (f * B) / disparity
            // where 'f' is the camera focal, 'B' is the camera baseline, 'disparity' is the pixel disparity
            depth.compute(frame.data, frame.width, frame.height);

            const sl_oc::tools::StereoDepth::Timing& timing = depth.timing();
            std::stringstream stereoElabInfo;
            stereoElabInfo << std::fixed << std::setprecision(1) << "Depth: " << timing.total() << " msec - " << 1000./timing.total() << " Hz";
            // <---- Rectification, stereo matching and depth extraction

            // ----> Show frames
            depth.getLeftColor(frame.data, left_rect);
            depth.getDepthImage(depth_image);

            sl_oc::tools::showImage("Left rect.", left_rect, params.res, true);
            // The depth map is displayed with the same size of the left image
            cv::Mat depth_display;
            cv::resize(depth_image, depth_display, left_rect.size(), 0, 0, cv::INTER_NEAREST);
            sl_oc::tools::showImage("Depth", depth_display, params.res, true, stereoElabInfo.str());
            // <---- Show frames

            // ----> Depth of the central pixel, once per second
            if( frame.timestamp-last_print_ts > 1000000000ULL )
            {
                last_print_ts = frame.timestamp;
                const cv::Mat& depth_map = depth.depth();
                const float central_depth = depth_map.at<float>(depth_map.rows/2, depth_map.cols/2);
                if( central_depth==central_depth )
                    std::cout << "Depth of the central pixel: " << central_depth << " mm" << std::endl;
                else
                    std::cout << "Depth of the central pixel: not available" << std::endl;
            }
            // <---- Depth of the central pixel

            // ----> Point Cloud: 3D coordinates in millimeters of each pixel of the depth map
            depth.getPointCloud(cloudMat);
            cv::resize(left_rect, cloudColors, cloudMat.size(), 0, 0, cv::INTER_AREA);
            pc_viewer.show(cloudMat, cloudColors);
            // <---- Point Cloud
        }
        // <---- If the frame is valid we can extract the depth and display it

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        // <---- Keyboard handling

    }

    return EXIT_SUCCESS;
}
