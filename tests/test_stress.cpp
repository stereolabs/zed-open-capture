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

// Stress test of the VideoCapture and SensorCapture classes.
// It repeats for a long time the operations of an application: open and close, manual gain and exposure at start-up
// and while streaming, settings changed by a thread while another one receives the frames, long acquisitions of
// video and sensors. Every case runs on each connected Stereolabs camera. No display is needed.
//
// The default duration is about 20 minutes for each camera. Set the environment variable ZED_OC_STRESS_SCALE to
// change it: e.g. 0.1 for a quick run, 3 for a longer one.
//
// The cases that verify the brightness of the image need a scene that is not too dark.

#include "test_common.hpp"

#ifdef SENSORS_MOD_AVAILABLE
#include "sensorcapture.hpp"
#endif

#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace sl_oc::video;

namespace {

//! Multiplier of iterations and durations
double stressScale()
{
    const char* value = getenv("ZED_OC_STRESS_SCALE");
    const double scale = value ? atof(value) : 1.0;
    return scale > 0. ? scale : 1.0;
}

int scaled(int count)
{
    return std::max(1, static_cast<int>(std::lround(count * stressScale())));
}

VideoParams makeParams(RESOLUTION res, FPS fps, int verbose = sl_oc::VERBOSITY::ERROR)
{
    VideoParams params;
    params.res = res;
    params.fps = fps;
    params.verbose = verbose;
    return params;
}

std::string devName(const test::VideoDev& dev)
{
    return test::modelTag(dev.name) + "_video" + std::to_string(dev.id);
}

//! Memory used by the process in MB
double memoryMB()
{
    std::ifstream status("/proc/self/status");
    std::string line;
    while(std::getline(status, line))
        if(line.compare(0, 6, "VmRSS:") == 0) return atof(line.c_str() + 6) / 1024.;
    return 0.;
}

// ----> Manual gain and exposure
//! Gain and exposure of the two sensors
struct Preset
{
    int gainL, expL, gainR, expR;
};

// Dark and bright settings, also different on the two sensors
const Preset kPresets[] = {{10, 12, 10, 12}, {55, 55, 55, 55}, {10, 12, 55, 55}, {55, 55, 10, 12}};
const int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

//! Result of the application of a preset
struct ApplyResult
{
    bool accepted = true;  //!< All the functions returned true
    bool readBack = true;  //!< The values read back are the ones written
    std::string detail;
};

ApplyResult applyPreset(VideoCapture& cap, const Preset& p)
{
    ApplyResult res;
    res.accepted = cap.setGain(CAM_SENS_POS::LEFT, p.gainL) && cap.setGain(CAM_SENS_POS::RIGHT, p.gainR) &&
                   cap.setExposure(CAM_SENS_POS::LEFT, p.expL) && cap.setExposure(CAM_SENS_POS::RIGHT, p.expR);

    const int gl = cap.getGain(CAM_SENS_POS::LEFT), gr = cap.getGain(CAM_SENS_POS::RIGHT);
    const int el = cap.getExposure(CAM_SENS_POS::LEFT), er = cap.getExposure(CAM_SENS_POS::RIGHT);
    res.readBack = std::abs(gl - p.gainL) <= 1 && std::abs(gr - p.gainR) <= 1 && std::abs(el - p.expL) <= 1 && std::abs(er - p.expR) <= 1;
    if(!res.readBack || !res.accepted)
    {
        res.detail = "written gain " + std::to_string(p.gainL) + "/" + std::to_string(p.gainR) + " exposure " + std::to_string(p.expL) + "/" +
                     std::to_string(p.expR) + ", read gain " + std::to_string(gl) + "/" + std::to_string(gr) + " exposure " +
                     std::to_string(el) + "/" + std::to_string(er);
    }
    return res;
}

/*!
 * \brief Brightness of the two images for the dark and the bright settings, used to verify that a setting has
 *        really been applied to the image and not only stored by the camera
 */
struct BrightnessReference
{
    double dark[2] = {0, 0}, bright[2] = {0, 0}; // left, right
    bool usable = false;

    void measure(VideoCapture& cap)
    {
        test::ImageStats l, r;
        applyPreset(cap, kPresets[0]);
        if(!test::measureImages(cap, 15, 5, l, r)) return;
        dark[0] = l.luma; dark[1] = r.luma;
        applyPreset(cap, kPresets[1]);
        if(!test::measureImages(cap, 15, 5, l, r)) return;
        bright[0] = l.luma; bright[1] = r.luma;
        usable = bright[0] > dark[0] * 1.8 + 8 && bright[1] > dark[1] * 1.8 + 8;
    }

