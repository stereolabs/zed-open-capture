cmake_minimum_required(VERSION 3.1)

set(PROJECT_NAME zed_open_capture)

project(${PROJECT_NAME} CXX)

############################################################################
# Flags

# if CMAKE_BUILD_TYPE is not specified, take 'Release' as default
set(${PROJECT_NAME}_CXX_FLAGS "-std=c++11")
set(CMAKE_CXX_FLAGS "${${PROJECT_NAME}_CXX_FLAGS} -Wall -pedantic -g")

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -flto") # File size optimization
    message("* Release mode")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message("* Debug mode")
    SET(CMAKE_DEBUG_POSTFIX  "d")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -flto") # File size optimization
    message("* Release mode")
endif()

############################################################################
# Options
option(BUILD_VIDEO      "Build the ZED Open Capture Video Modules (only for Linux)"   ON)
option(BUILD_SENSORS    "Build the ZED Open Capture Sensors Modules"                  ON)
option(BUILD_EXAMPLES   "Build the ZED Open Capture examples"                         ON)

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

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

############################################################################
# Required external libraries

list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake")

############################################################################
# Generate libraries
if(BUILD_SENSORS)
    message("* Sensors module available")
    add_definitions(-DSENSORS_MOD_AVAILABLE)

    find_package(HIDAPI REQUIRED)
    message(STATUS "libhidapi: include dir at ${HIDAPI_INCLUDE_DIRS}")
    message(STATUS "libhidapi: libraries at ${HIDAPI_LIBRARIES}")
    include_directories(${HIDAPI_INCLUDE_DIRS})

    set(SRC_FULL ${SRC_FULL} ${SRC_SENSORS})
    set(HDR_FULL ${HDR_FULL} ${HEADERS_SENSORS})
    set(DEP_LIBS ${DEP_LIBS}
        ${LibUSB_LIBRARIES}
        ${HIDAPI_LIBRARIES}
        pthread )
endif()

if(BUILD_VIDEO)
    message("* Video module available")
    add_definitions(-DVIDEO_MOD_AVAILABLE)

    find_package(LIBUSB 1 REQUIRED)
    message(STATUS "libusb: include dir at ${LibUSB_INCLUDE_DIRS}")
    message(STATUS "libusb: libraries at ${LibUSB_LIBRARIES}")
    include_directories(${LibUSB_INCLUDE_DIRS})
    set(SRC_FULL ${SRC_FULL} ${SRC_VIDEO})
    set(HDR_FULL ${HDR_FULL} ${HEADERS_VIDEO})
    set(DEP_LIBS ${DEP_LIBS}
         ${LibUSB_LIBRARIES}
         )

endif()

add_library(${PROJECT_NAME} SHARED ${SRC_FULL} )
target_link_libraries( ${PROJECT_NAME}  ${DEP_LIBS})

############################################################################
# Install rules
set_target_properties(${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${HDR_FULL}"
)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/zed-open-capture)


############################################################################
# Generate examples
if(BUILD_EXAMPLES)
    if(BUILD_VIDEO)
        message("* Video example available")

        ##### OpenCV is required
        find_package(OpenCV REQUIRED)
        include_directories(${OpenCV_INCLUDE_DIRS})

        ##### Video Example
        add_executable(${PROJECT_NAME}_video_example "${PROJECT_SOURCE_DIR}/examples/zed_oc_video_example.cpp")
        set_target_properties(${PROJECT_NAME}_video_example PROPERTIES PREFIX "")
        target_link_libraries(${PROJECT_NAME}_video_example
          ${PROJECT_NAME}
          ${OpenCV_LIBS}
        )
        install(TARGETS ${PROJECT_NAME}_video_example
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        )

        ##### Control Example
        add_executable(${PROJECT_NAME}_control_example "${PROJECT_SOURCE_DIR}/examples/zed_oc_control_example.cpp")
        set_target_properties(${PROJECT_NAME}_control_example PROPERTIES PREFIX "")
        target_link_libraries(${PROJECT_NAME}_control_example
          ${PROJECT_NAME}
          ${OpenCV_LIBS}
        )
        install(TARGETS ${PROJECT_NAME}_control_example
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        )

        ##### Rectify Example
        include_directories( ${PROJECT_SOURCE_DIR}/examples/include)
        add_executable(${PROJECT_NAME}_rectify_example "${PROJECT_SOURCE_DIR}/examples/zed_oc_rectify_example.cpp")
        set_target_properties(${PROJECT_NAME}_rectify_example PROPERTIES PREFIX "")
        target_link_libraries(${PROJECT_NAME}_rectify_example
          ${PROJECT_NAME}
          ${OpenCV_LIBS}
        )
        install(TARGETS ${PROJECT_NAME}_rectify_example
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        )
    endif()

    if(BUILD_SENSORS)
        message("* Sensors example available")

        ##### Sensors Example
        add_executable(${PROJECT_NAME}_sensors_example "${PROJECT_SOURCE_DIR}/examples/zed_oc_sensors_example.cpp")
        set_target_properties(${PROJECT_NAME}_sensors_example PROPERTIES PREFIX "")
        target_link_libraries(${PROJECT_NAME}_sensors_example          
          ${PROJECT_NAME}
        )
        install(TARGETS ${PROJECT_NAME}_sensors_example
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        )
    endif()

    if(BUILD_VIDEO AND BUILD_SENSORS)
        message("* Video/Sensors sync example available")

        ##### Synchronization Example
        add_executable(${PROJECT_NAME}_sync_example "${PROJECT_SOURCE_DIR}/examples/zed_oc_sync_example.cpp")
        set_target_properties(${PROJECT_NAME}_sync_example PROPERTIES PREFIX "")
        target_link_libraries(${PROJECT_NAME}_sync_example          
          ${PROJECT_NAME}
          ${PROJECT_NAME}
          ${OpenCV_LIBS}
        )
        install(TARGETS ${PROJECT_NAME}_sync_example
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        )
    endif()
endif()
