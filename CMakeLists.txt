cmake_minimum_required(VERSION 3.16)

project(zed_open_capture VERSION 1.0.0 LANGUAGES CXX)

############################################################################
# Flags

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# if CMAKE_BUILD_TYPE is not specified, take 'Release' as default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_DEBUG_POSTFIX "d")
endif()

message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")

############################################################################
# Options
option(BUILD_VIDEO      "Build the ZED Open Capture Video Modules (only for Linux)"   ON)
option(BUILD_SENSORS    "Build the ZED Open Capture Sensors Modules"                  ON)
option(BUILD_EXAMPLES   "Build the ZED Open Capture examples"                         ON)
option(BUILD_TESTS      "Build the hardware tests (they need a connected camera)"     OFF)
option(DEBUG_CAM_REG    "Add functions to log the values of the registers of camera"  OFF)
option(ZED_OC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"                  OFF)
set(ZED_OC_SANITIZER "" CACHE STRING "Build with a sanitizer: empty, 'address' (with undefined) or 'thread'")
set_property(CACHE ZED_OC_SANITIZER PROPERTY STRINGS "" address thread)

add_compile_options(-Wall -Wextra -pedantic)
if(ZED_OC_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
endif()

if(ZED_OC_SANITIZER STREQUAL "address")
    message(STATUS "Sanitizer: address + undefined")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
elseif(ZED_OC_SANITIZER STREQUAL "thread")
    message(STATUS "Sanitizer: thread")
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
elseif(ZED_OC_SANITIZER)
    message(FATAL_ERROR "ZED_OC_SANITIZER must be empty, 'address' or 'thread'")
endif()

############################################################################
# Sources
set(SRC_VIDEO
    ${PROJECT_SOURCE_DIR}/src/videocapture.cpp
)

set(SRC_SENSORS
    ${PROJECT_SOURCE_DIR}/src/sensorcapture.cpp
)

############################################################################
# Includes
set(HEADERS_VIDEO
    # Base
    ${PROJECT_SOURCE_DIR}/include/videocapture.hpp

    # Defines
    ${PROJECT_SOURCE_DIR}/include/defines.hpp
    ${PROJECT_SOURCE_DIR}/include/videocapture_def.hpp
)

set(HEADERS_SENSORS
    # Base
    ${PROJECT_SOURCE_DIR}/include/sensorcapture.hpp

    # Defines
    ${PROJECT_SOURCE_DIR}/include/defines.hpp
    ${PROJECT_SOURCE_DIR}/include/sensorcapture_def.hpp
)

############################################################################
# Required external libraries

list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake")
find_package(Threads REQUIRED)

############################################################################
# Generate libraries
set(SRC_FULL ${PROJECT_SOURCE_DIR}/src/common.cpp)
set(HDR_FULL "")
set(DEP_LIBS "")
set(DEP_INCLUDES "")
set(MOD_DEFINITIONS "")

if(DEBUG_CAM_REG)
    message(STATUS "Registers logging available")
    list(APPEND MOD_DEFINITIONS SENSOR_LOG_AVAILABLE)
endif()

if(BUILD_SENSORS)
    message(STATUS "Sensors module available")
    list(APPEND MOD_DEFINITIONS SENSORS_MOD_AVAILABLE)

    find_package(HIDAPI REQUIRED)
    message(STATUS "libhidapi: include dir at ${HIDAPI_INCLUDE_DIRS}")
    message(STATUS "libhidapi: libraries at ${HIDAPI_LIBRARIES}")

    list(APPEND SRC_FULL ${SRC_SENSORS})
    list(APPEND HDR_FULL ${HEADERS_SENSORS})
    list(APPEND DEP_LIBS ${HIDAPI_LIBRARIES})
    list(APPEND DEP_INCLUDES ${HIDAPI_INCLUDE_DIRS})
endif()

if(BUILD_VIDEO)
    message(STATUS "Video module available")
    list(APPEND MOD_DEFINITIONS VIDEO_MOD_AVAILABLE)

    # The video module only needs the V4L2/UVC kernel interfaces
    list(APPEND SRC_FULL ${SRC_VIDEO})
    list(APPEND HDR_FULL ${HEADERS_VIDEO})
endif()

if(NOT BUILD_VIDEO AND NOT BUILD_SENSORS)
    message(FATAL_ERROR "Nothing to build: enable BUILD_VIDEO and/or BUILD_SENSORS")
endif()

list(REMOVE_DUPLICATES HDR_FULL)

add_library(${PROJECT_NAME} SHARED ${SRC_FULL})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

include(GNUInstallDirs)
set(ZED_OC_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/zed-open-capture)

# The headers of hidapi are only needed to build the library
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${ZED_OC_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${DEP_INCLUDES}
)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${MOD_DEFINITIONS})
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads PRIVATE ${DEP_LIBS})

