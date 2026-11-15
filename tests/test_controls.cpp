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

// Hardware test of the camera controls of the VideoCapture class.
// Every control is checked over its full range: the camera must accept the value, return it when read back,
// and the image must change as expected. Every case runs on each connected Stereolabs camera. No display is needed.
//
// The cases that verify the effect on the image need a scene that does not change during the test, not too dark,
// with some colors and details. They are skipped if the scene is not suitable.

#include "videocapture.hpp"
#include "test_common.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace sl_oc::video;

namespace {

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

const CAM_SENS_POS kSides[] = {CAM_SENS_POS::LEFT, CAM_SENS_POS::RIGHT};

const char* sideName(CAM_SENS_POS side)
{
    return side == CAM_SENS_POS::LEFT ? "left" : "right";
}

class Controls : public testing::TestWithParam<test::VideoDev>
{
protected:
    void SetUp() override
    {
        if(!GetParam().isUsb3())
            GTEST_SKIP() << "the camera is connected in USB 2.0 mode";

        cap.reset(new VideoCapture(makeParams(RESOLUTION::HD720, FPS::FPS_30)));
        ASSERT_TRUE(cap->initializeVideo(GetParam().id));
    }

    //! Fixed gain and exposure, to measure the effect of the other controls
    void freezeExposure(int gain = 30, int exposure = 40)
    {
        ASSERT_TRUE(cap->setAECAGC(false));
        for(CAM_SENS_POS side : kSides)
        {
            ASSERT_TRUE(cap->setGain(side, gain));
            ASSERT_TRUE(cap->setExposure(side, exposure));
        }
    }

    //! Left image after a setting had the time to be applied
    test::ImageStats left(int skip = 6)
    {
        test::ImageStats l, r;
        EXPECT_TRUE(test::measureImages(*cap, skip, 4, l, r)) << "no frames received";
        return l;
    }

    //! Statistics of one of the two images
    test::ImageStats image(CAM_SENS_POS side, int skip = 6)
    {
        test::ImageStats l, r;
        EXPECT_TRUE(test::measureImages(*cap, skip, 4, l, r)) << "no frames received";
        return side == CAM_SENS_POS::LEFT ? l : r;
    }