    //! True if the image of a sensor is closer to the expected setting than to the other one
    bool matches(int side, bool expectBright, double luma) const
    {
        // Geometric mean: the brightness is proportional to gain and exposure
        const double threshold = std::sqrt(std::max(1., dark[side]) * std::max(1., bright[side]));
        return expectBright ? luma > threshold : luma < threshold;
    }
};

bool isBright(int gain)
{
    return gain > 30;
}

/*!
 * \brief Counters of the images that did not match the written settings
 */
struct ImageCheck
{
    int late[2] = {0, 0};       //!< Correct only after more frames than expected
    int persistent[2] = {0, 0}; //!< Still wrong after half a second, with the values correctly read back
    std::string first;

    /*!
     * \brief Verify that the two images match a preset. If not, wait and verify again: a setting that the sensor
     *        never applies stays wrong, a change of the scene or a late update do not
     */
    bool verify(VideoCapture& cap, const BrightnessReference& ref, const Preset& preset, int skip, const std::string& where)
    {
        test::ImageStats l, r;
        if(!test::measureImages(cap, skip, 1, l, r)) return false;
        bool ok[2] = {ref.matches(0, isBright(preset.gainL), l.luma), ref.matches(1, isBright(preset.gainR), r.luma)};
        if(ok[0] && ok[1]) return true;

        const double firstLuma[2] = {l.luma, r.luma};
        if(!test::measureImages(cap, 15, 2, l, r)) return false;
        const bool later[2] = {ref.matches(0, isBright(preset.gainL), l.luma), ref.matches(1, isBright(preset.gainR), r.luma)};
        for(int side = 0; side < 2; side++)
        {
            if(ok[side]) continue;
            if(later[side]) late[side]++; else persistent[side]++;
            if(first.empty())
            {
                first = where + ": " + (side == 0 ? "left" : "right") + " image brightness " + std::to_string(firstLuma[side]) + " then " +
                        std::to_string(side == 0 ? l.luma : r.luma) + ", expected " + (isBright(side == 0 ? preset.gainL : preset.gainR) ? "bright" : "dark") +
                        " (dark " + std::to_string(ref.dark[side]) + ", bright " + std::to_string(ref.bright[side]) + "), read back gain " +
                        std::to_string(cap.getGain(side == 0 ? CAM_SENS_POS::LEFT : CAM_SENS_POS::RIGHT)) + " exposure " +
                        std::to_string(cap.getExposure(side == 0 ? CAM_SENS_POS::LEFT : CAM_SENS_POS::RIGHT));
            }
        }
        return true;
    }
};

class Stress : public testing::TestWithParam<test::VideoDev>
{
protected:
    void SetUp() override
    {
        if(!GetParam().isUsb3())
            GTEST_SKIP() << "the camera is connected in USB 2.0 mode";
    }
};

// Manual gain and exposure written right after the open: they must always be applied to both sensors
TEST_P(Stress, ManualExposureRightAfterTheOpen)
{
    const VideoParams params = makeParams(RESOLUTION::HD720, FPS::FPS_30);

    BrightnessReference ref;
    {
        VideoCapture cap(params);
        ASSERT_TRUE(cap.initializeVideo(GetParam().id));
        ref.measure(cap);
    }
    if(!ref.usable)
        GTEST_SKIP() << "the scene is too dark or too bright to verify the exposure from the image";

    const int cycles = scaled(150);
    int openFailed = 0, refused = 0, readBackWrong = 0, noFrames = 0;
    ImageCheck images;
    std::string firstFailure;

    for(int i = 0; i < cycles; i++)
    {
        const Preset& preset = kPresets[i % kPresetCount];

        VideoCapture cap(params);
        if(!cap.initializeVideo(GetParam().id)) { openFailed++; continue; }

        // No wait: the first setting must work as soon as the open returns
        const ApplyResult res = applyPreset(cap, preset);
        if(!res.accepted) refused++;
        if(!res.readBack) readBackWrong++;
        if(firstFailure.empty() && !res.detail.empty()) firstFailure = "cycle " + std::to_string(i) + ": " + res.detail;

        if(!images.verify(cap, ref, preset, 6, "start-up " + std::to_string(i))) { noFrames++; continue; }

        // The values must still be there
        if(std::abs(cap.getGain(CAM_SENS_POS::LEFT) - preset.gainL) > 1 || std::abs(cap.getGain(CAM_SENS_POS::RIGHT) - preset.gainR) > 1 ||
           std::abs(cap.getExposure(CAM_SENS_POS::LEFT) - preset.expL) > 1 || std::abs(cap.getExposure(CAM_SENS_POS::RIGHT) - preset.expR) > 1)
            readBackWrong++;
    }

    RecordProperty("cycles", cycles);
    std::cout << "[  STRESS  ] " << cycles << " start-ups: open failed " << openFailed << ", settings refused " << refused << ", wrong read back "
              << readBackWrong << ", image never updated left " << images.persistent[0] << " right " << images.persistent[1]
              << ", image updated late left " << images.late[0] << " right " << images.late[1] << std::endl;

    EXPECT_EQ(openFailed, 0);
    EXPECT_EQ(noFrames, 0);
    EXPECT_EQ(refused, 0) << firstFailure;
    EXPECT_EQ(readBackWrong, 0) << firstFailure;
    EXPECT_EQ(images.persistent[0], 0) << "left image with the wrong exposure after the start-up. " << images.first;
    EXPECT_EQ(images.persistent[1], 0) << "right image with the wrong exposure after the start-up. " << images.first;
    EXPECT_LE(images.late[0] + images.late[1], cycles / 100) << "images updated later than 6 frames. " << images.first;
}

// Manual gain and exposure changed while streaming: every write must be applied to both sensors
TEST_P(Stress, ManualExposureWhileStreaming)
{
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    BrightnessReference ref;
    ref.measure(cap);
    if(!ref.usable)
        GTEST_SKIP() << "the scene is too dark or too bright to verify the exposure from the image";

    const int cycles = scaled(1200);
    int refused = 0, readBackWrong = 0, noFrames = 0;
    ImageCheck images;
    std::string firstFailure;
    std::mt19937 rng(42);

    for(int i = 0; i < cycles; i++)
    {
        const Preset& preset = kPresets[rng() % kPresetCount];

        const ApplyResult res = applyPreset(cap, preset);
        if(!res.accepted) refused++;
        if(!res.readBack) readBackWrong++;
        if(firstFailure.empty() && !res.detail.empty()) firstFailure = "cycle " + std::to_string(i) + ": " + res.detail;

        if(!images.verify(cap, ref, preset, 5, "change " + std::to_string(i))) { noFrames++; continue; }
    }

    std::cout << "[  STRESS  ] " << cycles << " changes while streaming: refused " << refused << ", wrong read back " << readBackWrong
              << ", image never updated left " << images.persistent[0] << " right " << images.persistent[1]
              << ", image updated late left " << images.late[0] << " right " << images.late[1] << std::endl;

    EXPECT_TRUE(cap.isOpened());
    EXPECT_FALSE(cap.getAECAGC()) << "the automatic exposure came back";
    EXPECT_EQ(noFrames, 0);
    EXPECT_EQ(refused, 0) << firstFailure;
    EXPECT_EQ(readBackWrong, 0) << firstFailure;
    EXPECT_EQ(images.persistent[0], 0) << "left image did not follow the written values. " << images.first;
    EXPECT_EQ(images.persistent[1], 0) << "right image did not follow the written values. " << images.first;
    EXPECT_LE(images.late[0] + images.late[1], cycles / 100) << "images updated later than 5 frames. " << images.first;
}

// With the automatic control disabled, gain, exposure and image brightness must stay constant
TEST_P(Stress, ManualExposureStaysConstant)
{
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    const Preset preset = {40, 40, 40, 40};
    ASSERT_TRUE(applyPreset(cap, preset).readBack);

    test::ImageStats l, r;
    ASSERT_TRUE(test::measureImages(cap, 20, 5, l, r));
    const double ratio0 = (l.luma + 1) / (r.luma + 1);

    const double duration = 300.0 * stressScale();
    int samples = 0, readErrors = 0, changed = 0, autoBack = 0;
    double minRatio = ratio0, maxRatio = ratio0, minLuma = l.luma, maxLuma = l.luma;
    std::string firstChange;

    const double start = test::nowSec();
    while(test::nowSec() - start < duration)
    {
        ASSERT_TRUE(test::measureImages(cap, 20, 5, l, r)) << "no frames after " << (test::nowSec() - start) << " s";

        const int gl = cap.getGain(CAM_SENS_POS::LEFT), gr = cap.getGain(CAM_SENS_POS::RIGHT);
        const int el = cap.getExposure(CAM_SENS_POS::LEFT), er = cap.getExposure(CAM_SENS_POS::RIGHT);
        if(gl < 0 || gr < 0 || el < 0 || er < 0) readErrors++;
        if(std::abs(gl - 40) > 1 || std::abs(gr - 40) > 1 || std::abs(el - 40) > 1 || std::abs(er - 40) > 1)
        {
            changed++;
            if(firstChange.empty())
                firstChange = "after " + std::to_string(static_cast<int>(test::nowSec() - start)) + " s: gain " + std::to_string(gl) + "/" +
                              std::to_string(gr) + " exposure " + std::to_string(el) + "/" + std::to_string(er);
        }
        if(cap.getAECAGC()) autoBack++;

        const double ratio = (l.luma + 1) / (r.luma + 1);
        minRatio = std::min(minRatio, ratio); maxRatio = std::max(maxRatio, ratio);
        minLuma = std::min(minLuma, l.luma); maxLuma = std::max(maxLuma, l.luma);
        samples++;
    }

    std::cout << "[  STRESS  ] " << static_cast<int>(duration) << " s with manual exposure, " << samples << " checks: values changed " << changed
              << ", read errors " << readErrors << ", left/right brightness ratio " << minRatio << " - " << maxRatio << ", left brightness "
              << minLuma << " - " << maxLuma << std::endl;

    EXPECT_EQ(readErrors, 0);
    EXPECT_EQ(changed, 0) << firstChange;
    EXPECT_EQ(autoBack, 0) << "the automatic exposure came back";
    // The scene can change, but in the same way for the two sensors: a drift of one sensor changes the ratio
    EXPECT_LT(maxRatio / minRatio, 1.15) << "the brightness of one image changed respect to the other";
}
// <---- Manual gain and exposure

// ----> Robustness
TEST_P(Stress, OpenAndCloseInEveryMode)
{
    struct Mode { RESOLUTION res; FPS fps; };
    const Mode modes[] = {{RESOLUTION::HD2K, FPS::FPS_15}, {RESOLUTION::HD1080, FPS::FPS_30}, {RESOLUTION::HD720, FPS::FPS_60},
                          {RESOLUTION::VGA, FPS::FPS_100}, {RESOLUTION::HD720, FPS::FPS_30}, {RESOLUTION::VGA, FPS::FPS_15}};

    const int cycles = scaled(80);
    const int fdBefore = test::openFdCount();
    double memoryStart = 0;
    int openFailed = 0, noFrames = 0, wrongSize = 0, restarts = 0;
    double slowestOpen = 0;

    // The camera is opened by serial number: the device can change if the library restarts the camera
    int sn = -1;
    for(const VideoDevice& dev : VideoCapture::getDeviceList())
        if(dev.dev_id == GetParam().id) sn = dev.serial_number;
    ASSERT_GT(sn, 0);

    for(int i = 0; i < cycles; i++)
    {
        const Mode& mode = modes[i % 6];
        VideoCapture cap(makeParams(mode.res, mode.fps));
        const double openStart = test::nowSec();
        if(!cap.initializeVideoBySerialNumber(sn)) { openFailed++; continue; }
        const double openTime = test::nowSec() - openStart;
        slowestOpen = std::max(slowestOpen, openTime);
        // An open that takes seconds means that the image sensors did not answer and the camera has been restarted
        if(openTime > 2.5) restarts++;

        const Frame& frame = cap.getLastFrame(1000);
        if(frame.data == nullptr || frame.timestamp == 0) noFrames++;
        int w = 0, h = 0;
        cap.getFrameSize(w, h);
        if(frame.width != w || frame.height != h) wrongSize++;

        // Half of the times the camera is closed explicitly, also twice
        if(i % 2) { cap.close(); cap.close(); }

        if(i == 11) memoryStart = memoryMB(); // After two rounds of all the modes
    }

    std::cout << "[  STRESS  ] " << cycles << " open/close: failed " << openFailed << ", without frames " << noFrames
              << ", opens that waited for the image sensors or restarted the camera " << restarts << ", slowest open " << slowestOpen << " s, memory "
              << memoryStart << " -> " << memoryMB() << " MB" << std::endl;

    EXPECT_EQ(openFailed, 0);
    EXPECT_EQ(noFrames, 0);
    EXPECT_EQ(wrongSize, 0);
    EXPECT_EQ(test::openFdCount(), fdBefore) << "file descriptors left open";
    EXPECT_EQ(test::videoMapCount(), 0) << "video buffers left mapped";
    if(cycles > 20)
    {
        EXPECT_LT(memoryMB(), memoryStart + 30.) << "the memory used by the process keeps growing";
    }
}

// A thread changes all the settings while another one receives the frames
TEST_P(Stress, SettingsChangedWhileReceivingFrames)
{
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_60));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    const double duration = 60.0 * stressScale();
    std::atomic<bool> stop{false};
    std::atomic<int> calls{0}, failed{0};
    std::string firstFailure;

    std::thread settings([&] {
        std::mt19937 rng(7);
        auto check = [&](bool ok, const char* what) {
            calls++;
            if(!ok) { if(failed++ == 0) firstFailure = what; }
        };
        while(!stop)
        {
            switch(rng() % 12)
            {
            case 0: { const int v = rng() % 9; check(cap.setBrightness(v) && cap.getBrightness() == v, "brightness"); break; }
            case 1: { const int v = rng() % 9; check(cap.setContrast(v) && cap.getContrast() == v, "contrast"); break; }
            case 2: { const int v = rng() % 9; check(cap.setSaturation(v) && cap.getSaturation() == v, "saturation"); break; }
            case 3: { const int v = rng() % 9; check(cap.setSharpness(v) && cap.getSharpness() == v, "sharpness"); break; }
            case 4: { const int v = rng() % 12; check(cap.setHue(v) && cap.getHue() == v, "hue"); break; }
            case 5: { const int v = 2800 + 100 * (rng() % 38);
                      check(cap.setWhiteBalance(v), "white balance refused"); check(cap.getWhiteBalance() == v, "white balance read back"); break; }
            case 6: { const int v = 1 + rng() % 9; check(cap.setGamma(v) && cap.getGamma() == v, "gamma"); break; }
            case 7: { const int v = rng() % 101; const CAM_SENS_POS s = (rng() % 2) ? CAM_SENS_POS::LEFT : CAM_SENS_POS::RIGHT;
                      check(cap.setGain(s, v) && std::abs(cap.getGain(s) - v) <= 1, "gain"); break; }
            case 8: { const int v = rng() % 60; const CAM_SENS_POS s = (rng() % 2) ? CAM_SENS_POS::LEFT : CAM_SENS_POS::RIGHT;
                      check(cap.setExposure(s, v) && std::abs(cap.getExposure(s) - v) <= 1, "exposure"); break; }
            case 9: { const bool v = rng() % 2; check(cap.setAECAGC(v) && cap.getAECAGC() == v, "automatic exposure"); break; }
            case 10: { const bool v = rng() % 2; bool led = !v; check(cap.setLEDstatus(v) && cap.getLEDstatus(&led) && led == v, "led"); break; }
            default: { uint16_t x = 0, y = 0, w = 0, h = 0; const uint16_t rx = rng() % 600, ry = rng() % 300;
                       check(cap.setROIforAECAGC(CAM_SENS_POS::LEFT, rx, ry, 600, 400) && cap.getROIforAECAGC(CAM_SENS_POS::LEFT, x, y, w, h) &&
                             x == rx && y == ry && w == 600 && h == 400, "automatic exposure region"); break; }
            }
        }
    });

    std::vector<double> intervals;
    uint64_t lastTs = 0, lastId = 0;
    int frames = 0, backwards = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < duration)
    {
        const Frame& frame = cap.getLastFrame(500);
        if(frame.data == nullptr || frame.timestamp == lastTs)
            continue;
        if(lastTs != 0)
        {
            if(frame.timestamp < lastTs || frame.frame_id <= lastId) backwards++;
            intervals.push_back(static_cast<double>(frame.timestamp - lastTs) / 1e6);
        }
        lastTs = frame.timestamp;
        lastId = frame.frame_id;
        frames++;
    }
    stop = true;
    settings.join();

    ASSERT_GT(intervals.size(), 10u);
    std::sort(intervals.begin(), intervals.end());
    const double median = intervals[intervals.size() / 2];
    const double worst = intervals.back();
    int lost = 0;
    for(double dt : intervals) lost += std::max(0, static_cast<int>(std::lround(dt / median)) - 1);

    std::cout << "[  STRESS  ] " << calls << " settings in " << static_cast<int>(duration) << " s: failed " << failed << " (" << firstFailure
              << "), frames " << frames << ", lost " << lost << ", longest interval " << worst << " ms" << std::endl;

    EXPECT_EQ(failed, 0) << "first failure: " << firstFailure;
    EXPECT_EQ(backwards, 0);
    EXPECT_NEAR(median, 1000. / 60., 2.0) << "frame period [ms]";
    EXPECT_LT(static_cast<double>(lost), 0.25 * frames) << "frames lost while changing the settings";
    EXPECT_LT(worst, 500.) << "the stream stopped while changing the settings [ms]";
    EXPECT_TRUE(cap.isOpened());

    // Everything can be restored
    EXPECT_TRUE(cap.resetBrightness() && cap.resetContrast() && cap.resetSaturation() && cap.resetSharpness() && cap.resetHue() &&
                cap.resetGamma() && cap.resetAutoWhiteBalance() && cap.resetAECAGC() && cap.resetROIforAECAGC(CAM_SENS_POS::LEFT));
}

