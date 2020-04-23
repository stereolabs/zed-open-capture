#ifndef SENSORCAPTURE_DEF_HPP
#define SENSORCAPTURE_DEF_HPP

namespace sl_drv {

typedef struct _sensor_params
{
    _sensor_params() {
        freq=400.0f;
        verbose=false;
    }

    float freq; // Acquisition frequency (max 400 Hz)
    bool verbose;
} SensorParams;

// ----> Command to be used with the REPORT ID "REP_ID_REQUEST_SET"
// Command to enter in bootload mode
//#define RQ_CMD_BOOT 0xF1
// Command to ping the MCU to communicate that host is alive
#define RQ_CMD_PING 0xF2
// Command to force a BMI270 re-trimming (CRT) operation
//#define RQ_CMD_BMI270_START_CRT 0xF3
// Command to reset the MCU
//#define RQ_CMD_RST 0xE1
// <---- Command to be used with the REPORT ID "REP_ID_REQUEST_SET"

typedef enum _customhid_report_id {
    // Input Reports
    REP_ID_SENSOR_DATA = 0x01,			// Report ID #1

//    // Generic commands
    REP_ID_REQUEST_SET 			= 0x21,	// Report ID #33 -> The same as `REQUEST_SET` in ZEDkit "IMUReader.hpp"

//    // OV580 Flash commands
//    REP_ID_OV580_CMD			= 0x22,	// Report ID #34
//    REP_ID_OV580_SEND_PACKET	= 0x23,	// Report ID #35
//    REP_ID_OV580_STATUS			= 0x24,	// Report ID #36 -> Returns the ID of the last command and "OV580_NACK" or "OV580_ACK"

//    // BMI270 re-trimming
//    REP_ID_BMI270_CRT_STATUS	= 0x25, // Report ID #37

//    // Features Reports
    REP_ID_SENSOR_STREAM_STATUS = 0x32,	// Report ID #50
//    REP_ID_CAMERA_DATA			= 0x33,	// Report ID #51
//    REP_ID_IMU_BIAS_DATA		= 0x34,	// Report ID #52
//    REP_ID_IMU_VARIANCE_DATA	= 0x35,	// Report ID #53
//    REP_ID_IMU_KALIB_DATA		= 0x36,	// Report ID #54
//    REP_ID_CAMCALIB_RES_2K_L	= 0x37, // Report ID #55
//    REP_ID_CAMCALIB_RES_2K_R	= 0x38, // Report ID #56
//    REP_ID_CAMCALIB_RES_FHD_L	= 0x39, // Report ID #57
//    REP_ID_CAMCALIB_RES_FHD_R	= 0x3A, // Report ID #58
//    REP_ID_CAMCALIB_RES_HD_L	= 0x3B, // Report ID #59
//    REP_ID_CAMCALIB_RES_HD_R	= 0x3C, // Report ID #60
//    REP_ID_CAMCALIB_RES_VGA_L	= 0x3D, // Report ID #61
//    REP_ID_CAMCALIB_RES_VGA_R	= 0x3E, // Report ID #62
//    REP_ID_CALIB_STEREO_DATA	= 0x3F,	// Report ID #63
//    REP_ID_CALIB_BAROMETER_DATA	= 0x40,	// Report ID #64
//    REP_ID_CALIB_EXT_GYRO_DATA	= 0x41,	// Report ID #65
//    REP_ID_CALIB_EXT_ACC_DATA	= 0x42,	// Report ID #66
//    REP_ID_CALIB_MAG_DATA		= 0x43,	// Report ID #67
//    REP_ID_IMU_ODR				= 0x44, // rEPORT id #68
//    REP_ID_DATA_TIME            = 0x45, // Report ID #69 -> Last Data Writing Date/Time UTC
} CUSTOMHID_REPORT_ID;

#pragma pack(push)  // push current alignment to stack
#pragma pack(1)     // set alignment to 1 byte boundary

typedef struct _sens_data {
    uint8_t struct_id;		//!< Struct identifier, it matches the USB HID Report ID
    uint8_t imu_not_valid; 	//!< Indicate if IMU data are valid [0->valid, 1->not_valid
    uint64_t timestamp;		//!< Data timestamp (from IMU sensor) [usec/39]
    int16_t gX;				//!< Raw Gyroscope X
    int16_t gY;				//!< Raw Gyroscope Y
    int16_t gZ;				//!< Raw Gyroscope Z
    int16_t aX;				//!< Raw Accelerometer X
    int16_t aY;				//!< Raw Accelerometer Y
    int16_t aZ;				//!< Raw Accelerometer Z
    uint8_t frame_sync;		//!< Indicates if data are synced to a camera frame
    uint8_t sync_capabilities; //!< Indicates if frame synchronization is active
    uint32_t frame_sync_count; //!< Counts the number of synced frames
    int16_t imu_temp;		//!< Temperature from the IMU sensor [0.01 °C]
    uint8_t mag_valid;		//!< Indicates if Magnetometer data are valid (put to 0 if no magnetometer is present)
    int16_t mX;				//!< Raw Magnetometer X
    int16_t mY;				//!< Raw Magnetometer Y
    int16_t mZ;				//!< Raw Magnetometer Z
    uint8_t camera_moving;	//!< Indicate if the camera is moving (uses BMI internal HW)
    uint32_t camera_moving_count; //!< Counts the number of camera moving interrupts
    uint8_t camera_falling;	//!< Indicate if the camera is free falling (uses BMI internal HW)
    uint32_t camera_falling_count; //!< Counts the number of camera falling interrupts
    uint8_t env_valid;		//!< Indicate if Environmental data are valid (put to `ENV_SENS_NOT_PRESENT` if no environmental sensor is present)
    int16_t temp;			//!< Temperature [0.01 °C]
    uint32_t press;			//!< Pressure [0.01 hPa]
    uint32_t humid;			//!< Relative humidity [1.0/1024.0 %rH]
    int16_t temp_cam_left;	//!< Temperature of the left camera sensor [0.01 °C]
    int16_t temp_cam_right; //!< Temperature of the right camera sensor [0.01 °C]
} SensData;

// Data streaming status
typedef struct _sens_stream_status {
    uint8_t struct_id;		//!< Struct identifier, it matches the USB HID Report ID
    uint8_t stream_status;	//!< Status of the USB streaming
} SensStreamStatus;

#pragma pack(pop) // Restore previous saved alignment

}

#endif // SENSORCAPTURE_DEF_HPP
