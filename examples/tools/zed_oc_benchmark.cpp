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
//   zed_open_capture_benchmark --record f.raw --res HD720 --frames 10
//   zed_open_capture_benchmark --replay f.raw --par blockSize=5 --out /tmp/test
//
// The last two commands are used to compare stereo matching parameters on the same images.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
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
#include "cloud_viewer.hpp"

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
    std::string record, replay, out;
    std::string resName = "HD720";
    int frames = 10;
    std::map<std::string,std::string> par;
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

//! Apply `name=value` overrides to the stereo parameters
void applyPar(tools::StereoSgbmPar& par, const std::map<std::string,std::string>& values)
{
    bool customP = false;
    for(const auto& kv : values)
    {
        const double v = atof(kv.second.c_str());
        if(kv.first=="blockSize") par.blockSize = static_cast<int>(v);
        else if(kv.first=="minDisparity") par.minDisparity = static_cast<int>(v);
        else if(kv.first=="numDisparities") par.numDisparities = static_cast<int>(v);
        else if(kv.first=="mode") par.mode = static_cast<int>(v);
        else if(kv.first=="disp12MaxDiff") par.disp12MaxDiff = static_cast<int>(v);
        else if(kv.first=="preFilterCap") par.preFilterCap = static_cast<int>(v);
        else if(kv.first=="uniquenessRatio") par.uniquenessRatio = static_cast<int>(v);
        else if(kv.first=="speckleWindowSize") par.speckleWindowSize = static_cast<int>(v);
        else if(kv.first=="speckleRange") par.speckleRange = static_cast<int>(v);
        else if(kv.first=="minDepth_mm") par.minDepth_mm = v;
        else if(kv.first=="maxDepth_mm") par.maxDepth_mm = v;
        else if(kv.first=="matchScale") par.matchScale = v;
        else if(kv.first=="medianSize") par.medianSize = static_cast<int>(v);
        else if(kv.first=="refineRectification") par.refineRectification = static_cast<int>(v);
        else if(kv.first=="P1" || kv.first=="P2" || kv.first=="p1Factor" || kv.first=="p2Factor") customP = true;
        else std::cerr << "Unknown parameter: " << kv.first << std::endl;
    }

    // The penalties follow the block size, unless explicitly set
    par.P1 = tools::P1_FACTOR*par.blockSize*par.blockSize;
    par.P2 = tools::P2_FACTOR*par.blockSize*par.blockSize;
    if(customP)
    {
        if(values.count("p1Factor")) par.P1 = atoi(values.at("p1Factor").c_str())*par.blockSize*par.blockSize;
        if(values.count("p2Factor")) par.P2 = atoi(values.at("p2Factor").c_str())*par.blockSize*par.blockSize;
        if(values.count("P1")) par.P1 = atoi(values.at("P1").c_str());
        if(values.count("P2")) par.P2 = atoi(values.at("P2").c_str());
    }
}

//! Percentage of pixels with a valid depth
double validPercent(const cv::Mat& depth)
{
    int valid = 0;
    for(int r=0; r<depth.rows; r++)
    {
        const float* d = depth.ptr<float>(r);
        for(int c=0; c<depth.cols; c++)
            if(d[c]==d[c]) valid++;
    }
    return 100.*valid/static_cast<double>(depth.total());
}

