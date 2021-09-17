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
#include <mutex>

#include "videocapture.hpp"

// OpenCV includes
#include <opencv2/opencv.hpp>

// Sample includes
#include "calibration.hpp"
#include "stopwatch.hpp"
#include "stereo.hpp"
#include "ocv_display.hpp"
// <---- Includes

// ----> MACROS
#define USE_HALF_SIZE_DISPARITY
// <---- MACROS

// ----> Global variables
sl_oc::video::VideoParams params;

std::string preFiltDispWinName = "Pre-filtered disparity";

sl_oc::tools::StereoSgbmPar stereoPar;

cv::Ptr<cv::StereoSGBM> left_matcher;

cv::Mat frameBGR, left_raw, left_rect, right_raw, right_rect, frameYUV, right_for_matcher, left_for_matcher, left_disp, left_disp_vis;
int maxMaxDisp=0;

bool params_initialized=false;
// <---- Global variables

// ----> Global functions
void applyStereoMatching();

void on_trackbar_block_size( int newBlockSize, void* );
void on_trackbar_min_disparities( int newMinDisparities, void* );
void on_trackbar_num_disparities( int newNumDisparities, void* );
void on_trackbar_mode( int newMode, void* );
void on_trackbar_disp12MaxDiff(int newDisp12MaxDiff, void* );
void on_trackbar_preFilterCap(int newPreFilterCap, void* );
void on_trackbar_uniquenessRatio(int newUniquenessRatio, void* );
void on_trackbar_speckleWindowSize(int newSpeckleWindowSize, void* );
void on_trackbar_speckleRange(int newSpeckleRange, void* );
// <---- Global functions

