# Changelog

v1.0.0 - in development
-----------------------

This release reviews the whole library. Some functions changed to report errors and to make the data safe to use from the
application: see **API changes** at the end of this section before updating an existing application.

### New Features

* Video
  * Add `VideoCapture::getDeviceList()` to list the connected cameras with model, serial number and USB speed
  * Add `VideoCapture::initializeVideoBySerialNumber()` to open a camera by serial number
  * Add `VideoCapture::close()` and `VideoCapture::isOpened()`. A disconnected camera is now detected
  * Add `VideoCapture::getFps()` and `VideoCapture::getCameraModel()`
* Sensors
  * Add `SensorCapture::getIMUData()`: it returns all the IMU samples received after the previous call, so no sample is lost when the application is slower than the sensor (800 Hz on ZED Mini)
  * Add `SensorCapture::close()` and `SensorCapture::isOpened()`. A disconnected camera is now detected
  * A warning is printed when the environmental and temperature sensors of the camera stop providing new data, a condition that `resetSensorModule()` recovers
* Examples and tools
  * Add `zed_open_capture_benchmark`: it measures image, sensors and depth performance on the host and prints the results as Markdown tables. It does not need a display
  * The depth example automatically corrects the vertical misalignment of the two images that a camera can show after a shock or far from the calibration temperature
* Build
  * Add hardware tests based on GoogleTest (`-DBUILD_TESTS=ON`). They run on every connected camera, without a display, and check video, sensors and video/sensors synchronization
  * `make install` now installs a CMake package (`find_package(zed_open_capture)`, target `zed_open_capture::zed_open_capture`) and a pkg-config file
  * Add the `ZED_OC_SANITIZER` build option to build with the address or thread sanitizer
  * Add the `ZED_OC_WARNINGS_AS_ERRORS` build option
  * Add automatic checks on GitHub for each pull request: build with GCC and Clang on x86_64 and ARM64 in every configuration, use of the installed package, tests without a camera also with sanitizers, documentation and static analysis

### Improvements

* Timestamps
  * Frames and sensors data now use the same clock, also without enabling the synchronization: the system time read when the first timestamp is needed, then advanced with the monotonic clock
  * IMU timestamps follow the host clock from the first sample. Before, they started with an error up to tens of seconds and, without a video stream, they drifted up to 1% (ZED 2i)
  * With `enableSensorSync()` each frame takes the timestamp of the IMU sample acquired with its hardware sync signal. The alignment is immediate: before, it took about 10 seconds to converge
* Video
  * The frame returned by `getLastFrame()` is no longer overwritten while the application uses it
  * `frame_id` is the frame counter of the driver: a gap reveals lost frames
  * The automatic camera search only tries Stereolabs devices and it is not limited to the first 64 video devices
  * A camera connected in USB 2.0 mode is reported in the error messages, and the frame rate really in use is available with `getFps()`
  * Frames are received with 4 driver buffers and without polling
* Sensors
  * Data are no longer lost while the keep-alive request waits for the camera (up to 1 second of data, from time to time)
  * `initializeSensors()` can be called again on the same object to connect to another camera
  * Waiting for new data no longer uses polling
* Depth example
  * Depth is extracted from grayscale images reduced before the rectification, and the color image is only converted for display: less processing for the same depth map
  * New default matching parameters: on the test scenes the pixels with a wrong depth are less than half (from 15% to 5% in the most difficult scene), and the noise of the depth of a static scene is 2 to 3 times lower
  * The disparity is no longer used where the matching failed, and the distance range is applied to the depth map
  * The OpenCV Transparent API (OpenCL) is no longer used: the stereo matching of OpenCV does not support it
  * New parameters `matchScale`, `medianSize`, `refineRectification`, `P1` and `P2` in the stereo parameters file
* Build
  * The library and the examples are now built with the C++17 standard. CMake v3.16 or newer is required
  * Compiler warnings are enabled for the library and the examples, that now build without warnings
  * The video module no longer requires libusb: `libusb-1.0-0-dev` is not needed to build
  * Applications no longer need the hidapi headers, and the headers no longer require the `VIDEO_MOD_AVAILABLE` and `SENSORS_MOD_AVAILABLE` definitions
  * The library is no longer tuned for the Raspberry Pi 4 CPU on every ARM computer
  * The `zed_open_capture_depth_example` application is now installed with the other examples
  * The script that installs the udev rules can be started from any folder and applies the rules without replugging the camera

### Bug Fixes

* Video
  * Fix the frames modified while the application was reading them (image tearing)
  * Fix file descriptors and memory left allocated when opening a camera fails, e.g. during the automatic search with a camera already in use
  * Fix `initializeVideo()` that kept configuring the camera when the stream could not be started
  * Fix a buffer overflow in the commands that control the LED
  * Fix `getAECAGC()` that reported the automatic exposure as active when the camera did not reply
  * Fix the automatic exposure setting written with wrong values after a failed read
  * Fix an invalid memory access with a resolution value out of range
  * Fix `getGamma()`: it now returns the value really used by the camera. `resetGamma()` restores the factory gamma
