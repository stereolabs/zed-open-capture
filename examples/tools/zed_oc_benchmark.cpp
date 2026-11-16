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

// Measures the performance of image, sensors and depth acquisition on the host. It does not need a display,
// so it can be started from a remote terminal:
//
//   zed_open_capture_benchmark                     all the measures, results as Markdown tables
//   zed_open_capture_benchmark --video --sensors   only the selected measures

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>

#include "videocapture.hpp"
#ifdef SENSORS_MOD_AVAILABLE
#include "sensorcapture.hpp"
#endif

#include <opencv2/opencv.hpp>

#include "calibration.hpp"
#include "stereo.hpp"
#include "depth.hpp"

using namespace sl_oc;

namespace {

struct Mode
{
    video::RESOLUTION res;
    video::FPS fps;
    const char* name;
};

const Mode kModes[] = {
    {video::RESOLUTION::HD2K,   video::FPS::FPS_15,  "HD2K"},
    {video::RESOLUTION::HD1080, video::FPS::FPS_30,  "HD1080"},
    {video::RESOLUTION::HD720,  video::FPS::FPS_60,  "HD720"},
    {video::RESOLUTION::HD720,  video::FPS::FPS_30,  "HD720"},
    {video::RESOLUTION::VGA,    video::FPS::FPS_100, "VGA"},
    {video::RESOLUTION::VGA,    video::FPS::FPS_30,  "VGA"},
};

double nowSec() { return static_cast<double>(getSteadyTimestamp())/1e9; }

//! CPU time used by the process in seconds: 1 second per second for each core fully used
double cpuSec()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1e6 + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1e6;
}

//! Memory used by the process in MB
double memoryMB()
{
    std::ifstream status("/proc/self/status");
    std::string line;
    while(std::getline(status, line))
    {
        if(line.compare(0, 6, "VmRSS:")==0)
            return atof(line.c_str()+6)/1024.;
    }
    return 0.;
}

double percentile(std::vector<double> values, double p)
{
    if(values.empty())
        return std::nan("");
    std::sort(values.begin(), values.end());
    return values[std::min(values.size()-1, static_cast<size_t>(p*values.size()))];
}

struct Options
{
    bool video = false, sensors = false, depth = false;
    double duration = 5.0;
    int devId = -1;
};

video::VideoParams makeParams(const Mode& mode)
{
    video::VideoParams params;
    params.res = mode.res;
    params.fps = mode.fps;
    params.verbose = VERBOSITY::ERROR;
    return params;
}

// ----> Statistics of a sensors stream, collected by a thread
#ifdef SENSORS_MOD_AVAILABLE
struct SensorsStats
{
    std::atomic<bool> stop{false};
    std::vector<uint64_t> imuTs, syncTs;
    std::vector<double> latencyMs;
    int mag = 0, env = 0;

    void run(sensors::SensorCapture& sens)
    {
        while(!stop)
        {
            const std::vector<sensors::data::Imu> queue = sens.getIMUData(20000);
            const uint64_t now = getTimestampNow();
            for(const sensors::data::Imu& imu : queue)
            {
                imuTs.push_back(imu.timestamp);
                if(imu.sync) syncTs.push_back(imu.timestamp);
            }
            if(!queue.empty())
                latencyMs.push_back((static_cast<double>(now)-static_cast<double>(queue.back().timestamp))/1e6);
            if(sens.getLastMagnetometerData(100).valid==sensors::data::Magnetometer::NEW_VAL) mag++;
            if(sens.getLastEnvironmentData(100).valid==sensors::data::Environment::NEW_VAL) env++;
        }
    }

