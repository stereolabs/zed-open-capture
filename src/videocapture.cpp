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

#include "videocapture.hpp"
#include "internal.hpp"

#ifdef SENSORS_MOD_AVAILABLE
#include "sensorcapture.hpp"
#endif

#include <sys/stat.h>         // for stat, S_ISCHR
#include <errno.h>            // for errno, EBADRQC, EINVAL, ENOBUFS, ENOENT
#include <fcntl.h>            // for open, O_NONBLOCK, O_RDONLY, O_RDWR
#include <unistd.h>           // for usleep, close
#include <dirent.h>           // for opendir, readdir
#include <poll.h>             // for poll

#include <linux/usb/video.h>  // for UVC_GET_CUR, UVC_SET_CUR, UVC_GET_LEN
#include <linux/uvcvideo.h>   // for uvc_xu_control_query, UVCIOC_CTRL_QUERY
#include <linux/videodev2.h>  // for v4l2_buffer, v4l2_queryctrl, V4L2_BUF_T...
#include <sys/mman.h>         // for mmap, munmap, MAP_SHARED, PROT_READ
#include <sys/ioctl.h>        // for ioctl

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <fstream>            // for char_traits, basic_istream::operator>>

#include <cmath>              // for round

#define IOCTL_RETRY 3

#define READ_MODE   1
#define WRITE_MODE  2

#define XU_BUF_SIZE 384       // Size of the buffer of the vendor control: 384 bytes with USB3, 64 bytes with USB2

// ----> Camera control
#define cbs_xu_unit_id          0x04 //mapped to wIndex 0x0400
#define cbs_xu_control_selector 0x02 //mapped to wValue 0x0200

#define ISP_CTRL_LEFT    0x80181033
#define ISP_CTRL_RIGHT   0x80181833

#define ISP_GAMMA_LEFT   0x80181500
#define ISP_GAMMA_RIGHT  0x80181D00
#define ISP_GAMMA_EN_OFFSET 0x10
#define ISP_GAMMA_VALUES 15

#define ISP_ROI_LEFT     0x801810C0
#define ISP_ROI_RIGHT    0x801818C0

#define AEG_AGC_MASK_ON     0x02
#define AEG_AGC_MASK_OFF    0xFD

#define ADDR_EXP_H   0x3500
#define ADDR_EXP_M   0x3501
#define ADDR_EXP_L   0x3502
#define ADDR_GAIN_H  0x3507
#define ADDR_GAIN_M  0x3508
#define ADDR_GAIN_L  0x3509
#define ADDR_CHIP_ID_H  0x300A
#define ADDR_CHIP_ID_L  0x300B
#define OV4689_CHIP_ID  0x4688

#define XU_TASK_SET     0x50
#define XU_TASK_GET     0x51
#define XU_ISP_CTRL     0x07
#define XU_EXP_GAIN     0x25

#define UNIQUE_ID_START 0x18000

#define LINUX_CTRL_BRIGHTNESS   V4L2_CID_BRIGHTNESS
#define LINUX_CTRL_CONTRAST     V4L2_CID_CONTRAST
#define LINUX_CTRL_HUE          V4L2_CID_HUE
#define LINUX_CTRL_SATURATION   V4L2_CID_SATURATION
#define LINUX_CTRL_AWB          V4L2_CID_WHITE_BALANCE_TEMPERATURE
#define LINUX_CTRL_AWB_AUTO     V4L2_CID_AUTO_WHITE_BALANCE
#define LINUX_CTRL_SHARPNESS    V4L2_CID_SHARPNESS

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

// ----> Capture
#define UVC_BUFFER_COUNT        4       // Buffers shared with the driver
#define GRAB_POLL_TIMEOUT_MSEC  100     // Maximum time the grabbing thread waits before checking the stop flag
#define GRAB_MAX_ERRORS         20      // Consecutive errors before considering the camera disconnected
#define SYNC_MAX_DISTANCE_NSEC   8000000ULL  // Maximum distance between the timestamps of a frame and of its sync signal
// <---- Capture

