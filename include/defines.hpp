///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <stdint.h>
#include <string>
#include <string.h>
#include <iostream>
#include <vector>
#include <chrono>

#if defined _WIN32
#if defined(SL_OC_COMPIL)
#define SL_OC_EXPORT __declspec(dllexport)
#else
#define SL_OC_EXPORT __declspec(dllimport)
#endif
#elif __GNUC__
#define SL_OC_EXPORT __attribute__((visibility("default")))
#else
#define SL_OC_EXPORT
#endif

//// SDK VERSION NUMBER
#define ZED_OC_MAJOR_VERSION 1
#define ZED_OC_MINOR_VERSION 0
#define ZED_OC_PATCH_VERSION 0

/*!
 * \brief Get the current system clock as steady clock, so with no jumps even if the system time changes
 * \return the current steady system clock in nanoseconds
 */
inline uint64_t getSteadyTimestamp() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}

/*!
 * \brief Get the current system clock as wall clock (it can have jumps if the system clock is updated by a sync service)
 * \return the current wall system clock in nanoseconds
 */
inline uint64_t getWallTimestamp() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}

namespace sl_oc {

static const uint16_t SL_USB_VENDOR = 0x2b03;           //!< Stereolabs Vendor ID

static const uint16_t SL_USB_PROD_ZED_REVA = 0xf580;         //!< Old ZED firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M_REVA = 0xf680;       //!< Old ZED-M binary modified firmware Product ID
static const uint16_t SL_USB_PROD_ZED_REVB = 0xf582;     //!< CBS ZED Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M_REVB = 0xf682;   //!< CBS ZED-M Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_2_REVB = 0xf780;   //!< CBS ZED 2 Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_2i = 0xf880;   //!< CBS ZED 2i Firmware Product ID
static const uint16_t SL_USB_PROD_MCU_ZEDM_REVA= 0xf681; //!< MCU sensor device for ZED-M
static const uint16_t SL_USB_PROD_MCU_ZED2_REVA = 0xf781; //!< MCU sensor device for ZED2
static const uint16_t SL_USB_PROD_MCU_ZED2i_REVA = 0xf881; //!< MCU sensor device for ZED2i

enum VERBOSITY {
    NONE = 0,
    ERROR = 1,
    WARNING = 2,
    INFO = 3
};

/*!
 * \brief Convert a steady clock time to the time base used for all the frame and sensor timestamps
 *
 * The time base is the wall clock read once, the first time a timestamp is needed, and then advanced with the
 * steady clock. Timestamps are therefore comparable with the system time, never go backwards and are not affected
 * by later adjustments of the system clock.
 *
 * \param steady_nsec a steady clock time in nanoseconds (see \ref getSteadyTimestamp)
 * \return the corresponding timestamp in nanoseconds
 */
SL_OC_EXPORT uint64_t steadyToTimestamp(uint64_t steady_nsec);

/*!
 * \brief Get the current time in the time base used for all the frame and sensor timestamps
 * \return the current timestamp in nanoseconds
 */
inline uint64_t getTimestampNow() { return steadyToTimestamp(getSteadyTimestamp()); }

}

#endif //DEFINES_HPP
