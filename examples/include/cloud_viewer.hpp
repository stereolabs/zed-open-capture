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

#ifndef CLOUD_VIEWER_HPP
#define CLOUD_VIEWER_HPP

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace sl_oc {
namespace tools {

/*!
 * \brief The CloudViewer class shows a colored point cloud in a standard OpenCV window
 *
 * The point cloud is drawn by the CPU, so the viewer does not need OpenGL or the Viz module of OpenCV and it
 * behaves in the same way on every computer.
 *
 * Mouse controls: drag with the left button to rotate the view around the scene, drag with the right button or
 * use the wheel to move closer or farther, double click to restore the view of the camera.
 */
class CloudViewer
{
public:
    /*!
     * \brief Constructor
     * \param name the name of the window
     * \param size the size of the window in pixels
     */
    CloudViewer(const std::string& name = "Point Cloud", cv::Size size = cv::Size(672, 376))
        : mName(name), mSize(size)
    {
    }

    /*!
     * \brief Draw a point cloud and show it in the window
     * \param cloud 3D coordinates [X right, Y down, Z forward] of the points in millimeters [CV_32FC3]. The points
     *        that contain NaN are not drawn.
     * \param colors the color of each point [CV_8UC3, BGR]. It is resized if its size is different from the cloud.
     */
    void show(const cv::Mat& cloud, const cv::Mat& colors)
    {
        render(cloud, colors, mImage);

        cv::putText(mImage, "Left drag: rotate - Right drag/wheel: zoom - Double click: reset", cv::Point(8, mSize.height-10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(160,160,160), 1);

        cv::imshow(mName, mImage);

        if( !mCallbackSet )
        {
            cv::setMouseCallback(mName, &CloudViewer::onMouse, this);
            mCallbackSet = true;
        }
    }

    /*!
     * \brief Draw a point cloud into an image, with the current point of view
     * \param cloud 3D coordinates of the points in millimeters [CV_32FC3]
     * \param colors the color of each point [CV_8UC3, BGR]
     * \param image the result [CV_8UC3, BGR]
     */
    void render(const cv::Mat& cloud, const cv::Mat& colors, cv::Mat& image)
    {
        image.create(mSize, CV_8UC3);
        image.setTo(cv::Scalar(40,40,40));

        if( cloud.empty() || cloud.type()!=CV_32FC3 )
            return;

        if( colors.size()==cloud.size() && colors.type()==CV_8UC3 )
            mColors = colors;
        else if( !colors.empty() && colors.type()==CV_8UC3 )
            cv::resize(colors, mColors, cloud.size(), 0, 0, cv::INTER_AREA);
        else
            mColors = cv::Mat(cloud.size(), CV_8UC3, cv::Scalar(255,255,255));

        // The view rotates around a point in front of the camera, at the typical distance of the scene
        if( !mHasTarget )
        {
            mTargetZ = medianDepth(cloud);
            mDistance = mTargetZ;
            mHasTarget = true;
        }

        // ----> Point of view: rotation around the target, then translation along the view direction
        const float cy = std::cos(mYaw), sy = std::sin(mYaw);
        const float cp = std::cos(mPitch), sp = std::sin(mPitch);
        // R = Rx(pitch)*Ry(yaw)
        const float r00 = cy,     r01 = 0.f, r02 = sy;
        const float r10 = sp*sy,  r11 = cp,  r12 = -sp*cy;
        const float r20 = -cp*sy, r21 = sp,  r22 = cp*cy;
        // <---- Point of view

        const float focal = 0.6f*mSize.width; // About 80 degrees of horizontal field of view
        const float u0 = 0.5f*mSize.width, v0 = 0.5f*mSize.height;
        const float nearPlane = 50.f;

        // Angle between two adjacent points of the cloud: it sets the size of the drawn points
        const float pitch = pointPitch(cloud);

        mDepthBuffer.create(mSize, CV_32FC1);
        mDepthBuffer.setTo(std::numeric_limits<float>::max());

        for( int r=0; r<cloud.rows; r++ )
        {
            const cv::Vec3f* pt = cloud.ptr<cv::Vec3f>(r);
            const cv::Vec3b* col = mColors.ptr<cv::Vec3b>(r);

            for( int c=0; c<cloud.cols; c++ )
            {
                const float z0 = pt[c][2];
                if( !(z0==z0) ) // NaN
                    continue;

                const float x = pt[c][0], y = pt[c][1], z = z0-mTargetZ;

                const float zv = r20*x + r21*y + r22*z + mDistance;
                if( zv<nearPlane )
                    continue;

                const float xv = r00*x + r01*y + r02*z;
                const float yv = r10*x + r11*y + r12*z;

                const int u = static_cast<int>(focal*xv/zv + u0);
                const int v = static_cast<int>(focal*yv/zv + v0);

                // Each point is drawn as a square that covers the distance from the adjacent points, to not leave holes
                const int side = std::max(1, std::min(8, static_cast<int>(focal*pitch*z0/zv + 0.999f)));
                for( int dv=0; dv<side; dv++ )
                {
                    const int vv = v+dv;
                    if( vv<0 || vv>=mSize.height )
                        continue;
                    float* zbuf = mDepthBuffer.ptr<float>(vv);
                    cv::Vec3b* out = image.ptr<cv::Vec3b>(vv);
                    for( int du=0; du<side; du++ )
                    {
                        const int uu = u+du;
                        if( uu<0 || uu>=mSize.width || zv>=zbuf[uu] )
                            continue;
                        zbuf[uu] = zv;
                        out[uu] = col[c];
                    }
                }
            }
        }
    }

    /*!
     * \brief Set the point of view
     * \param yaw rotation around the vertical axis in radians, 0 is the view of the camera
     * \param pitch rotation around the horizontal axis in radians, 0 is the view of the camera
     * \param zoom values higher than 1 move the point of view closer to the scene
     */
    void setView(float yaw, float pitch, float zoom = 1.f)
    {
        mYaw = yaw;
        mPitch = pitch;
        mZoom = std::max(0.1f, zoom);
        if( mHasTarget )
            mDistance = mTargetZ/mZoom;
    }

    /*!
     * \brief Restore the point of view of the camera
     */
    void resetView()
    {
        mYaw = 0.f;
        mPitch = 0.f;
        mZoom = 1.f;
        mHasTarget = false; // The distance of the scene is measured again
    }

private:
    /*!
     * \brief Typical distance of the scene: median of the depth of a subset of the points
     */
    static float medianDepth(const cv::Mat& cloud)
    {
        std::vector<float> depth;
        for( int r=0; r<cloud.rows; r+=4 )
        {
            const cv::Vec3f* pt = cloud.ptr<cv::Vec3f>(r);
            for( int c=0; c<cloud.cols; c+=4 )
            {
                if( pt[c][2]==pt[c][2] )
                    depth.push_back(pt[c][2]);
            }
        }

        if( depth.empty() )
            return 2000.f;

        std::nth_element(depth.begin(), depth.begin()+depth.size()/2, depth.end());
        return std::max(300.f, depth[depth.size()/2]);
    }

    /*!
     * \brief Angle in radians between two adjacent points of the cloud, seen from the camera
     */
    static float pointPitch(const cv::Mat& cloud)
    {
        std::vector<float> pitch;
        for( int r=cloud.rows/4; r<cloud.rows; r+=std::max(1,cloud.rows/8) )
        {
            const cv::Vec3f* pt = cloud.ptr<cv::Vec3f>(r);
            for( int c=0; c+1<cloud.cols; c+=3 )
            {
                if( pt[c][2]==pt[c][2] && pt[c+1][2]==pt[c+1][2] && pt[c][2]>0.f && pt[c+1][2]>0.f )
                    pitch.push_back(pt[c+1][0]/pt[c+1][2] - pt[c][0]/pt[c][2]);
            }
        }

        if( pitch.empty() )
            return 0.003f;

        std::nth_element(pitch.begin(), pitch.begin()+pitch.size()/2, pitch.end());
        return std::max(0.0002f, std::min(0.02f, pitch[pitch.size()/2]));
    }

    static void onMouse(int event, int x, int y, int flags, void* userdata)
    {
        CloudViewer* viewer = static_cast<CloudViewer*>(userdata);

        switch(event)
        {
        case cv::EVENT_LBUTTONDOWN:
        case cv::EVENT_RBUTTONDOWN:
            viewer->mLastMouse = cv::Point(x,y);
            break;

        case cv::EVENT_MOUSEMOVE:
            if( flags & cv::EVENT_FLAG_LBUTTON )
            {
                viewer->mYaw -= 0.01f*(x-viewer->mLastMouse.x);
                viewer->mPitch += 0.01f*(y-viewer->mLastMouse.y);
                viewer->mPitch = std::max(-1.5f, std::min(1.5f, viewer->mPitch));
            }
            else if( flags & cv::EVENT_FLAG_RBUTTON )
            {
                viewer->zoom(std::exp(-0.01f*(y-viewer->mLastMouse.y)));
            }
            viewer->mLastMouse = cv::Point(x,y);
            break;

        case cv::EVENT_MOUSEWHEEL:
            viewer->zoom(cv::getMouseWheelDelta(flags)>0 ? 1.1f : 1.f/1.1f);
            break;

        case cv::EVENT_LBUTTONDBLCLK:
            viewer->resetView();
            break;

        default:
            break;
        }
    }

    void zoom(float factor)
    {
        mZoom = std::max(0.1f, std::min(20.f, mZoom*factor));
        if( mHasTarget )
            mDistance = mTargetZ/mZoom;
    }

private:
    std::string mName;
    cv::Size mSize;

    cv::Mat mImage;
    cv::Mat mColors;
    cv::Mat mDepthBuffer;

    float mYaw = 0.f;
    float mPitch = 0.f;
    float mZoom = 1.f;
    float mTargetZ = 2000.f;    // Distance from the camera of the center of rotation [mm]
    float mDistance = 2000.f;   // Distance of the point of view from the center of rotation [mm]
    bool mHasTarget = false;

    cv::Point mLastMouse;
    bool mCallbackSet = false;
};

} // namespace tools
} // namespace sl_oc

#endif // CLOUD_VIEWER_HPP