void benchDepth(const Options& opt)
{
    printf("\n### Depth generation\n\n");
    printf("| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |\n");
    printf("|---|---|---|---|---|---|---|---|---|---|---|\n");

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
            applyPar(par, opt.par);
            par.matchScale = scale;

            tools::StereoDepth depth;
            if(!depth.init(calibration_file, cv::Size(w/2,h), par))
                return;

            // The first frames include the refinement of the rectification
            uint64_t lastTs = 0;
            double refine_ms = 0;
            for(int i=0; i<3; )
            {
                const video::Frame& frame = cap.getLastFrame(500);
                if(frame.data==nullptr || frame.timestamp==lastTs)
                    continue;
                lastTs = frame.timestamp;
                depth.compute(frame.data, frame.width, frame.height);
                refine_ms = std::max(refine_ms, depth.timing().refine_ms);
                i++;
            }

            tools::StereoDepth::Timing sum;
            double valid = 0;
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
                valid += validPercent(depth.depth());
                frames++;
            }
            const double elapsed = nowSec()-start;

            printf("| %s @ %d FPS | %dx%d | %.1f | %.1f | %.1f | %.1f | %.1f | %.1f | %.0f%% | %.0f%% | %.0f |\n", mode->name, cap.getFps(),
                   depth.depthSize().width, depth.depthSize().height, sum.convert_ms/frames, sum.rectify_ms/frames,
                   sum.match_ms/frames, sum.depth_ms/frames, sum.total()/frames, frames/elapsed, valid/frames,
                   100.*(cpuSec()-cpu0)/elapsed, refine_ms);
            fflush(stdout);
        }
    }

    printf("\n*Depth FPS*: depth maps per second, including the image acquisition. *Valid pixels*: pixels with a depth "
           "in the configured range, it depends on the scene. *CPU*: 100%% is one core fully used. *Refine*: time of the "
           "refinement of the rectification, executed on the first two frames and then once every 300 frames, not included in the total.\n");
}

// ----> Tuning utilities: the same raw frames are processed with different parameters
struct RawHeader
{
    char magic[8];
    int32_t width, height, count, serial;
};

int recordFrames(const Options& opt)
{
    Mode mode = kModes[3];
    for(const Mode& m : kModes)
        if(opt.resName==m.name && static_cast<int>(m.fps)<=30) mode = m;
    if(opt.resName=="HD2K") mode = kModes[0];

    video::VideoCapture cap(makeParams(mode));
    if(!cap.initializeVideo(opt.devId))
        return EXIT_FAILURE;

    // Let the automatic exposure settle
    const double start = nowSec();
    while(nowSec()-start < 2.0)
        cap.getLastFrame(500);

    std::ofstream file(opt.record, std::ios::binary);
    RawHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "ZEDOCRAW", 8);
    int w=0, h=0;
    cap.getFrameSize(w, h);
    header.width = w;
    header.height = h;
    header.count = opt.frames;
    header.serial = cap.getSerialNumber();
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    uint64_t lastTs = 0;
    for(int i=0; i<opt.frames; )
    {
        const video::Frame& frame = cap.getLastFrame(500);
        if(frame.data==nullptr || frame.timestamp==lastTs)
            continue;
        lastTs = frame.timestamp;
        file.write(reinterpret_cast<const char*>(frame.data), static_cast<std::streamsize>(w)*h*2);
        i++;
    }

    printf("Saved %d frames %dx%d of camera %d to %s\n", opt.frames, w, h, header.serial, opt.record.c_str());
    return file.good() ? EXIT_SUCCESS : EXIT_FAILURE;
}

