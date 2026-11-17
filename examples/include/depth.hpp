///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2026, STEREOLABS.
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

#ifndef DEPTH_HPP
#define DEPTH_HPP

#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "calibration.hpp"
#include "stereo.hpp"

namespace sl_oc {
namespace tools {

/*!
 * \brief The StereoDepth class contains the depth processing of the depth example, using only the CPU
 *
 * The frame is converted to grayscale, reduced to the matching size, rectified and matched with the
 * Semi-Global Block Matching algorithm of OpenCV. Everything that is not required to get the depth, like the
 * color conversion, is done on request.
 */
class StereoDepth
{
public:
    /*!
     * \brief Processing time of each step of the last frame, in milliseconds
     */
    struct Timing
    {
        double convert_ms = 0.;  //!< Grayscale conversion and resize
        double rectify_ms = 0.;  //!< Rectification of the two images
        double match_ms = 0.;    //!< Stereo matching
        double depth_ms = 0.;    //!< Depth extraction
        double total() const { return convert_ms+rectify_ms+match_ms+depth_ms; }
    };

    /*!
     * \brief Initialize the processing for a camera and a resolution
     * \param calibration_file the calibration file of the camera (see \ref downloadCalibrationFile)
     * \param image_size the size of a single image of the camera (half of the frame width)
     * \param par the stereo matching parameters
     * \return false if the calibration cannot be loaded
     */
    bool init(const std::string& calibration_file, cv::Size image_size, const StereoSgbmPar& par)
    {
        mPar = par;
        mImageSize = image_size;

        // The matching size is an integer fraction of the image size
        mScale = (par.matchScale>0. && par.matchScale<=1.) ? par.matchScale : 1.;
        mMatchSize = cv::Size(static_cast<int>(std::lround(image_size.width*mScale)),
                              static_cast<int>(std::lround(image_size.height*mScale)));

        // ----> Calibration of the full size images
        cv::Mat map_left_x, map_left_y, map_right_x, map_right_y;
        cv::Mat cameraMatrix_right;
        if( !initCalibration(calibration_file, image_size, map_left_x, map_left_y, map_right_x, map_right_y,
                             mCameraMatrix, cameraMatrix_right, &mBaseline) )
        {
            return false;
        }

        // Fixed point maps are faster to apply
        cv::convertMaps(map_left_x, map_left_y, mColorMap1, mColorMap2, CV_16SC2);
        // <---- Calibration of the full size images

        // ----> Rectification maps for images already reduced to the matching size
        // An output pixel (u,v) of the reduced image corresponds to the full size rectified pixel
        // ((u+0.5)/scale-0.5, (v+0.5)/scale-0.5): the full size maps are sampled there, and the source
        // coordinates are converted to the reduced input image
        cv::Mat lx, ly, rx, ry;
        reduceMap(map_left_x, lx);
        reduceMap(map_left_y, ly);
        reduceMap(map_right_x, rx);
        reduceMap(map_right_y, ry);
        cv::convertMaps(lx, ly, mLeftMap1, mLeftMap2, CV_16SC2);
        cv::convertMaps(rx, ry, mRightMap1, mRightMap2, CV_16SC2);
        // <---- Rectification maps

        // ----> Camera parameters of the depth map
        mFx = mCameraMatrix.at<double>(0,0)*mScale;
        mFy = mCameraMatrix.at<double>(1,1)*mScale;
        mCx = (mCameraMatrix.at<double>(0,2)+0.5)*mScale-0.5;
        mCy = (mCameraMatrix.at<double>(1,2)+0.5)*mScale-0.5;
        // <---- Camera parameters of the depth map

        // ----> Stereo matcher
        // The disparity range is expressed in pixels of the depth map
        mMinDisp = par.minDisparity;
        mNumDisp = std::max(16, ((par.numDisparities+15)/16)*16);

        mMatcher = cv::StereoSGBM::create(mMinDisp, mNumDisp, par.blockSize);
        mMatcher->setP1(par.P1);
        mMatcher->setP2(par.P2);
        mMatcher->setDisp12MaxDiff(par.disp12MaxDiff);
        mMatcher->setMode(par.mode);
        mMatcher->setPreFilterCap(par.preFilterCap);
        mMatcher->setUniquenessRatio(par.uniquenessRatio);
        mMatcher->setSpeckleWindowSize(par.speckleWindowSize);
        mMatcher->setSpeckleRange(par.speckleRange);
        // <---- Stereo matcher

        mInitialized = true;
        return true;
    }