    void print(const std::string& label, double elapsed, double cpu, const std::vector<uint64_t>& frameTs) const
    {
        // Frames that received the timestamp of the IMU sample acquired with their sync signal
        std::string synced = "-";
        if(!frameTs.empty())
        {
            int count = 0;
            for(uint64_t ts : frameTs)
                if(std::binary_search(syncTs.begin(), syncTs.end(), ts)) count++;
            synced = std::to_string(static_cast<int>(std::lround(100.*count/static_cast<double>(frameTs.size())))) + "%";
        }

        std::vector<double> dt;
        for(size_t i=1; i<imuTs.size(); i++)
            dt.push_back((static_cast<double>(imuTs[i])-static_cast<double>(imuTs[i-1]))/1e6);
        const double period = percentile(dt, 0.5);
        int lost = 0;
        for(double d : dt)
            lost += std::max(0, static_cast<int>(std::lround(d/period))-1);

        printf("| %s | %.1f | %d | %.3f | %.3f / %.3f | %.2f | %.1f | %.1f | %s | %.0f%% |\n", label.c_str(),
               imuTs.size()/elapsed, lost, period, percentile(dt,0.01), percentile(dt,0.99), percentile(latencyMs,0.5),
               mag/elapsed, env/elapsed, synced.c_str(), cpu);
    }

    static void header()
    {
        printf("| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1%% / 99%% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |\n");
        printf("|---|---|---|---|---|---|---|---|---|---|\n");
    }
};
#endif
// <---- Statistics of a sensors stream

void benchVideo(const Options& opt)
{
    printf("\n### Image retrieval\n\n");
    printf("| Resolution | Requested FPS | Received FPS | Lost frames | Frame period 1%% / 99%% [ms] | Latency [ms] | CPU | Memory [MB] | Open [s] |\n");
    printf("|---|---|---|---|---|---|---|---|---|\n");

    for(const Mode& mode : kModes)
    {
        video::VideoCapture cap(makeParams(mode));
        const double openStart = nowSec();
        if(!cap.initializeVideo(opt.devId))
        {
            printf("| %s | %d | not available | | | | | | |\n", mode.name, static_cast<int>(mode.fps));
            continue;
        }
        const double openTime = nowSec()-openStart;

        int w=0, h=0;
        cap.getFrameSize(w, h);

        // Let the stream start
        for(const double t0=nowSec(); nowSec()-t0<0.7; )
            cap.getLastFrame(500);

        std::vector<double> dt, latency;
        uint64_t lastTs = 0;
        int frames = 0;
        volatile unsigned int checksum = 0;

        const double cpu0 = cpuSec();
        const double start = nowSec();
        while(nowSec()-start < opt.duration)
        {
            const video::Frame& frame = cap.getLastFrame(500);
            if(frame.data==nullptr || frame.timestamp==lastTs)
                continue;

            latency.push_back((static_cast<double>(getTimestampNow())-static_cast<double>(frame.timestamp))/1e6);
            if(lastTs!=0)
                dt.push_back((static_cast<double>(frame.timestamp)-static_cast<double>(lastTs))/1e6);
            lastTs = frame.timestamp;
            frames++;

            // Touch the image as a minimal use of the data
            checksum = checksum + frame.data[0] + frame.data[static_cast<size_t>(w)*h*2-1];
        }
        const double elapsed = nowSec()-start;
        const double cpu = 100.*(cpuSec()-cpu0)/elapsed;

        // A missing frame leaves a gap of two or more periods between the timestamps
        const double period = 1000./cap.getFps();
        int lost = 0;
        for(double d : dt)
            lost += std::max(0, static_cast<int>(std::lround(d/period))-1);

        printf("| %s (%dx%d) | %d | %.1f | %.1f%% | %.2f / %.2f | %.1f | %.0f%% | %.0f | %.2f |\n", mode.name, w, h, cap.getFps(),
               frames/elapsed, frames>0 ? 100.*lost/static_cast<double>(frames+lost) : 100.,
               percentile(dt,0.01), percentile(dt,0.99), percentile(latency,0.5), cpu, memoryMB(), openTime);
        fflush(stdout);
    }

    printf("\n*Received FPS*: frames available to the application. *Lost frames*: frames sent by the camera that did not "
           "reach the application, detected from the gaps between the timestamps. *Latency*: time from the start of the "
           "frame to its availability. *CPU*: 100%% is one core fully used. *Memory*: memory used by the process, including "
           "the three copies of the frame kept by the library. *Open*: time to open the camera and get it ready for settings.\n");
}