namespace sl_oc {

namespace video {

namespace {

const char* LOG_TAG = "sl_oc::video::VideoCapture";

std::string errnoMsg(const std::string& what, const std::string& dev)
{
    return what + " '" + dev + "': [" + std::to_string(errno) + "] " + std::string(strerror(errno));
}

SL_DEVICE modelFromIds(int vid, int pid)
{
    if( vid!=SL_USB_VENDOR )
        return SL_DEVICE::NONE;

    switch(pid)
    {
    case SL_USB_PROD_ZED_REVA:   return SL_DEVICE::ZED;
    case SL_USB_PROD_ZED_M_REVA: return SL_DEVICE::ZED_M;
    case SL_USB_PROD_ZED_REVB:   return SL_DEVICE::ZED_CBS;
    case SL_USB_PROD_ZED_M_REVB: return SL_DEVICE::ZED_M_CBS;
    case SL_USB_PROD_ZED_2_REVB: return SL_DEVICE::ZED_2;
    case SL_USB_PROD_ZED_2i:     return SL_DEVICE::ZED_2i;
    default:                     return SL_DEVICE::NONE;
    }
}

/*!
 * \brief Read the camera model of a video device from the information exposed by the kernel
 * \param devId index of the `/dev/video*` device
 * \param usbSpeed if not null receives the USB speed in Mbps, 0 if unknown
 * \param captureNode if not null receives false for the nodes that do not provide images (e.g. metadata)
 */
SL_DEVICE readCameraModel(int devId, int* usbSpeed=nullptr, bool* captureNode=nullptr)
{
    const std::string sysPath = "/sys/class/video4linux/video" + std::to_string(devId);

    if(usbSpeed) *usbSpeed = 0;
    if(captureNode) *captureNode = true;

    std::string modalias;
    if (!(std::ifstream(sysPath + "/device/modalias") >> modalias))
        return SL_DEVICE::NONE;

    if (modalias.size() < 14 || modalias.substr(0, 5) != "usb:v" || modalias[9] != 'p')
        return SL_DEVICE::NONE;

    int vid = 0, pid = 0;
    if (!(std::istringstream(modalias.substr(5, 4)) >> std::hex >> vid))
        return SL_DEVICE::NONE;
    if (!(std::istringstream(modalias.substr(10, 4)) >> std::hex >> pid))
        return SL_DEVICE::NONE;

    if(usbSpeed)
    {
        int speed = 0;
        if( std::ifstream(sysPath + "/device/../speed") >> speed )
            *usbSpeed = speed;
    }

    if(captureNode)
    {
        int index = 0;
        if( std::ifstream(sysPath + "/index") >> index )
            *captureNode = (index==0);
    }

    return modelFromIds(vid, pid);
}

/*!
 * \brief List the indices of the `/dev/video*` capture devices of the Stereolabs cameras
 */
std::vector<int> listCameraIds()
{
    std::vector<int> ids;

    DIR* dir = opendir("/sys/class/video4linux");
    if(!dir)
        return ids;

    while( struct dirent* entry = readdir(dir) )
    {
        const std::string name = entry->d_name;
        if( name.size()<=5 || name.size()>9 || name.compare(0,5,"video")!=0 ||
                name.find_first_not_of("0123456789",5)!=std::string::npos )
            continue;

        const int id = std::atoi(name.c_str()+5);
        bool captureNode = true;
        if( readCameraModel(id, nullptr, &captureNode)!=SL_DEVICE::NONE && captureNode )
            ids.push_back(id);
    }
    closedir(dir);

    std::sort(ids.begin(), ids.end());
    return ids;
}

const char* xuErrorMsg(int err)
{
    switch (err) {
    case ENOENT:  return "Extension unit or control not found";
    case ENOBUFS: return "Buffer size does not match control size";
    case EINVAL:  return "Invalid request code";
    case EBADRQC: return "Request not supported by control";
    default:      return strerror(err);
    }
}

/*!
 * \brief Send a command to the vendor control of the camera and optionally read the reply
 * \param fd file descriptor of the video device
 * \param buf buffer of XU_BUF_SIZE bytes containing the command, and receiving the reply
 * \param error if not null receives the description of the error
 * \return 0 if successful
 */
int xuControl(int fd, uint8_t* buf, bool readReply, bool safe, std::string* error=nullptr)
{
    // A command is a sequence of requests: the sequences of different objects must not be mixed, because two
    // objects can communicate with the same camera (e.g. while listing the connected devices)
    static std::mutex xuMutex;
    const std::lock_guard<std::mutex> lock(xuMutex);

    unsigned char tmp[2] = {0};
    struct uvc_xu_control_query xu_query_info;
    memset(&xu_query_info, 0, sizeof(xu_query_info));
    xu_query_info.unit = cbs_xu_unit_id;
    xu_query_info.selector = cbs_xu_control_selector;
    xu_query_info.query = UVC_GET_LEN;
    xu_query_info.size = 2;
    xu_query_info.data = tmp;

    if (ioctl(fd, UVCIOC_CTRL_QUERY, &xu_query_info) != 0)
        return -4;

    // 384 bytes with USB3, 64 bytes with USB2
    int len = (tmp[1] << 8) + tmp[0];
    if( len<=0 || len>XU_BUF_SIZE )
        return -2;

    // we use the UVC_SET_CUR to write the cmd
    struct uvc_xu_control_query xu_query_send;
    memset(&xu_query_send, 0, sizeof(xu_query_send));
    xu_query_send.unit = cbs_xu_unit_id;
    xu_query_send.selector = cbs_xu_control_selector;
    xu_query_send.query = UVC_SET_CUR;
    xu_query_send.size = static_cast<__u16> (len);
    xu_query_send.data = buf;

    if (ioctl(fd, UVCIOC_CTRL_QUERY, &xu_query_send) != 0)
    {
        const int res = errno;
        if(error)
            *error = std::string("CBS SET failed: ") + xuErrorMsg(res) + ". (System code: " + std::to_string(res) + ") " + std::to_string(len);
        return -1;
    }

    usleep(safe ? 2000 : 300);

    if (readReply)
    {
        struct uvc_xu_control_query xu_query;
        memset(&xu_query, 0, sizeof(xu_query));
        xu_query.unit = cbs_xu_unit_id;
        xu_query.selector = cbs_xu_control_selector;
        xu_query.query = UVC_GET_CUR;
        xu_query.size = static_cast<__u16> (len);
        xu_query.data = buf;

        if (ioctl(fd, UVCIOC_CTRL_QUERY, &xu_query) != 0)
        {
            const int res = errno;
            if(error)
                *error = std::string("CBS GET failed: ") + xuErrorMsg(res) + ". (System code: " + std::to_string(res) + ") " + std::to_string(len);
            return -1;
        }
    }

    return 0;
}

/*!
 * \brief Read the serial number from the flash memory of the camera
 * \param fd file descriptor of the video device
 * \param maxTries number of attempts
 * \return the serial number, -1 if it cannot be read
 */
int readSerialNumber(int fd, int maxTries)
{
    for( int i=0; i<maxTries; i++ )
    {
        const int len = 64;
        const int Adr = UNIQUE_ID_START;

        uint8_t xu_buf[XU_BUF_SIZE];
        memset(xu_buf, 0, XU_BUF_SIZE);

        xu_buf[0] = 0x51;
        xu_buf[1] = 0xA1;
        xu_buf[2] = 0x03;

        xu_buf[5] = (Adr >> 24) & 0xff;
        xu_buf[6] = (Adr >> 16) & 0xff;
        xu_buf[7] = (Adr >> 8) & 0xff;
        xu_buf[8] = (Adr) & 0xff;

        const int pack_val = 36864 + len;
        xu_buf[9] = ((pack_val) >> 8) & 0xff;
        xu_buf[10] = ((pack_val) >> 0) & 0xff;

        xu_buf[11] = ((len) >> 8) & 0xff;
        xu_buf[12] = ((len) >> 0) & 0xff;

        const uint8_t* UNIQUE_BUF = &xu_buf[17];

        if( xuControl(fd, xu_buf, true, true)==0 && UNIQUE_BUF[0]=='O' && UNIQUE_BUF[1]=='V' )
        {
            // The serial number is stored as binary-coded decimal
            int sn = 0;
            for( int b=2; b<6; b++ )
            {
                const int hi = UNIQUE_BUF[b]>>4;
                const int lo = UNIQUE_BUF[b]&0x0f;
                if( hi>9 || lo>9 )
                    return -1;
                sn = sn*100 + hi*10 + lo;
            }
            return sn;
        }

        usleep(1000);
    }

    return -1;
}

}

VideoCapture::VideoCapture(VideoParams params)
    : mParams(params)
{

    std::string ver =
            "ZED Open Capture - Camera module - Version: "
            + std::to_string(ZED_OC_MAJOR_VERSION) + "."
            + std::to_string(ZED_OC_MINOR_VERSION) + "."
            + std::to_string(ZED_OC_PATCH_VERSION);
    detail::log(mParams.verbose, VERBOSITY::INFO, LOG_TAG, ver);

    // Check that FPS is coherent with user resolution
    checkResFps( );

    // Calculate gain zones (required because the raw gain control is not continuous in the range of values)
    mGainSegMax = (GAIN_ZONE4_MAX-GAIN_ZONE4_MIN)+(GAIN_ZONE3_MAX-GAIN_ZONE3_MIN)+(GAIN_ZONE2_MAX-GAIN_ZONE2_MIN)+(GAIN_ZONE1_MAX-GAIN_ZONE1_MIN);
}

VideoCapture::~VideoCapture()
{
    close();
}

std::vector<VideoDevice> VideoCapture::getDeviceList()
{
    std::vector<VideoDevice> devices;

    for( int id : listCameraIds() )
    {
        VideoDevice dev;
        dev.dev_id = id;
        dev.dev_name = "/dev/video" + std::to_string(id);
        dev.model = readCameraModel(id, &dev.usb_speed);

        const int fd = ::open(dev.dev_name.c_str(), O_RDWR|O_NONBLOCK, 0);
        if( fd!=-1 )
        {
            dev.serial_number = readSerialNumber(fd, 20);
            ::close(fd);
        }

        devices.push_back(dev);
    }

    return devices;
}

void VideoCapture::close()
{
    if( mInitialized && !mDeviceLost )
        setLEDstatus( false );

    const bool wasInitialized = mInitialized;
    mInitialized=false;

    mStopCapture = true;

    if( mGrabThread.joinable() )
    {
        mGrabThread.join();
    }

    closeDevice();

    {
        const std::lock_guard<std::mutex> lock(mBufMutex);
        mSyncLink.reset();
        mNewFrame = false;
        mLastFrame.data = nullptr;
        for( std::vector<uint8_t>& img : mFrameData )
            std::vector<uint8_t>().swap(img);
    }
    mFrameCond.notify_all();

    if( wasInitialized )
    {
        detail::log(mParams.verbose, VERBOSITY::INFO, LOG_TAG, "Device closed");
    }
}

void VideoCapture::closeDevice()
{
    if (mFileDesc != -1)
    {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(mFileDesc, VIDIOC_STREAMOFF, &type);
    }

    if( mBuffers )
    {
        for (unsigned int i = 0; i < mBufCount; ++i)
        {
            if( mBuffers[i].start!=nullptr && mBuffers[i].start!=MAP_FAILED )
                munmap(mBuffers[i].start, mBuffers[i].length);
        }
        free(mBuffers);
        mBuffers = nullptr;
    }
    mBufCount = 0;

    if (mFileDesc != -1)
    {
        ::close(mFileDesc);
        mFileDesc=-1;
    }
}

bool VideoCapture::isOpened() const
{
    return mInitialized && !mDeviceLost;
}

void VideoCapture::checkResFps()
{
    if( static_cast<int>(mParams.res)<0 || static_cast<int>(mParams.res)>=static_cast<int>(RESOLUTION::LAST) )
    {
        detail::log(mParams.verbose, VERBOSITY::WARNING, LOG_TAG, "RESOLUTION not supported. Using HD2K");
        mParams.res = RESOLUTION::HD2K;
    }

    mWidth = cameraResolution[static_cast<int>(mParams.res)].width*2;
    mHeight = cameraResolution[static_cast<int>(mParams.res)].height;
    mFps = static_cast<int>(mParams.fps);

    const int requested = mFps;

    switch (mParams.res)
    {
    default:
    case RESOLUTION::HD2K:
        mFps = 15;
        break;

    case RESOLUTION::HD1080:
        mFps = (mFps <= 22) ? 15 : 30;
        break;

    case RESOLUTION::HD720:
        if( mFps <= 22  )
            mFps = 15;
        else if( mFps < 45  )
            mFps = 30;
        else
            mFps = 60;
        break;

    case RESOLUTION::VGA:
        if( mFps <= 22  )
            mFps = 15;
        else if( mFps < 45  )
            mFps = 30;
        else if( mFps < 80  )
            mFps = 60;
        else
            mFps = 100;
        break;
    }

    if( mFps!=requested )
    {
        detail::log(mParams.verbose, VERBOSITY::WARNING, LOG_TAG, "FPS not supported for the chosen resolution. Using the best value");
    }

    // FPS mapping
    if( mFps <= 15 )
        mExpoureRawMax = EXP_RAW_MAX_15FPS;
    else if( mFps <= 30 )
        mExpoureRawMax = EXP_RAW_MAX_30FPS;
    else if( mFps <= 60 )
        mExpoureRawMax = EXP_RAW_MAX_60FPS;
    else
        mExpoureRawMax = EXP_RAW_MAX_100FPS;

    detail::log(mParams.verbose, VERBOSITY::INFO, LOG_TAG,
                "Camera resolution: " + std::to_string(mWidth) + "x" + std::to_string(mHeight) + "@" + std::to_string(mFps) + "Hz");
}

bool VideoCapture::initializeVideo( int devId/*=-1*/ )
{
    close();

    bool opened=false;

    if( devId==-1 )
    {
        // Try to open all the Stereolabs cameras until the first success
        const std::vector<int> ids = listCameraIds();

        if( ids.empty() )
        {
            detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, "No Stereolabs camera detected");
        }

        for( int id : ids )
        {
            opened = openCamera( id );
            if(opened) break;
        }
    }
    else
    {
        opened = openCamera( devId );
    }

