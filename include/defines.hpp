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

namespace zed {

enum class SL_DEVICE {
    ZED,
    ZED_M,
    ZED_CBS,
    ZED_M_CBS,
    ZED_2_CBS,
    ZED_M_MCU,
    ZED_2_MCU,
    DFU_MCU,
    NONE
};

enum class RESOLUTION {
    HD2K,       /**< 2208*1242, available framerates: 15 fps.*/
    HD1080,     /**< 1920*1080, available framerates: 15, 30 fps.*/
    HD720,      /**< 1280*720, available framerates: 15, 30, 60 fps.*/
    VGA,        /**< 672*376, available framerates: 15, 30, 60, 100 fps.*/
    LAST
};

enum class FPS {
    FPS_15 = 15,
    FPS_30 = 30,
    FPS_60 = 60,
    FPS_100 = 100,
    LAST = 101
};

typedef struct _video_params
{
    _video_params() {
        res = RESOLUTION::HD2K;
        fps = FPS::FPS_15;
        verbose=false;
    }

    RESOLUTION res;
    FPS fps;
    bool verbose;
} VideoParams;

struct Resolution {
    size_t width; /**< array width in pixels  */
    size_t height; /**< array height in pixels*/

    Resolution(size_t w_ = 0, size_t h_ = 0) {
        width = w_;
        height = h_;
    }
};

static const std::vector<zed::Resolution> cameraResolution = {
    zed::Resolution(2208, 1242), /**< sl::RESOLUTION::HD2K */
    zed::Resolution(1920, 1080), /**< sl::RESOLUTION::HD1080 */
    zed::Resolution(1280, 720), /**< sl::RESOLUTION::HD720 */
    zed::Resolution(672, 376) /**< sl::RESOLUTION::VGA */
};

static const uint16_t SL_USB_VENDOR = 0x2b03;           //!< Stereolabs Vendor ID
static const uint16_t SL_USB_PROD_ZED = 0xf580;         //!< Old ZED firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M = 0xf680;       //!< Old ZED-M binary modified firmware Product ID
static const uint16_t SL_USB_PROD_ZED_CBS = 0xf582;     //!< CBS ZED Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M_CBS = 0xf682;   //!< CBS ZED-M Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_2_CBS = 0xf780;   //!< CBS ZED 2 Firmware Product ID
}

#endif //DEFINES_HPP
