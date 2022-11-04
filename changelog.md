# Changelog

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
