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

// Hardware test of the VideoCapture class.
// Every case runs on each connected Stereolabs camera. No display is needed.

#include "videocapture.hpp"
#include "test_common.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <cmath>
#include <tuple>

using namespace sl_oc::video;

namespace {

struct Mode
{
    RESOLUTION res;
    FPS fps;
    const char* name;
};

const Mode kModes[] = {
    {RESOLUTION::HD2K, FPS::FPS_15, "HD2K_15"},
    {RESOLUTION::HD1080, FPS::FPS_30, "HD1080_30"},
    {RESOLUTION::HD720, FPS::FPS_60, "HD720_60"},
    {RESOLUTION::VGA, FPS::FPS_100, "VGA_100"},
};

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

//! Used by GoogleTest to print the parameter of a failed test
void PrintTo(const Mode& mode, std::ostream* os)
{
    *os << mode.name;
}

// The tests that need the full set of video modes are skipped for a camera connected in USB 2.0 mode
#define SKIP_IF_USB2(dev) \
    if(!(dev).isUsb3()) GTEST_SKIP() << "the camera is connected in USB 2.0 mode"

TEST(Setup, CameraConnected)
{
    ASSERT_FALSE(test::listStereolabsVideoDevices().empty()) << "No Stereolabs camera found";
}

TEST(Setup, CamerasUseUsb3)
{
    for(const test::VideoDev& dev : test::listStereolabsVideoDevices())
        EXPECT_TRUE(dev.isUsb3()) << "/dev/video" << dev.id << " (" << dev.name << ") is connected in USB 2.0 mode ("
                                  << dev.usbSpeed << " Mbps): check port and cable, its tests are skipped";
}

TEST(Setup, DeviceListMatchesTheSystem)
{
    const std::vector<test::VideoDev> expected = test::listStereolabsVideoDevices();
    const std::vector<VideoDevice> devices = VideoCapture::getDeviceList();
    ASSERT_EQ(devices.size(), expected.size());
    for(size_t i = 0; i < devices.size(); i++)
    {
        EXPECT_EQ(devices[i].dev_id, expected[i].id);
        EXPECT_EQ(devices[i].usb_speed, expected[i].usbSpeed);
        EXPECT_NE(devices[i].model, SL_DEVICE::NONE);
        EXPECT_GT(devices[i].serial_number, 0) << "serial number of /dev/video" << devices[i].dev_id;
    }
}

// ----> Streaming: every camera in every mode
class Streaming : public testing::TestWithParam<std::tuple<test::VideoDev, Mode>>
{
};

// Grab for a few seconds and check the frame rate, the timestamps and that a returned frame is not
// modified while the caller still uses it
TEST_P(Streaming, DeliversStableFramesAtNominalRate)
{
    const test::VideoDev dev = std::get<0>(GetParam());
    const Mode mode = std::get<1>(GetParam());
    SKIP_IF_USB2(dev);

    const int fdBefore = test::openFdCount();
    {
        VideoCapture cap(makeParams(mode.res, mode.fps));
        const double t0 = test::nowSec();
        ASSERT_TRUE(cap.initializeVideo(dev.id));
        EXPECT_LT(test::nowSec() - t0, 1.5) << "open is too slow";
        EXPECT_GT(cap.getSerialNumber(), 0);

        int w = 0, h = 0;
        cap.getFrameSize(w, h);

        const double nominalFps = static_cast<double>(mode.fps);
        const double period = 1.0 / nominalFps;
        const double duration = 4.0;

        uint64_t lastTs = 0, firstTs = 0, lastId = 0;
        int frames = 0, backwards = 0, modified = 0, wrongSize = 0, idNotIncreasing = 0;
        const double start = test::nowSec();
        while(test::nowSec() - start < duration)
        {
            const Frame& frame = cap.getLastFrame(200);
            if(frame.data == nullptr || frame.timestamp == lastTs)
                continue;
            if(lastTs != 0 && frame.timestamp < lastTs)
                backwards++;
            lastTs = frame.timestamp;
            if(frames == 0) firstTs = frame.timestamp;
            else if(frame.frame_id <= lastId) idNotIncreasing++;
            lastId = frame.frame_id;
            frames++;
            if(frame.width != w || frame.height != h || frame.channels != 2)
                wrongSize++;

            if(frames % 20 == 0)
            {
                const size_t size = static_cast<size_t>(frame.width) * frame.height * frame.channels;
                const uint64_t h1 = test::sparseHash(frame.data, size);
                usleep(static_cast<useconds_t>(period * 1.2e6));
                if(test::sparseHash(frame.data, size) != h1)
                    modified++;
            }
        }
        // The timestamps are assigned when the frames are acquired: they also reveal the frames that did not
        // reach the application
        const double expected = static_cast<double>(lastTs - firstTs) / 1e9 * nominalFps + 1.0;
        const double lost = 1.0 - static_cast<double>(frames) / expected;

        EXPECT_TRUE(cap.isOpened());
        EXPECT_EQ(cap.getFps(), static_cast<int>(mode.fps));
        EXPECT_GT(frames, static_cast<int>(duration * nominalFps * 0.5)) << "too few frames received";
        EXPECT_EQ(idNotIncreasing, 0) << "the frame ID must always increase";
        // The test holds a frame for more than a period once every 20 frames, so up to 2 frames in 20 are skipped
        EXPECT_LT(lost, 0.15) << "frames lost: " << frames << " received of " << static_cast<int>(expected + 0.5)
                              << ". The USB host or the system cannot keep up with the camera";
        EXPECT_EQ(wrongSize, 0) << "frames do not match the requested mode";
        EXPECT_EQ(backwards, 0) << "frame timestamps went backwards";
        EXPECT_EQ(modified, 0) << "returned frames were modified while the caller was using them";
    }
    EXPECT_EQ(test::openFdCount(), fdBefore) << "file descriptors left open after close";
    EXPECT_EQ(test::videoMapCount(), 0) << "video buffers left mapped after close";
}

INSTANTIATE_TEST_SUITE_P(Cameras, Streaming,
                         testing::Combine(testing::ValuesIn(test::listStereolabsVideoDevices()), testing::ValuesIn(kModes)),
                         [](const testing::TestParamInfo<Streaming::ParamType>& info) {
                             return devName(std::get<0>(info.param)) + "_" + std::get<1>(info.param).name;
                         });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Streaming);
