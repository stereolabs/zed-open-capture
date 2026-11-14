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

} // namespace test

#endif // TEST_COMMON_HPP
