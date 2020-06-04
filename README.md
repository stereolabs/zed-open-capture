# ZED Driver

The ZED Driver allows the low level control of ZED, ZED Mini and ZED 2 camera. The driver provides methods to access video frames, to control the video parameters and to acquire data from the internal sensors (only ZED Mini and ZED2). A synchronization mechanism is provided to get the correct sensor data associated to each video frame.

## Installation

### Prerequisites

 * A Stereolabs camera: [ZED](https://www.stereolabs.com/zed/), [ZED Mini](https://www.stereolabs.com/zed-mini/), [ZED2](https://www.stereolabs.com/zed-2/)
 * Linux OS [Tested on Ubuntu 18.04 and Ubuntu 20.04]
 * GCC compiler [at least v7.5]
 * CMake build system [at least v3.1] 
 * HIDAPI and LIBUSB Libraries for USB communication
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
    
    $ git clone https://github.com/stereolabs/zed-driver.git
    $ cd zed-driver

### Compile

#### Option 1

Compile the library and the examples

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j4

#### Option 2

Compile only the library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_EXAMPLES=OFF 
    $ make -j4

#### Option 3

Compile only the video library with the video example

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_SENSORS=OFF
    $ make -j4 

#### Option 4

Compile only the video library

    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_SENSORS=OFF -DBUILD_EXAMPLES=OFF
    $ make -j4

#### Option 5

Compile only the sensors library with the sensors example
    
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_VIDEO=OFF
    $ make -j4

#### Option 6

Compile only the sensors library
    
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_VIDEO=OFF -DBUILD_EXAMPLES=OFF
    $ make -j4