    /*!
     * \brief Extract disparity and depth from a camera frame
     * \param yuyv the frame data: two images side by side in YUV 4:2:2 format
     * \param width the frame width (two images)
     * \param height the frame height
     * \return false if the frame does not match the initialization
     */
    bool compute(const uint8_t* yuyv, int width, int height)
    {
        if( !mInitialized || yuyv==nullptr || width!=mImageSize.width*2 || height!=mImageSize.height )
            return false;

        typedef std::chrono::steady_clock Clock;
        const Clock::time_point t0 = Clock::now();

        // ----> Grayscale images at matching size
        // The luminance is the first channel of YUV 4:2:2: no color conversion is required
        const cv::Mat frame(height, width, CV_8UC2, const_cast<uint8_t*>(yuyv));
        cv::extractChannel(frame, mGray, 0);

        if( mScale<1. )
        {
            // The average of the pixels reduces the noise and avoids aliasing
            cv::resize(mGray, mGraySmall, cv::Size(mMatchSize.width*2, mMatchSize.height), 0, 0, cv::INTER_AREA);
        }
        else
        {
            mGraySmall = mGray;
        }

        const cv::Mat left_raw = mGraySmall(cv::Rect(0, 0, mMatchSize.width, mMatchSize.height));
        const cv::Mat right_raw = mGraySmall(cv::Rect(mMatchSize.width, 0, mMatchSize.width, mMatchSize.height));
        // <---- Grayscale images at matching size

        const Clock::time_point t1 = Clock::now();

        // ----> Rectification
        cv::remap(left_raw, mLeftRect, mLeftMap1, mLeftMap2, cv::INTER_LINEAR);
        cv::remap(right_raw, mRightRect, mRightMap1, mRightMap2, cv::INTER_LINEAR);
        // <---- Rectification

        const Clock::time_point t2 = Clock::now();

        // Stereo matching: the result is a fixed point disparity with 4 fractional bits
        mMatcher->compute(mLeftRect, mRightRect, mDisp16);

        const Clock::time_point t3 = Clock::now();

        // ----> Disparity and depth
        // The pixels without a valid match have a disparity lower than the minimum
        mDisparity.create(mMatchSize, CV_32FC1);
        mDepth.create(mMatchSize, CV_32FC1);

        const float invalid = std::numeric_limits<float>::quiet_NaN();
        const float fxB = static_cast<float>(mFx*mBaseline);
        const float minDepth = static_cast<float>(mPar.minDepth_mm);
        const float maxDepth = static_cast<float>(mPar.maxDepth_mm);
        const short minValid = static_cast<short>(std::max(mMinDisp*16, 1));

        cv::parallel_for_(cv::Range(0, mMatchSize.height), [&](const cv::Range& range) {
            for( int r=range.start; r<range.end; r++ )
            {
                const short* d16 = mDisp16.ptr<short>(r);
                float* disp = mDisparity.ptr<float>(r);
                float* depth = mDepth.ptr<float>(r);

                for( int c=0; c<mMatchSize.width; c++ )
                {
                    if( d16[c]<minValid )
                    {
                        disp[c] = invalid;
                        depth[c] = invalid;
                        continue;
                    }

                    // depth = (f * B) / disparity
                    const float d = static_cast<float>(d16[c])*(1.f/16.f);
                    const float z = fxB/d;

                    disp[c] = d;
                    depth[c] = (z>=minDepth && z<=maxDepth) ? z : invalid;
                }
            }
        });
        // <---- Disparity and depth

        const Clock::time_point t4 = Clock::now();

        mTiming.convert_ms = std::chrono::duration<double,std::milli>(t1-t0).count();
        mTiming.rectify_ms = std::chrono::duration<double,std::milli>(t2-t1).count();
        mTiming.match_ms = std::chrono::duration<double,std::milli>(t3-t2).count();
        mTiming.depth_ms = std::chrono::duration<double,std::milli>(t4-t3).count();

        return true;
    }

    /*!
     * \brief Get the rectified left color image at full size
     * \param yuyv the same frame data passed to \ref compute
     * \param left_rect the rectified left image in BGR format
     */
    void getLeftColor(const uint8_t* yuyv, cv::Mat& left_rect)
    {
        const cv::Mat frame(mImageSize.height, mImageSize.width*2, CV_8UC2, const_cast<uint8_t*>(yuyv));
        // Only the left half of the frame is converted. The width of an image is always even, as required by YUV 4:2:2
        cv::cvtColor(frame(cv::Rect(0, 0, mImageSize.width, mImageSize.height)), mLeftColor, cv::COLOR_YUV2BGR_YUYV);
        cv::remap(mLeftColor, left_rect, mColorMap1, mColorMap2, cv::INTER_LINEAR);
    }

    /*!
     * \brief Create a color image of the last depth map to be displayed: near is red, far is blue, not valid is black
     * \param image the color image in BGR format, with the size of the depth map
     */
    void getDepthImage(cv::Mat& image)
    {
        // Inverse depth has a uniform resolution along the range
        const float nearInv = 1.f/static_cast<float>(mPar.minDepth_mm);
        const float farInv = 1.f/static_cast<float>(mPar.maxDepth_mm);

        cv::Mat gray(mMatchSize, CV_8UC1);
        cv::Mat mask(mMatchSize, CV_8UC1);
        for( int r=0; r<mMatchSize.height; r++ )
        {
            const float* depth = mDepth.ptr<float>(r);
            uint8_t* g = gray.ptr<uint8_t>(r);
            uint8_t* m = mask.ptr<uint8_t>(r);
            for( int c=0; c<mMatchSize.width; c++ )
            {
                const bool valid = depth[c]==depth[c]; // NaN is not equal to itself
                m[c] = valid ? 255 : 0;
                g[c] = valid ? cv::saturate_cast<uint8_t>(255.f*(1.f/depth[c]-farInv)/(nearInv-farInv)) : 0;
            }
        }

        cv::Mat color;
        cv::applyColorMap(gray, color, cv::COLORMAP_JET);
        image = cv::Mat::zeros(mMatchSize, CV_8UC3);
        color.copyTo(image, mask);
    }