    if(!opened)
    {
        return false;
    }

    return finalizeInit();
}

bool VideoCapture::initializeVideoBySerialNumber( int serial_number )
{
    close();

    for( int id : listCameraIds() )
    {
        const std::string name = "/dev/video" + std::to_string(id);
        const int fd = ::open(name.c_str(), O_RDWR|O_NONBLOCK, 0);
        if( fd==-1 )
            continue;

        const int sn = readSerialNumber(fd, 20);
        ::close(fd);

        if( sn==serial_number )
        {
            return openCamera(id) && finalizeInit();
        }
    }

    detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG,
                "Camera with serial number " + std::to_string(serial_number) + " not found");

    return false;
}

bool VideoCapture::finalizeInit()
{
    if( !startCapture() )
    {
        closeDevice();
        return false;
    }

    mInitialized = true;

    // After STREAMON the sensors ignore register writes and reads return 0 for ~150 ms
    if( !waitSensorsReady(2000) )
    {
        // Chip ID only verified on ZED 2 and ZED 2i: other models must not lose the ability to open
        if( mCameraModel==SL_DEVICE::ZED_2 || mCameraModel==SL_DEVICE::ZED_2i )
        {
            detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, "Sensors not ready after 2 s, closing the camera");
            close();
            return false;
        }
        detail::log(mParams.verbose, VERBOSITY::WARNING, LOG_TAG, "Sensors not ready after 2 s, camera settings may not be applied");
    }

    detail::log(mParams.verbose, VERBOSITY::INFO, LOG_TAG, "Device '" + mDevName + "' opened");

    setLEDstatus( true );

    resetAECAGC();
    resetAutoWhiteBalance();
    resetBrightness();
    resetContrast();
    resetGamma();
    resetHue();
    resetSaturation();
    resetSharpness();

    return true;
}

bool VideoCapture::openCamera( int devId )
{
    const int verbose = mParams.verbose;

    mDevId = devId;
    mDevName = std::string("/dev/video") + std::to_string(mDevId);
    mSerialNumber = -1;

    detail::log(verbose, VERBOSITY::INFO, LOG_TAG, "Trying to open the device '" + mDevName + "'");

    // The requested values: the size is replaced by the one accepted by the driver
    checkResFps();
    const int reqWidth = mWidth;
    const int reqHeight = mHeight;

    // Check camera model
    int usbSpeed = 0;
    mCameraModel = readCameraModel(mDevId, &usbSpeed);

    if( mCameraModel==SL_DEVICE::NONE )
    {
        detail::log(verbose, VERBOSITY::WARNING, LOG_TAG, "The device '" + mDevName + "' is not a Stereolabs camera");
        return false;
    }

    if( mCameraModel==SL_DEVICE::ZED ||
            mCameraModel==SL_DEVICE::ZED_M )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "The FW of the device '" + mDevName + "' is not supported. Please update it.");
        mCameraModel = SL_DEVICE::NONE;
        return false;
    }

    // ----> Open
    struct stat st;
    memset(&st, 0, sizeof (struct stat));
    if (-1 == stat(mDevName.c_str(), &st))
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot identify", mDevName));
        return false;
    }

    if (!S_ISCHR(st.st_mode))
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, mDevName + " is no device");
        return false;
    }

    mFileDesc = ::open(mDevName.c_str(), O_RDWR|O_NONBLOCK,0); // Reading are non blocking

    if (-1 == mFileDesc)
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot open", mDevName));
        return false;
    }
    // <---- Open

    // From here the device must be released in case of error
    struct Guard {
        VideoCapture* cap; bool release;
        ~Guard(){ if(release) cap->closeDevice(); }
    } guard{this,true};

    // ----> Init
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof (v4l2_capability));

    if( -1==xioctl(mFileDesc, VIDIOC_QUERYCAP, &cap) )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot query capabilities of", mDevName));
        return false;
    }

    const __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    if( !(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING) )
    {
        // E.g. the metadata node that the driver creates for each camera
        detail::log(verbose, VERBOSITY::WARNING, LOG_TAG, "The device '" + mDevName + "' does not provide images");
        return false;
    }

    mSerialNumber = readSerialNumber(mFileDesc, 500);
    detail::log(verbose, VERBOSITY::INFO, LOG_TAG, "Opened camera with SN: " + std::to_string(mSerialNumber));

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof (v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    fmt.fmt.pix.width = reqWidth;
    fmt.fmt.pix.height = reqHeight;

    if( -1==xioctl(mFileDesc, VIDIOC_S_FMT, &fmt) )
    {
        std::string msg = errnoMsg("Cannot set pixel format of", mDevName);
        if( errno==EBUSY )
            msg += ". The camera is in use by another application";
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, msg);
        return false;
    }

    // Asked resolution not available, exiting
    if ( static_cast<int>(fmt.fmt.pix.width) != reqWidth || static_cast<int>(fmt.fmt.pix.height) != reqHeight ||
         fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV )
    {
        std::string msg = "Error setting the camera resolution: " + std::to_string(reqWidth) + "x" + std::to_string(reqHeight) +
                " is not available";
        if( usbSpeed>0 && usbSpeed<5000 )
            msg += ". The camera is connected in USB 2.0 mode: use a USB 3.0 port and cable to enable all the resolutions";
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, msg);
        return false;
    }

    mWidth = fmt.fmt.pix.width;
    mHeight = fmt.fmt.pix.height;
    mChannels = fmt.fmt.pix.bytesperline / mWidth;
    mFrameSize = static_cast<size_t>(mWidth) * mHeight * mChannels;

    if( mChannels!=2 || fmt.fmt.pix.sizeimage<mFrameSize )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "Unexpected image format from '" + mDevName + "'");
        return false;
    }

    // ----> Frame rate
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = mFps;

    if( -1==xioctl(mFileDesc, VIDIOC_S_PARM, &streamparm) )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "Error setting the camera framerate");
    }
    else if( streamparm.parm.capture.timeperframe.numerator>0 )
    {
        const int realFps = static_cast<int>(std::round(
                    static_cast<double>(streamparm.parm.capture.timeperframe.denominator)/streamparm.parm.capture.timeperframe.numerator));
        if( realFps>0 && realFps!=mFps )
        {
            std::string msg = "The camera cannot provide " + std::to_string(mFps) + " FPS at this resolution, using " +
                    std::to_string(realFps) + " FPS";
            if( usbSpeed>0 && usbSpeed<5000 )
                msg += ". The camera is connected in USB 2.0 mode: use a USB 3.0 port and cable to enable all the frame rates";
            detail::log(verbose, VERBOSITY::WARNING, LOG_TAG, msg);

            mFps = realFps;
            if( mFps <= 15 )
                mExpoureRawMax = EXP_RAW_MAX_15FPS;
            else if( mFps <= 30 )
                mExpoureRawMax = EXP_RAW_MAX_30FPS;
            else if( mFps <= 60 )
                mExpoureRawMax = EXP_RAW_MAX_60FPS;
            else
                mExpoureRawMax = EXP_RAW_MAX_100FPS;
        }
    }
    // <---- Frame rate

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof (v4l2_requestbuffers));

    req.count = UVC_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if( -1==xioctl(mFileDesc, VIDIOC_REQBUFS, &req) )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot request buffers for", mDevName));
        return false;
    }

    if( req.count<2 )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "Insufficient buffer memory on '" + mDevName + "'");
        return false;
    }

    // Create buffers
    mBuffers = static_cast<UVCBuffer*>(calloc(req.count, sizeof(*mBuffers)));
    if( !mBuffers )
    {
        detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "Out of memory");
        return false;
    }
    mBufCount = static_cast<uint8_t>(req.count);

    for(unsigned int i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof (v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if( -1==xioctl(mFileDesc, VIDIOC_QUERYBUF, &buf))
        {
            detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot query buffer for", mDevName));
            return false;
        }

        void* start = mmap(nullptr /* start anywhere */,
                           buf.length,
                           PROT_READ | PROT_WRITE /* required */,
                           MAP_SHARED /* recommended */,
                           mFileDesc, buf.m.offset);

        if( start==MAP_FAILED )
        {
            detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot map buffer for", mDevName));
            return false;
        }

        mBuffers[i].start = start;
        mBuffers[i].length = buf.length;

        if( buf.length<mFrameSize )
        {
            detail::log(verbose, VERBOSITY::ERROR, LOG_TAG, "Buffer too small on '" + mDevName + "'");
            return false;
        }
    }
    // <---- Init

    // ----> Output frames allocation
    {
        const std::lock_guard<std::mutex> lock(mBufMutex);
        for( std::vector<uint8_t>& img : mFrameData )
            img.assign(mFrameSize, 0);
        mWriteIdx = 0;
        mReadyIdx = 1;
        mUserIdx = 2;
        mNewFrame = false;
        mReadyFrameId = 0;
        mReadyTimestamp = 0;

        mLastFrame = Frame();
        mLastFrame.width = static_cast<uint16_t>(mWidth);
        mLastFrame.height = static_cast<uint16_t>(mHeight);
        mLastFrame.channels = static_cast<uint8_t>(mChannels);
        mLastFrame.data = mFrameData[mUserIdx].data();
    }
    // <---- Output frames allocation

    guard.release = false;
    return true;
}

