cmake_minimum_required(VERSION 2.8)

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
option(${PROJECT_NAME}_BUILD_EXAMPLES "Build the ZED Driver examples" ON)

############################################################################
# Sources
set(SRC
    # Base
    ${CMAKE_HOME_DIRECTORY}/src/sensorcapture.cpp
    ${CMAKE_HOME_DIRECTORY}/src/videocapture.cpp )

############################################################################
# Includes
set(HEADERS
    # Base
    ${CMAKE_HOME_DIRECTORY}/include/sensorcapture.hpp
    ${CMAKE_HOME_DIRECTORY}/include/videocapture.hpp
    
    # Defines
    ${CMAKE_HOME_DIRECTORY}/include/defines.hpp
)

include_directories(
    ${CMAKE_HOME_DIRECTORY}/include
)

############################################################################
# Libraries

# LIBUSB 1.0
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libusb-1.0.cmake)
include_directories(${LIBUSB_1_INCLUDE_DIRS})

# LIBHIDAPI
include_directories(/usr/include/hidapi)
set(LIB_HIDAPI hidapi-libusb)

############################################################################
# Generate library
add_library(${PROJECT_NAME} SHARED ${SRC} )
target_link_libraries( ${PROJECT_NAME}
    ${LIBUSB_1_LIBRARY}
    ${LIB_HIDAPI}
    )

############################################################################
# Generate examples
if(${PROJECT_NAME}_BUILD_EXAMPLES)
  find_package(OpenCV REQUIRED)
  include_directories(${OpenCV_INCLUDE_DIRS})

  add_executable(${PROJECT_NAME}_example "examples/zed_drv_example.cpp")

  target_link_libraries(${PROJECT_NAME}_example
      PUBLIC
      ${PROJECT_NAME}
      ${OpenCV_LIBS})
endif()
