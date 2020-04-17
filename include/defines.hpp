#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <stdint.h>
#include <string>
#include <string.h>
#include <cxxabi.h>
#include <iostream>
#include <vector>

#if defined _WIN32
#if defined(SL_SDK_COMPIL)
#define SL_SDK_EXPORT __declspec(dllexport)
#else
#define SL_SDK_EXPORT __declspec(dllimport)
#endif
#elif __GNUC__
#define SL_DRV_EXPORT __attribute__((visibility("default")))
#if defined(__arm__) || defined(__aarch64__)
#define _SL_JETSON_
#endif
#endif

// SDK VERSION NUMBER
#define ZED_DRV_MAJOR_VERSION 0
#define ZED_DRV_MINOR_VERSION 1
#define ZED_DRV_PATCH_VERSION 0

#define ZED_DRV_VERSION_ATTRIBUTE private: uint32_t mDrvMajorVer = ZED_DRV_MAJOR_VERSION, mDrvMinorVer = ZED_DRV_MINOR_VERSION, mDrvPatchVer = ZED_DRV_PATCH_VERSION;

// Debug output
#define VERBOSE_OUT(msg) { int status_dem_0; std::cout << "[" << abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status_dem_0) << "] INFO: " << msg << std::endl; }
#define WARNING_OUT(msg) { int status_dem_0; std::cerr << "[" << abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status_dem_0) << "] WARNING: " << msg << std::endl; }
#define ERROR_OUT(msg) { int status_dem_0; std::cerr << "[" << abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status_dem_0) << "] ERROR: " << msg << std::endl; }

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC                   1000000000ULL
#endif

#endif //DEFINES_HPP
