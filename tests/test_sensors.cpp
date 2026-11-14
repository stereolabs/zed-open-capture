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

// Hardware test of the SensorCapture class.
// Every case runs on each connected Stereolabs camera with sensors (ZED Mini, ZED 2, ZED 2i).
//
// The video module reset case reboots the video module of one camera, so it is disabled by default.
// Run it with: test_sensors --gtest_also_run_disabled_tests --gtest_filter='*Reset*'

#include "sensorcapture.hpp"
#include "test_common.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <cmath>

using namespace sl_oc::sensors;

namespace {

std::vector<int> listSerials()
{
    SensorCapture sens(sl_oc::VERBOSITY::NONE);
    return sens.getDeviceList();
}

TEST(Setup, CameraWithSensorsConnected)
{
    ASSERT_FALSE(listSerials().empty()) << "No Stereolabs camera with sensors found (are the udev rules installed?)";
}

// ----> Cases that crash or hang with a defective library: they run in a child process (death test).
// The child must exit with code 0 before the alarm fires.
class Robustness : public testing::Test
{
protected:
    void SetUp() override
    {
        testing::GTEST_FLAG(death_test_style) = "threadsafe";
        serials = listSerials();
        if(serials.empty())
            GTEST_SKIP() << "no camera with sensors";
    }

    std::vector<int> serials;
};

TEST_F(Robustness, InitializeTwiceOnTheSameCamera)
{
    EXPECT_EXIT({
        alarm(20);
        SensorCapture sens(sl_oc::VERBOSITY::NONE);
        bool ok = sens.initializeSensors(serials.front());
        sens.initializeSensors(serials.front()); // the result does not matter, it must only return
        _exit(ok ? 0 : 1);
    }, testing::ExitedWithCode(0), "");
}

TEST_F(Robustness, SwitchToAnotherCamera)
{
    if(serials.size() < 2)
        GTEST_SKIP() << "needs two cameras with sensors";

    EXPECT_EXIT({
        alarm(20);
        SensorCapture sens(sl_oc::VERBOSITY::NONE);
        bool ok = sens.initializeSensors(serials[0]) && sens.initializeSensors(serials[1]) && sens.getSerialNumber() == serials[1];
        _exit(ok ? 0 : 1);
    }, testing::ExitedWithCode(0), "");
}

TEST_F(Robustness, UnknownSerialNumberFailsCleanly)
{
    EXPECT_EXIT({
        alarm(20);
        SensorCapture sens(sl_oc::VERBOSITY::NONE);
        _exit(sens.initializeSensors(1) ? 1 : 0); // no camera has this serial number
    }, testing::ExitedWithCode(0), "");
}

TEST_F(Robustness, DISABLED_ResetOfFirstAvailableCameraReturns)
{
    EXPECT_EXIT({
        alarm(20);
        SensorCapture::resetVideoModule(0);
        _exit(0);
    }, testing::ExitedWithCode(0), "");
    sleep(6); // let the camera come back
}
// <---- Robustness

// ----> Cases that run once per camera
class Sensors : public testing::TestWithParam<int>
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(sens.initializeSensors(GetParam()));
        uint16_t fwMajor = 0, fwMinor = 0;
        sens.getFirmwareVersion(fwMajor, fwMinor);
        isZedMini = (fwMajor == 2); // ZED Mini firmware is 2.x, ZED 2 and ZED 2i firmware is 3.x
    }

    SensorCapture sens{sl_oc::VERBOSITY::ERROR};
    bool isZedMini = false;
};

TEST_P(Sensors, ReportsItsSerialNumber)
{
    EXPECT_EQ(sens.getSerialNumber(), GetParam());
}