// <---- Streaming

// ----> Cases that run once per camera, at HD720@30
class Camera : public testing::TestWithParam<test::VideoDev>
{
};

TEST_P(Camera, DefaultsAfterOpen)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    EXPECT_TRUE(cap.getAECAGC());
    EXPECT_TRUE(cap.getAutoWhiteBalance());
    EXPECT_EQ(cap.getGamma(), 1);
    EXPECT_EQ(cap.getBrightness(), 4);
    EXPECT_EQ(cap.getContrast(), 4);
    EXPECT_EQ(cap.getSaturation(), 4);
}

TEST_P(Camera, ImageControls)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    cap.setBrightness(7);
    EXPECT_EQ(cap.getBrightness(), 7);
    cap.resetBrightness();
    EXPECT_EQ(cap.getBrightness(), 4);

    cap.setContrast(2);
    EXPECT_EQ(cap.getContrast(), 2);
    cap.resetContrast();

    cap.setSaturation(6);
    EXPECT_EQ(cap.getSaturation(), 6);
    cap.resetSaturation();

    cap.setSharpness(3);
    EXPECT_EQ(cap.getSharpness(), 3);
    cap.resetSharpness();

    for(int gamma : {5, 9, 1})
    {
        cap.setGamma(gamma);
        EXPECT_EQ(cap.getGamma(), gamma);
    }
    cap.resetGamma();

    cap.setWhiteBalance(4000);
    EXPECT_EQ(cap.getWhiteBalance(), 4000);
    EXPECT_FALSE(cap.getAutoWhiteBalance());
    cap.resetAutoWhiteBalance();
    EXPECT_TRUE(cap.getAutoWhiteBalance());
}

TEST_P(Camera, GainAndExposure)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    for(CAM_SENS_POS side : {CAM_SENS_POS::LEFT, CAM_SENS_POS::RIGHT})
    {
        SCOPED_TRACE(side == CAM_SENS_POS::LEFT ? "left sensor" : "right sensor");
        for(int value : {0, 25, 50, 100})
        {
            cap.setGain(side, value);
            cap.setExposure(side, value);
            usleep(100000);
            EXPECT_EQ(cap.getGain(side), value);
            EXPECT_EQ(cap.getExposure(side), value);
        }
    }
    EXPECT_FALSE(cap.getAECAGC()) << "setting gain/exposure must disable the automatic control";
    cap.resetAECAGC();
    EXPECT_TRUE(cap.getAECAGC());
}

TEST_P(Camera, AutoExposureRegion)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    uint16_t x = 0, y = 0, w = 0, h = 0;
    EXPECT_TRUE(cap.setROIforAECAGC(CAM_SENS_POS::LEFT, 300, 200, 600, 400));
    EXPECT_TRUE(cap.getROIforAECAGC(CAM_SENS_POS::LEFT, x, y, w, h));
    EXPECT_EQ(x, 300);
    EXPECT_EQ(y, 200);
    EXPECT_EQ(w, 600);
    EXPECT_EQ(h, 400);
    EXPECT_FALSE(cap.setROIforAECAGC(CAM_SENS_POS::LEFT, 1000, 0, 600, 400)) << "a region outside the image must be refused";
    EXPECT_TRUE(cap.resetROIforAECAGC(CAM_SENS_POS::LEFT));
}

TEST_P(Camera, Led)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));

    bool led = true;
    EXPECT_TRUE(cap.setLEDstatus(false));
    EXPECT_TRUE(cap.getLEDstatus(&led));
    EXPECT_FALSE(led);
    EXPECT_TRUE(cap.setLEDstatus(true));
    EXPECT_TRUE(cap.getLEDstatus(&led));
    EXPECT_TRUE(led);
}

