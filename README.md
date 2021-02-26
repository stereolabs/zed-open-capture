<h1 align="center">
  Open Capture Camera API
</h1>

<h4 align="center">A platform-agnostic camera and sensor capture API for the ZED stereo camera family</h4>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#build">Build</a> •
  <a href="#run">Run</a> • 
  <a href="#documentation">Documentation</a> •
  <a href="#running-the-examples">Examples</a> •
  <a href="#related">Related</a> •
  <a href="#license">License</a>
</p>
<br>


## Key Features
 * Open source C++ capture library compatible with C++11 standard
 * Video Capture
    - YUV 4:2:2 data format
    - Camera controls
 * Sensor Data Capture
    - 6-DOF IMU (3-DOF accelerometer + 3-DOF gyroscope)
    - 3-DOF Magnetometer
    - Barometer
    - Sensors temperature
 * Sensors/video Synchronization
 * Portable
    - Tested on Linux
    - Tested on x64, ARM
 * Small Size
    - ~100KB library size
    - libusb, hidapi dependencies
 * Complete set of examples
    - Video capture
    - Camera control
    - Stereo rectification
    - IMU, magnetometer and barometer data capture
    - Video and sensors synchronization

## Description

The ZED Open Capture is a multi-platform, open-source C++ library for low-level camera and sensor capture for the ZED stereo camera family. It doesn't require CUDA and therefore can be used on many desktop and embedded platforms.

The open-source library provides methods to access raw video frames, calibration data, camera controls and raw data from the camera sensors (on ZED 2 and ZED Mini). A synchronization mechanism is provided to get the correct sensor data associated to a video frame.

**Note:** While in the ZED SDK all output data is calibrated and compensated, here the extracted raw data is not corrected by the camera and sensor calibration parameters. You can retrieve camera and sensor calibration data using the [ZED SDK](https://www.stereolabs.com/docs/video/camera-calibration/) to correct your camera data.

## Build

### Prerequisites

 * Stereo camera: [ZED 2](https://www.stereolabs.com/zed-2/), [ZED](https://www.stereolabs.com/zed/), [ZED Mini](https://www.stereolabs.com/zed-mini/)
 * Linux OS
 * GCC (v7.5+)
 * CMake (v3.1+)

### Install prerequisites

* Install GCC compiler and build tools

    `$ sudo apt install build-essential`

* Install CMake build system

    `$ sudo apt install cmake`

* Install HIDAPI and LIBUSB libraries:

    `$ sudo apt install libusb-1.0-0-dev libhidapi-libusb0 libhidapi-dev`

* Install OpenCV to build the examples (optional)

    `$ sudo apt install opencv-dev`

### Clone the repository

    $ git clone https://github.com/stereolabs/zed-open-capture.git
    $ cd zed-open-capture

### Add udev rule
Stereo cameras such as ZED 2 and ZED Mini have built-in sensors (e.g. IMU) that are identified as USB HID devices.
To be able to access the USB HID device, you must add a udev rule contained in the `udev` folder:

    $ cd udev
    $ bash install_udev_rule.sh
    $ cd ..

### Build

#### Build library and examples

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j$(nproc)

#### Build only the library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_EXAMPLES=OFF
    $ make -j$(nproc)

#### Build only the video capture library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_SENSORS=OFF -DBUILD_EXAMPLES=OFF
    $ make -j$(nproc)

#### Build only the sensor capture library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_VIDEO=OFF -DBUILD_EXAMPLES=OFF
    $ make -j$(nproc)

## Run

To install the library, go to the `build` folder and launch the following commands:

    $ sudo make install
    $ sudo ldconfig

### Get video data

Include the `videocapture.hpp` header, declare a `VideoCapture` object and retrieve a video frame (in YUV 4:2:2 format) with `getLastFrame()`:

    #include "videocapture.hpp"
    sl_oc::video::VideoCapture cap;
    cap.initializeVideo();
    const sl_oc::video::Frame frame = cap.getLastFrame();
    
### Get sensors data

Include the `SensorCapture` header, declare a `SensorCapture` object, get a list of available devices, initialize the first one and finally retrieve sensors data:

    #include "sensorcapture.hpp"
    sl_oc::sensors::SensorCapture sens;
    std::vector<int> devs = sens.getDeviceList();
    sens.initializeSensors( devs[0] );
    const sl_oc::sensors::data::Imu imuData = sens.getLastIMUData(5000);
    const sl_oc::sensors::data::Magnetometer magData = sens.getLastMagnetometerData(100);
    const sl_oc::sensors::data::Environment envData = sens.getLastEnvironmentData(100);
    const sl_oc::sensors::data::Temperature tempData = sens.getLastCameraTemperatureData(100);
    
## Running the examples

After installing the library and examples, you will have the following sample applications in your `build` directory:

* [zed_open_capture_video_example](https://github.com/stereolabs/zed-open-capture/blob/fix_doc/examples/zed_oc_video_example.cpp): This application captures and displays video frames from the camera.
* [zed_open_capture_control_example](https://github.com/stereolabs/zed-open-capture/blob/fix_doc/examples/zed_oc_control_example.cpp): This application captures and displays video frames from the camera and provides runtime control of camera parameters using keyboard shortcuts.
* [zed_open_capture_rectify_example](https://github.com/stereolabs/zed-open-capture/blob/fix_doc/examples/zed_oc_rectify_example.cpp): This application downloads factory stereo calibration parameters from Stereolabs server, performs stereo image rectification and displays original and rectified frames.
* [zed_open_capture_sensors_example](https://github.com/stereolabs/zed-open-capture/blob/fix_doc/examples/zed_oc_sensors_example.cpp): This application creates a `SensorCapture` object and displays on the command console the values of camera sensors acquired at full rate.
* [zed_open_capture_sync_example](https://github.com/stereolabs/zed-open-capture/blob/fix_doc/examples/zed_oc_sync_example.cpp): This application creates a `VideoCapture` and a `SensorCapture` object, initialize the camera/sensors synchronization and displays on screen the video stream with the synchronized IMU data.

To run the examples, open a terminal console and enter the following commands:

```
$ zed_open_capture_video_example
$ zed_open_capture_control_example
$ zed_open_capture_rectify_example
$ zed_open_capture_sensors_example
$ zed_open_capture_sync_example
```

**Note:** OpenCV is used in the examples for controls and display.


## Documentation

The API is documented in the Include.h files. It is also generated as a Doxygen for simpler navigation: https://stereolabs.github.io/zed-open-capture

You can also generate the documentation locally in HTML format (with Doxygen) using the commands below. Access the docs by opening `doc/html/index.html` in your web browser.

​
    $ sudo apt-get install -y doxygen # if not previously installed
    $ cd doc
    $ ./generate_doc.sh



## Coordinates system

The coordinate system is only used for sensors data. The given IMU and Magnetometer data are expressed in the RAW coordinate system as show below

![](./images/imu_axis.jpg)

## Related

- [Stereolabs](https://www.stereolabs.com)
- [ZED 2 multi-sensor camera](https://www.stereolabs.com/zed-2/)
- [ZED SDK](https://www.stereolabs.com/developers/)

## License

This library is licensed under the MIT License.

## Support
If you need assistance go to our Community site at https://community.stereolabs.com/