    std::unique_ptr<VideoCapture> cap;
};

// ----> Every value of the range is accepted and returned, the values out of the range are refused
struct RangeControl
{
    const char* name;
    int min, max, step, def;
    std::function<bool(VideoCapture&, int)> set;
    std::function<int(VideoCapture&)> get;
    std::function<bool(VideoCapture&)> reset;
};

TEST_P(Controls, EveryValueIsAcceptedAndReadBack)
{
    const std::vector<RangeControl> controls = {
        {"brightness", 0, 8, 1, 4, [](VideoCapture& c, int v) { return c.setBrightness(v); }, [](VideoCapture& c) { return c.getBrightness(); }, [](VideoCapture& c) { return c.resetBrightness(); }},
        {"contrast", 0, 8, 1, 4, [](VideoCapture& c, int v) { return c.setContrast(v); }, [](VideoCapture& c) { return c.getContrast(); }, [](VideoCapture& c) { return c.resetContrast(); }},
        {"saturation", 0, 8, 1, 4, [](VideoCapture& c, int v) { return c.setSaturation(v); }, [](VideoCapture& c) { return c.getSaturation(); }, [](VideoCapture& c) { return c.resetSaturation(); }},
        {"sharpness", 0, 8, 1, 0, [](VideoCapture& c, int v) { return c.setSharpness(v); }, [](VideoCapture& c) { return c.getSharpness(); }, [](VideoCapture& c) { return c.resetSharpness(); }},
        {"hue", 0, 11, 1, 0, [](VideoCapture& c, int v) { return c.setHue(v); }, [](VideoCapture& c) { return c.getHue(); }, [](VideoCapture& c) { return c.resetHue(); }},
        {"gamma", 1, 9, 1, 1, [](VideoCapture& c, int v) { return c.setGamma(v); }, [](VideoCapture& c) { return c.getGamma(); }, [](VideoCapture& c) { return c.resetGamma(); }},
        {"white balance", 2800, 6500, 100, -1, [](VideoCapture& c, int v) { return c.setWhiteBalance(v); }, [](VideoCapture& c) { return c.getWhiteBalance(); }, [](VideoCapture& c) { return c.resetAutoWhiteBalance(); }},
    };

    for(const RangeControl& ctrl : controls)
    {
        SCOPED_TRACE(ctrl.name);

        for(int value = ctrl.min; value <= ctrl.max; value += ctrl.step)
        {
            EXPECT_TRUE(ctrl.set(*cap, value)) << "value " << value << " refused";
            EXPECT_EQ(ctrl.get(*cap), value);
        }

        // Values out of the range must be refused, without changing the current one
        const int current = ctrl.get(*cap);
        EXPECT_FALSE(ctrl.set(*cap, ctrl.min - ctrl.step)) << "value below the minimum accepted";
        EXPECT_FALSE(ctrl.set(*cap, ctrl.max + ctrl.step)) << "value above the maximum accepted";
        EXPECT_EQ(ctrl.get(*cap), current) << "a refused value changed the control";

        EXPECT_TRUE(ctrl.reset(*cap));
        if(ctrl.def >= 0)
        {
            EXPECT_EQ(ctrl.get(*cap), ctrl.def) << "default value after reset";
        }
    }

    // The manual white balance disables the automatic control, the reset enables it
    EXPECT_TRUE(cap->getAutoWhiteBalance());
    EXPECT_TRUE(cap->setWhiteBalance(4000));
    EXPECT_FALSE(cap->getAutoWhiteBalance());
    EXPECT_TRUE(cap->setAutoWhiteBalance(true));
    EXPECT_TRUE(cap->getAutoWhiteBalance());
    EXPECT_TRUE(cap->setAutoWhiteBalance(false));
    EXPECT_FALSE(cap->getAutoWhiteBalance());
    EXPECT_TRUE(cap->resetAutoWhiteBalance());
    EXPECT_TRUE(cap->getAutoWhiteBalance());
}

TEST_P(Controls, GainAndExposureValuesAreAcceptedAndReadBack)
{
    ASSERT_TRUE(cap->setAECAGC(false));
    EXPECT_FALSE(cap->getAECAGC());

    for(CAM_SENS_POS side : kSides)
    {
        SCOPED_TRACE(sideName(side));
        const CAM_SENS_POS other = side == CAM_SENS_POS::LEFT ? CAM_SENS_POS::RIGHT : CAM_SENS_POS::LEFT;

        ASSERT_TRUE(cap->setGain(other, 33));
        ASSERT_TRUE(cap->setExposure(other, 33));

        for(int gain = 0; gain <= 100; gain += 5)
        {
            EXPECT_TRUE(cap->setGain(side, gain));
            EXPECT_NEAR(cap->getGain(side), gain, 1) << "gain " << gain;
        }

        // The exposure cannot be longer than a frame: the values above the limit of the video mode are
        // reduced to it, and the value read back is the one really applied
        int limit = -1;
        for(int exposure = 0; exposure <= 100; exposure += 5)
        {
            EXPECT_TRUE(cap->setExposure(side, exposure));
            const int applied = cap->getExposure(side);
            if(limit < 0 && applied < exposure - 1)
                limit = applied;
            if(limit < 0)
                EXPECT_NEAR(applied, exposure, 1) << "exposure " << exposure;
            else
                EXPECT_NEAR(applied, limit, 1) << "exposure " << exposure << " above the limit";
        }

        // Values out of the range are limited to it
        EXPECT_TRUE(cap->setGain(side, -20));
        EXPECT_EQ(cap->getGain(side), 0);
        EXPECT_TRUE(cap->setGain(side, 150));
        EXPECT_EQ(cap->getGain(side), 100);
        EXPECT_TRUE(cap->setExposure(side, -20));
        EXPECT_EQ(cap->getExposure(side), 0);

        // The other sensor did not change
        EXPECT_NEAR(cap->getGain(other), 33, 1);
        EXPECT_NEAR(cap->getExposure(other), 33, 1);
    }

    EXPECT_FALSE(cap->setGain(CAM_SENS_POS::LAST, 10)) << "invalid sensor accepted";
    EXPECT_FALSE(cap->setExposure(CAM_SENS_POS::LAST, 10)) << "invalid sensor accepted";
    EXPECT_LT(cap->getGain(CAM_SENS_POS::LAST), 0);
    EXPECT_LT(cap->getExposure(CAM_SENS_POS::LAST), 0);
}
// <---- Every value of the range is accepted

// ----> Effect of the controls on the image
TEST_P(Controls, ImageControlsChangeTheImage)
{
    freezeExposure();
    const test::ImageStats ref = left(20);
    if(ref.luma < 12 || ref.luma > 200 || ref.contrast < 8 || ref.chroma < 2)
        GTEST_SKIP() << "the scene is not suitable: luminance " << ref.luma << ", contrast " << ref.contrast << ", color " << ref.chroma;

    // Brightness: the image becomes brighter
    {
        ASSERT_TRUE(cap->setBrightness(0)); const double low = left().luma;
        ASSERT_TRUE(cap->setBrightness(8)); const double high = left().luma;
        ASSERT_TRUE(cap->resetBrightness()); const double def = left().luma;
        EXPECT_LT(low + 5, def) << "brightness 0 vs default";
        EXPECT_LT(def + 5, high) << "default vs brightness 8";
        EXPECT_NEAR(def, ref.luma, 0.1 * ref.luma + 2) << "the reset must restore the image";
    }

    // Contrast: the difference between dark and bright areas increases
    {
        ASSERT_TRUE(cap->setContrast(0)); const double low = left().contrast;
        ASSERT_TRUE(cap->setContrast(8)); const double high = left().contrast;
        ASSERT_TRUE(cap->resetContrast()); const double def = left().contrast;
        EXPECT_LT(low * 1.3, def) << "contrast 0 vs default";
        EXPECT_LT(def * 1.2, high) << "default vs contrast 8";
    }

    // Saturation: 0 gives a gray image, higher values stronger colors
    {
        ASSERT_TRUE(cap->setSaturation(0)); const double low = left().chroma;
        ASSERT_TRUE(cap->setSaturation(8)); const double high = left().chroma;
        ASSERT_TRUE(cap->resetSaturation()); const double def = left().chroma;
        EXPECT_LT(low, 0.5) << "saturation 0 must remove the colors";
        EXPECT_LT(def * 1.3, high) << "default vs saturation 8";
        EXPECT_NEAR(def, ref.chroma, 0.15 * ref.chroma + 0.5) << "the reset must restore the image";
    }

    // Sharpness: the details are enhanced
    {
        ASSERT_TRUE(cap->setSharpness(8)); const double high = left().detail;
        ASSERT_TRUE(cap->resetSharpness()); const double def = left().detail;
        EXPECT_LT(def * 1.1, high) << "default vs sharpness 8";
    }

    // Gamma: higher values give a brighter image, the reset restores the factory curve that matches the value 1
    {
        ASSERT_TRUE(cap->setGamma(1)); const double g1 = left().luma;
        ASSERT_TRUE(cap->setGamma(5)); const double g5 = left().luma;
        ASSERT_TRUE(cap->setGamma(9)); const double g9 = left().luma;
        ASSERT_TRUE(cap->resetGamma()); const double def = left().luma;
        EXPECT_LT(g1 * 1.1, g5) << "gamma 1 vs 5";
        EXPECT_LT(g5 * 1.1, g9) << "gamma 5 vs 9";
        EXPECT_NEAR(def, g1, 0.05 * g1 + 1) << "factory gamma vs gamma 1";
    }

    // White balance: from low to high temperature the image moves from blue to red
    {
        ASSERT_TRUE(cap->setWhiteBalance(2800)); const test::ImageStats cold = left(10);
        ASSERT_TRUE(cap->setWhiteBalance(4600)); const test::ImageStats mid = left(10);
        ASSERT_TRUE(cap->setWhiteBalance(6500)); const test::ImageStats warm = left(10);
        EXPECT_GT(cold.u, mid.u + 3); EXPECT_GT(mid.u, warm.u + 3);
        EXPECT_LT(cold.v, mid.v - 3); EXPECT_LT(mid.v, warm.v - 3);
        EXPECT_TRUE(cap->resetAutoWhiteBalance());
    }

    // Hue: the colors rotate
    {
        ASSERT_TRUE(cap->setWhiteBalance(4600)); // Fixed colors during the measure
        ASSERT_TRUE(cap->setHue(0)); const test::ImageStats h0 = left(10);
        ASSERT_TRUE(cap->setHue(6)); const test::ImageStats h6 = left(10);
        EXPECT_GT(std::fabs(h6.u - h0.u) + std::fabs(h6.v - h0.v), 1.0) << "hue 0 vs 6: the colors did not change";
        EXPECT_TRUE(cap->resetHue());
        EXPECT_TRUE(cap->resetAutoWhiteBalance());
    }
}

// The checks on the image are repeated if they fail, because a change of the scene (a person passing, a light)
// gives the same result of a wrong setting: a real problem fails every time
TEST_P(Controls, GainAndExposureChangeTheImageOfTheirSensor)
{
    freezeExposure(30, 30);
    const test::ImageStats ref = left(20);
    if(ref.luma < 8 || ref.luma > 120 || ref.contrast < 5)
        GTEST_SKIP() << "the scene is not suitable: luminance " << ref.luma << ", contrast " << ref.contrast;

    for(CAM_SENS_POS side : kSides)
    {
        SCOPED_TRACE(sideName(side));
        const CAM_SENS_POS other = side == CAM_SENS_POS::LEFT ? CAM_SENS_POS::RIGHT : CAM_SENS_POS::LEFT;

        // Returns the description of the first problem found, empty if everything is as expected
        auto verify = [&]() -> std::string {
            if(!cap->setGain(side, 30) || !cap->setExposure(side, 30) || !cap->setGain(other, 30) || !cap->setExposure(other, 30))
                return "settings refused";
            const double otherRef = image(other, 8).luma;
            auto otherChanged = [&](double luma) { return std::fabs(luma - otherRef) > 0.1 * otherRef + 2; };

            // Gain
            double previous = -1;
            for(int gain : {0, 25, 50, 75, 100})
            {
                if(!cap->setGain(side, gain)) return "gain refused";
                const double luma = image(side).luma;
                if(luma <= previous * 1.1) return "gain " + std::to_string(gain) + " is not brighter than the previous value";
                previous = luma;
            }
            if(otherChanged(image(other, 0).luma)) return "the gain of a sensor changed the other image";
            if(!cap->setGain(side, 30)) return "gain refused";

            // Exposure, up to the limit of the video mode
            previous = -1;
            for(int exposure : {0, 10, 25, 50, 70})
            {
                if(!cap->setExposure(side, exposure)) return "exposure refused";
                const double luma = image(side).luma;
                if(luma <= previous * 1.08) return "exposure " + std::to_string(exposure) + " is not brighter than the previous value";
                previous = luma;
            }

            // The maximum exposure on one sensor only must not stop the images
            if(!cap->setExposure(side, 100)) return "exposure refused";
            test::ImageStats l, r;
            if(!test::measureImages(*cap, 8, 4, l, r)) return "no frames with the maximum exposure";
            if((side == CAM_SENS_POS::LEFT ? l : r).luma < previous * 0.9) return "the maximum exposure is darker than exposure 70";
            if(otherChanged((side == CAM_SENS_POS::LEFT ? r : l).luma)) return "the exposure of a sensor changed the other image";
            if(l.chroma + r.chroma < 0.5) return "empty images with the maximum exposure";

            // Same settings, same image on the two sensors
            if(!cap->setExposure(side, 30)) return "exposure refused";
            if(!test::measureImages(*cap, 8, 4, l, r)) return "no frames";
            if(std::fabs(l.luma - r.luma) > 0.2 * l.luma + 3) return "left and right images have a different brightness with the same settings";

            return "";
        };

        std::string problem;
        for(int attempt = 0; attempt < 3; attempt++)
        {
            problem = verify();
            if(problem.empty()) break;
        }
        EXPECT_TRUE(problem.empty()) << problem << " (3 attempts)";
    }
}

TEST_P(Controls, AutomaticExposure)
{
    EXPECT_TRUE(cap->getAECAGC()) << "automatic exposure must be active after the open";
    const double autoLuma = left(30).luma;

    // A manual value disables the automatic control
    EXPECT_TRUE(cap->setGain(CAM_SENS_POS::LEFT, 5));
    EXPECT_FALSE(cap->getAECAGC());
    EXPECT_TRUE(cap->setExposure(CAM_SENS_POS::LEFT, 5));
    EXPECT_TRUE(cap->setGain(CAM_SENS_POS::RIGHT, 5));
    EXPECT_TRUE(cap->setExposure(CAM_SENS_POS::RIGHT, 5));
    const double darkLuma = left().luma;

    // The values stay where they are until the automatic control is enabled again
    usleep(1000000);
    EXPECT_NEAR(cap->getGain(CAM_SENS_POS::LEFT), 5, 1);
    EXPECT_NEAR(cap->getExposure(CAM_SENS_POS::LEFT), 5, 1);
    EXPECT_NEAR(left(0).luma, darkLuma, 0.1 * darkLuma + 2);

    EXPECT_TRUE(cap->setAECAGC(true));
    EXPECT_TRUE(cap->getAECAGC());
    const double restored = left(45).luma;
    if(autoLuma > darkLuma * 1.3 + 5)
    {
        EXPECT_NEAR(restored, autoLuma, 0.25 * autoLuma + 5) << "the automatic exposure did not restore the image";
    }

    EXPECT_TRUE(cap->setAECAGC(false));
    EXPECT_FALSE(cap->getAECAGC());
    EXPECT_TRUE(cap->resetAECAGC());
    EXPECT_TRUE(cap->getAECAGC());
}

TEST_P(Controls, AutomaticExposureRegion)
{
    int width = 0, height = 0;
    cap->getFrameSize(width, height);
    width /= 2;

    for(CAM_SENS_POS side : kSides)
    {
        SCOPED_TRACE(sideName(side));
        const CAM_SENS_POS other = side == CAM_SENS_POS::LEFT ? CAM_SENS_POS::RIGHT : CAM_SENS_POS::LEFT;

        uint16_t ox = 0, oy = 0, ow = 0, oh = 0;
        ASSERT_TRUE(cap->getROIforAECAGC(other, ox, oy, ow, oh));

        const uint16_t rects[][4] = {{0, 0, 320, 180}, {300, 200, 600, 400}, {959, 539, 321, 181}, {1, 1, 11, 11},
                                     {0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height)}};
        for(const uint16_t* rect : rects)
        {
            uint16_t x = 0, y = 0, w = 0, h = 0;
            EXPECT_TRUE(cap->setROIforAECAGC(side, rect[0], rect[1], rect[2], rect[3]));
            EXPECT_TRUE(cap->getROIforAECAGC(side, x, y, w, h));
            EXPECT_EQ(x, rect[0]); EXPECT_EQ(y, rect[1]); EXPECT_EQ(w, rect[2]); EXPECT_EQ(h, rect[3]);
        }

        EXPECT_FALSE(cap->setROIforAECAGC(side, 1000, 0, 600, 400)) << "region outside the image accepted";
        EXPECT_FALSE(cap->setROIforAECAGC(side, 0, 500, 600, 400)) << "region outside the image accepted";
        EXPECT_FALSE(cap->setROIforAECAGC(side, 10, 10, 0, 100)) << "empty region accepted";
        EXPECT_FALSE(cap->setROIforAECAGC(side, 10, 10, 10, 10)) << "region too small accepted";

        EXPECT_TRUE(cap->resetROIforAECAGC(side));

        uint16_t x = 0, y = 0, w = 0, h = 0;
        EXPECT_TRUE(cap->getROIforAECAGC(other, x, y, w, h));
        EXPECT_EQ(x, ox); EXPECT_EQ(y, oy); EXPECT_EQ(w, ow); EXPECT_EQ(h, oh);
    }