int main(int argc, char *argv[])
{
    // ----> Silence unused warning
    (void)argc;
    (void)argv;
    // <---- Silence unused warning

    sl_oc::VERBOSITY verbose = sl_oc::VERBOSITY::INFO;

    // ----> Set Video parameters
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
    sl_oc::tools::initCalibration(calibration_file, cv::Size(w/2,h), map_left_x, map_left_y, map_right_x, map_right_y,
                                  cameraMatrix_left, cameraMatrix_right);

    std::cout << " Camera Matrix L: \n" << cameraMatrix_left << std::endl << std::endl;
    std::cout << " Camera Matrix R: \n" << cameraMatrix_right << std::endl << std::endl;
    // <---- Initialize calibration

    // Grab first valid frame couple
    while (1)
    {
        // Get a new frame from camera
        const sl_oc::video::Frame frame = cap.getLastFrame();

        // ----> If the frame is valid we can convert, rectify and display it
        if(frame.data!=nullptr)
        {
            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            frameYUV = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // ----> Extract left and right images from side-by-side
            left_raw = frameBGR(cv::Rect(0, 0, frameBGR.cols / 2, frameBGR.rows));
            right_raw = frameBGR(cv::Rect(frameBGR.cols / 2, 0, frameBGR.cols / 2, frameBGR.rows));
            // <---- Extract left and right images from side-by-side

            // ----> Apply rectification
            cv::remap(left_raw, left_rect, map_left_x, map_left_y, cv::INTER_LINEAR );
            cv::remap(right_raw, right_rect, map_right_x, map_right_y, cv::INTER_LINEAR );

            sl_oc::tools::showImage("Right rect.", right_rect, params.res);
            sl_oc::tools::showImage("Left rect.", left_rect, params.res);
            // <---- Apply rectification

            break;
        }

        cv::waitKey(10);
    }

    // ----> Create tune GUI
    maxMaxDisp = right_rect.cols;
#ifdef USE_HALF_SIZE_DISPARITY
    maxMaxDisp/=2;
#endif

    cv::namedWindow(preFiltDispWinName, cv::WINDOW_AUTOSIZE); // Create Window

    cv::createTrackbar( "blockSize", preFiltDispWinName, nullptr, 255, on_trackbar_block_size );
    cv::setTrackbarMin( "blockSize", preFiltDispWinName, 1 );

    cv::createTrackbar( "numDisparities", preFiltDispWinName, nullptr, maxMaxDisp, on_trackbar_num_disparities );
    cv::setTrackbarMin( "numDisparities", preFiltDispWinName, 16 );

    cv::createTrackbar( "minDisparity", preFiltDispWinName, nullptr, maxMaxDisp-16, on_trackbar_min_disparities );
    cv::setTrackbarMin( "minDisparity", preFiltDispWinName, -maxMaxDisp );

    cv::createTrackbar( "mode", preFiltDispWinName, nullptr, 3, on_trackbar_mode);
    cv::setTrackbarMin( "mode", preFiltDispWinName, 0 );

    cv::createTrackbar( "disp12MaxDiff", preFiltDispWinName, nullptr, maxMaxDisp, on_trackbar_disp12MaxDiff);
    cv::setTrackbarMin( "disp12MaxDiff", preFiltDispWinName, -1 );

    cv::createTrackbar( "preFilterCap", preFiltDispWinName, nullptr, 1024, on_trackbar_preFilterCap);
    cv::setTrackbarMin( "preFilterCap", preFiltDispWinName, 0 );

    cv::createTrackbar( "uniquenessRatio", preFiltDispWinName, nullptr, 99, on_trackbar_uniquenessRatio);
    cv::setTrackbarMin( "uniquenessRatio", preFiltDispWinName, 0 );

    cv::createTrackbar( "speckleWindowSize", preFiltDispWinName, nullptr, 255, on_trackbar_speckleWindowSize);
    cv::setTrackbarMin( "speckleWindowSize", preFiltDispWinName, 0 );

    cv::createTrackbar( "speckleRange", preFiltDispWinName, nullptr, 16, on_trackbar_speckleRange);
    cv::setTrackbarMin( "speckleRange", preFiltDispWinName, 1 );

    // <---- Create tune GUI

    // ----> Stereo matcher initialization
    stereoPar.load();

    left_matcher = cv::StereoSGBM::create(stereoPar.minDisparity,stereoPar.numDisparities,stereoPar.blockSize);
    left_matcher->setMinDisparity(stereoPar.minDisparity);
    left_matcher->setNumDisparities(stereoPar.numDisparities);
    left_matcher->setBlockSize(stereoPar.blockSize);
    left_matcher->setP1(stereoPar.P1);
    left_matcher->setP2(stereoPar.P2);
    left_matcher->setDisp12MaxDiff(stereoPar.disp12MaxDiff);
    left_matcher->setMode(stereoPar.mode);
    left_matcher->setPreFilterCap(stereoPar.preFilterCap);
    left_matcher->setUniquenessRatio(stereoPar.uniquenessRatio);
    left_matcher->setSpeckleWindowSize(stereoPar.speckleWindowSize);
    left_matcher->setSpeckleRange(stereoPar.speckleRange);

    stereoPar.print();

    params_initialized=true;
    // <---- Stereo matcher initialization

    // ----> Update GUI value
    cv::setTrackbarPos( "blockSize", preFiltDispWinName, stereoPar.blockSize );
    cv::setTrackbarPos( "numDisparities", preFiltDispWinName, stereoPar.numDisparities );
    cv::setTrackbarPos( "minDisparity", preFiltDispWinName, stereoPar.minDisparity );
    cv::setTrackbarPos( "mode", preFiltDispWinName, stereoPar.mode );
    cv::setTrackbarPos( "disp12MaxDiff", preFiltDispWinName, stereoPar.disp12MaxDiff );
    cv::setTrackbarPos( "preFilterCap", preFiltDispWinName, stereoPar.preFilterCap );
    cv::setTrackbarPos( "uniquenessRatio", preFiltDispWinName, stereoPar.uniquenessRatio );
    cv::setTrackbarPos( "speckleWindowSize", preFiltDispWinName, stereoPar.speckleWindowSize );
    cv::setTrackbarPos( "speckleRange", preFiltDispWinName, stereoPar.speckleRange );
    // <---- Update GUI value

    applyStereoMatching();

    while(1)
    {
        // ----> Keyboard handling
        int key = cv::waitKey( 5 );
        if(key=='q' || key==27) // Quit
        {
            std::cout << "Save current stereo parameters? [Y/N]" << std::endl;
            key = cv::waitKey(-1);
            if(key=='Y' || key=='y')
            {
                stereoPar.save();
            }
            break;
        }
        if(key=='l' || key=='L') //  load parameters)
        {
            if(stereoPar.load())
            {
                left_matcher->setMinDisparity(stereoPar.minDisparity);
                left_matcher->setNumDisparities(stereoPar.numDisparities);
                left_matcher->setBlockSize(stereoPar.blockSize);
                left_matcher->setP1(stereoPar.P1);
                left_matcher->setP2(stereoPar.P2);
                left_matcher->setDisp12MaxDiff(-(stereoPar.disp12MaxDiff-1));
                left_matcher->setMode(stereoPar.mode);
                left_matcher->setPreFilterCap(stereoPar.preFilterCap);
                left_matcher->setUniquenessRatio(stereoPar.uniquenessRatio);
                left_matcher->setSpeckleWindowSize(stereoPar.speckleWindowSize);
                left_matcher->setSpeckleRange(stereoPar.speckleRange);

                // ----> Update GUI value
                cv::setTrackbarPos( "blockSize", preFiltDispWinName, stereoPar.blockSize );
                cv::setTrackbarPos( "numDisparities", preFiltDispWinName, stereoPar.numDisparities );
                cv::setTrackbarPos( "minDisparity", preFiltDispWinName, stereoPar.minDisparity );
                cv::setTrackbarPos( "mode", preFiltDispWinName, stereoPar.mode );
                cv::setTrackbarPos( "disp12MaxDiff", preFiltDispWinName, stereoPar.disp12MaxDiff );
                cv::setTrackbarPos( "preFilterCap", preFiltDispWinName, stereoPar.preFilterCap );
                cv::setTrackbarPos( "uniquenessRatio", preFiltDispWinName, stereoPar.uniquenessRatio );
                cv::setTrackbarPos( "speckleWindowSize", preFiltDispWinName, stereoPar.speckleWindowSize );
                cv::setTrackbarPos( "speckleRange", preFiltDispWinName, stereoPar.speckleRange );
                // <---- Update GUI value

                applyStereoMatching();
            }
        }

        if(key=='s' || key=='S') //  load parameters
        {
            stereoPar.save();
        }

        if(key=='r' || key=='R') //  reset default parameters
        {
            stereoPar.setDefaultValues();
            // ----> Update GUI value
            cv::setTrackbarPos( "blockSize", preFiltDispWinName, stereoPar.blockSize );
            cv::setTrackbarPos( "numDisparities", preFiltDispWinName, stereoPar.numDisparities );
            cv::setTrackbarPos( "minDisparity", preFiltDispWinName, stereoPar.minDisparity );
            cv::setTrackbarPos( "mode", preFiltDispWinName, stereoPar.mode );
            cv::setTrackbarPos( "disp12MaxDiff", preFiltDispWinName, stereoPar.disp12MaxDiff );
            cv::setTrackbarPos( "preFilterCap", preFiltDispWinName, stereoPar.preFilterCap );
            cv::setTrackbarPos( "uniquenessRatio", preFiltDispWinName, stereoPar.uniquenessRatio );
            cv::setTrackbarPos( "speckleWindowSize", preFiltDispWinName, stereoPar.speckleWindowSize );
            cv::setTrackbarPos( "speckleRange", preFiltDispWinName, stereoPar.speckleRange );
            // <---- Update GUI value
            applyStereoMatching();
        }

        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}

void applyStereoMatching()
{
    if(!params_initialized)
        return;

    // ----> Stereo matching
    sl_oc::tools::StopWatch stereo_clock;

#ifdef USE_HALF_SIZE_DISPARITY
    cv::resize(left_rect,  left_for_matcher,  cv::Size(), 0.5, 0.5, cv::INTER_AREA);
    cv::resize(right_rect, right_for_matcher, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
#else
    left_for_matcher = left_rect.clone();
    right_for_matcher = right_rect.clone();
#endif

    left_matcher->setMinDisparity(stereoPar.minDisparity);
    left_matcher->setNumDisparities(stereoPar.numDisparities);
    left_matcher->setBlockSize(stereoPar.blockSize);
    left_matcher->setP1(stereoPar.P1);
    left_matcher->setP2(stereoPar.P2);
    left_matcher->setDisp12MaxDiff(stereoPar.disp12MaxDiff);
    left_matcher->setMode(stereoPar.mode);
    left_matcher->setPreFilterCap(stereoPar.preFilterCap);
    left_matcher->setUniquenessRatio(stereoPar.uniquenessRatio);
    left_matcher->setSpeckleWindowSize(stereoPar.speckleWindowSize);
    left_matcher->setSpeckleRange(stereoPar.speckleRange);
#ifdef USE_HALF_SIZE_DISPARITY
    left_disp*=2;
#endif

    std::cout << "Start stereo matching..." << std::endl;
    left_matcher->compute(left_for_matcher, right_for_matcher, left_disp);
    std::cout << "... finished stereo matching" << std::endl;

    double elapsed = stereo_clock.toc();
    std::cout << "Stereo processing: " << elapsed << " sec - Freq: " << 1./elapsed << std::endl;
    // <---- Stereo matching

    // ----> Show disparity
    //cv::ximgproc::getDisparityVis(left_disp,left_disp_vis,3.0);
    cv::normalize(left_disp, left_disp_vis, 0, 255, cv::NORM_MINMAX, CV_8UC1);
#ifdef USE_HALF_SIZE_DISPARITY
    cv::resize(left_disp_vis, left_disp_vis, cv::Size(), 2.0, 2.0, cv::INTER_AREA);
#endif
    sl_oc::tools::showImage(preFiltDispWinName, left_disp_vis, params.res, false);
    // <---- Show disparity
}


void on_trackbar_block_size( int newBlockSize, void* )
{
    bool fixed = false;
    if(newBlockSize==stereoPar.blockSize)
        return;

    if(newBlockSize%2!=1)
    {
        if(newBlockSize>stereoPar.blockSize)
            ++newBlockSize;
        else
            --newBlockSize;
        fixed=true;
    }
    if(newBlockSize<1)
    {
        newBlockSize=1;
        fixed=true;
    }
    if(newBlockSize>255)
    {
        newBlockSize=255;
        fixed=true;
    }

    stereoPar.blockSize = newBlockSize;
    stereoPar.P1 = 24*stereoPar.blockSize*stereoPar.blockSize;
    stereoPar. P2 = 96*stereoPar.blockSize*stereoPar.blockSize;

    if(fixed)
    {
        cv::setTrackbarPos("blockSize", preFiltDispWinName, newBlockSize);
    }

    std::cout << "New 'blockSize' value: " << stereoPar.blockSize << std::endl;
    applyStereoMatching();
}

void on_trackbar_min_disparities( int newMinDisparities, void* )
{
    if(newMinDisparities==stereoPar.minDisparity)
        return;

    stereoPar.minDisparity = newMinDisparities;

    if(stereoPar.numDisparities+stereoPar.minDisparity>maxMaxDisp)
    {
        int newNumDisparities = stereoPar.numDisparities-(stereoPar.numDisparities+stereoPar.minDisparity-maxMaxDisp);
        cv::setTrackbarPos("numDisparities", preFiltDispWinName, newNumDisparities);
        std::cout << "New 'numDisparities' value: " << stereoPar.numDisparities << std::endl;
    }

    std::cout << "New 'minDisparity' value: " << stereoPar.minDisparity << std::endl;
    applyStereoMatching();
}

void on_trackbar_num_disparities( int newNumDisparities, void* )
{
    bool fixed = false;
    if(newNumDisparities==stereoPar.numDisparities)
        return;

    if(newNumDisparities%16!=0)
    {
        if(newNumDisparities<stereoPar.numDisparities)
            newNumDisparities = newNumDisparities/16;
        else
            newNumDisparities = newNumDisparities/16 + 1;
        newNumDisparities*=16;
        fixed=true;
    }

    stereoPar.numDisparities = newNumDisparities;

    if(fixed)
    {
        cv::setTrackbarPos("numDisparities", preFiltDispWinName, newNumDisparities);
    }

    if(stereoPar.numDisparities+stereoPar.minDisparity>maxMaxDisp)
    {
        int newMinDisparity = stereoPar.minDisparity-(stereoPar.numDisparities+stereoPar.minDisparity-maxMaxDisp);
        cv::setTrackbarPos("minDisparity", preFiltDispWinName, newMinDisparity);
        std::cout << "New 'minDisparity' value: " << stereoPar.minDisparity << std::endl;
    }

    std::cout << "New 'numDisparities' value: " << stereoPar.numDisparities << std::endl;
    applyStereoMatching();
}

void on_trackbar_mode( int newMode, void* )
{
    if(newMode==stereoPar.mode)
        return;

    stereoPar.mode = newMode;

    std::cout << "New 'mode' value: " << stereoPar.mode << std::endl;
    applyStereoMatching();
}

void on_trackbar_disp12MaxDiff (int newDisp12MaxDiff, void* )
{
    if(newDisp12MaxDiff==stereoPar.disp12MaxDiff)
        return;

    stereoPar.disp12MaxDiff = newDisp12MaxDiff;

    std::cout << "New 'disp12MaxDiff' value: " << stereoPar.disp12MaxDiff << std::endl;
    applyStereoMatching();
}

void on_trackbar_preFilterCap(int newPreFilterCap, void* )
{
    if(newPreFilterCap==stereoPar.preFilterCap)
        return;

    stereoPar.preFilterCap = newPreFilterCap;

    std::cout << "New 'preFilterCap' value: " << stereoPar.preFilterCap << std::endl;
    applyStereoMatching();
}

void on_trackbar_uniquenessRatio(int newUniquenessRatio, void* )
{
    if(newUniquenessRatio==stereoPar.uniquenessRatio)
        return;

    stereoPar.uniquenessRatio = newUniquenessRatio;

    std::cout << "New 'uniquenessRatio' value: " << stereoPar.uniquenessRatio << std::endl;
    applyStereoMatching();
}

void on_trackbar_speckleWindowSize(int newSpeckleWindowSize, void* )
{
    if(newSpeckleWindowSize==stereoPar.speckleWindowSize)
        return;

    stereoPar.speckleWindowSize = newSpeckleWindowSize;

    std::cout << "New 'speckleWindowSize' value: " << stereoPar.speckleWindowSize << std::endl;
    applyStereoMatching();
}

void on_trackbar_speckleRange(int newSpeckleRange, void* )
{
    if(newSpeckleRange==stereoPar.speckleRange)
        return;

    stereoPar.speckleRange = newSpeckleRange;

    std::cout << "New 'speckleRange' value: " << stereoPar.speckleRange << std::endl;
    applyStereoMatching();
}