int VideoCapture::getSerialNumber()
{
    if( mSerialNumber<0 && mFileDesc!=-1 )
    {
        const std::lock_guard<std::mutex> lock(mComMutex);
        mSerialNumber = readSerialNumber(mFileDesc, 100);
    }

    return mSerialNumber;
}

bool VideoCapture::startCapture()
{
    // ----> Start capturing
    for (unsigned int i = 0; i < mBufCount; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if( -1==xioctl(mFileDesc, VIDIOC_QBUF, &buf) )
        {
            detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, errnoMsg("Cannot queue buffer for", mDevName));
            return false;
        }
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( -1==xioctl(mFileDesc, VIDIOC_STREAMON, &type) )
    {
        std::string msg = errnoMsg("Cannot start streaming for", mDevName);
        if( errno==ENOSPC )
            msg += ". Not enough USB bandwidth: connect the camera to another USB controller or reduce resolution and FPS";
        detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, msg);
        return false;
    }
    // <---- Start capturing

    // The flags must be ready before the thread starts: `close` can be called at any time
    mStopCapture = false;
    mDeviceLost = false;
    mGrabThread = std::thread( &VideoCapture::grabThreadFunc,this );

    return true;
}

int VideoCapture::xioctl(int fd, uint64_t IOCTL_X, void *arg)
{
    int ret = 0;
    int tries = IOCTL_RETRY;
    do
    {
        ret = ioctl(fd, IOCTL_X, arg);
    } while (ret && tries-- &&
             ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));

    return (ret);
}

void VideoCapture::grabThreadFunc()
{
    struct pollfd pfd;
    pfd.fd = mFileDesc;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int errorCount = 0;
    int logFrameCount = 0;

    // A sync signal is assigned to a frame only if closer than half frame period
    const uint64_t syncMaxDistance = std::min<uint64_t>(SYNC_MAX_DISTANCE_NSEC, (NSEC_PER_SEC*45ULL/100ULL)/static_cast<uint64_t>(mFps>0?mFps:15));

    while (!mStopCapture)
    {
        if( errorCount>=GRAB_MAX_ERRORS )
        {
            detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, "Connection lost: '" + mDevName + "' does not provide frames");
            mDeviceLost = true;
            break;
        }

        // ----> Wait for a frame
        const int ready = poll(&pfd, 1, GRAB_POLL_TIMEOUT_MSEC);

        if( ready==0 )
            continue;

        if( ready<0 )
        {
            if( errno!=EINTR )
            {
                errorCount++;
                usleep(1000);
            }
            continue;
        }
        // <---- Wait for a frame

        struct v4l2_buffer buf;
        memset(&(buf), 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if( ioctl(mFileDesc, VIDIOC_DQBUF, &buf)!=0 )
        {
            if( errno==ENODEV )
            {
                errorCount = GRAB_MAX_ERRORS; // Camera unplugged
            }
            else if( errno!=EAGAIN && errno!=EINTR )
            {
                errorCount++;
                usleep(1000);
            }
            else if( pfd.revents & (POLLERR|POLLHUP|POLLNVAL) )
            {
                // Nothing to read and the device reports an error: avoid a busy loop
                errorCount++;
                usleep(5000);
            }
            continue;
        }

        const bool valid = buf.index<mBufCount && !(buf.flags & V4L2_BUF_FLAG_ERROR) && buf.bytesused>=mFrameSize;

        if (valid)
        {
            errorCount = 0;

            // ----> Timestamp
            // The driver stores the time of arrival of the first data of the frame, using the monotonic clock
            const uint64_t now = getTimestampNow();
            uint64_t ts = now;

            if( (buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK)==V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC )
            {
                const uint64_t ts_uvc = static_cast<uint64_t>(buf.timestamp.tv_sec)*NSEC_PER_SEC +
                        static_cast<uint64_t>(buf.timestamp.tv_usec)*1000ULL;
                const uint64_t ts_frame = steadyToTimestamp(ts_uvc);

                if( ts_frame<=now && now-ts_frame<NSEC_PER_SEC )
                    ts = ts_frame;
            }
            // <---- Timestamp

            // The image is written without locking: this buffer is not visible to the user
            memcpy(mFrameData[mWriteIdx].data(), mBuffers[buf.index].start, mFrameSize);

            {
                const std::lock_guard<std::mutex> lock(mBufMutex);

                if( mSyncLink )
                {
                    // The driver timestamp and the sync signal both mark the start of the frame, but the signal is
                    // timestamped by the sensors module: using it removes the jitter between frames and sensors data
                    uint64_t pulse = 0;
                    if( mSyncLink->findPulse(ts, syncMaxDistance, pulse) )
                        ts = pulse;
                }

                // Timestamps never go backwards
                if( ts<=mReadyTimestamp )
                    ts = mReadyTimestamp+1;

                std::swap(mWriteIdx, mReadyIdx);
                mReadyFrameId = buf.sequence;
                mReadyTimestamp = ts;
                mNewFrame = true;
            }
            mFrameCond.notify_all();

            // ----> AEC/AGC register logging
            if(mLogEnable)
            {
                if((++logFrameCount)>=mLogFrameSkip)
                    logFrameCount = 0;

                if(logFrameCount==0)
                {
                    saveLogDataLeft(ts);
                    saveLogDataRight(ts);
                }
            }
            // <---- AEC/AGC register logging
        }

        if( ioctl(mFileDesc, VIDIOC_QBUF, &buf)!=0 )
        {
            errorCount++;
        }
    }

    mFrameCond.notify_all();
}

const Frame& VideoCapture::getLastFrame( uint64_t timeout_msec )
{
    std::unique_lock<std::mutex> lock(mBufMutex);

    // The full timeout is also used when the camera is closed or disconnected: a loop that calls this function
    // without checking `isOpened` must not keep a CPU core busy
    mFrameCond.wait_for(lock, std::chrono::milliseconds(timeout_msec), [this]{ return mNewFrame; });

    if( mNewFrame )
    {
        // The image given to the user is no longer touched by the grabbing thread
        std::swap(mUserIdx, mReadyIdx);
        mNewFrame = false;

        mLastFrame.data = mFrameData[mUserIdx].data();
        mLastFrame.frame_id = mReadyFrameId;
        mLastFrame.timestamp = mReadyTimestamp;
    }

    return mLastFrame;
}

int VideoCapture::ll_VendorControl(uint8_t *buf, int readMode, bool safe, bool force)
{
    if (!force && !mInitialized)
        return -3;

    if( mFileDesc==-1 )
        return -3;

    const std::lock_guard<std::mutex> lock(mComMutex);

    std::string error;
    const int res = xuControl(mFileDesc, buf, readMode==READ_MODE, safe, &error);

    if( res==-1 )
    {
        detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, error);
    }

    return res;
}

/**
 * @brief cbs_set_gpio_value
 * @param gpio_number (0 to 4)
 * @param value(0x00: low, 0x01 : high)
 * @return 0 if success
 */
int VideoCapture::ll_get_gpio_value(int gpio_number, uint8_t *value)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_GET;
    xu_buf[1] = 0x13;
    xu_buf[2] = gpio_number;

    int hr = ll_VendorControl(xu_buf, READ_MODE);
    *value = xu_buf[17];
    return hr;
}

/**
 * @brief cbs_set_gpio_value
 * @param gpio_number (0 to 4)
 * @param value(0x00: low, 0x01 : high)
 * @return 0 if success
 */
int VideoCapture::ll_set_gpio_value(int gpio_number, uint8_t value)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_SET;
    xu_buf[1] = 0x12;
    xu_buf[2] = gpio_number;
    xu_buf[3] = value;

    int hr = ll_VendorControl(xu_buf, 0);
    return hr;
}

/**
 * @brief cbs_set_gpio_direction
 * @param gpio_number (0 to 4)
 * @param direction (0x00 output, 0x01 input)
 * @return  0 if success
 */
int VideoCapture::ll_set_gpio_direction(int gpio_number, int direction)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_SET;
    xu_buf[1] = 0x10;
    xu_buf[2] = gpio_number;
    xu_buf[3] = direction;

    int hr = ll_VendorControl(xu_buf, 0);
    return hr;
}

int VideoCapture::ll_read_system_register(uint64_t address, uint8_t *value)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_GET;
    xu_buf[1] = 0xA2;
    xu_buf[2] = 0;
    xu_buf[3] = 0x04; //Address width in bytes
    xu_buf[4] = 0x01; //data width in bytes

    xu_buf[5] = ((address) >> 24) & 0xff;
    xu_buf[6] = ((address) >> 16) & 0xff;
    xu_buf[7] = ((address) >> 8) & 0xff;
    xu_buf[8] = (address) & 0xff;
    xu_buf[9] = 0x00;
    xu_buf[10] = 0x01;
    xu_buf[11] = 0x00;
    xu_buf[12] = 0x01;

    int hr = ll_VendorControl(xu_buf, READ_MODE);
    *value = xu_buf[17];
    return hr;
}