    EXPECT_FALSE(cap->setROIforAECAGC(CAM_SENS_POS::LAST, 0, 0, 100, 100)) << "invalid sensor accepted";
}

TEST_P(Controls, Led)
{
    bool led = false;
    for(int i = 0; i < 10; i++)
    {
        const bool target = (i % 2) == 0;
        EXPECT_TRUE(cap->setLEDstatus(target));
        EXPECT_TRUE(cap->getLEDstatus(&led));
        EXPECT_EQ(led, target);
    }
    for(int i = 0; i < 6; i++)
    {
        bool before = false, toggled = false, after = false;
        EXPECT_TRUE(cap->getLEDstatus(&before));
        EXPECT_TRUE(cap->toggleLED(&toggled));
        EXPECT_TRUE(cap->getLEDstatus(&after));
        EXPECT_EQ(toggled, !before);
        EXPECT_EQ(after, toggled);
    }
    EXPECT_FALSE(cap->getLEDstatus(nullptr));
    EXPECT_TRUE(cap->setLEDstatus(true));
}

// The settings of a session must not be found by the next one
TEST_P(Controls, DefaultsAreRestoredByTheOpen)
{
    EXPECT_TRUE(cap->setBrightness(7));
    EXPECT_TRUE(cap->setContrast(1));
    EXPECT_TRUE(cap->setSaturation(7));
    EXPECT_TRUE(cap->setSharpness(5));
    EXPECT_TRUE(cap->setHue(4));
    EXPECT_TRUE(cap->setGamma(7));
    EXPECT_TRUE(cap->setWhiteBalance(3000));
    EXPECT_TRUE(cap->setGain(CAM_SENS_POS::LEFT, 80));
    EXPECT_TRUE(cap->setExposure(CAM_SENS_POS::RIGHT, 10));

    ASSERT_TRUE(cap->initializeVideo(GetParam().id));

    EXPECT_EQ(cap->getBrightness(), 4);
    EXPECT_EQ(cap->getContrast(), 4);
    EXPECT_EQ(cap->getSaturation(), 4);
    EXPECT_EQ(cap->getSharpness(), 0);
    EXPECT_EQ(cap->getHue(), 0);
    EXPECT_EQ(cap->getGamma(), 1);
    EXPECT_TRUE(cap->getAutoWhiteBalance());
    EXPECT_TRUE(cap->getAECAGC());
}

