# Changelog

v0.4.0 - 2021 05 28
-------------------
* Replace `CMAKE_HOME_DIRECTORY` with `PROJECT_SOURCE_DIR` in `CMakeLists.txt` to be able to import the library as subproject. Thx @pieniacy
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
