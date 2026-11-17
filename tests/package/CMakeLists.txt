# Verifies that an application can use the installed library through its CMake package:
#   cmake -S tests/package -B build_package -DCMAKE_PREFIX_PATH=<install prefix>
cmake_minimum_required(VERSION 3.16)

project(zed_open_capture_package_test LANGUAGES CXX)

find_package(zed_open_capture 1.0 REQUIRED)

add_executable(package_test main.cpp)
target_link_libraries(package_test PRIVATE zed_open_capture::zed_open_capture)