INSTANTIATE_TEST_SUITE_P(Cameras, Controls, testing::ValuesIn(test::listStereolabsVideoDevices()),
                         [](const testing::TestParamInfo<test::VideoDev>& info) { return devName(info.param); });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Controls);
// <---- Effect of the controls on the image

// ----> Gain and exposure in every video mode
struct Mode
{
    RESOLUTION res;
    FPS fps;
    const char* name;
};

void PrintTo(const Mode& mode, std::ostream* os)
{
    *os << mode.name;
}

const Mode kModes[] = {
    {RESOLUTION::HD2K, FPS::FPS_15, "HD2K_15"},   {RESOLUTION::HD1080, FPS::FPS_30, "HD1080_30"}, {RESOLUTION::HD1080, FPS::FPS_15, "HD1080_15"},
    {RESOLUTION::HD720, FPS::FPS_60, "HD720_60"}, {RESOLUTION::HD720, FPS::FPS_30, "HD720_30"},   {RESOLUTION::HD720, FPS::FPS_15, "HD720_15"},
    {RESOLUTION::VGA, FPS::FPS_100, "VGA_100"},   {RESOLUTION::VGA, FPS::FPS_60, "VGA_60"},       {RESOLUTION::VGA, FPS::FPS_30, "VGA_30"},
    {RESOLUTION::VGA, FPS::FPS_15, "VGA_15"},
};