* Sensors
  * Fix `resetSensorModule()` and `resetVideoModule()` that never returned when a ZED Mini was listed before the ZED 2 or ZED 2i to reset
  * Fix the crash when `initializeSensors()` was called to connect to a second camera
  * Fix the application blocked forever when a `SensorCapture` or `VideoCapture` object was closed right after being opened
  * Fix `initializeSensors()` that returned `true` when the data stream could not be started
  * Fix the invalid memory access when the `SensorCapture` object used for the synchronization was destroyed before the `VideoCapture` object
  * Fix the loss of precision of the IMU timestamps
  * Fix the validity of the temperature of the right image sensor, that was not checked
  * Fix the application closed by the library (`exit`) with a camera reporting an unexpected serial number
  * Fix a memory leak at each log message
* Examples
  * Fix the depth map of the depth example: the disparity was increased by one pixel before the conversion to depth
  * Fix the sensors example that lost one third of the IMU samples of a ZED Mini
  * Fix the multi-camera example that only worked with the cameras on `/dev/video0` and `/dev/video2`
  * Fix the examples that continued with an empty calibration file after a failed download, and the crash without the `HOME` environment variable
* Build
  * Fix the build flags: the C++ standard and the warning flags set in `CMakeLists.txt` were silently ignored
  * Fix the configuration error of other CMake checks caused by the HIDAPI search module
  * Fix the build with `-DDEBUG_CAM_REG=ON`: the `zed_open_capture_video_reg_log` example did not compile

### API changes

* The functions that change a camera setting (`setBrightness`, `setGain`, `setExposure`, `setAECAGC`, `setGamma`, `setLEDstatus`, the `reset...` functions, ...) now return `true` if the camera accepted the setting. `setAECAGC`, `setLEDstatus`, `getLEDstatus` and `toggleLED` returned an `int` with `0` for success
* `getLastFrame()` and the `getLast...Data()` functions return data that stay valid and unchanged until the next call of the same function
* `Frame::frame_id` is the counter of the driver and no longer starts from 1
* The timeout of the `getLast...Data()` functions is in microseconds, as the name of the parameter says. The magnetometer, environment and temperature functions waited longer than requested
* `enableSensorSync()` must be called after opening both the camera and the sensors, and again if one of them is opened again
* The functions used internally by the synchronization (`setReadyToSync`, `updateTimestampOffset`, `setStartTimestamp`, `setVideoPtr`) and the log macros (`INFO_OUT`, `WARNING_OUT`, `ERROR_OUT`) are no longer public
* `setColorBars()`, `resetAGCAECregisters()` and the register logging functions are always available. `DEBUG_CAM_REG` only builds the related example

v0.6.0 - 2022 11 04
-------------------
* Add multi-camera video example

v0.5.0 - 2021 09 10
-------------------
* Add example to extract disparity map, depth map and point cloud using OpenCV and T-API (OpenCL) 
  `cv::StereoSGBM` algorithm based on the paper "Heiko Hirschmuller. Stereo processing by semiglobal matching and mutual information. Pattern Analysis and Machine Intelligence, IEEE Transactions on, 30(2):328–341, 2008."
* Add example to tune disparity map creation
* Add tool to load/save StereoSGBM depth matching parameters

v0.4.1 - 2021 05 28
-------------------
* Fix udev rules to access the sensors module of the new ZED2i

v0.4.0 - 2021 05 28
-------------------
* Replace `CMAKE_HOME_DIRECTORY` with `PROJECT_SOURCE_DIR` in `CMakeLists.txt` to be able to import the library as subproject. Thx pieniacy
* Add support for the new "ZED 2i" camera model
* Improve AEC/AGC ROI support


v0.3.0 - 2020 03 01
-------------------
* Add AEC/AGC ROI support
* Update the "control example" with AEC/AGC ROI support, drawing ROIs on display image

v0.2.1 - 2020 02 08
-------------------
* Fix FPS issue caused by wrong default timeout in `getLastFrame` function (see #10)

v0.2 - 2020 06 10
-----------------
* Fix issue downloading camera settings for the rectification example
* Documentation refactoring
* New "sl_oc::video" namespace
* New "sl_oc::sensors" namespace
* New "sl_oc::sensors::data" namespace
* New "sl_oc::sensors::usb" namespace
* Sensors data and image data are now returned as reference instead of pointer
* Improved sensors data validity field

v0.1 - 2020 06 04
-----------------
* Video grabber 
* Video grabber controls
* Sensors data grabber
* Video example
* Sensors example
* Video and Sensors synchronization example