#ifdef SENSORS_MOD_AVAILABLE
// Video and sensors together for a long time, with the synchronization enabled
TEST_P(Stress, LongAcquisitionOfVideoAndSensors)
{
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));
    const int sn = cap.getSerialNumber();

    sl_oc::sensors::SensorCapture sens(sl_oc::VERBOSITY::ERROR);
    const std::vector<int> serials = sens.getDeviceList();
    if(std::find(serials.begin(), serials.end(), sn) == serials.end())
        GTEST_SKIP() << "camera " << sn << " has no sensors module";
    ASSERT_TRUE(sens.initializeSensors(sn));
    ASSERT_TRUE(cap.enableSensorSync(&sens));

    const double duration = 180.0 * stressScale();
    std::atomic<bool> stop{false};
    std::vector<uint64_t> imuTs;
    int imuBackwards = 0;
    std::vector<double> imuLatencyMs;
    long envSamples = 0;
    double envFirst = -1., envLast = -1., envLongestGap = 0.;
    const double sensStart = test::nowSec();

    std::thread sensThread([&] {
        uint64_t last = 0;
        while(!stop)
        {
            const std::vector<sl_oc::sensors::data::Imu> queue = sens.getIMUData(20000);
            if(sens.getLastEnvironmentData(100).valid == sl_oc::sensors::data::Environment::NEW_VAL)
            {
                const double t = test::nowSec() - sensStart;
                if(envFirst < 0.) envFirst = t;
                else envLongestGap = std::max(envLongestGap, t - envLast);
                envLast = t;
                envSamples++;
            }
            if(queue.empty()) continue;
            const double latency = (static_cast<double>(sl_oc::getTimestampNow()) - static_cast<double>(queue.back().timestamp)) / 1e6;
            if(imuTs.size() > 2000) imuLatencyMs.push_back(std::fabs(latency));
            for(const sl_oc::sensors::data::Imu& imu : queue)
            {
                if(imu.timestamp <= last) imuBackwards++;
                last = imu.timestamp;
                imuTs.push_back(imu.timestamp);
            }
        }
    });

    const double memoryStart = memoryMB();
    std::vector<double> intervals;
    uint64_t lastTs = 0;
    int frames = 0, backwards = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < duration)
    {
        const Frame& frame = cap.getLastFrame(500);
        if(frame.data == nullptr || frame.timestamp == lastTs)
            continue;
        if(lastTs != 0)
        {
            if(frame.timestamp < lastTs) backwards++;
            intervals.push_back(static_cast<double>(frame.timestamp - lastTs) / 1e6);
        }
        lastTs = frame.timestamp;
        frames++;
    }
    stop = true;
    sensThread.join();

    ASSERT_GT(intervals.size(), 10u);
    ASSERT_GT(imuTs.size(), 100u);
    std::sort(intervals.begin(), intervals.end());
    const double period = intervals[intervals.size() / 2];
    int lostFrames = 0;
    for(double dt : intervals) lostFrames += std::max(0, static_cast<int>(std::lround(dt / period)) - 1);

    std::vector<double> imuDt;
    for(size_t i = 1; i < imuTs.size(); i++) imuDt.push_back(static_cast<double>(imuTs[i] - imuTs[i - 1]) / 1e6);
    std::vector<double> sorted = imuDt;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double imuPeriod = sorted[sorted.size() / 2];
    int lostImu = 0, imuPauses = 0;
    double longestPauseMs = 0;
    for(double dt : imuDt)
    {
        lostImu += std::max(0, static_cast<int>(std::lround(dt / imuPeriod)) - 1);
        // The sensors module of the camera can stop for a while: the timestamps must show it
        if(dt > 100.) { imuPauses++; longestPauseMs = std::max(longestPauseMs, dt); }
    }

    // Distance of the newest sample from the host clock, without the rare samples delivered late by the camera
    ASSERT_GT(imuLatencyMs.size(), 10u);
    std::sort(imuLatencyMs.begin(), imuLatencyMs.end());
    const double imuLatency99 = imuLatencyMs[imuLatencyMs.size() * 99 / 100];

    std::cout << "[  STRESS  ] " << static_cast<int>(duration) << " s of video and sensors: frames " << frames << ", lost " << lostFrames
              << ", IMU samples " << imuTs.size() << ", lost " << lostImu << " (pauses of the sensors module: " << imuPauses << ", longest "
              << longestPauseMs << " ms), IMU latency 99% " << imuLatency99 << " ms, environmental samples "
              << envSamples << " (first after " << envFirst << " s, last after " << envLast << " s, longest gap " << envLongestGap
              << " s), memory " << memoryStart << " -> " << memoryMB() << " MB" << std::endl;

    EXPECT_TRUE(cap.isOpened());
    EXPECT_TRUE(sens.isOpened());
    EXPECT_EQ(backwards, 0);
    EXPECT_EQ(imuBackwards, 0);
    EXPECT_NEAR(period, 1000. / 30., 1.5) << "frame period [ms]";
    EXPECT_LT(static_cast<double>(lostFrames), 0.2 * frames) << "frames lost";
    EXPECT_LT(static_cast<double>(lostImu), 0.03 * imuTs.size()) << "IMU samples lost";
    EXPECT_LE(imuPauses, 1 + static_cast<int>(duration / 120.)) << "the sensors module stopped too many times, longest pause " << longestPauseMs << " ms";
    EXPECT_LT(imuLatency99, 30.) << "IMU timestamps far from the host clock";
    EXPECT_LT(memoryMB(), memoryStart + 30.) << "the memory used by the process keeps growing";
}

