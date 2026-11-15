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

#ifndef TEST_COMMON_HPP
#define TEST_COMMON_HPP

#include <dirent.h>
#include <time.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <cmath>
#include <ostream>
#include <string>
#include <vector>

namespace test {

// ----> Process helpers
inline double nowSec()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) / 1e9;
}

//! Number of file descriptors opened by this process
inline int openFdCount()
{
    int n = 0;
    DIR* d = opendir("/proc/self/fd");
    if(!d) return -1;
    while(readdir(d)) n++;
    closedir(d);
    return n - 3; // ".", ".." and the descriptor of the directory itself
}

//! Number of video buffers still mapped in this process
inline int videoMapCount()
{
    int n = 0;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while(std::getline(maps, line))
        if(line.find("/dev/video") != std::string::npos) n++;
    return n;
}

//! Sparse hash of a buffer, used to detect that its content changed
inline uint64_t sparseHash(const uint8_t* p, size_t n)
{
    uint64_t h = 1469598103934665603ULL;
    for(size_t i = 0; i < n; i += 499)
    {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}
// <---- Process helpers

// ----> Camera discovery
struct VideoDev
{
    int id;           //!< index of /dev/video<id>
    std::string name; //!< name reported by the driver
    int usbSpeed = 0; //!< speed of the USB link in Mbps (480 = USB 2.0, 5000 = USB 3.0)

    //! With USB 2.0 the cameras only provide a few low resolution modes
    bool isUsb3() const { return usbSpeed >= 5000; }
};

//! Used by GoogleTest to print the parameter of a failed test
inline void PrintTo(const VideoDev& dev, std::ostream* os)
{
    *os << "/dev/video" << dev.id << " (" << dev.name << ", USB " << dev.usbSpeed << " Mbps)";
}

/*!
 * \brief List the video capture nodes of the connected Stereolabs cameras
 */
inline std::vector<VideoDev> listStereolabsVideoDevices()
{
    std::vector<VideoDev> devs;
    for(int id = 0; id < 256; id++)
    {
        const std::string base = "/sys/class/video4linux/video" + std::to_string(id);
        std::string modalias, name;
        int index = -1;
        if(!(std::ifstream(base + "/device/modalias") >> modalias)) continue;
        if(modalias.rfind("usb:v2B03", 0) != 0) continue;
        std::ifstream(base + "/index") >> index;
        if(index != 0) continue; // index 1 is the metadata node
        std::getline(std::ifstream(base + "/name"), name);
        int speed = 0;
        std::ifstream(base + "/device/../speed") >> speed;
        devs.push_back({id, name, speed});
    }
    return devs;
}

//! Short model name usable in a test name ("ZED 2i: ZED 2i" -> "ZED2i")
inline std::string modelTag(const std::string& name)
{
    std::string tag;
    for(char ch : name.substr(0, name.find(':')))
        if(std::isalnum(static_cast<unsigned char>(ch))) tag += ch;
    return tag;
}
// <---- Camera discovery

// ----> Image measures
#ifdef VIDEO_MOD_AVAILABLE
} // namespace test

#include "videocapture.hpp"

namespace test {
/*!
 * \brief Statistics of one of the two images of a frame
 */
struct ImageStats
{
    double luma = 0.;      //!< Mean luminance [0,255]
    double contrast = 0.;  //!< Standard deviation of the luminance
    double u = 128.;       //!< Mean of the U chroma component
    double v = 128.;       //!< Mean of the V chroma component
    double chroma = 0.;    //!< Mean distance of the chroma components from gray
    double detail = 0.;    //!< Mean horizontal variation of the luminance, higher for a sharper image
};

/*!
 * \brief Measure the left (side 0) or right (side 1) image of a side-by-side YUV 4:2:2 frame
 */
inline ImageStats imageStats(const sl_oc::video::Frame& frame, int side)
{
    const int w = frame.width / 2, h = frame.height, x0 = side * w;
    double sy = 0, sy2 = 0, su = 0, sv = 0, sc = 0, sd = 0;
    long n = 0;
    for(int r = 8; r < h - 8; r += 4)
    {
        const uint8_t* p = frame.data + static_cast<size_t>(r) * frame.width * 2;
        for(int c = x0 + 8; c < x0 + w - 8; c += 4)
        {
            const int cc = c & ~1; // U and V are shared by a pair of pixels
            const double y = p[cc * 2], u = p[cc * 2 + 1], v = p[cc * 2 + 3];
            sy += y; sy2 += y * y; su += u; sv += v;
            sc += std::fabs(u - 128.) + std::fabs(v - 128.);
            sd += std::fabs(2. * y - p[(cc - 2) * 2] - p[(cc + 2) * 2]);
            n++;
        }
    }
    ImageStats st;
    st.luma = sy / n;
    st.contrast = std::sqrt(std::max(0., sy2 / n - st.luma * st.luma));
    st.u = su / n;
    st.v = sv / n;
    st.chroma = sc / n;
    st.detail = sd / n;
    return st;
}

/*!
 * \brief Wait for new frames and measure the last ones
 * \param cap an opened camera
 * \param skip frames to wait before measuring, to let a new setting take effect
 * \param average frames to measure
 * \param left statistics of the left image
 * \param right statistics of the right image
 * \return false if the frames did not arrive
 */
inline bool measureImages(sl_oc::video::VideoCapture& cap, int skip, int average, ImageStats& left, ImageStats& right)
{
    ImageStats l, r;
    l.u = l.v = r.u = r.v = 0.;
    uint64_t lastTs = cap.getLastFrame(0).timestamp;
    int got = 0, tries = 0;
    while(got < skip + average)
    {
        const sl_oc::video::Frame& frame = cap.getLastFrame(500);
        if(frame.data == nullptr || frame.timestamp == lastTs)
        {
            if(++tries > 20) return false;
            continue;
        }
        tries = 0;
        lastTs = frame.timestamp;
        got++;
        if(got <= skip)
            continue;
        const ImageStats a = imageStats(frame, 0), b = imageStats(frame, 1);
        l.luma += a.luma; l.contrast += a.contrast; l.u += a.u; l.v += a.v; l.chroma += a.chroma; l.detail += a.detail;
        r.luma += b.luma; r.contrast += b.contrast; r.u += b.u; r.v += b.v; r.chroma += b.chroma; r.detail += b.detail;
    }
    const double k = average;
    left = {l.luma / k, l.contrast / k, l.u / k, l.v / k, l.chroma / k, l.detail / k};
    right = {r.luma / k, r.contrast / k, r.u / k, r.v / k, r.chroma / k, r.detail / k};
    return true;
}
#endif
// <---- Image measures

} // namespace test

#endif // TEST_COMMON_HPP
