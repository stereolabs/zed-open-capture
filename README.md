<h1 align="center">
  Open Capture Camera API
</h1>

<h4 align="center">A platform-agnostic camera and sensor capture API for the <a href="https://docs.stereolabs.com/docs/products/cameras/zed">ZED 2, ZED 2i, and ZED Mini</a> stereo cameras</h4>
<h5 align="center">*** not compatible with GMSL2 devices: <a href="https://docs.stereolabs.com/docs/products/cameras">ZED X, ZED X Mini, and ZED X One</a> ***</h5>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build-and-install">Build and install</a> •
  <a href="#run">Run</a> • 
  <a href="#documentation">Documentation</a> •
  <a href="#running-the-examples">Examples</a> •
  <a href="#performance">Performance</a> •
  <a href="#known-issues">Known issues</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>
<br>

## Key Features

* Open source C++ capture library, built with the C++17 standard
* No ZED SDK and no NVIDIA GPU required: it works on any Linux computer, including Raspberry Pi 4
* Video Capture
  * raw YUV 4:2:2 data format
  * Camera controls
  * Camera selection by device or by serial number
  * HIGH-RESOLUTION mode with USB3
  * compatibility VGA mode with USB2
* Sensor Data Capture
  * 6-DOF IMU (3-DOF accelerometer + 3-DOF gyroscope), without losing samples
  * 3-DOF Magnetometer (Only ZED 2 and ZED 2i)
  * Barometer (Only ZED 2 and ZED 2i)
  * Sensors temperature (Only ZED 2 and ZED 2i)
* Frames and sensor data on the same clock, with hardware Sensors/video Synchronization
* Portable
  * Tested on Linux
  * Tested on x64, ARM (NVIDIA Jetson AGX Thor, Raspberry Pi 4)
* Small Size
  * ~200KB library size
  * hidapi is the only dependency (sensors module)
* Complete set of examples
  * Video capture
  * Camera control
  * Stereo rectification
  * IMU, magnetometer, and barometer data capture
  * Video and sensor synchronization
  * Disparity/Depth/Point Cloud extraction on CPU using OpenCV
  * Depth tuning using OpenCV control GUI
  * Benchmark of image, sensors and depth performance

## Description

The ZED Open Capture is a multi-platform, open-source C++ library for low-level camera and sensor capture for the ZED stereo camera family. It doesn't require CUDA and therefore can be used on many desktop and embedded platforms.

The open-source library provides methods to access raw video frames, calibration data, camera controls, and raw data from the USB3 camera sensors (on ZED 2, ZED 2i, and ZED Mini). A synchronization mechanism is provided to get the correct sensor data associated with a video frame.