// The sensors alone for a long time, with the video device of the camera not in use: the stream must not stop and the
// environmental data must keep arriving. The time of each event is printed, to compare it with the system logs
TEST_P(Stress, LongAcquisitionOfSensors)
{
    int sn = -1;
    bool hasEnvSensors = false;
    {
        VideoCapture cap(makeParams(RESOLUTION::VGA, FPS::FPS_30, sl_oc::VERBOSITY::NONE));
        ASSERT_TRUE(cap.initializeVideo(GetParam().id));
        sn = cap.getSerialNumber();
        hasEnvSensors = cap.getCameraModel() == sl_oc::video::SL_DEVICE::ZED_2 || cap.getCameraModel() == sl_oc::video::SL_DEVICE::ZED_2i;
    }

    {
        sl_oc::sensors::SensorCapture probe(sl_oc::VERBOSITY::NONE);
        const std::vector<int> serials = probe.getDeviceList();
        if(std::find(serials.begin(), serials.end(), sn) == serials.end())
            GTEST_SKIP() << "camera " << sn << " has no sensors module";
    }

    // The earlier cases stream the video without the sensors module, which can stop the environmental sensors of the
    // camera (see "Environmental sensors not updated" in the README). Restart the camera to start from a known state,
    // then open the video first and the sensors after, as the README advises
    if(hasEnvSensors)
    {
        ASSERT_TRUE(sl_oc::sensors::SensorCapture::resetSensorModule(sn));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        bool videoBack = false;
        const double t0 = test::nowSec();
        while(!videoBack && test::nowSec() - t0 < 20.0)
        {
            VideoCapture cap(makeParams(RESOLUTION::VGA, FPS::FPS_30, sl_oc::VERBOSITY::NONE));
            videoBack = cap.initializeVideoBySerialNumber(sn);
            if(!videoBack) std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        ASSERT_TRUE(videoBack) << "camera " << sn << " not back 20 s after the reset of its sensors module";
    }

    sl_oc::sensors::SensorCapture sens(sl_oc::VERBOSITY::ERROR);
    ASSERT_TRUE(sens.initializeSensors(sn));
    if(hasEnvSensors) // The environmental data can need a few seconds to start after the connection
    {
        const double t0 = test::nowSec();
        while(test::nowSec() - t0 < 10.0 && sens.getLastEnvironmentData(100).valid != sl_oc::sensors::data::Environment::NEW_VAL) {}
    }

    auto clockTime = [] {
        char buf[16];
        const time_t now = time(nullptr);
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        strftime(buf, sizeof(buf), "%H:%M:%S", &tmNow);
        return std::string(buf);
    };

    const double duration = 240.0 * stressScale();
    long imuSamples = 0, envSamples = 0;
    int imuPauses = 0, envStalls = 0, backwards = 0;
    double longestPauseMs = 0, longestEnvGap = 0;
    uint64_t lastTs = 0;
    bool envStalled = false;
    std::string events;

    const double start = test::nowSec();
    double lastEnv = start;
    while(test::nowSec() - start < duration)
    {
        for(const sl_oc::sensors::data::Imu& imu : sens.getIMUData(20000))
        {
            if(lastTs != 0)
            {
                if(imu.timestamp <= lastTs) backwards++;
                const double dt = static_cast<double>(imu.timestamp - lastTs) / 1e6;
                if(dt > 100.)
                {
                    imuPauses++;
                    longestPauseMs = std::max(longestPauseMs, dt);
                    events += " [" + clockTime() + " IMU pause " + std::to_string(static_cast<int>(dt)) + " ms]";
                }
            }
            lastTs = imu.timestamp;
            imuSamples++;
        }

        const double now = test::nowSec();
        if(sens.getLastEnvironmentData(100).valid == sl_oc::sensors::data::Environment::NEW_VAL)
        {
            longestEnvGap = std::max(longestEnvGap, now - lastEnv);
            if(envStalled) events += " [" + clockTime() + " environmental data back]";
            envStalled = false;
            lastEnv = now;
            envSamples++;
        }
        else if(hasEnvSensors && !envStalled && now - lastEnv > 2.0) // Also when the data never start
        {
            envStalled = true;
            envStalls++;
            events += " [" + clockTime() + " environmental data stopped]";
        }
    }

    std::cout << "[  STRESS  ] " << static_cast<int>(duration) << " s of sensors only: IMU samples " << imuSamples << ", pauses " << imuPauses
              << " (longest " << longestPauseMs << " ms), environmental samples " << envSamples << ", stops " << envStalls
              << ", longest gap " << longestEnvGap << " s" << (events.empty() ? "" : ", events:") << events << std::endl;

    EXPECT_TRUE(sens.isOpened());
    EXPECT_EQ(backwards, 0);
    EXPECT_GT(imuSamples, static_cast<long>(duration * 380));
    EXPECT_EQ(imuPauses, 0) << "the sensors module stopped sending data:" << events;
    if(hasEnvSensors)
    {
        EXPECT_GT(envSamples, static_cast<long>(duration * 15)) << "environmental data are expected at about 25 Hz";
    }
    EXPECT_EQ(envStalls, 0) << "the environmental sensors stopped. `SensorCapture::resetSensorModule` recovers them:" << events;
}

// The sensors are connected and disconnected many times
TEST_P(Stress, SensorsOpenAndClose)
{
    int sn = -1;
    {
        VideoCapture cap(makeParams(RESOLUTION::VGA, FPS::FPS_30, sl_oc::VERBOSITY::NONE));
        ASSERT_TRUE(cap.initializeVideo(GetParam().id));
        sn = cap.getSerialNumber();
    }

    sl_oc::sensors::SensorCapture sens(sl_oc::VERBOSITY::ERROR);
    const std::vector<int> serials = sens.getDeviceList();
    if(std::find(serials.begin(), serials.end(), sn) == serials.end())
        GTEST_SKIP() << "camera " << sn << " has no sensors module";

    const int cycles = scaled(60);
    const int fdBefore = test::openFdCount();
    int openFailed = 0, noData = 0;
    for(int i = 0; i < cycles; i++)
    {
        if(!sens.initializeSensors(sn)) { openFailed++; continue; }
        if(sens.getIMUData(500000).empty()) noData++;
        if(i % 3 == 0) usleep(200000);
        sens.close();
    }

    std::cout << "[  STRESS  ] " << cycles << " sensors open/close: failed " << openFailed << ", without data " << noData << std::endl;

    EXPECT_EQ(openFailed, 0);
    EXPECT_EQ(noData, 0);
    EXPECT_LE(test::openFdCount(), fdBefore + 8) << "file descriptors left open";
}
#endif

INSTANTIATE_TEST_SUITE_P(Cameras, Stress, testing::ValuesIn(test::listStereolabsVideoDevices()),
                         [](const testing::TestParamInfo<test::VideoDev>& info) { return devName(info.param); });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Stress);