// IMU rate, timestamp order and timestamp agreement with the host clock, using the queue that keeps every sample
TEST_P(Sensors, ImuRateAndTimestamps)
{
    const double nominalHz = isZedMini ? 800.0 : 400.0;
    const double periodMs = 1000.0 / nominalHz;
    const double duration = 10.0;

    uint64_t lastTs = 0;
    int samples = 0, notIncreasing = 0, gaps = 0;
    double firstOffsetMs = 0, lastOffsetMs = 0, worstOffsetMs = 0, gravity = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < duration)
    {
        const std::vector<data::Imu> queue = sens.getIMUData(20000);
        const uint64_t now = getWallTimestamp();
        for(const data::Imu& imu : queue)
        {
            EXPECT_EQ(imu.valid, data::Imu::NEW_VAL);
            if(lastTs != 0)
            {
                if(imu.timestamp <= lastTs) notIncreasing++;
                else if((imu.timestamp - lastTs) / 1e6 > 2.5 * periodMs) gaps++;
            }
            lastTs = imu.timestamp;
            gravity = std::sqrt(imu.aX * imu.aX + imu.aY * imu.aY + imu.aZ * imu.aZ);
            samples++;
        }
        if(queue.empty())
            continue;

        // The newest sample has just been received
        lastOffsetMs = (static_cast<double>(queue.back().timestamp) - static_cast<double>(now)) / 1e6;
        if(samples == static_cast<int>(queue.size())) firstOffsetMs = lastOffsetMs;
        if(test::nowSec() - start > 3.0 && std::fabs(lastOffsetMs) > std::fabs(worstOffsetMs)) worstOffsetMs = lastOffsetMs;
    }
    const double rate = samples / (test::nowSec() - start);

    EXPECT_TRUE(sens.isOpened());
    EXPECT_NEAR(rate, nominalHz, nominalHz * 0.03) << "IMU rate is not within 3% of nominal";
    EXPECT_EQ(notIncreasing, 0) << "IMU timestamps did not always increase";
    // A sample lost by the USB host from time to time is tolerated: 0.2% at most
    EXPECT_LE(gaps, samples / 500) << "IMU samples were lost";
    EXPECT_NEAR(firstOffsetMs, 0.0, 50.0) << "first IMU timestamp is far from the host clock [ms]";
    EXPECT_NEAR(worstOffsetMs, 0.0, 15.0) << "IMU timestamps are far from the host clock after 3 s [ms]";
    EXPECT_NEAR(lastOffsetMs - firstOffsetMs, 0.0, 20.0) << "IMU timestamps drifted from the host clock in " << duration << " s [ms]";
    EXPECT_NEAR(gravity, 9.8, 1.0) << "the accelerometer must measure gravity when the camera is still";
}

// The function that only returns the most recent sample must provide data at a rate close to nominal
TEST_P(Sensors, LastImuDataIsFresh)
{
    const double nominalHz = isZedMini ? 800.0 : 400.0;

    uint64_t lastTs = 0;
    int samples = 0, backwards = 0;
    const double start = test::nowSec();
    while(test::nowSec() - start < 3.0)
    {
        const data::Imu& imu = sens.getLastIMUData(5000);
        if(imu.valid != data::Imu::NEW_VAL)
            continue;
        if(imu.timestamp <= lastTs) backwards++;
        lastTs = imu.timestamp;
        samples++;
    }
    EXPECT_EQ(backwards, 0);
    EXPECT_GT(samples / (test::nowSec() - start), nominalHz * 0.7);
    EXPECT_NEAR((static_cast<double>(lastTs) - static_cast<double>(getWallTimestamp())) / 1e6, 0.0, 20.0);
}

TEST_P(Sensors, CloseAndReopen)
{
    for(int i = 0; i < 3; i++)
    {
        sens.close();
        sens.close(); // closing twice is allowed
        EXPECT_FALSE(sens.isOpened());
        EXPECT_EQ(sens.getSerialNumber(), -1);
        EXPECT_TRUE(sens.getIMUData(1000).empty());

        ASSERT_TRUE(sens.initializeSensors(GetParam())) << "open #" << i;
        EXPECT_FALSE(sens.getIMUData(500000).empty()) << "no data after open #" << i;
    }
}

TEST_P(Sensors, MagnetometerEnvironmentAndTemperature)
{
    if(isZedMini)
        GTEST_SKIP() << "the ZED Mini has no magnetometer, barometer or temperature sensors";

    int mag = 0, env = 0, temp = 0;
    data::Environment lastEnv;
    // The environmental sensors can need a few seconds to provide the first data after the connection
    const double t0 = test::nowSec();
    while(test::nowSec() - t0 < 10.0 && (mag < 10 || env < 10 || temp < 10))
    {
        if(sens.getLastMagnetometerData(2000).valid == data::Magnetometer::NEW_VAL) mag++;
        const data::Environment e = sens.getLastEnvironmentData(2000);
        if(e.valid == data::Environment::NEW_VAL) { env++; lastEnv = e; }
        if(sens.getLastCameraTemperatureData(2000).valid == data::Temperature::NEW_VAL) temp++;
    }
    EXPECT_GT(mag, 0);
    EXPECT_GT(temp, 0);
    ASSERT_GT(env, 0);
    EXPECT_NEAR(lastEnv.press, 950.0, 150.0) << "pressure is not plausible [hPa]";
    EXPECT_NEAR(lastEnv.temp, 35.0, 35.0) << "temperature is not plausible [C]";
}

INSTANTIATE_TEST_SUITE_P(Cameras, Sensors, testing::ValuesIn(listSerials()),
                         [](const testing::TestParamInfo<int>& info) { return "SN" + std::to_string(info.param); });
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Sensors);
// <---- Cases that run once per camera

} // namespace
