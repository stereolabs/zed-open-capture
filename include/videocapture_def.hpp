#ifndef VIDEOCAPTURE_DEF_HPP
#define VIDEOCAPTURE_DEF_HPP

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC                   1000000000ULL
#endif

namespace sl_drv {

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

enum class CAM_SENS_POS {
    LEFT = 0,
    RIGHT = 1,
    LAST = 3
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

static const std::vector<sl_drv::Resolution> cameraResolution = {
    sl_drv::Resolution(2208, 1242), /**< sl::RESOLUTION::HD2K */
    sl_drv::Resolution(1920, 1080), /**< sl::RESOLUTION::HD1080 */
    sl_drv::Resolution(1280, 720), /**< sl::RESOLUTION::HD720 */
    sl_drv::Resolution(672, 376) /**< sl::RESOLUTION::VGA */
};

static const uint16_t SL_USB_PROD_ZED = 0xf580;         //!< Old ZED firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M = 0xf680;       //!< Old ZED-M binary modified firmware Product ID
static const uint16_t SL_USB_PROD_ZED_CBS = 0xf582;     //!< CBS ZED Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_M_CBS = 0xf682;   //!< CBS ZED-M Firmware Product ID
static const uint16_t SL_USB_PROD_ZED_2_CBS = 0xf780;   //!< CBS ZED 2 Firmware Product ID

/*!
 * \brief The Buffer struct used by UVC to store frame data
 */
struct Buffer {
    void *start;
    size_t length;
};

// ----> Camera control
#define cbs_xu_unit_id          0x04 //mapped to wIndex 0x0400
#define cbs_xu_control_selector 0x02 //mapped to wValue 0x0200
#define READ_MODE   1
#define WRITE_MODE  2

#define ISP_LEFT    0x80181033
#define ISP_RIGHT   0x80181833

#define MASK_ON     0x02
#define MASK_OFF    0xFD

#define ADD_EXP_H   0x3500
#define ADD_EXP_M   0x3501
#define ADD_EXP_L   0x3502
#define ADD_GAIN_H  0x3507
#define ADD_GAIN_M  0x3508
#define ADD_GAIN_L  0x3509

#define XU_TASK_SET     0x50
#define XU_TASK_GET     0x51
#define XU_ISP_CTRL     0x07
#define XU_EXP_GAIN     0x25

#define UNIQUE_ID_START 0x18000

#define LINUX_CTRL_BRIGHTNESS   9963776
#define LINUX_CTRL_CONTRAST     9963777
#define LINUX_CTRL_HUE          9963779
#define LINUX_CTRL_SATURATION   9963778
#define LINUX_CTRL_GAIN         9963795
#define LINUX_CTRL_AWB          9963802
#define LINUX_CTRL_AWB_AUTO     9963788
#define LINUX_CTRL_SHARPNESS    9963803
#define LINUX_CTRL_GAMMA        9963792

#define DEFAULT_GAMMA_NOECT 1
#define DEFAULT_MIN_GAMMA   1
#define DEFAULT_MAX_GAMMA   9

#define DEFAULT_MIN_GAIN   0
#define DEFAULT_MAX_GAIN   100
#define DEFAULT_MIN_EXP    0
#define DEFAULT_MAX_EXP    100

// Gain working zones
#define GAIN_ZONE1_MIN 0
#define GAIN_ZONE1_MAX 255
#define GAIN_ZONE2_MIN 378
#define GAIN_ZONE2_MAX 511
#define GAIN_ZONE3_MIN 890
#define GAIN_ZONE3_MAX 1023
#define GAIN_ZONE4_MIN 1914
#define GAIN_ZONE4_MAX 2047

// Raw exposure max values
#define EXP_RAW_MAX_15FPS   1550
#define EXP_RAW_MAX_30FPS   1100
#define EXP_RAW_MAX_60FPS   880
#define EXP_RAW_MAX_100FPS  720

#define EXP_RAW_MIN         2
// <---- Camera Control

}

#endif // VIDEOCAPTURE_DEF_HPP
