## Installation

### Prerequisites

 * A Stereolabs camera: [ZED](https://www.stereolabs.com/zed/), [ZED Mini](https://www.stereolabs.com/zed-mini/), [ZED2](https://www.stereolabs.com/zed-2/)
 * Linux OS [Tested on Ubuntu 18.04 and Ubuntu 20.04]
 * GCC compiler [at least v7.5]
 * CMake build system [at least v3.1] 
 * HIDAPI and LBIUSB Libraries for USB communication
 * OpenCV [at least v3.4. Required only by examples]
 * git [if installing from source]

#### Install prerequisites

* Install GCC compiler and build tools

    `$ sudo apt install build-essential`

* Install CMake build system

    `$ sudo apt install cmake`

* Install HIDAPI and LIBUSB libraries:

    `$ sudo apt install libusb-1.0-0-dev libhidapi-libusb0 libhidapi-dev`

* Install OpenCV to compile the examples

    `$ sudo apt install opencv-dev`

### Install the udev rule 
To be able to access the USB you must install the udev rule contained in the `udev` folder:

    $ cd udev
    $ bash install_udev_rule.sh
    $ cd ..
    $ udevadm trigger

### Clone the repository
    
    $ git clone https://github.com/stereolabs/zed-open-capture.git
    $ cd zed-open-capture

### Compile

#### Option 1

Compile the library and the examples

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ sudo make install
    $ sudo ldconfig

#### Option 2

Compile only the library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_EXAMPLES=OFF 
    $ make
    $ sudo make install
    $ sudo ldconfig

#### Option 3

Compile only the video library with the video example

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_SENSORS=OFF
    $ make 
    $ sudo make install
    $ sudo ldconfig

#### Option 4

Compile only the video library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_SENSORS=OFF -DBUILD_EXAMPLES=OFF
    $ make
    $ sudo make install
    $ sudo ldconfig

#### Option 5

Compile only the sensors library with the sensors example
    
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_VIDEO=OFF
    $ make
    $ sudo make install
    $ sudo ldconfig

#### Option 6

Compile only the sensors library
    
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_VIDEO=OFF -DBUILD_EXAMPLES=OFF
    $ make
    $ sudo make install
    $ sudo ldconfig

### Test the library

If you compiled and installed the examples you can test that everything is correctly working:

 1. Connect the camera to an USB3 port
 2. Test the video functionalities: `$ zed_open_capture_video_example`
 3. If you own a ZED Mini or a ZED2 you can also test the sensors: `$ zed_open_capture_sensors_example`
 4. Finally, if you own a ZED Mini or a ZED2, you can test the video/sensors synchronization: `$ zed_open_capture_sync_example`


