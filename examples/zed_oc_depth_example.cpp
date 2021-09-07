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
#include <iostream>
#include <sstream>
#include <string>

#include "videocapture.hpp"

// OpenCV includes
#include <opencv2/opencv.hpp>
#include "opencv2/ximgproc.hpp"

// Sample includes
#include "calibration.hpp"
#include "stopwatch.hpp"
#include "stereo.hpp"
// <---- Includes

#define USE_OCV_TAPI // Comment to use "normal" cv::Mat instead of CV::UMat
#define USE_HALF_SIZE_DISP // Comment to compute depth matching on full image frames

// ----> Global functions
/// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res, std::string info="" );
#ifdef USE_OCV_TAPI
/// Rescale the images [cv::UMat] according to the selected resolution to better display them on screen
void showImage( std::string name, cv::UMat& img, sl_oc::video::RESOLUTION res, std::string info="" );
#endif
// <---- Global functions

int main(int argc, char** argv) {

    sl_oc::VERBOSITY verbose = sl_oc::VERBOSITY::INFO;

    // ----> Set Video parameters
    sl_oc::video::VideoParams params;
    params.res = sl_oc::video::RESOLUTION::HD720;
    params.fps = sl_oc::video::FPS::FPS_60;
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
    // ZED Calibration
    unsigned int serial_number = sn;
    // Download camera calibration file
    if( !sl_oc::tools::downloadCalibrationFile(serial_number, calibration_file) )
    {
        std::cerr << "Could not load calibration file from Stereolabs servers" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Calibration file found. Loading..." << std::endl;

    // ----> Frame size
    int w,h;
    cap.getFrameSize(w,h);
    // <---- Frame size

    // ----> Initialize calibration
    cv::Mat map_left_x, map_left_y;
    cv::Mat map_right_x, map_right_y;
    cv::Mat cameraMatrix_left, cameraMatrix_right;
    double baseline;
    sl_oc::tools::initCalibration(calibration_file, cv::Size(w/2,h), map_left_x, map_left_y, map_right_x, map_right_y,
                                  cameraMatrix_left, cameraMatrix_right, &baseline);

    std::cout << " Camera Matrix L: \n" << cameraMatrix_left << std::endl << std::endl;
    std::cout << " Camera Matrix R: \n" << cameraMatrix_right << std::endl << std::endl;

#ifdef USE_OCV_TAPI
    cv::UMat map_left_x_gpu = map_left_x.getUMat(cv::ACCESS_READ,cv::USAGE_ALLOCATE_DEVICE_MEMORY);
    cv::UMat map_left_y_gpu = map_left_y.getUMat(cv::ACCESS_READ,cv::USAGE_ALLOCATE_DEVICE_MEMORY);
    cv::UMat map_right_x_gpu = map_right_x.getUMat(cv::ACCESS_READ,cv::USAGE_ALLOCATE_DEVICE_MEMORY);
    cv::UMat map_right_y_gpu = map_right_y.getUMat(cv::ACCESS_READ,cv::USAGE_ALLOCATE_DEVICE_MEMORY);
#endif
    // ----> Initialize calibration

    // ----> Declare OpenCV images
#ifdef USE_OCV_TAPI
    cv::UMat frameYUV;  // Full frame side-by-side in YUV 4:2:2 format
    cv::UMat frameBGR(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Full frame side-by-side in BGR format
    cv::UMat left_raw(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Left unrectified image
    cv::UMat right_raw(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Right unrectified image
    cv::UMat left_rect(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Left rectified image
    cv::UMat right_rect(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Right rectified image
    cv::UMat left_for_matcher(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Left image for the stereo matcher
    cv::UMat right_for_matcher(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Right image for the stereo matcher
    cv::UMat left_disp_half(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Half sized disparity map
    cv::UMat left_disp(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Full output disparity
    cv::UMat left_disp_float(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Final disparity map in float32
    cv::UMat left_disp_image(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Normalized and color remapped disparity map to be displayed
    cv::UMat left_depth_map(cv::USAGE_ALLOCATE_DEVICE_MEMORY); // Depth map in float32
#else
    cv::Mat frameBGR, left_raw, left_rect, right_raw, right_rect, frameYUV, left_for_matcher, right_for_matcher, left_disp_half,left_disp,left_disp_float, left_disp_vis;
#endif
    // <---- Declare OpenCV images

    // ----> Stereo matcher initialization
    sl_oc::tools::StereoSgbmPar stereoPar;

    //Note: you can use the tool 'zed_open_capture_depth_tune_stereo' to tune the parameters and save them to YAML
    if(!stereoPar.load())
    {
        stereoPar.save(); // Save default parameters.
    }

    cv::Ptr<cv::StereoSGBM> left_matcher = cv::StereoSGBM::create();
    left_matcher->setMinDisparity(stereoPar.minDisparity);
    left_matcher->setNumDisparities(stereoPar.numDisparities);
    left_matcher->setBlockSize(stereoPar.blockSize);
    left_matcher->setP1(stereoPar.P1);
    left_matcher->setP2(stereoPar.P2);
    left_matcher->setDisp12MaxDiff(-stereoPar.disp12MaxDiff);
    left_matcher->setMode(stereoPar.mode);
    left_matcher->setPreFilterCap(stereoPar.preFilterCap);
    left_matcher->setUniquenessRatio(stereoPar.uniquenessRatio);
    left_matcher->setSpeckleWindowSize(stereoPar.speckleWindowSize);
    left_matcher->setSpeckleRange(stereoPar.speckleRange);

    stereoPar.print();
    // <---- Stereo matcher initialization

    uint64_t last_ts=0; // Used to check new frame arrival

    // Infinite video grabbing loop
    while (1)
    {
        // Get a new frame from camera
        const sl_oc::video::Frame frame = cap.getLastFrame();

        // ----> If the frame is valid we can convert, rectify and display it
        if(frame.data!=nullptr && frame.timestamp!=last_ts)
        {
            last_ts = frame.timestamp;

            // ----> Conversion from YUV 4:2:2 to BGR for visualization
#ifdef USE_OCV_TAPI
            cv::Mat frameYUV_cpu = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
            frameYUV = frameYUV_cpu.getUMat(cv::ACCESS_READ,cv::USAGE_ALLOCATE_HOST_MEMORY);
#else
            frameYUV = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
#endif
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // ----> Extract left and right images from side-by-side
            left_raw = frameBGR(cv::Rect(0, 0, frameBGR.cols / 2, frameBGR.rows));
            right_raw = frameBGR(cv::Rect(frameBGR.cols / 2, 0, frameBGR.cols / 2, frameBGR.rows));
            // <---- Extract left and right images from side-by-side

            // ----> Apply rectification
            sl_oc::tools::StopWatch remap_clock;
#ifdef USE_OCV_TAPI
            cv::remap(left_raw, left_rect, map_left_x_gpu, map_left_y_gpu, cv::INTER_LINEAR );
            cv::remap(right_raw, right_rect, map_right_x_gpu, map_right_y_gpu, cv::INTER_LINEAR );
#else
            cv::remap(left_raw, left_rect, map_left_x, map_left_y, cv::INTER_LINEAR );
            cv::remap(right_raw, right_rect, map_right_x, map_right_y, cv::INTER_LINEAR );
#endif
            double remap_elapsed = remap_clock.toc();
            std::stringstream remapElabInfo;
            remapElabInfo << "Rectif. processing: " << remap_elapsed << " sec - Freq: " << 1./remap_elapsed;
            // <---- Apply rectification

            // ----> Stereo matching
            sl_oc::tools::StopWatch stereo_clock;
            float resize_fact = 1.0f;
#ifdef USE_HALF_SIZE_DISP
            resize_fact = 0.5f;
            // Resize the original images to improve performances
            cv::resize(left_rect,  left_for_matcher,  cv::Size(), resize_fact, resize_fact);
            cv::resize(right_rect, right_for_matcher, cv::Size(), resize_fact, resize_fact);
#else
            left_for_matcher = left_rect; // No data copy
            right_for_matcher = right_rect; // No data copy
#endif
            // Apply stereo matching
            left_matcher->compute(left_for_matcher, right_for_matcher,left_disp_half);

//            double minVal,maxVal;
//            cv::minMaxIdx(left_disp_half, &minVal, &maxVal );
//            std::cout << "Original Disparity range: " << minVal << " , " << maxVal << std::endl;

            left_disp_half.convertTo(left_disp_float,CV_32FC1);
            cv::multiply(left_disp_float,1.f/16.f,left_disp_float); // Last 4 bits of SGBM disparity are decimal

#ifdef USE_HALF_SIZE_DISP
            cv::resize(left_disp_float, left_disp_float, cv::Size(), 1./resize_fact, 1./resize_fact, cv::INTER_LINEAR);
#else
            left_disp = left_disp_float;
#endif


            double elapsed = stereo_clock.toc();
            std::stringstream stereoElabInfo;
            stereoElabInfo << "Stereo processing: " << elapsed << " sec - Freq: " << 1./elapsed;
            // <---- Stereo matching

            // ----> Show frames
            showImage("Right rect.", right_rect, params.res, remapElabInfo.str());
            showImage("Left rect.", left_rect, params.res, remapElabInfo.str());
            // <---- Show frames

            // ----> Show disparity image
            cv::add(left_disp_float,-static_cast<float>(stereoPar.minDisparity-1),left_disp_float); // Minimum disparity offset correction
            cv::multiply(left_disp_float,1.f/stereoPar.numDisparities,left_disp_image,255., CV_8UC1 ); // Normalization and rescaling
            cv::applyColorMap(left_disp_image,left_disp_image,cv::COLORMAP_INFERNO);
            showImage("Disparity", left_disp_image, params.res, stereoElabInfo.str());
            // <---- Show disparity image

            // ----> Extract Depth map
            // The DISPARITY MAP can be now transformed in DEPTH MAP using the formula
            // depth = (f * B) / disparity
            // where 'f' is the camera focal, 'B' is the camera baseline, 'disparity' is the pixel disparity

            double f = cameraMatrix_left.at<double>(0,0);
            float num = static_cast<float>(f*baseline)*resize_fact;
            cv::divide(num,left_disp_float,left_depth_map);

            float depth = left_depth_map.getMat(cv::ACCESS_READ).at<float>(left_depth_map.rows/2, left_depth_map.cols/2 );
            std::cout << "Central depth [1]: " << depth << " mm" << std::endl;
            // <---- Extract Depth map
        }

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key=='Q') // Quit
            break;
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}

#ifdef USE_OCV_TAPI
void showImage( std::string name, cv::UMat& img, sl_oc::video::RESOLUTION res, std::string info  )
{
    cv::UMat resized;
    switch(res)
    {
    default:
    case sl_oc::video::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        name += " [Resize factor 0.6]";
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        name += " [Resize factor 0.4]";
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    if(!info.empty())
    {
        cv::putText( resized, info, cv::Point(20,40),cv::FONT_HERSHEY_SIMPLEX, 0.75,
                 cv::Scalar(100,100,100), 2);
    }

    cv::imshow( name, resized );
}
#endif

// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res, std::string info )
{
    cv::Mat resized;
    switch(res)
    {
    default:
    case sl_oc::video::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        name += " [Resize factor 0.6]";
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        name += " [Resize factor 0.4]";
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    if(!info.empty())
    {
        cv::putText( resized, info, cv::Point(20,40),cv::FONT_HERSHEY_SIMPLEX, 0.75,
                 cv::Scalar(100,100,100), 2);
    }

    cv::imshow( name, resized );
}