    /*!
     * \brief Create the point cloud of the last depth map
     * \param cloud the 3D coordinates [X right, Y down, Z forward] in millimeters of each pixel of the depth map.
     *        The pixels without a valid depth contain NaN.
     */
    void getPointCloud(cv::Mat& cloud)
    {
        cloud.create(mMatchSize, CV_32FC3);

        const float invalid = std::numeric_limits<float>::quiet_NaN();
        const float fx = static_cast<float>(mFx), fy = static_cast<float>(mFy);
        const float cx = static_cast<float>(mCx), cy = static_cast<float>(mCy);

        cv::parallel_for_(cv::Range(0, mMatchSize.height), [&](const cv::Range& range) {
            for( int r=range.start; r<range.end; r++ )
            {
                const float* depth = mDepth.ptr<float>(r);
                cv::Vec3f* pt = cloud.ptr<cv::Vec3f>(r);
                for( int c=0; c<mMatchSize.width; c++ )
                {
                    const float z = depth[c];
                    if( z==z )
                        pt[c] = cv::Vec3f((c-cx)*z/fx, (r-cy)*z/fy, z);
                    else
                        pt[c] = cv::Vec3f(invalid, invalid, invalid);
                }
            }
        });
    }

    const cv::Mat& disparity() const { return mDisparity; } //!< Disparity in pixels of the depth map [CV_32FC1], NaN if not valid
    const cv::Mat& depth() const { return mDepth; }         //!< Depth in millimeters [CV_32FC1], NaN if not valid or out of range
    const cv::Mat& leftGray() const { return mLeftRect; }   //!< Rectified left image used for the matching [CV_8UC1]
    const cv::Mat& rightGray() const { return mRightRect; } //!< Rectified right image used for the matching [CV_8UC1]
    const Timing& timing() const { return mTiming; }        //!< Processing time of the last frame
    cv::Size depthSize() const { return mMatchSize; }       //!< Size of the depth map
    double baseline() const { return mBaseline; }           //!< Distance between the two cameras in millimeters
    double minDistance() const { return mFx*mBaseline/(mMinDisp+mNumDisp-1); } //!< Minimum measurable distance in millimeters, set by the disparity range
    double fx() const { return mFx; }                       //!< Horizontal focal length of the depth map in pixels
    double fy() const { return mFy; }                       //!< Vertical focal length of the depth map in pixels
    double cx() const { return mCx; }                       //!< Horizontal optical center of the depth map in pixels
    double cy() const { return mCy; }                       //!< Vertical optical center of the depth map in pixels

private:
    /*!
     * \brief Convert a full size rectification map to the matching size
     */
    void reduceMap(const cv::Mat& full, cv::Mat& reduced) const
    {
        if( mScale>=1. )
        {
            reduced = full;
            return;
        }

        // Position in the full size map of each pixel of the reduced map
        cv::Mat px(mMatchSize, CV_32FC1), py(mMatchSize, CV_32FC1);
        for( int r=0; r<mMatchSize.height; r++ )
        {
            for( int c=0; c<mMatchSize.width; c++ )
            {
                px.at<float>(r,c) = static_cast<float>((c+0.5)/mScale-0.5);
                py.at<float>(r,c) = static_cast<float>((r+0.5)/mScale-0.5);
            }
        }

        cv::Mat sampled;
        cv::remap(full, sampled, px, py, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

        // The maps contain coordinates of the full size source image: convert them to the reduced source image
        reduced = (sampled+0.5)*mScale-0.5;
    }

private:
    bool mInitialized = false;
    StereoSgbmPar mPar;

    cv::Size mImageSize;
    cv::Size mMatchSize;
    double mScale = 1.;
    int mMinDisp = 0;
    int mNumDisp = 0;

    cv::Mat mCameraMatrix;
    double mBaseline = 0.;
    double mFx = 0., mFy = 0., mCx = 0., mCy = 0.;

    cv::Mat mLeftMap1, mLeftMap2, mRightMap1, mRightMap2; // Rectification maps at matching size
    cv::Mat mColorMap1, mColorMap2;                       // Left rectification maps at full size

    cv::Ptr<cv::StereoSGBM> mMatcher;

    cv::Mat mGray, mGraySmall, mLeftRect, mRightRect, mLeftColor;
    cv::Mat mDisp16, mDisparity, mDepth;

    Timing mTiming;
};

} // namespace tools
} // namespace sl_oc

#endif // DEPTH_HPP