**Note:** While in the ZED SDK all output data is calibrated and compensated, here the extracted raw data is not corrected by the camera and sensor calibration parameters. You can retrieve camera and sensor calibration data using the [ZED SDK](https://docs.stereolabs.com/docs/development/zed-sdk/modules/camera/camera-calibration) to correct your camera data [see `zed_open_capture_rectify_example` example](#running-the-examples).

## Build and install

### Prerequisites

* USB3 Stereolabs Stereo camera: ZED 2i, ZED 2, ZED Mini or ZED (see the [ZED USB 3 cameras](https://docs.stereolabs.com/docs/products/cameras/zed) documentation and their [specifications](https://docs.stereolabs.com/docs/products/cameras/zed/specifications))
* Linux OS
* GCC (v7.5+)
* CMake (v3.16+)
* OpenCV (v3.4.0+) -Optional for the examples-
* GoogleTest -Optional for the hardware tests-

### Install prerequisites

* Install GCC compiler and build tools

    `sudo apt install build-essential`

* Install CMake build system

    `sudo apt install cmake`

* Install the HIDAPI library (required by the sensors module):

    `sudo apt install libhidapi-libusb0 libhidapi-dev`

* Install OpenCV to build the examples (optional)

    `sudo apt install libopencv-dev`

* Install GoogleTest to build the hardware tests (optional)

    `sudo apt install libgtest-dev`

### Clone the repository

```bash
git clone https://github.com/stereolabs/zed-open-capture.git
cd zed-open-capture
```

### Add udev rule

Stereo cameras such as ZED 2 and ZED Mini have built-in sensors (e.g. IMU) that are identified as USB HID devices.
To be able to access the USB HID device, you must add a udev rule contained in the `udev` folder:

```bash
bash udev/install_udev_rule.sh
```

### Build

#### Build library and examples

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

#### Build only the library

```bash
mkdir build
cd build
cmake .. -DBUILD_EXAMPLES=OFF
make -j$(nproc)
```

#### Build only the video capture library

```bash
mkdir build
cd build
cmake .. -DBUILD_SENSORS=OFF -DBUILD_EXAMPLES=OFF
make -j$(nproc)
```

#### Build only the sensor capture library

```bash
mkdir build
cd build
cmake .. -DBUILD_VIDEO=OFF -DBUILD_EXAMPLES=OFF
make -j$(nproc)
```

#### Build options

| Option | Default | Description |
|---|---|---|
| `BUILD_VIDEO` | `ON` | Build the video capture module |
| `BUILD_SENSORS` | `ON` | Build the sensor capture module |
| `BUILD_EXAMPLES` | `ON` | Build the examples (requires OpenCV) |
| `BUILD_TESTS` | `OFF` | Build the hardware tests (requires GoogleTest) |
| `DEBUG_CAM_REG` | `OFF` | Add the functions that log the camera registers |
| `ZED_OC_SANITIZER` | empty | Build with a sanitizer: `address` (with undefined behavior checks) or `thread` |
| `ZED_OC_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |

The default build type is `Release`. Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.

### Install

To install the library, go to the `build` folder and launch the following commands:

```bash
sudo make install
sudo ldconfig
```

### Use the library in your application

After the installation the library is available as a CMake package:

```cmake
find_package(zed_open_capture REQUIRED)
target_link_libraries(my_application PRIVATE zed_open_capture::zed_open_capture)
```

A pkg-config file is also installed: `pkg-config --cflags --libs zed_open_capture`.

## Run

### Get video data

Include the `videocapture.hpp` header, declare a `VideoCapture` object, and retrieve a video frame (in YUV 4:2:2 format) with `getLastFrame()`:

```C++
#include "videocapture.hpp"

sl_oc::video::VideoCapture cap;
if( !cap.initializeVideo() ) // first available camera
    return EXIT_FAILURE;

uint64_t last_ts = 0;
while( cap.isOpened() )
{
    const sl_oc::video::Frame& frame = cap.getLastFrame();
    if( frame.data!=nullptr && frame.timestamp!=last_ts ) // a new frame is available
    {
        last_ts = frame.timestamp;
        // frame.data contains two images side by side, frame.width x frame.height pixels
    }
}
```

The returned frame stays valid and unchanged until the next call of `getLastFrame()`.

To select a camera when more than one is connected:

```C++
for( const sl_oc::video::VideoDevice& dev : sl_oc::video::VideoCapture::getDeviceList() )
    std::cout << dev.dev_name << " - serial number: " << dev.serial_number << std::endl;

cap.initializeVideoBySerialNumber( 12345678 ); // or cap.initializeVideo( dev.dev_id )
```

### Get sensor data

Include the `SensorCapture` header, declare a `SensorCapture` object, get a list of available devices, initialize the first one, and finally retrieve sensor data:

```C++
#include "sensorcapture.hpp"

sl_oc::sensors::SensorCapture sens;
std::vector<int> devs = sens.getDeviceList();
sens.initializeSensors( devs[0] );

// All the IMU samples received after the previous call (400 Hz, 800 Hz for ZED Mini): no sample is lost
const std::vector<sl_oc::sensors::data::Imu> imuQueue = sens.getIMUData(5000);

// Only the most recent data
const sl_oc::sensors::data::Imu& imuData = sens.getLastIMUData(5000);
const sl_oc::sensors::data::Magnetometer& magData = sens.getLastMagnetometerData(100);
const sl_oc::sensors::data::Environment& envData = sens.getLastEnvironmentData(100);
const sl_oc::sensors::data::Temperature& tempData = sens.getLastCameraTemperatureData(100);
```

### Timestamps and synchronization

Frames and sensor data use the same clock: the system time read when the first timestamp is needed, then advanced with the monotonic clock, so that the timestamps never go backwards. The clock of the sensors module of the camera is continuously aligned to it.

For the best alignment enable the hardware synchronization: the camera signals each frame to the sensors module, and the frame takes the timestamp of the IMU sample acquired with that signal (the sample with `sync` set to `true`).

```C++
videoCap.initializeVideo();
sensCap.initializeSensors( videoCap.getSerialNumber() );
videoCap.enableSensorSync( &sensCap );
```

## Running the tests

The tests check the library on real hardware: they need at least one camera connected and they do not need a display.
Each test runs on every connected camera.

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

| Test | What it checks |
|---|---|
| `test_video` | Frame rate and frame integrity in every video mode, camera controls, camera selection, open/close without leaks |
| `test_sensors` | IMU rate, lost samples and timestamps, magnetometer, barometer and temperature data, robustness of the connection |
| `test_sync` | Agreement of video and IMU timestamps when the synchronization is enabled, closing order of the two objects |

A camera connected in USB 2.0 mode is reported as a failure and most of its tests are skipped.

Each test is a GoogleTest executable, so a single case can be selected:

```bash
tests/test_video --gtest_list_tests
tests/test_video --gtest_filter='*Controls*'
```

To look for memory errors or data races, build the tests with a sanitizer:

```bash
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF -DZED_OC_SANITIZER=thread -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Checks without a camera

The script used by the automatic checks of the GitHub repository can also be started on your computer. It builds all the configurations of the library with warnings as errors, verifies that an application can use the installed library, and runs the tests that do not need a camera:

```bash
.github/scripts/check_without_camera.sh
```

Set `CXX=clang++` to use another compiler and `SANITIZERS=1` to also build and run with the address and thread sanitizers.

## Running the examples

After installing the library and examples, you will have the following sample applications in your `build` directory:

* [zed_open_capture_video_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_video_example.cpp): This application captures and displays video frames from the camera.
* [zed_open_capture_multicam_video_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_multi_video_example.cpp): This application captures and displays video frames from two cameras.
* [zed_open_capture_control_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_control_example.cpp): This application captures and displays video frames from the camera and provides runtime control of camera parameters using keyboard shortcuts.
* [zed_open_capture_rectify_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_rectify_example.cpp): This application downloads factory stereo calibration parameters from Stereolabs server, performs stereo image rectification and displays original and rectified frames.
* [zed_open_capture_sensors_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_sensors_example.cpp): This application creates a `SensorCapture` object and displays on the command console the values of camera sensors acquired at full rate.
* [zed_open_capture_sync_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_sync_example.cpp): This application creates a `VideoCapture` and a `SensorCapture` object, initialize the camera/sensors synchronization and displays on screen the video stream with the synchronized IMU data.
* [zed_open_capture_depth_example](https://github.com/stereolabs/zed-open-capture/blob/master/examples/zed_oc_depth_example.cpp): This application captures video frames, extracts the depth map and the point cloud using only the CPU, and displays the result with the processing time. The point cloud is shown in a window that does not need OpenGL: drag with the left mouse button to rotate the view, with the right button or the wheel to zoom, double click to restore the view of the camera. See [Depth settings](#depth-settings).
* [zed_open_capture_depth_tune_stereo](https://github.com/stereolabs/zed-open-capture/blob/master/examples/tools/zed_oc_tune_stereo_sgbm.cpp): This application captures the first available stereo frames and provides GUI Controls to tune the disparity map results and save them to be used in the `zed_open_capture_depth_example` example
* [zed_open_capture_benchmark](https://github.com/stereolabs/zed-open-capture/blob/master/examples/tools/zed_oc_benchmark.cpp): This application measures the performance of image, sensors and depth acquisition on your computer. It does not need a display.

To run the examples, open a terminal console and enter one of the following commands:

```bash
zed_open_capture_video_example
zed_open_capture_multicam_video_example
zed_open_capture_control_example
zed_open_capture_rectify_example
zed_open_capture_sensors_example
zed_open_capture_sync_example
zed_open_capture_depth_example
zed_open_capture_depth_tune_stereo
zed_open_capture_benchmark
```

**Note:** OpenCV is used in the examples for controls, display, and depth extraction.

## Performance

Performance measured on a **Raspberry Pi 4** (Model B, 8 GB, Raspberry Pi OS 13 64 bit, OpenCV 4.10) with a ZED 2 on a USB 3.0 port, with the tool `zed_open_capture_benchmark`. The desktop was running, with a remote desktop application that used about one CPU core. The complete results, also for NVIDIA Jetson AGX Thor, are in the [benchmarks page](./doc/api_doc/benchmarks.md) of the documentation.

### Image retrieval

| Resolution | Requested FPS | Received FPS | Lost frames | Frame period 1% / 99% [ms] | Latency [ms] | CPU | Memory [MB] | Open [s] |
|---|---|---|---|---|---|---|---|---|
| HD2K (4416x1242) | 15 | 13.0 | 13.9% | 66.14 / 198.96 | 59.7 | 9% | 120 | 0.27 |
| HD1080 (3840x1080) | 30 | 23.8 | 20.3% | 33.26 / 133.73 | 38.7 | 16% | 102 | 0.25 |
| HD720 (2560x720) | 60 | 55.5 | 7.5% | 16.45 / 49.63 | 17.0 | 16% | 72 | 0.21 |
| HD720 (2560x720) | 30 | 28.8 | 4.0% | 33.17 / 66.77 | 31.1 | 8% | 72 | 0.21 |
| VGA (1344x376) | 100 | 99.2 | 0.6% | 9.87 / 10.21 | 6.6 | 9% | 61 | 0.19 |
| VGA (1344x376) | 30 | 29.9 | 0.3% | 33.23 / 33.57 | 19.9 | 3% | 61 | 0.19 |

At VGA resolution all the frames are received. At higher resolutions the USB host of the Raspberry Pi 4 loses part of the data sent by the camera and the incomplete frames are discarded: the same happens reading the camera with `v4l2-ctl`, without the library. *Latency* is the time from the start of the frame to its availability; *CPU* is 100% for one core fully used.

### Sensors retrieval

| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1% / 99% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |
|---|---|---|---|---|---|---|---|---|---|
| none | 396.3 | 2 | 2.522 | 2.286 / 2.798 | 0.74 | 49.4 | 24.9 | - | 10% |
| HD2K @ 15 FPS | 396.3 | 2 | 2.522 | 2.325 / 2.798 | 0.88 | 49.4 | 24.7 | 98% | 21% |
| HD1080 @ 30 FPS | 393.2 | 34 | 2.522 | 2.248 / 3.113 | 0.96 | 48.8 | 24.4 | 100% | 29% |
| HD720 @ 60 FPS | 395.0 | 15 | 2.522 | 2.208 / 2.916 | 0.90 | 48.7 | 24.9 | 100% | 28% |
| HD720 @ 30 FPS | 395.3 | 13 | 2.522 | 2.207 / 2.916 | 0.78 | 49.1 | 24.6 | 99% | 18% |
| VGA @ 100 FPS | 395.6 | 9 | 2.522 | 2.365 / 2.799 | 0.83 | 49.2 | 24.7 | 100% | 20% |
| VGA @ 30 FPS | 396.3 | 2 | 2.522 | 2.246 / 2.838 | 0.79 | 49.2 | 24.9 | 100% | 13% |

The IMU is received at full rate with every video mode (this ZED 2 provides 396 samples per second), with less than 1% of lost samples. *Synced frames* are the frames that received the timestamp of the IMU sample acquired with their hardware sync signal.

### Depth generation

| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |
|---|---|---|---|---|---|---|---|---|---|---|
| HD2K @ 15 FPS | 2208x1242 | 9.8 | 44.8 | 5048.0 | 17.6 | 5120.1 | 0.2 | 56% | 219% | 656 |
| HD2K @ 15 FPS | 1104x621 | 12.7 | 14.1 | 1068.7 | 5.4 | 1100.9 | 0.9 | 70% | 213% | 242 |
| HD1080 @ 30 FPS | 1920x1080 | 7.7 | 49.0 | 3633.9 | 15.1 | 3705.7 | 0.3 | 55% | 224% | 403 |
| HD1080 @ 30 FPS | 960x540 | 10.5 | 10.8 | 741.7 | 4.0 | 767.0 | 1.3 | 71% | 215% | 243 |
| HD720 @ 30 FPS | 1280x720 | 4.0 | 19.2 | 1518.4 | 6.3 | 1547.8 | 0.6 | 68% | 212% | 314 |
| HD720 @ 30 FPS | 640x360 | 5.2 | 6.0 | 294.2 | 2.1 | 307.5 | 3.2 | 69% | 211% | 187 |
| VGA @ 30 FPS | 672x376 | 1.0 | 6.3 | 299.1 | 1.9 | 308.3 | 3.2 | 68% | 228% | 151 |
| VGA @ 30 FPS | 336x188 | 1.6 | 3.0 | 37.8 | 0.5 | 42.9 | 22.9 | 62% | 212% | 113 |

For each resolution the first row is the depth map at full size (`matchScale` 1.0) and the second row at half size (`matchScale` 0.5, the default). On a Raspberry Pi 4 use the VGA resolution with the default settings to get the depth map in real time (about 23 FPS), or a 640x360 depth map at about 3 FPS when more detail is required.

### Other values

| | Raspberry Pi 4 | Jetson AGX Thor |
|---|---|---|
| Time to open a camera | 0.2 - 0.3 s | 0.2 s |
| Memory used grabbing frames, VGA / HD2K | 61 MB / 120 MB | 22 MB / 81 MB |
| IMU latency (acquisition to application) | 0.7 - 1.0 ms | less than 0.5 ms |
| Distance between frame timestamp and hardware sync signal, without synchronization | 0.7 ms (median), 5 ms (99%) | 1 ms (median), 2 ms (99%) |
| Build time of library, examples and tests | about 2 minutes | not measured |
| Depth map 640x360 from HD720 | 3.2 FPS | 26 FPS (30 FPS with `taskset`, see the benchmarks page) |

Run `zed_open_capture_benchmark` to get the same tables for your computer:

```bash
zed_open_capture_benchmark                 # all the measures
zed_open_capture_benchmark --depth         # only the selected measures: --video, --sensors, --depth
zed_open_capture_benchmark --duration 20   # seconds for each measure, default 5
```

## Depth settings

The depth example and the benchmark use the Semi-Global Block Matching algorithm of OpenCV on the CPU. The parameters are stored in `~/zed/settings/zed_oc_stereo.yaml`, created at the first start of `zed_open_capture_depth_example` and modified by `zed_open_capture_depth_tune_stereo`. The default values have been selected comparing the results on indoor scenes with a ZED Mini and a ZED 2: compared to the previous values the pixels with a wrong depth are less than half (from 15% to 5% in the most difficult scene) and the noise of the depth of a static scene is 2 to 3 times lower.

| Parameter | Default | Effect |
|---|---|---|
| `matchScale` | 0.5 | Size of the depth map relative to the camera image. 1.0 gives more detail but it is 4 to 8 times slower |
| `numDisparities` | 96 | Disparity range in pixels of the depth map. A higher value measures closer objects (minimum distance = focal x baseline / numDisparities) and it is proportionally slower |
| `blockSize` | 5 | Size of the matched block. Higher values give a smoother and more dense depth map with less detail |
| `mode` | 2 | 2 (`MODE_SGBM_3WAY`) is the fastest on a Raspberry Pi 4. 3 (`MODE_HH4`) gives fewer wrong values on textured scenes, but more on repetitive patterns: it is as fast as mode 2 on a computer with many cores and 35-60% slower on a Raspberry Pi 4. 1 (`MODE_HH`) gives the best result (up to 60% less wrong values in our tests) but it is about 6 times slower |
| `disp12MaxDiff` | 1 | Maximum difference in pixels accepted by the left-right consistency check. Higher values give a more dense depth map with more wrong values |
| `uniquenessRatio` | 5 | Higher values remove uncertain matches |
| `speckleWindowSize`, `speckleRange` | 255, 1 | Removal of small isolated regions |
| `refineRectification` | 1 | Measures and corrects the vertical misalignment of the two images while running. A camera can show a misalignment of a few pixels after a shock or far from the calibration temperature |
| `medianSize` | 0 | Median filter of the disparity map: 0 (disabled), 3 or 5 |
| `minDepth_mm`, `maxDepth_mm` | 300, 10000 | Range of the valid depth values |

## Documentation

The API is documented in the Include.h files. It is also generated as a Doxygen for simpler navigation: https://stereolabs.github.io/zed-open-capture

You can also generate the documentation locally in HTML format (with Doxygen) using the commands below. Access the docs by opening `doc/html/index.html` in your web browser.

```bash
sudo apt-get install -y doxygen # if not previously installed
cd doc
./generate_doc.sh
```

## Coordinates system

The coordinate system is only used for sensor data. The given IMU and Magnetometer data are expressed in the RAW coordinate system as shown below

![Coordinates](./images/imu_axis.jpg)

## Known issues

### USB 2.0 connection

A camera connected to a USB 2.0 port, or with a USB 2.0 cable, only provides a few low resolution modes at a reduced frame rate. In this case `initializeVideo()` fails for the other resolutions and the error message reports the USB 2.0 connection. `VideoCapture::getDeviceList()` and `zed_open_capture_benchmark` show the speed of the connection of each camera.

### Lost frames on Raspberry Pi 4

On a Raspberry Pi 4 part of the frames are lost at resolutions higher than VGA, because the USB host cannot always receive all the data sent by the camera (see [Performance](#performance)). Lost frames can be detected from the gaps of `Frame::frame_id` or of the frame timestamps. Use the VGA resolution, or HD720 at 30 FPS, when all the frames are required.

## Related

* [Stereolabs documentation](https://docs.stereolabs.com/docs)
* [ZED USB 3 cameras: ZED 2i, ZED 2 and ZED Mini](https://docs.stereolabs.com/docs/products/cameras/zed)
* [ZED SDK](https://docs.stereolabs.com/docs/development/zed-sdk)

## License

This library is licensed under the MIT License.

## Support

If you need assistance go to our Community site at https://community.stereolabs.com/