class ControlsInMode : public testing::TestWithParam<std::tuple<test::VideoDev, Mode>>
{
};

// In every mode the exposure must work over its full range: brighter up to the limit of the mode, never darker or
// without image above it, also when the two sensors receive different values
TEST_P(ControlsInMode, ExposureWorksOverTheFullRange)
{
    const test::VideoDev dev = std::get<0>(GetParam());
    const Mode mode = std::get<1>(GetParam());
    if(!dev.isUsb3())
        GTEST_SKIP() << "the camera is connected in USB 2.0 mode";

    VideoCapture cap(makeParams(mode.res, mode.fps));
    ASSERT_TRUE(cap.initializeVideo(dev.id));
    ASSERT_TRUE(cap.setAECAGC(false));
    for(CAM_SENS_POS side : kSides)
    {
        ASSERT_TRUE(cap.setGain(side, 25));
        ASSERT_TRUE(cap.setExposure(side, 20));
    }

    test::ImageStats l, r;
    ASSERT_TRUE(test::measureImages(cap, 10, 3, l, r));
    if(l.luma < 6 || l.luma > 150)
        GTEST_SKIP() << "the scene is not suitable: luminance " << l.luma;

    // Repeated if it fails: a change of the scene gives the same result of a wrong setting
    auto verify = [&]() -> std::string {
        if(!cap.setExposure(CAM_SENS_POS::LEFT, 20)) return "exposure refused";
        if(!test::measureImages(cap, 8, 3, l, r)) return "no frames";
        const double rightRef = r.luma;

        double previous = l.luma;
        for(int exposure : {40, 60, 80, 100})
        {
            if(!cap.setExposure(CAM_SENS_POS::LEFT, exposure)) return "exposure refused";
            if(!test::measureImages(cap, 6, 3, l, r)) return "no frames";
            if(l.luma < previous * 0.9) return "exposure " + std::to_string(exposure) + " is darker than the previous value";
            if(l.chroma + r.chroma < 0.5) return "empty images with exposure " + std::to_string(exposure);
            if(std::fabs(r.luma - rightRef) > 0.1 * rightRef + 2) return "right image changed with left exposure " + std::to_string(exposure);
            if(cap.getExposure(CAM_SENS_POS::LEFT) > exposure + 1) return "exposure read back higher than the written value";
            previous = l.luma;
        }
        return "";
    };

    std::string problem;
    for(int attempt = 0; attempt < 3; attempt++)
    {
        problem = verify();
        if(problem.empty()) break;
    }
    EXPECT_TRUE(problem.empty()) << problem << " (3 attempts)";

    // The frame rate does not change with the longest exposure. The median interval is not affected by lost frames
    std::vector<double> intervals;
    uint64_t lastTs = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < 2.0)
    {
        const Frame& frame = cap.getLastFrame(300);
        if(frame.data == nullptr || frame.timestamp == lastTs)
            continue;
        if(lastTs != 0) intervals.push_back(static_cast<double>(frame.timestamp - lastTs) / 1e9);
        lastTs = frame.timestamp;
    }
    ASSERT_GT(intervals.size(), 4u);
    std::nth_element(intervals.begin(), intervals.begin() + intervals.size() / 2, intervals.end());
    const double fps = 1.0 / intervals[intervals.size() / 2];
    EXPECT_NEAR(fps, static_cast<double>(mode.fps), static_cast<double>(mode.fps) * 0.1) << "frame rate with the maximum exposure";
}

INSTANTIATE_TEST_SUITE_P(Cameras, ControlsInMode,
                         testing::Combine(testing::ValuesIn(test::listStereolabsVideoDevices()), testing::ValuesIn(kModes)),
                         [](const testing::TestParamInfo<ControlsInMode::ParamType>& info) {
                             return devName(std::get<0>(info.param)) + "_" + std::get<1>(info.param).name;
                         });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ControlsInMode);
// <---- Gain and exposure in every video mode

} // namespace