int VideoCapture::ll_write_system_register(uint64_t address, uint8_t value)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_SET;
    xu_buf[1] = 0xA2;
    xu_buf[2] = 0;
    xu_buf[3] = 0x04; //Address width in bytes
    xu_buf[4] = 0x01; //data width in bytes

    xu_buf[5] = ((address) >> 24) & 0xff;
    xu_buf[6] = ((address) >> 16) & 0xff;
    xu_buf[7] = ((address) >> 8) & 0xff;
    xu_buf[8] = (address) & 0xff;
    xu_buf[9] = 0x00;
    xu_buf[10] = 0x01;
    xu_buf[11] = 0x00;
    xu_buf[12] = 0x01;
    xu_buf[16] = value;

    int hr = ll_VendorControl(xu_buf, 0);
    return hr;
}

#define ASIC_INT_NULL_I2C    0xa3
#define ASIC_INT_I2C         0xa5

int VideoCapture::ll_read_sensor_register(int side, int sscb_id, uint64_t address, uint8_t* value)
{
    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);


    //Set xubuf
    xu_buf[0] = XU_TASK_GET;
    if (side == 0)
        xu_buf[1] = ASIC_INT_NULL_I2C;
    else
        xu_buf[1] = ASIC_INT_I2C;
    xu_buf[2] = 0x6c;
    xu_buf[3] = sscb_id + 1; //Address width in bytes
    xu_buf[4] = 0x01; //data width in bytes

    xu_buf[5] = ((address) >> 24) & 0xff;
    xu_buf[6] = ((address) >> 16) & 0xff;
    xu_buf[7] = ((address) >> 8) & 0xff;
    xu_buf[8] = (address) & 0xff;

    xu_buf[9] = 0x00;
    xu_buf[10] = 0x01;
    xu_buf[11] = 0x00;
    xu_buf[12] = 0x01;

    int limit = 1;

    xu_buf[9] = (limit >> 8) & 0xff;
    xu_buf[10] = (limit >> 0) & 0xff;

    //set page addr
    xu_buf[9] = xu_buf[9] & 0x0f;
    xu_buf[9] = xu_buf[9] | 0x10;
    xu_buf[9] = xu_buf[9] | 0x80;

    int hr = ll_VendorControl(xu_buf, READ_MODE);
    *value = xu_buf[17];
    return hr;
}

bool VideoCapture::waitSensorsReady(int timeout_msec)
{
    const uint64_t start = getSteadyTimestamp();
    int good = 0;
    while( (getSteadyTimestamp()-start) < static_cast<uint64_t>(timeout_msec)*1000000ULL )
    {
        bool ok = true;
        for( int side=0; side<2 && ok; side++ )
        {
            uint8_t hi=0, lo=0;
            ok = ll_read_sensor_register(side, 1, ADDR_CHIP_ID_H, &hi)==0 &&
                 ll_read_sensor_register(side, 1, ADDR_CHIP_ID_L, &lo)==0 &&
                 ((hi<<8)|lo)==OV4689_CHIP_ID;
        }
        // Replies are unreliable around the transition: require consecutive matches
        good = ok ? good+1 : 0;
        if(good>=3)
            return true;
        usleep(2000);
    }
    return false;
}

int VideoCapture::ll_write_sensor_register(int side, int sscb_id, uint64_t address, uint8_t value)
{

    unsigned char xu_buf[XU_BUF_SIZE];
    memset(xu_buf, 0, XU_BUF_SIZE);

    //Set xubuf
    xu_buf[0] = XU_TASK_SET;
    if (side == 0)
        xu_buf[1] = ASIC_INT_NULL_I2C;
    else
        xu_buf[1] = ASIC_INT_I2C;
    xu_buf[2] = 0x6c;
    xu_buf[3] = sscb_id + 1; //Address width in bytes
    xu_buf[4] = 0x01; //data width in bytes

    xu_buf[5] = ((address) >> 24) & 0xff;
    xu_buf[6] = ((address) >> 16) & 0xff;
    xu_buf[7] = ((address) >> 8) & 0xff;
    xu_buf[8] = (address) & 0xff;
    xu_buf[9] = 0x00;
    xu_buf[10] = 0x01;
    xu_buf[11] = 0x00;
    xu_buf[12] = 0x01;
    xu_buf[16] = value;


    xu_buf[9] = 0x00;
    xu_buf[10] = 0x01;
    xu_buf[11] = 0x00;
    xu_buf[12] = 0x01;

    int limit = 1;
    xu_buf[9] = (limit >> 8) & 0xff;
    xu_buf[10] = (limit >> 0) & 0xff;

    //set page addr
    xu_buf[9] = xu_buf[9] & 0x0f;
    xu_buf[9] = xu_buf[9] | 0x10;
    xu_buf[9] = xu_buf[9] | 0x80;
    memcpy(&xu_buf[16], &value, sizeof (uint8_t));
    int hr = ll_VendorControl(xu_buf, 0);

    return hr;
}

int VideoCapture::ll_isp_aecagc_enable(int side, bool enable) {
    uint64_t Address = 0;
    uint8_t value = 0;
    if (side == 0)
        Address = ISP_CTRL_LEFT; //ISP L
    else if (side == 1)
        Address = ISP_CTRL_RIGHT; //ISP R
    else
        return -2;

    //Read Current sysregister: the other bits must be preserved
    int hr = ll_read_system_register(Address, &value);
    if( hr!=0 )
        return hr;

    // Adjust Value
    if (enable)
        value = value | AEG_AGC_MASK_ON;
    else
        value = value & AEG_AGC_MASK_OFF;

    return ll_write_system_register(Address, value);
}


int VideoCapture::ll_isp_is_aecagc(int side) {
    uint64_t Address = 0;
    uint8_t value = 0;
    int result = 0;
    int hr = 0;
    if (side == 0)
        Address = ISP_CTRL_LEFT; //ISP L
    else if (side == 1)
        Address = ISP_CTRL_RIGHT; //ISP R
    else
        return -2;

    //Read Current sysregister
    hr += ll_read_system_register(Address, &value);

    // Adjust Value
    if (hr == 0)
        result = ((value & AEG_AGC_MASK_ON) == AEG_AGC_MASK_ON); //check that second bit is on
    else
        return hr;

    return result;
}

int VideoCapture::ll_isp_get_gain(uint8_t *val, uint8_t sensorID) {
    int hr = 0;
    uint8_t buffL, buffM, buffH;

    hr += ll_read_sensor_register(sensorID, 1, ADDR_GAIN_H, &buffH);
    hr += ll_read_sensor_register(sensorID, 1, ADDR_GAIN_M, &buffM);
    hr += ll_read_sensor_register(sensorID, 1, ADDR_GAIN_L, &buffL);

    *val = buffL;
    *(val + 1) = buffM;
    *(val + 2) = buffH;

    return hr;
}

int VideoCapture::ll_isp_set_gain(unsigned char ucGainH, unsigned char ucGainM, unsigned char ucGainL, int sensorID)
{
    int hr = 0;
    hr += ll_write_sensor_register(sensorID, 1, ADDR_GAIN_H, ucGainH);
    hr += ll_write_sensor_register(sensorID, 1, ADDR_GAIN_M, ucGainM);
    hr += ll_write_sensor_register(sensorID, 1, ADDR_GAIN_L, ucGainL);
    return hr;
}

int VideoCapture::ll_isp_get_exposure(unsigned char *val, unsigned char sensorID)
{
    int hr = 0;
    uint8_t buffL = 0;
    uint8_t buffM = 0;
    uint8_t buffH = 0;

    hr += ll_read_sensor_register(sensorID, 1, ADDR_EXP_H, (uint8_t*) & buffH);
    usleep(10);
    hr += ll_read_sensor_register(sensorID, 1, ADDR_EXP_M, (uint8_t*) & buffM);
    usleep(10);
    hr += ll_read_sensor_register(sensorID, 1, ADDR_EXP_L, (uint8_t*) & buffL);

    *val = buffL;
    *(val + 1) = buffM;
    *(val + 2) = buffH;

    return hr;
}

int VideoCapture::ll_isp_set_exposure(unsigned char ucExpH, unsigned char ucExpM, unsigned char ucExpL, int sensorID)
{
    int hr = 0;
    hr += ll_write_sensor_register(sensorID, 1, ADDR_EXP_H, ucExpH);
    hr += ll_write_sensor_register(sensorID, 1, ADDR_EXP_M, ucExpM);
    hr += ll_write_sensor_register(sensorID, 1, ADDR_EXP_L, ucExpL);
    return hr;
}