int replayFrames(const Options& opt)
{
    std::ifstream file(opt.replay, std::ios::binary);
    RawHeader header;
    if(!file.read(reinterpret_cast<char*>(&header), sizeof(header)) || memcmp(header.magic, "ZEDOCRAW", 8)!=0)
    {
        std::cerr << "Not a recording: " << opt.replay << std::endl;
        return EXIT_FAILURE;
    }

    const size_t frameSize = static_cast<size_t>(header.width)*header.height*2;
    std::vector<std::vector<uint8_t>> frames(header.count, std::vector<uint8_t>(frameSize));
    for(std::vector<uint8_t>& f : frames)
        file.read(reinterpret_cast<char*>(f.data()), static_cast<std::streamsize>(frameSize));

    std::string calibration_file;
    if(!tools::downloadCalibrationFile(static_cast<unsigned int>(header.serial), calibration_file))
        return EXIT_FAILURE;

    tools::StereoSgbmPar par;
    applyPar(par, opt.par);

    tools::StereoDepth depth;
    if(!depth.init(calibration_file, cv::Size(header.width/2, header.height), par))
        return EXIT_FAILURE;

    // ----> Matcher used to verify the disparity: right image as reference, obtained by mirroring the images
    const cv::Size size = depth.depthSize();
    const int minDisp = par.minDisparity;
    const int numDisp = std::max(16, ((par.numDisparities+15)/16)*16);
    // Its parameters are fixed, to compare different configurations with the same reference
    const int checkBlock = 5;
    cv::Ptr<cv::StereoSGBM> checker = cv::StereoSGBM::create(minDisp, numDisp, checkBlock, tools::P1_FACTOR*checkBlock*checkBlock,
                                                              tools::P2_FACTOR*checkBlock*checkBlock, -1, 63, 10, 0, 0,
                                                              cv::StereoSGBM::MODE_HH);
    // <---- Matcher used to verify the disparity

    double valid = 0, confirmed = 0, wrong = 0;
    tools::StereoDepth::Timing sum;
    std::vector<cv::Mat> disparities;

    // The rectification is refined on the first frames
    for(int i=0; i<2; i++)
        depth.compute(frames.front().data(), header.width, header.height);
    {
        double offset=0, sx=0, sy=0;
        depth.getRectificationCorrection(offset, sx, sy);
        printf("rectification correction [px]: offset=%.2f, %.2f along the width, %.2f along the height\n", offset,
               sx*size.width, sy*size.height);
    }

    for(const std::vector<uint8_t>& f : frames)
    {
        depth.compute(f.data(), header.width, header.height);
        sum.convert_ms += depth.timing().convert_ms;
        sum.rectify_ms += depth.timing().rectify_ms;
        sum.match_ms += depth.timing().match_ms;
        sum.depth_ms += depth.timing().depth_ms;
        valid += validPercent(depth.depth());
        disparities.push_back(depth.disparity().clone());

        // Disparity of the right image
        cv::Mat leftFlip, rightFlip, dispRight16, dispRight;
        cv::flip(depth.leftGray(), leftFlip, 1);
        cv::flip(depth.rightGray(), rightFlip, 1);
        checker->compute(rightFlip, leftFlip, dispRight16);
        cv::flip(dispRight16, dispRight, 1);

        // A depth is confirmed if the two disparities agree within one pixel
        int ok = 0, bad = 0;
        for(int r=0; r<size.height; r++)
        {
            const float* dl = depth.disparity().ptr<float>(r);
            const float* z = depth.depth().ptr<float>(r);
            const short* dr = dispRight.ptr<short>(r);
            for(int c=0; c<size.width; c++)
            {
                if(z[c]!=z[c])
                    continue;
                const int cr = static_cast<int>(std::lround(c-dl[c]));
                if(cr<0 || dr[cr]<=0)
                    continue; // Not visible by the right camera or not matched: unknown
                if(std::fabs(dl[c]-dr[cr]/16.f)<=1.f) ok++;
                else bad++;
            }
        }
        confirmed += 100.*ok/static_cast<double>(size.area());
        wrong += 100.*bad/static_cast<double>(size.area());
    }

    // ----> Temporal noise of a static scene: standard deviation of the disparity of each pixel
    std::vector<double> noise;
    if(disparities.size()>=3)
    {
        for(int r=0; r<size.height; r+=2)
        {
            for(int c=0; c<size.width; c+=2)
            {
                double s=0, s2=0;
                bool all = true;
                for(const cv::Mat& d : disparities)
                {
                    const float v = d.at<float>(r,c);
                    if(v!=v) { all = false; break; }
                    s += v; s2 += static_cast<double>(v)*v;
                }
                if(!all)
                    continue;
                const double n = static_cast<double>(disparities.size());
                noise.push_back(std::sqrt(std::max(0., s2/n-(s/n)*(s/n))));
            }
        }
    }
    // <---- Temporal noise

    const double n = static_cast<double>(frames.size());
    printf("depth %dx%d valid=%.1f%% confirmed=%.1f%% wrong=%.2f%% noise[px] p50=%.3f p90=%.3f stable=%.1f%% | ms convert=%.1f rectify=%.1f match=%.1f depth=%.1f total=%.1f\n",
           size.width, size.height, valid/n, confirmed/n, wrong/n, percentile(noise,0.5), percentile(noise,0.9),
           100.*noise.size()*4./static_cast<double>(size.area()),
           sum.convert_ms/n, sum.rectify_ms/n, sum.match_ms/n, sum.depth_ms/n, sum.total()/n);

    // ----> Quality of the rectification: the same point must be on the same row of the two images
    {
        cv::Ptr<cv::ORB> orb = cv::ORB::create(4000, 1.2f, 4, 15, 0, 2, cv::ORB::HARRIS_SCORE, 31, 10);
        std::vector<cv::KeyPoint> kl, kr;
        cv::Mat dl, dr;
        orb->detectAndCompute(depth.leftGray(), cv::noArray(), kl, dl);
        orb->detectAndCompute(depth.rightGray(), cv::noArray(), kr, dr);
        std::vector<double> dy;
        if(!dl.empty() && !dr.empty())
        {
            std::vector<cv::DMatch> matches;
            cv::BFMatcher(cv::NORM_HAMMING, true).match(dl, dr, matches);
            for(const cv::DMatch& m : matches)
            {
                const cv::Point2f a = kl[m.queryIdx].pt, b = kr[m.trainIdx].pt;
                if(m.distance<40 && a.x-b.x>-2 && a.x-b.x<numDisp+minDisp && std::fabs(a.y-b.y)<6)
                {
                    dy.push_back(a.y-b.y);
                }
            }
        }
        std::vector<double> ady;
        for(double v : dy) ady.push_back(std::fabs(v));
        printf("rectification: %zu matched points, vertical offset [px] median=%.2f |median|=%.2f p90=%.2f\n", dy.size(),
               percentile(dy,0.5), percentile(ady,0.5), percentile(ady,0.9));
    }
    // <---- Quality of the rectification

    if(!opt.out.empty())
    {
        cv::Mat image, left;
        depth.getDepthImage(image);
        depth.getLeftColor(frames.back().data(), left);
        cv::imwrite(opt.out + "_depth.png", image);
        cv::imwrite(opt.out + "_left.jpg", left);

        // Point cloud seen from the side
        cv::Mat cloud, colors, cloudImage;
        depth.getPointCloud(cloud);
        cv::resize(left, colors, cloud.size(), 0, 0, cv::INTER_AREA);
        tools::CloudViewer viewer("Point Cloud", cv::Size(960, 540));
        viewer.setView(0.5f, -0.25f, 0.8f);
        viewer.render(cloud, colors, cloudImage);
        cv::imwrite(opt.out + "_cloud.png", cloudImage);

        // Rectified images side by side with horizontal lines, to verify the alignment
        cv::Mat pair;
        cv::hconcat(depth.leftGray(), depth.rightGray(), pair);
        for(int r=20; r<pair.rows; r+=40)
            cv::line(pair, cv::Point(0,r), cv::Point(pair.cols,r), cv::Scalar(255), 1);
        cv::imwrite(opt.out + "_pair.jpg", pair);
    }

    return EXIT_SUCCESS;
}
// <---- Tuning utilities