############################################################################
# Install rules
set_target_properties(${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${HDR_FULL}"
)
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${ZED_OC_INSTALL_INCLUDEDIR})

# CMake package: find_package(zed_open_capture) + target zed_open_capture::zed_open_capture
include(CMakePackageConfigHelpers)
set(ZED_OC_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${ZED_OC_INSTALL_CMAKEDIR})

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${ZED_OC_INSTALL_CMAKEDIR})

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${ZED_OC_INSTALL_CMAKEDIR})

# pkg-config file
string(REPLACE ";" " -D" ZED_OC_PC_DEFINITIONS "${MOD_DEFINITIONS}")
if(ZED_OC_PC_DEFINITIONS)
    set(ZED_OC_PC_DEFINITIONS "-D${ZED_OC_PC_DEFINITIONS}")
endif()
configure_file(${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}.pc.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

############################################################################
# Generate examples

# Adds an example executable linked to the library and installs it
function(zed_oc_add_example target source)
    add_executable(${target} "${PROJECT_SOURCE_DIR}/examples/${source}")
    set_target_properties(${target} PROPERTIES PREFIX "")
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/examples/include)
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME} ${ARGN})
    # Allows to start the application by double clicking the icon on Ubuntu
    target_link_options(${target} PRIVATE -no-pie)
    install(TARGETS ${target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()

if(BUILD_EXAMPLES)
    # Used by the depth example to select a lighter default resolution
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|armv7l")
        add_compile_definitions(EMBEDDED_ARM)
    endif()

    if(BUILD_VIDEO)
        message(STATUS "Video examples available")

        ##### OpenCV is required
        find_package(OpenCV REQUIRED)
        include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})

        zed_oc_add_example(${PROJECT_NAME}_video_example            zed_oc_video_example.cpp            ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_multicam_video_example   zed_oc_multi_video_example.cpp      ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_control_example          zed_oc_control_example.cpp          ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_rectify_example          zed_oc_rectify_example.cpp          ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_depth_example            zed_oc_depth_example.cpp            ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_depth_tune_stereo        tools/zed_oc_tune_stereo_sgbm.cpp   ${OpenCV_LIBS})
        zed_oc_add_example(${PROJECT_NAME}_benchmark                tools/zed_oc_benchmark.cpp          ${OpenCV_LIBS})

        if(DEBUG_CAM_REG)
            ##### Video with AEG/AGC registers log
            zed_oc_add_example(${PROJECT_NAME}_video_reg_log zed_oc_video_reg_log.cpp ${OpenCV_LIBS})
        endif()
    endif()

    if(BUILD_SENSORS)
        message(STATUS "Sensors example available")
        zed_oc_add_example(${PROJECT_NAME}_sensors_example zed_oc_sensors_example.cpp)
    endif()

    if(BUILD_VIDEO AND BUILD_SENSORS)
        message(STATUS "Video/Sensors sync example available")
        zed_oc_add_example(${PROJECT_NAME}_sync_example zed_oc_sync_example.cpp ${OpenCV_LIBS})
    endif()
endif()

############################################################################
# Hardware tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
