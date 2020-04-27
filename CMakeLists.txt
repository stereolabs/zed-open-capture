cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME zed_driver)

project(${PROJECT_NAME} CXX)

############################################################################
# Flags

# if CMAKE_BUILD_TYPE is not specified, take 'Release' as default
set(${PROJECT_NAME}_CXX_FLAGS "-std=c++11")
set(CMAKE_CXX_FLAGS "${${PROJECT_NAME}_CXX_FLAGS} -Wall -pedantic -g")

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message("Debug mode")
    SET(CMAKE_DEBUG_POSTFIX  "d")
else()
    message("Release mode")
endif()

############################################################################
# Options
option(${PROJECT_NAME}_BUILD_VIDEO      "Build the ZED Driver Video Modules (only for Linux)"   ON)
option(${PROJECT_NAME}_BUILD_SENSORS    "Build the ZED Driver Sensors Modules"                  ON)
option(${PROJECT_NAME}_BUILD_EXAMPLES   "Build the ZED Driver examples"                         ON)


############################################################################
# Sources
set(SRC_VIDEO
    ${CMAKE_HOME_DIRECTORY}/src/videocapture.cpp
)

set(SRC_SENSORS
    ${CMAKE_HOME_DIRECTORY}/src/sensorcapture.cpp
)

############################################################################
# Includes
set(HEADERS_VIDEO
    # Base
    ${CMAKE_HOME_DIRECTORY}/include/videocapture.hpp
    
    # Defines
    ${CMAKE_HOME_DIRECTORY}/include/defines.hpp
    ${CMAKE_HOME_DIRECTORY}/include/videocapture_def.hpp
)

set(HEADERS_SENSORS
    # Base
    ${CMAKE_HOME_DIRECTORY}/include/sensorcapture.hpp

    # Defines
    ${CMAKE_HOME_DIRECTORY}/include/defines.hpp
    ${CMAKE_HOME_DIRECTORY}/include/sensorcapture_def.hpp
)

include_directories(
    ${CMAKE_HOME_DIRECTORY}/include
)

############################################################################
# Required external libraries

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")

find_package(LIBUSB 1 REQUIRED)
message(STATUS "libusb: include dir at ${LibUSB_INCLUDE_DIRS}")
message(STATUS "libusb: libraries at ${LibUSB_LIBRARIES}")
include_directories(${LibUSB_INCLUDE_DIRS})

find_package(HIDAPI REQUIRED)
message(STATUS "libhidapi: include dir at ${HIDAPI_INCLUDE_DIRS}")
message(STATUS "libhidapi: libraries at ${HIDAPI_LIBRARIES}")
include_directories(${HIDAPI_INCLUDE_DIRS})

############################################################################
# Generate libraries
if(${PROJECT_NAME}_BUILD_VIDEO)
    add_library(${PROJECT_NAME}_video SHARED ${SRC_VIDEO} )
    target_link_libraries( ${PROJECT_NAME}_video
        ${LibUSB_LIBRARIES}
    )
endif()

if(${PROJECT_NAME}_BUILD_SENSORS)
    add_library(${PROJECT_NAME}_sensors SHARED ${SRC_SENSORS} )
    target_link_libraries(${PROJECT_NAME}_sensors
        ${LibUSB_LIBRARIES}
        ${HIDAPI_LIBRARIES}
        -lpthread
    )
endif()

############################################################################
# Generate examples
if(${PROJECT_NAME}_BUILD_EXAMPLES)

    if(${PROJECT_NAME}_BUILD_VIDEO)
        ##### Video Example
        find_package(OpenCV REQUIRED)
        include_directories(${OpenCV_INCLUDE_DIRS})

        add_executable(${PROJECT_NAME}_video_example "examples/zed_drv_video_example.cpp")
        target_link_libraries(${PROJECT_NAME}_video_example
          PUBLIC
          ${PROJECT_NAME}_video
          ${OpenCV_LIBS}
        )
    endif()

    if(${PROJECT_NAME}_BUILD_SENSORS)
        ##### Sensors Example
        add_executable(${PROJECT_NAME}_sensors_example "examples/zed_drv_sensors_example.cpp")
        target_link_libraries(${PROJECT_NAME}_sensors_example
          PUBLIC
          ${PROJECT_NAME}_sensors
        )
    endif()

endif()
