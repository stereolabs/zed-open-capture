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

// Hardware test of the video/sensors synchronization.
// It runs on each connected Stereolabs camera with sensors. No display is needed.

#include "sensorcapture.hpp"
#include "videocapture.hpp"
#include "test_common.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <unistd.h>

using namespace sl_oc;

namespace {

TEST(Setup, CameraConnected)
{
    ASSERT_FALSE(test::listStereolabsVideoDevices().empty()) << "No Stereolabs camera found";
}

class Sync : public testing::TestWithParam<test::VideoDev>
{
};

TEST_P(Sync, FrameAndImuTimestampsAgree)
{
    video::VideoParams params;
    params.res = video::RESOLUTION::HD720;
    params.fps = video::FPS::FPS_30;
    params.verbose = VERBOSITY::ERROR;

    if(!GetParam().isUsb3())
        GTEST_SKIP() << "the camera is connected in USB 2.0 mode";

    video::VideoCapture videoCap(params);
    ASSERT_TRUE(videoCap.initializeVideo(GetParam().id));
    const int sn = videoCap.getSerialNumber();

    sensors::SensorCapture sensCap(VERBOSITY::ERROR);
    const std::vector<int> serials = sensCap.getDeviceList();
    if(std::find(serials.begin(), serials.end(), sn) == serials.end())
        GTEST_SKIP() << "camera " << sn << " has no sensors module";

    ASSERT_TRUE(sensCap.initializeSensors(sn));
    ASSERT_TRUE(videoCap.enableSensorSync(&sensCap));

    // Sensors run faster than video: read them in their own thread and keep the timestamps of the
    // samples flagged as synchronized with a frame
    std::atomic<bool> stop{false};
    std::atomic<int> syncCount{0};
    std::mutex syncMutex;
    std::deque<uint64_t> syncTs;
    std::thread sensThread([&] {
        while(!stop)
        {
            for(const sensors::data::Imu& imu : sensCap.getIMUData(20000))
            {
                if(imu.sync)
                {
                    const std::lock_guard<std::mutex> lock(syncMutex);
                    syncTs.push_back(imu.timestamp);
                    if(syncTs.size() > 64) syncTs.pop_front();
                    syncCount++;
                }
            }
        }
    });

    const double settle = 3.0; // time allowed to the clock alignment to settle
    const double duration = 12.0;
    uint64_t lastTs = 0;
    int frames = 0, measures = 0, exact = 0, far = 0;
    double sum = 0, worst = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < duration)
    {
        const video::Frame& frame = videoCap.getLastFrame(100);
        if(frame.data == nullptr || frame.timestamp == lastTs)
            continue;
        lastTs = frame.timestamp;
        frames++;

        if(test::nowSec() - start < settle)
            continue;

        // The synchronized IMU sample closest to the frame
        double offsetMs = 1e9;
        {
            const std::lock_guard<std::mutex> lock(syncMutex);
            for(uint64_t ts : syncTs)
            {
                const double d = (static_cast<double>(frame.timestamp) - static_cast<double>(ts)) / 1e6;
                if(std::fabs(d) < std::fabs(offsetMs)) offsetMs = d;
            }
        }
        if(offsetMs > 1e8)
            continue;

        sum += offsetMs;
        if(std::fabs(offsetMs) > std::fabs(worst)) worst = offsetMs;
        if(offsetMs == 0.0) exact++;
        if(std::fabs(offsetMs) > 8.0) far++;
        measures++;
    }
    const double elapsed = test::nowSec() - start;
    stop = true;
    sensThread.join();

    EXPECT_NEAR(frames / elapsed, 30.0, 3.0) << "frame rate";
    EXPECT_NEAR(syncCount / elapsed, 30.0, 2.0) << "there must be one synchronized IMU sample per frame";
    ASSERT_GT(measures, 0) << "no synchronized IMU sample received";
    EXPECT_NEAR(sum / measures, 0.0, 1.0) << "mean video/IMU timestamp offset after " << settle << " s [ms]";
    // Frame and sync signal are timestamped by two devices on the same clock: a few milliseconds at most.
    // A sync signal can be missing when the USB host loses sensors data: 2% of the frames at most
    EXPECT_LE(far, measures / 50) << "frames without a synchronized IMU sample closer than 8 ms: " << far << " of " << measures
                                  << ", worst offset " << worst << " ms";
    // The frames take the timestamp of their synchronized IMU sample
    EXPECT_GT(exact, measures * 8 / 10) << "frames with the timestamp of a synchronized IMU sample: " << exact << " of " << measures;
}

// The two objects share the synchronization data: closing or destroying one of them while the other is
// still grabbing must be safe, in any order
TEST_P(Sync, ObjectsCanBeDestroyedInAnyOrder)
{
    if(!GetParam().isUsb3())
        GTEST_SKIP() << "the camera is connected in USB 2.0 mode";

    video::VideoParams params;
    params.res = video::RESOLUTION::VGA;
    params.fps = video::FPS::FPS_100;
    params.verbose = VERBOSITY::ERROR;

    for(int order = 0; order < 2; order++)
    {
        std::unique_ptr<video::VideoCapture> videoCap(new video::VideoCapture(params));
        ASSERT_TRUE(videoCap->initializeVideo(GetParam().id));
        const int sn = videoCap->getSerialNumber();

        std::unique_ptr<sensors::SensorCapture> sensCap(new sensors::SensorCapture(VERBOSITY::ERROR));
        const std::vector<int> serials = sensCap->getDeviceList();
        if(std::find(serials.begin(), serials.end(), sn) == serials.end())
            GTEST_SKIP() << "camera " << sn << " has no sensors module";

        ASSERT_TRUE(sensCap->initializeSensors(sn));
        ASSERT_TRUE(videoCap->enableSensorSync(sensCap.get()));
        usleep(500000);

        if(order == 0)
        {
            sensCap.reset();
            usleep(300000);
            EXPECT_NE(videoCap->getLastFrame(500).data, nullptr) << "video must keep working without the sensors";
            videoCap.reset();
        }
        else
        {
            videoCap.reset();
            usleep(300000);
            EXPECT_FALSE(sensCap->getIMUData(100000).empty()) << "sensors must keep working without the video";
            sensCap.reset();
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Cameras, Sync, testing::ValuesIn(test::listStereolabsVideoDevices()),
                         [](const testing::TestParamInfo<test::VideoDev>& info) {
                             return test::modelTag(info.param.name) + "_video" + std::to_string(info.param.id);
                         });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Sync);

} // namespace