// <---- Robustness

// ----> All the cameras together
TEST(StressAllCameras, StreamTogether)
{
    std::vector<test::VideoDev> devs;
    for(const test::VideoDev& dev : test::listStereolabsVideoDevices())
        if(dev.isUsb3()) devs.push_back(dev);
    if(devs.size() < 2)
        GTEST_SKIP() << "needs two cameras";

    const double duration = 60.0 * stressScale();
    std::vector<std::unique_ptr<VideoCapture>> caps;
    for(const test::VideoDev& dev : devs)
    {
        caps.emplace_back(new VideoCapture(makeParams(RESOLUTION::HD720, FPS::FPS_30)));
        ASSERT_TRUE(caps.back()->initializeVideo(dev.id)) << "/dev/video" << dev.id;
    }

    std::vector<int> frames(caps.size(), 0), lost(caps.size(), 0);
    std::vector<std::thread> threads;
    for(size_t c = 0; c < caps.size(); c++)
    {
        threads.emplace_back([&, c] {
            uint64_t lastTs = 0;
            const double start = test::nowSec();
            while(test::nowSec() - start < duration)
            {
                const Frame& frame = caps[c]->getLastFrame(500);
                if(frame.data == nullptr || frame.timestamp == lastTs)
                    continue;
                if(lastTs != 0)
                    lost[c] += std::max(0, static_cast<int>(std::lround(static_cast<double>(frame.timestamp - lastTs) / (1e9 / 30.))) - 1);
                lastTs = frame.timestamp;
                frames[c]++;
            }
        });
    }
    for(std::thread& t : threads) t.join();

    for(size_t c = 0; c < caps.size(); c++)
    {
        std::cout << "[  STRESS  ] /dev/video" << devs[c].id << " with the other cameras: frames " << frames[c] << ", lost " << lost[c] << std::endl;
        EXPECT_TRUE(caps[c]->isOpened());
        EXPECT_GT(frames[c], static_cast<int>(duration * 30 * 0.8)) << "/dev/video" << devs[c].id;
    }
}
// <---- All the cameras together

} // namespace