void benchSensors(const Options& opt)
{
#ifdef SENSORS_MOD_AVAILABLE
    printf("\n### Sensors retrieval\n\n");

    // The serial number of the camera selects its sensors module
    int sn = -1;
    {
        video::VideoParams params;
        params.res = video::RESOLUTION::VGA;
        params.fps = video::FPS::FPS_15;
        params.verbose = VERBOSITY::NONE;
        video::VideoCapture cap(params);
        if(cap.initializeVideo(opt.devId))
            sn = cap.getSerialNumber();
    }

    SensorsStats::header();

    for(int m=-1; m<static_cast<int>(sizeof(kModes)/sizeof(kModes[0])); m++)
    {
        sensors::SensorCapture sens(VERBOSITY::ERROR);
        std::unique_ptr<video::VideoCapture> cap;
        std::string label = "none";

        if(m>=0)
        {
            cap.reset(new video::VideoCapture(makeParams(kModes[m])));
            if(!cap->initializeVideo(opt.devId))
                continue;
            label = std::string(kModes[m].name) + " @ " + std::to_string(cap->getFps()) + " FPS";
        }

        if(!sens.initializeSensors(sn))
        {
            printf("The camera has no sensors module\n");
            return;
        }

        if(cap)
            cap->enableSensorSync(&sens);

        // Let the streams and the clock alignment settle
        sens.getIMUData(500000);
        usleep(1500000);
        sens.getIMUData(0);

        SensorsStats stats;
        const double cpu0 = cpuSec();
        const double start = nowSec();
        std::thread sensThread([&]{ stats.run(sens); });

        uint64_t lastTs = 0;
        std::vector<uint64_t> frameTs;
        while(nowSec()-start < opt.duration)
        {
            if(cap)
            {
                const video::Frame& frame = cap->getLastFrame(100);
                if(frame.data!=nullptr && frame.timestamp!=lastTs)
                {
                    lastTs = frame.timestamp;
                    frameTs.push_back(frame.timestamp);
                }
            }
            else
            {
                usleep(10000);
            }
        }
        stats.stop = true;
        sensThread.join();
        const double elapsed = nowSec()-start;

        // The last frames can precede the reading of their sensors data
        if(frameTs.size()>10)
            frameTs.resize(frameTs.size()-5);

        stats.print(label, elapsed, 100.*(cpuSec()-cpu0)/elapsed, frameTs);
        fflush(stdout);
    }

    printf("\n*Lost samples*: gaps in the IMU timestamps. *Latency*: time from the acquisition of a sample to its availability. "
           "*Synced frames*: frames that received the timestamp of the IMU sample acquired with their hardware sync signal. "
           "*CPU* includes the video stream, when active.\n");
#else
    (void)opt;
    printf("\nSensors module not available\n");
#endif
}