void VideoCapture::ll_activate_sync()
{
    uint8_t sync_val_left = 0x0;
    uint8_t sync_val_right = 0x0;

    // Activate VSYNC output for both camera sensors
    if (ll_read_sensor_register(0, 1, 0x3002, &sync_val_left) == 0)
    {
        sync_val_left = sync_val_left | 0x80;

        ll_write_sensor_register(0, 1, 0x3002, sync_val_left);
    }

    if (ll_read_sensor_register(1, 1, 0x3002, &sync_val_right) == 0)
    {
        sync_val_right = sync_val_right | 0x80;

        ll_write_sensor_register(1, 1, 0x3002, sync_val_right);
    }
}

bool VideoCapture::setLEDstatus(bool status)
{
    //LED GPIO : GPIO 2
    int hr = ll_set_gpio_direction(2, 0);
    hr += ll_set_gpio_value(2, status?1:0);

    return hr==0;
}

bool VideoCapture::getLEDstatus(bool *status)
{
    if( status==nullptr)
    {
        return false;
    }

    uint8_t val = 0;
    int hr = ll_set_gpio_direction(2, 1);
    hr += ll_get_gpio_value(2, &val);
    if( hr!=0 )
        return false;

    *status = val!=0;
    return true;
}

bool VideoCapture::toggleLED(bool* value)
{
    bool curVal = false;

    if( !getLEDstatus( &curVal ) )
        return false;

    const bool newVal = !curVal;
    if( !setLEDstatus( newVal ) )
        return false;

    if( value!=nullptr )
    {
        *value = newVal;
    }

    return true;
}

int VideoCapture::getCameraControlSettings(int ctrl_id)
{
    if( mFileDesc==-1 )
        return -1;

    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof (queryctrl));
    memset(&control_s, 0, sizeof (control_s));
    int res = -1;

    queryctrl.id = ctrl_id;

    if (0 != ioctl(mFileDesc, VIDIOC_QUERYCTRL, &queryctrl))
        return res;

    control_s.id = ctrl_id;
    if (ioctl(mFileDesc, VIDIOC_G_CTRL, &control_s) == 0)
        res = (int) control_s.value;

    return res;
}

bool VideoCapture::setCameraControlSettings(int ctrl_id, int ctrl_val) {
    if( mFileDesc==-1 )
        return false;

    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof (queryctrl));
    memset(&control_s, 0, sizeof (control_s));

    queryctrl.id = ctrl_id;

    if( 0!=ioctl(mFileDesc, VIDIOC_QUERYCTRL, &queryctrl) )
        return false;

    if( ctrl_val<queryctrl.minimum || ctrl_val>queryctrl.maximum )
        return false;

    control_s.id = ctrl_id;
    control_s.value = ctrl_val;

    return ioctl(mFileDesc, VIDIOC_S_CTRL, &control_s) == 0;
}

bool VideoCapture::resetCameraControlSettings(int ctrl_id) {
    if( mFileDesc==-1 )
        return false;

    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    memset(&queryctrl, 0, sizeof (queryctrl));
    memset(&control_s, 0, sizeof (control_s));

    queryctrl.id = ctrl_id;
    if( 0!=ioctl(mFileDesc, VIDIOC_QUERYCTRL, &queryctrl) )
        return false;

    control_s.id = ctrl_id;
    control_s.value = queryctrl.default_value;
    return ioctl(mFileDesc, VIDIOC_S_CTRL, &control_s) == 0;
}

int VideoCapture::setGammaPreset(int side, int value)
{
    if (!mInitialized)
        return -1;

    if(value < DEFAULT_MIN_GAMMA)
        value = DEFAULT_MIN_GAMMA;
    if(value > DEFAULT_MAX_GAMMA)
        value = DEFAULT_MAX_GAMMA;

    const uint64_t base = (side == 1) ? ISP_GAMMA_RIGHT : ISP_GAMMA_LEFT;

    int hr = 0;

    for (int i = 0; i < ISP_GAMMA_VALUES; i++) {
        hr += ll_write_system_register(base+i, PRESET_GAMMA[value-1][i]);
        usleep(10);
        uint8_t valRead = 0x0;
        hr += ll_read_system_register(base+i, &valRead);
        if (valRead != PRESET_GAMMA[value-1][i]) {
            return -3;
        }
    }

    // Enable the custom curve
    hr += ll_write_system_register(base+ISP_GAMMA_EN_OFFSET, 0x01);
    usleep(10);
    uint8_t valRead = 0x0;
    hr += ll_read_system_register(base+ISP_GAMMA_EN_OFFSET, &valRead);
    if (valRead != 0x01)
        return -2;

    return hr;
}

bool VideoCapture::setBrightness(int brightness)
{
    return setCameraControlSettings(LINUX_CTRL_BRIGHTNESS, brightness);
}

bool VideoCapture::resetBrightness()
{
    return resetCameraControlSettings(LINUX_CTRL_BRIGHTNESS);
}

int VideoCapture::getBrightness()
{
    return getCameraControlSettings(LINUX_CTRL_BRIGHTNESS);
}

bool VideoCapture::setSharpness(int sharpness)
{
    return setCameraControlSettings(LINUX_CTRL_SHARPNESS, sharpness);
}

bool VideoCapture::resetSharpness()
{
    return resetCameraControlSettings(LINUX_CTRL_SHARPNESS);
}

int VideoCapture::getSharpness()
{
    return getCameraControlSettings(LINUX_CTRL_SHARPNESS);
}

bool VideoCapture::setContrast(int contrast)
{
    return setCameraControlSettings(LINUX_CTRL_CONTRAST, contrast);
}

bool VideoCapture::resetContrast()
{
    return resetCameraControlSettings(LINUX_CTRL_CONTRAST);
}

int VideoCapture::getContrast()
{
    return getCameraControlSettings(LINUX_CTRL_CONTRAST);
}

bool VideoCapture::setHue(int hue)
{
    return setCameraControlSettings(LINUX_CTRL_HUE, hue);
}

bool VideoCapture::resetHue()
{
    return resetCameraControlSettings(LINUX_CTRL_HUE);
}

int VideoCapture::getHue()
{
    return getCameraControlSettings(LINUX_CTRL_HUE);
}

bool VideoCapture::setSaturation(int saturation)
{
    return setCameraControlSettings(LINUX_CTRL_SATURATION, saturation);
}

bool VideoCapture::resetSaturation()
{
    return resetCameraControlSettings(LINUX_CTRL_SATURATION);
}

int VideoCapture::getSaturation()
{
    return getCameraControlSettings(LINUX_CTRL_SATURATION);
}

int VideoCapture::getWhiteBalance()
{
    return getCameraControlSettings(LINUX_CTRL_AWB);
}

bool VideoCapture::setWhiteBalance(int wb)
{
    // Disable auto white balance if active
    if (getAutoWhiteBalance())
        setAutoWhiteBalance(false);

    return setCameraControlSettings(LINUX_CTRL_AWB, wb);
}

bool VideoCapture::getAutoWhiteBalance()
{
    return (getCameraControlSettings(LINUX_CTRL_AWB_AUTO)>0);
}

bool VideoCapture::setAutoWhiteBalance(bool active)
{
    return setCameraControlSettings(LINUX_CTRL_AWB_AUTO, active?1:0);
}

bool VideoCapture::resetAutoWhiteBalance()
{
    return setAutoWhiteBalance(true);
}

bool VideoCapture::setGamma(int gamma)
{
    if( gamma<DEFAULT_MIN_GAMMA || gamma>DEFAULT_MAX_GAMMA )
        return false;

    const int resL = setGammaPreset(0,gamma);
    const int resR = setGammaPreset(1,gamma);

    return resL==0 && resR==0;
}

bool VideoCapture::resetGamma()
{
    // Without the custom curve the camera uses its factory gamma, that matches the first preset
    int hr = ll_write_system_register(ISP_GAMMA_LEFT+ISP_GAMMA_EN_OFFSET, 0x00);
    hr += ll_write_system_register(ISP_GAMMA_RIGHT+ISP_GAMMA_EN_OFFSET, 0x00);

    return hr==0;
}

int VideoCapture::getGamma() {
    uint8_t enabled = 0;
    if( ll_read_system_register(ISP_GAMMA_LEFT+ISP_GAMMA_EN_OFFSET, &enabled)!=0 )
        return -1;

    if( enabled==0 )
        return DEFAULT_GAMMA_NOECT;

    // The value is not stored by the camera: search for the preset that matches the curve in use
    uint8_t curve[ISP_GAMMA_VALUES];
    for( int i=0; i<ISP_GAMMA_VALUES; i++ )
    {
        if( ll_read_system_register(ISP_GAMMA_LEFT+i, &curve[i])!=0 )
            return -1;
    }

    for( int preset=DEFAULT_MIN_GAMMA; preset<=DEFAULT_MAX_GAMMA; preset++ )
    {
        if( memcmp(curve, PRESET_GAMMA[preset-1], ISP_GAMMA_VALUES)==0 )
            return preset;
    }

    return -1;
}