// Opening with automatic detection while another object holds a camera must not leave descriptors open
TEST_P(Camera, AutoDetectDoesNotLeak)
{
    SKIP_IF_USB2(GetParam());
    const VideoParams params = makeParams(RESOLUTION::HD720, FPS::FPS_30, sl_oc::VERBOSITY::NONE);

    VideoCapture holder(params);
    ASSERT_TRUE(holder.initializeVideo(GetParam().id));

    const int fdBefore = test::openFdCount();
    for(int i = 0; i < 3; i++)
    {
        VideoCapture other(params);
        other.initializeVideo(-1); // succeeds only if a second camera is connected
    }
    EXPECT_EQ(test::openFdCount(), fdBefore) << "file descriptors left open by automatic detection";
}

TEST_P(Camera, ReopenOnTheSameObject)
{
    SKIP_IF_USB2(GetParam());
    const int fdBefore = test::openFdCount();
    {
        VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
        for(int i = 0; i < 3; i++)
        {
            ASSERT_TRUE(cap.initializeVideo(GetParam().id)) << "open #" << i;
            EXPECT_NE(cap.getLastFrame(500).data, nullptr);
        }
    }
    EXPECT_EQ(test::openFdCount(), fdBefore);
    EXPECT_EQ(test::videoMapCount(), 0);
}

TEST_P(Camera, OpenBySerialNumber)
{
    SKIP_IF_USB2(GetParam());
    int sn = -1;
    for(const VideoDevice& dev : VideoCapture::getDeviceList())
        if(dev.dev_id == GetParam().id) sn = dev.serial_number;
    ASSERT_GT(sn, 0);

    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30, sl_oc::VERBOSITY::NONE));
    ASSERT_TRUE(cap.initializeVideoBySerialNumber(sn));
    EXPECT_EQ(cap.getDeviceId(), GetParam().id);
    EXPECT_EQ(cap.getSerialNumber(), sn);
    EXPECT_FALSE(cap.initializeVideoBySerialNumber(1)) << "no camera has this serial number";
    EXPECT_FALSE(cap.isOpened());
}

TEST_P(Camera, CloseStopsTheStream)
{
    SKIP_IF_USB2(GetParam());
    VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30));
    EXPECT_FALSE(cap.isOpened());
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));
    EXPECT_TRUE(cap.isOpened());
    EXPECT_NE(cap.getLastFrame(500).data, nullptr);

    cap.close();
    cap.close(); // closing twice is allowed
    EXPECT_FALSE(cap.isOpened());
    EXPECT_EQ(cap.getLastFrame(50).data, nullptr) << "no image is available after close";
    EXPECT_FALSE(cap.setLEDstatus(true)) << "settings must be refused after close";
    EXPECT_EQ(test::videoMapCount(), 0);
}

// With USB 2.0 the camera only provides low resolutions at a reduced frame rate
TEST_P(Camera, Usb2ConnectionIsReported)
{
    if(GetParam().isUsb3())
        GTEST_SKIP() << "the camera is connected in USB 3.0 mode";

    {
        VideoCapture cap(makeParams(RESOLUTION::HD2K, FPS::FPS_15, sl_oc::VERBOSITY::NONE));
        EXPECT_FALSE(cap.initializeVideo(GetParam().id)) << "HD2K is not available with USB 2.0";
        EXPECT_EQ(test::videoMapCount(), 0);
    }

    VideoCapture cap(makeParams(RESOLUTION::VGA, FPS::FPS_100, sl_oc::VERBOSITY::NONE));
    ASSERT_TRUE(cap.initializeVideo(GetParam().id));
    EXPECT_LT(cap.getFps(), 100) << "100 FPS are not available with USB 2.0";

    uint64_t lastTs = 0;
    int frames = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < 3.0)
    {
        const Frame& frame = cap.getLastFrame(200);
        if(frame.data == nullptr || frame.timestamp == lastTs)
            continue;
        lastTs = frame.timestamp;
        frames++;
    }
    EXPECT_NEAR(frames / (test::nowSec() - start), cap.getFps(), cap.getFps() * 0.1) << "the reported frame rate is not the real one";
}

INSTANTIATE_TEST_SUITE_P(Cameras, Camera, testing::ValuesIn(test::listStereolabsVideoDevices()),
                         [](const testing::TestParamInfo<test::VideoDev>& info) { return devName(info.param); });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Camera);
// <---- Cases that run once per camera

TEST(NoCamera, OpenOfMissingDeviceFailsCleanly)
{
    const int fdBefore = test::openFdCount();
    {
        VideoCapture cap(makeParams(RESOLUTION::HD720, FPS::FPS_30, sl_oc::VERBOSITY::NONE));
        EXPECT_FALSE(cap.initializeVideo(63));
    }
    EXPECT_EQ(test::openFdCount(), fdBefore);
}

} // namespace