void usage()
{
    std::cout << "Usage: zed_open_capture_benchmark [--video] [--sensors] [--depth] [--duration <sec>] [--dev <id>]\n"
                 "                                  [--par <name>=<value>]...\n"
                 "       zed_open_capture_benchmark --record <file> [--res HD2K|HD1080|HD720|VGA] [--frames <n>]\n"
                 "       zed_open_capture_benchmark --replay <file> [--par <name>=<value>]... [--out <prefix>]\n";
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
        else if(arg=="--record" && hasValue) opt.record = argv[++i];
        else if(arg=="--replay" && hasValue) opt.replay = argv[++i];
        else if(arg=="--out" && hasValue) opt.out = argv[++i];
        else if(arg=="--res" && hasValue) opt.resName = argv[++i];
        else if(arg=="--frames" && hasValue) opt.frames = atoi(argv[++i]);
        else if(arg=="--par" && hasValue)
        {
            const std::string kv = argv[++i];
            const size_t eq = kv.find('=');
            if(eq==std::string::npos) { usage(); return EXIT_FAILURE; }
            opt.par[kv.substr(0,eq)] = kv.substr(eq+1);
        }
        else { usage(); return arg=="--help" ? EXIT_SUCCESS : EXIT_FAILURE; }
    }

    if(!opt.record.empty())
        return recordFrames(opt);
    if(!opt.replay.empty())
        return replayFrames(opt);

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