bool VideoCapture::setAECAGC(bool active)
{
    const bool wasOn = !active && (ll_isp_is_aecagc(0)==1 || ll_isp_is_aecagc(1)==1);

    int res = 0;
    res += ll_isp_aecagc_enable(0, active);
    res += ll_isp_aecagc_enable(1, active);

    // After AEC is disabled the ISP can still apply a pending update ~3 frames later
    // (measured 205-215 ms at 15 fps), overwriting manual values written in between
    if( wasOn && mFps>0 )
        usleep( 5 * 1000000 / mFps );

    return res==0;
}

bool VideoCapture::getAECAGC()
{
    // A negative value indicates a communication error
    return ll_isp_is_aecagc(0)==1 && ll_isp_is_aecagc(1)==1;
}

bool VideoCapture::resetAECAGC()
{
    return setAECAGC(true);
}

bool VideoCapture::setROIforAECAGC(CAM_SENS_POS side, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if(side!=CAM_SENS_POS::LEFT && side!=CAM_SENS_POS::RIGHT)
    {
        return false;
    }
    if(w==0 || h==0)
    {
        return false;
    }
    if((x+w)>(mWidth/2) || (y+h)>mHeight)
    {
        return false;
    }
    if(w*h<=100)
    {
        return false;
    }

    // High byte first: X, Y, width, height
    const uint16_t values[4] = {x, y, w, h};

    uint32_t ulAddr = (side==CAM_SENS_POS::RIGHT) ? ISP_ROI_RIGHT : ISP_ROI_LEFT;

    int r = 0;
    for( int i=0; i<4; i++ )
    {
        r += ll_write_system_register(ulAddr++,static_cast<uint8_t>(values[i]>>8));usleep(100);
        r += ll_write_system_register(ulAddr++,static_cast<uint8_t>(values[i]&0xff));usleep(100);
    }

    return (r==0);
}

bool VideoCapture::resetROIforAECAGC(CAM_SENS_POS side)
{
    return setROIforAECAGC(side, 0,0, mWidth/2, mHeight);
}

bool VideoCapture::getROIforAECAGC(CAM_SENS_POS side, uint16_t &x, uint16_t &y, uint16_t &w, uint16_t &h)
{
    if(side!=CAM_SENS_POS::LEFT && side!=CAM_SENS_POS::RIGHT)
    {
        return false;
    }

    uint32_t ulAddr = (side==CAM_SENS_POS::RIGHT) ? ISP_ROI_RIGHT : ISP_ROI_LEFT;

    uint16_t values[4] = {0,0,0,0};

    int r = 0;
    for( int i=0; i<4; i++ )
    {
        uint8_t high = 0;
        uint8_t low = 0;
        r += ll_read_system_register(ulAddr++,&high);usleep(100);
        r += ll_read_system_register(ulAddr++,&low);usleep(100);
        values[i] = static_cast<uint16_t>(high*256 + low);
    }

    if( r!=0 )
        return false;

    x = values[0];
    y = values[1];
    w = values[2];
    h = values[3];

    return true;
}

bool VideoCapture::setGain(CAM_SENS_POS cam, int gain)
{
    if(cam!=CAM_SENS_POS::LEFT && cam!=CAM_SENS_POS::RIGHT)
        return false;

    if(getAECAGC())
        setAECAGC(false);

    if (gain <= DEFAULT_MIN_GAIN)
        gain = DEFAULT_MIN_GAIN;
    else if (gain >= DEFAULT_MAX_GAIN)
        gain = DEFAULT_MAX_GAIN;

    uint8_t ucGainH=0, ucGainM=0, ucGainL=0;

    int rawGain = calcRawGainValue(gain);

    int sensorId = static_cast<int>(cam);

    ucGainM = (rawGain >> 8) & 0xff;
    ucGainL = rawGain & 0xff;
    return ll_isp_set_gain(ucGainH, ucGainM, ucGainL, sensorId)==0;
}

int VideoCapture::getGain(CAM_SENS_POS cam)
{
    if(cam!=CAM_SENS_POS::LEFT && cam!=CAM_SENS_POS::RIGHT)
        return -1;

    int rawGain=0;

    uint8_t val[3];
    memset(val, 0, 3);

    int sensorId = static_cast<int>(cam);
    int r = ll_isp_get_gain(val, sensorId);
    if(r<0)
        return r;

    rawGain = (int) ((val[1] << 8) + val[0]);
    return calcGainValue(rawGain);
}

bool VideoCapture::setExposure(CAM_SENS_POS cam, int exposure)
{
    if(cam!=CAM_SENS_POS::LEFT && cam!=CAM_SENS_POS::RIGHT)
        return false;

    unsigned char ucExpH, ucExpM, ucExpL;

    if(getAECAGC())
        setAECAGC(false);

    if(exposure < DEFAULT_MIN_EXP)
        exposure = DEFAULT_MIN_EXP;
    if(exposure > DEFAULT_MAX_EXP)
        exposure = DEFAULT_MAX_EXP;

    int rawExp = (mExpoureRawMax * ((float) exposure / 100.0));
    if(rawExp<EXP_RAW_MIN)
        rawExp = EXP_RAW_MIN;

    int sensorId = static_cast<int>(cam);

    ucExpH = (rawExp >> 12) & 0xff;
    ucExpM = (rawExp >> 4) & 0xff;
    ucExpL = (rawExp << 4) & 0xf0;
    return ll_isp_set_exposure(ucExpH, ucExpM, ucExpL, sensorId)==0;
}

int VideoCapture::getExposure(CAM_SENS_POS cam)
{
    if(cam!=CAM_SENS_POS::LEFT && cam!=CAM_SENS_POS::RIGHT)
        return -1;

    int rawExp=0;

    unsigned char val[3];
    memset(val, 0, 3);

    int sensorId = static_cast<int>(cam);

    int r = ll_isp_get_exposure(val, sensorId);
    if(r<0)
        return r;
    rawExp = (int) ((val[2] << 12) + (val[1] << 4) + (val[0] >> 4));

    int exposure = static_cast<int>(std::round((100.0*rawExp)/mExpoureRawMax));
    return exposure;
}

int VideoCapture::calcRawGainValue(int gain) {

    // From [0,100] to segmented gain
    int segmentedGain = static_cast<int>(std::round(mGainSegMax * (static_cast<double>(gain) / 100.0)));
    int rawGain = 0;

    // ----> Calculate correct GAIN ZONE
    int gainZone1nbVal = GAIN_ZONE1_MAX - GAIN_ZONE1_MIN;
    int gainZone2nbVal = GAIN_ZONE2_MAX - GAIN_ZONE2_MIN;
    int gainZone3nbVal = GAIN_ZONE3_MAX - GAIN_ZONE3_MIN;
    int gainZone4nbVal = GAIN_ZONE4_MAX - GAIN_ZONE4_MIN;

    if (segmentedGain <= gainZone1nbVal)
        rawGain = segmentedGain + GAIN_ZONE1_MIN;
    else if (segmentedGain <= gainZone1nbVal + gainZone2nbVal)
        rawGain = segmentedGain + GAIN_ZONE2_MIN - (gainZone1nbVal);
    else if (segmentedGain <= gainZone1nbVal + gainZone2nbVal + gainZone3nbVal)
        rawGain = segmentedGain + GAIN_ZONE3_MIN - (gainZone1nbVal + gainZone2nbVal);
    else if (segmentedGain <= gainZone1nbVal + gainZone2nbVal + gainZone3nbVal + gainZone4nbVal)
        rawGain = segmentedGain + GAIN_ZONE4_MIN - (gainZone1nbVal + gainZone2nbVal + gainZone3nbVal);
    // <---- Calculate correct GAIN ZONE

    return rawGain;
}

int VideoCapture::calcGainValue(int rawGain)
{
    int segmentedGain;

    // ----> Calculate correct GAIN ZONE
    int gainZone1nbVal = GAIN_ZONE1_MAX - GAIN_ZONE1_MIN;
    int gainZone2nbVal = GAIN_ZONE2_MAX - GAIN_ZONE2_MIN;
    int gainZone3nbVal = GAIN_ZONE3_MAX - GAIN_ZONE3_MIN;
    if (rawGain >= GAIN_ZONE1_MIN && rawGain <= GAIN_ZONE1_MAX)
        segmentedGain = rawGain - GAIN_ZONE1_MIN;
    else if (rawGain >= GAIN_ZONE2_MIN && rawGain <= GAIN_ZONE2_MAX)
        segmentedGain = rawGain - GAIN_ZONE2_MIN + gainZone1nbVal;
    else if (rawGain >= GAIN_ZONE3_MIN && rawGain <= GAIN_ZONE3_MAX)
        segmentedGain = rawGain - GAIN_ZONE3_MIN + gainZone1nbVal + gainZone2nbVal;
    else if (rawGain >= GAIN_ZONE4_MIN && rawGain <= GAIN_ZONE4_MAX)
        segmentedGain = rawGain - GAIN_ZONE4_MIN + gainZone1nbVal + gainZone2nbVal + gainZone3nbVal;
    else
        segmentedGain = -1;
    // <---- Calculate correct GAIN ZONE

    // From segmented gain to [0,100]
    int gain = static_cast<int>(std::round((100.0*segmentedGain)/mGainSegMax));

    return gain;
}