void benchDepth(const Options& opt)
{
    printf("\n### Depth generation\n\n");
    printf("| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | CPU |\n");
    printf("|---|---|---|---|---|---|---|---|---|\n");

    const Mode* depthModes[] = {&kModes[0], &kModes[1], &kModes[3], &kModes[5]};

    for(const Mode* mode : depthModes)
    {
        for(double scale : {1.0, 0.5})
        {
            video::VideoCapture cap(makeParams(*mode));
            if(!cap.initializeVideo(opt.devId))
                continue;

            int w=0, h=0;
            cap.getFrameSize(w, h);

            std::string calibration_file;
            if(!tools::downloadCalibrationFile(static_cast<unsigned int>(cap.getSerialNumber()), calibration_file))
                return;

            // Default parameters, to get comparable results
            tools::StereoSgbmPar par;
            par.matchScale = scale;

            tools::StereoDepth depth;
            if(!depth.init(calibration_file, cv::Size(w/2,h), par))
                return;

            // The first frames are not measured: memory allocation
            uint64_t lastTs = 0;
            for(int i=0; i<3; )
            {
                const video::Frame& frame = cap.getLastFrame(500);
                if(frame.data==nullptr || frame.timestamp==lastTs)
                    continue;
                lastTs = frame.timestamp;
                depth.compute(frame.data, frame.width, frame.height);
                i++;
            }

            tools::StereoDepth::Timing sum;
            int frames = 0;

            const double cpu0 = cpuSec();
            const double start = nowSec();
            while(nowSec()-start < opt.duration || frames<5)
            {
                const video::Frame& frame = cap.getLastFrame(500);
                if(frame.data==nullptr || frame.timestamp==lastTs)
                    continue;
                lastTs = frame.timestamp;

                depth.compute(frame.data, frame.width, frame.height);

                sum.convert_ms += depth.timing().convert_ms;
                sum.rectify_ms += depth.timing().rectify_ms;
                sum.match_ms += depth.timing().match_ms;
                sum.depth_ms += depth.timing().depth_ms;
                frames++;
            }
            const double elapsed = nowSec()-start;

            printf("| %s @ %d FPS | %dx%d | %.1f | %.1f | %.1f | %.1f | %.1f | %.1f | %.0f%% |\n", mode->name, cap.getFps(),
                   depth.depthSize().width, depth.depthSize().height, sum.convert_ms/frames, sum.rectify_ms/frames,
                   sum.match_ms/frames, sum.depth_ms/frames, sum.total()/frames, frames/elapsed,
                   100.*(cpuSec()-cpu0)/elapsed);
            fflush(stdout);
        }
    }

    printf("\n*Depth FPS*: depth maps per second, including the image acquisition. *CPU*: 100%% is one core fully used.\n");
}

void usage()
{
    std::cout << "Usage: zed_open_capture_benchmark [--video] [--sensors] [--depth] [--duration <sec>] [--dev <id>]\n";
}

}

int main(int argc, char *argv[])
{
    Options opt;

    for(int i=1; i<argc; i++)
    {
        const std::string arg = argv[i];
        const bool hasValue = (i+1<argc);

        if(arg=="--video") opt.video = true;
        else if(arg=="--sensors") opt.sensors = true;
        else if(arg=="--depth") opt.depth = true;
        else if(arg=="--duration" && hasValue) opt.duration = atof(argv[++i]);
        else if(arg=="--dev" && hasValue) opt.devId = atoi(argv[++i]);
        else { usage(); return arg=="--help" ? EXIT_SUCCESS : EXIT_FAILURE; }
    }

    if(!opt.video && !opt.sensors && !opt.depth)
        opt.video = opt.sensors = opt.depth = true;

    // ----> Description of the test
    const std::vector<video::VideoDevice> devices = video::VideoCapture::getDeviceList();
    if(devices.empty())
    {
        std::cerr << "No Stereolabs camera detected" << std::endl;
        return EXIT_FAILURE;
    }

    std::string host = "unknown";
    {
        std::ifstream model("/proc/device-tree/model");
        if(model.good())
        {
            std::getline(model, host, '\0');
        }
    }
    printf("ZED Open Capture v%d.%d.%d benchmark - host: %s, %u cores - OpenCV %s\n", ZED_OC_MAJOR_VERSION, ZED_OC_MINOR_VERSION,
           ZED_OC_PATCH_VERSION, host.c_str(), std::thread::hardware_concurrency(), CV_VERSION);
    for(const video::VideoDevice& dev : devices)
    {
        if(opt.devId==-1 || opt.devId==dev.dev_id)
            printf("Camera: %s, serial number %d, USB %d Mbps%s\n", dev.dev_name.c_str(), dev.serial_number, dev.usb_speed,
                   dev.usb_speed<5000 ? " (USB 2.0 mode: most of the resolutions are not available)" : "");
        if(opt.devId==-1)
            break;
    }
    // <---- Description of the test

    if(opt.video) benchVideo(opt);
    if(opt.sensors) benchSensors(opt);
    if(opt.depth) benchDepth(opt);

    return EXIT_SUCCESS;
}