std::string VideoCapture::getCurrentDateTime( DateTime type )
{
    time_t now = time(nullptr);
    struct tm  tstruct;
    char  buf[80];
    memset(buf, 0, sizeof(buf));
    localtime_r(&now, &tstruct);
    if(type==FULL)
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    else if(type==DATE)
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    else if(type==TIME)
        strftime(buf, sizeof(buf), "%X", &tstruct);
    return std::string(buf);
}

bool VideoCapture::enableAecAgcSensLogging(bool enable, int frame_skip/*=10*/)
{
    if(!enable)
    {
        mLogEnable=false;
        if(mLogFileLeft.is_open())
        {
            mLogFileLeft.close();
        }
        if(mLogFileRight.is_open())
        {
            mLogFileRight.close();
        }
        return true;
    }

    mLogFrameSkip = frame_skip;

    mLogFilenameLeft = getCurrentDateTime(DATE);
    mLogFilenameLeft += "_";
    mLogFilenameLeft += getCurrentDateTime(TIME);
    mLogFilenameLeft += "_agc_aec_registers-LEFT.csv";

    mLogFilenameRight = getCurrentDateTime(DATE);
    mLogFilenameRight += "_";
    mLogFilenameRight += getCurrentDateTime(TIME);
    mLogFilenameRight += "_agc_aec_registers-RIGHT.csv";

    mLogFileLeft.open(mLogFilenameLeft, std::ofstream::out );
    mLogFileRight.open(mLogFilenameRight, std::ofstream::out );

    if(mLogFileLeft.bad())
    {
        std::cerr << "Logging not started. Error creating the log file: '" << mLogFilenameLeft << "'" << std::endl;
        mLogEnable = false;
        return false;
    }

    if(mLogFileRight.bad())
    {
        std::cerr << "Logging not started. Error creating the log file: '" << mLogFilenameRight << "'" << std::endl;
        mLogEnable = false;
        mLogFileLeft.close();
        return false;
    }

    mLogFileLeft << "TIMESTAMP" << LOG_SEP;
    mLogFileLeft << "OV580-ISP_EN_HIGH" << LOG_SEP;
    mLogFileLeft << "OV580-yavg_low" << LOG_SEP;
    mLogFileLeft << "OV580-yavg_high" << LOG_SEP;
    mLogFileLeft << "OV580-interrupt_ctrl1";

    mLogFileRight << "TIMESTAMP" << LOG_SEP;
    mLogFileRight << "OV580-ISP_EN_HIGH" << LOG_SEP;
    mLogFileRight << "OV580-yavg_low" << LOG_SEP;
    mLogFileRight << "OV580-yavg_high" << LOG_SEP;
    mLogFileRight << "OV580-interrupt_ctrl1";

    for (int addr = 0x00; addr <= 0x22; ++addr)
    {
        mLogFileLeft << LOG_SEP << "OV580-YAVG[0x" << std::hex << std::setfill('0') << std::setw(2) << addr << "]";
        mLogFileRight << LOG_SEP << "OV580-YAVG[0x" << std::hex << std::setfill('0') << std::setw(2) << addr << "]";
    }

    for (int addr = 0x3500; addr <= 0x3515; ++addr)
    {
        mLogFileLeft << LOG_SEP << "OV4689-GAIN_EXP[0x" << std::hex << std::setfill('0') << std::setw(4) << addr << "]";
        mLogFileRight << LOG_SEP << "OV4689-GAIN_EXP[0x" << std::hex << std::setfill('0') << std::setw(4) << addr << "]";
    }

    mLogFileLeft << std::endl;
    mLogFileRight << std::endl;

    mLogEnable = true;
    return true;
}

void VideoCapture::setColorBars(int side, bool c)
{


    unsigned long long ulAddr2 = 0x80181080;

    if (side==1)
         ulAddr2 = 0x80181880;

    unsigned char ulValue2 =c?128:0;
    ll_write_system_register(ulAddr2,ulValue2);
}


void VideoCapture::saveAllISPRegisters(std::string filename)
{
    std::ofstream logFile;
    logFile.open(filename,std::ofstream::out);
    unsigned long long ulAddrL =  0x80181000;
    unsigned long long   ulAddrR =  0x80181800;

    for (int p = 0;p<0x800;p++)
    {
        uint8_t valL=0,valR=0;
        ll_read_system_register( ulAddrL+p, &valL);
        ll_read_system_register( ulAddrR+p, &valR);

        logFile <<"0x" << std::hex << std::setfill('0') << std::setw(8) << static_cast<unsigned long long>(ulAddrL+p)<<" , "<<std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned long long>(valL)<<" , "<<std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned long long>(valR)<<std::endl;
        usleep(10);
    }

    logFile.close();
}

void VideoCapture::saveAllSensorsRegisters(std::string filename)
{
    std::ofstream logFile;
    logFile.open(filename,std::ofstream::out);

    for (int addr = 0x3000; addr <= 0x6000; ++addr)
    {
        uint8_t valL=0,valR=0;
        ll_read_sensor_register( 0, 1, addr, &valL);
        ll_read_sensor_register( 1, 1, addr, &valR);

        usleep(10);
        logFile <<"0x" << std::hex << std::setfill('0') << std::setw(8) << static_cast<int>(addr)<<" , "<<std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned long long>(valL)<<" , "<<std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned long long>(valR)<<std::endl;
    }

    logFile.close();
}

void VideoCapture::saveLogDataLeft(uint64_t timestamp)
{
    const int reg_count = 61;
    uint8_t values[reg_count];
    memset(values, 0, sizeof(values));
    int idx = 0;

    int res = 0;
    (void)res;
    res += ll_read_system_register( 0x80181002, &values[idx++]);
    res += ll_read_system_register( 0x80181031, &values[idx++]);
    res += ll_read_system_register( 0x80181032, &values[idx++]);
    res += ll_read_system_register( 0x80181033, &values[idx++]);

    for(int reg_addr = 0x00; reg_addr <= 0x22; ++reg_addr)
    {
        uint64_t addr = 0x801810C0+reg_addr;
        res += ll_read_system_register( addr, &values[idx++]);
    }

    for (int addr = 0x3500; addr <= 0x3515; ++addr)
    {
        res += ll_read_sensor_register( 0, 1, addr, &values[idx++]);
    }

    mLogFileLeft << std::dec << timestamp << LOG_SEP;
    for(int i=0; i<reg_count; i++)
    {
        mLogFileLeft << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(values[i]);
        if(i==(reg_count-1))
            mLogFileLeft << std::endl;
        else
            mLogFileLeft << LOG_SEP;
    }
}

void VideoCapture::saveLogDataRight(uint64_t timestamp)
{
    const int reg_count = 61;
    uint8_t values[reg_count];
    memset(values, 0, sizeof(values));
    int idx = 0;

    int res = 0;
    (void)res;
    res += ll_read_system_register( 0x80181802, &values[idx++]);
    res += ll_read_system_register( 0x80181831, &values[idx++]);
    res += ll_read_system_register( 0x80181832, &values[idx++]);
    res += ll_read_system_register( 0x80181833, &values[idx++]);

    for(int reg_addr = 0x00; reg_addr <= 0x22; ++reg_addr)
    {
        uint64_t addr = 0x801818C0+reg_addr;
        res += ll_read_system_register( addr, &values[idx++]);
    }

    for (int addr = 0x3500; addr <= 0x3515; ++addr)
    {
        res += ll_read_sensor_register( 1, 1, addr, &values[idx++]);
    }

    mLogFileRight << std::dec << timestamp << LOG_SEP;
    for(int i=0; i<reg_count; i++)
    {
        mLogFileRight << "0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(values[i]);
        if(i==(reg_count-1))
            mLogFileRight << std::endl;
        else
            mLogFileRight << LOG_SEP;
    }
}

bool VideoCapture::resetAGCAECregisters() {
    int res = 0;

    res += ll_write_sensor_register( 0, 1, 0x3503, 0x04);
    res += ll_write_sensor_register( 1, 1, 0x3503, 0x04);
    res += ll_write_sensor_register( 0, 1, 0x3505, 0x00);
    res += ll_write_sensor_register( 1, 1, 0x3505, 0x00);

    return res==0;
}

bool VideoCapture::enableSensorSync( sensors::SensorCapture* sensCap )
{
#ifdef SENSORS_MOD_AVAILABLE
    if( !sensCap || !mInitialized || !sensCap->isOpened() )
        return false;

    // Activate low level sync mechanism
    ll_activate_sync();

    std::shared_ptr<detail::SyncLink> link = std::make_shared<detail::SyncLink>();

    sensCap->setSyncLink(link);

    {
        const std::lock_guard<std::mutex> lock(mBufMutex);
        mSyncLink = link;
    }

    return true;
#else
    (void)sensCap;
    detail::log(mParams.verbose, VERBOSITY::ERROR, LOG_TAG, "The library has been built without the sensors module");
    return false;
#endif
}

}

}
