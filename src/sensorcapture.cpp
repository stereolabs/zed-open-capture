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

#include "sensorcapture.hpp"
#include "internal.hpp"

#include "hidapi.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <unistd.h>           // for usleep

namespace sl_oc {

namespace sensors {

namespace {

const char* LOG_TAG = "sl_oc::sensors::SensorCapture";

const size_t IMU_QUEUE_MAX_SIZE = 1600;             //!< 2 seconds of ZED Mini data (800 Hz), 4 seconds for ZED 2/2i
const int HID_READ_TIMEOUT_MSEC = 100;              //!< Maximum time the grabbing thread waits before checking the stop flag
const int HID_MAX_CONSECUTIVE_ERRORS = 10;          //!< Read errors before considering the device disconnected
const int STALE_REPORT_COUNT = 3;                   //!< The first reports can still contain data of a previous connection
const uint64_t PING_PERIOD_NSEC = 1000000000ULL;    //!< The MCU stops the stream if not pinged for 6 seconds

// ----> Conversion of the MCU timestamps
const int64_t TS_MAX_STEP_NSEC = 500000000LL;       //!< Maximum distance between two consecutive samples
const int64_t TS_MAX_AHEAD_NSEC = 100000000LL;      //!< A timestamp cannot be much later than the arrival of its data
const int64_t TS_MAX_DELAY_NSEC = 50000000LL;       //!< Maximum offset removed gradually
const double TS_MAX_SLEW = 0.005;                   //!< Maximum rate correction used to remove the offset
const double TS_MAX_SLEW_BACK = 0.2;                //!< Maximum rate correction when the timestamps are in the future
const double TS_MAX_DRIFT = 0.02;                   //!< Maximum clock drift accepted in a single measure
// <---- Conversion of the MCU timestamps

/*!
 * \brief Convert the serial number string of a device
 * \return false if the string is not a valid serial number
 */
bool parseSerialNumber(const wchar_t* wstr, int& sn)
{
    const std::string str = wstr2str(wstr);

    if( str.empty() || str.size()>9 )
        return false;

    int value = 0;
    for( char c : str )
    {
        if( c<'0' || c>'9' )
            return false;
        value = value*10 + (c-'0');
    }

    sn = value;
    return true;
}

inline bool isSensorModule(uint16_t pid)
{
    return pid==SL_USB_PROD_MCU_ZEDM_REVA || pid==SL_USB_PROD_MCU_ZED2_REVA || pid==SL_USB_PROD_MCU_ZED2i_REVA;
}

inline bool supportsReset(uint16_t pid)
{
    return pid==SL_USB_PROD_MCU_ZED2_REVA || pid==SL_USB_PROD_MCU_ZED2i_REVA;
}

/*!
 * \brief Open a HID device. A device just released by another connection can refuse the first attempts.
 */
hid_device* openHidDevice(uint16_t pid, int serial_number)
{
    const std::string sn_str = std::to_string(serial_number);
    const std::wstring wide_sn_string(sn_str.begin(), sn_str.end());

    for( int attempt=0; attempt<5; attempt++ )
    {
        hid_device* handle = hid_open(SL_USB_VENDOR, pid, wide_sn_string.c_str());
        if(handle)
            return handle;
        usleep(100000);
    }

    return nullptr;
}

}

SensorCapture::SensorCapture(VERBOSITY verbose_lvl )
{
    mVerbose = verbose_lvl;

    std::string ver =
            "ZED Open Capture - Sensors module - Version: "
            + std::to_string(ZED_OC_MAJOR_VERSION) + "."
            + std::to_string(ZED_OC_MINOR_VERSION) + "."
            + std::to_string(ZED_OC_PATCH_VERSION);
    detail::log(mVerbose, VERBOSITY::INFO, LOG_TAG, ver);
}

SensorCapture::~SensorCapture()
{
    close();
}

int SensorCapture::enumerateDevices()
{
    mSlDevPid.clear();
    mSlDevFwVer.clear();

    if (hid_init()==-1)
        return 0;

    struct hid_device_info* devs = hid_enumerate(SL_USB_VENDOR, 0x0);

    for( struct hid_device_info* cur_dev = devs; cur_dev!=nullptr; cur_dev = cur_dev->next )
    {
        const uint16_t pid = cur_dev->product_id;
        int sn = -1;

        if( !isSensorModule(pid) || !parseSerialNumber(cur_dev->serial_number, sn) )
            continue;

        mSlDevPid[sn]=pid;
        mSlDevFwVer[sn]=cur_dev->release_number;

        if(mVerbose>=VERBOSITY::INFO)
        {
            const int fw_major = cur_dev->release_number>>8;
            const int fw_minor = cur_dev->release_number&0x00FF;

            std::ostringstream smsg;

            smsg << "Device Found: " << std::endl;
            smsg << "  VID: " << std::hex << cur_dev->vendor_id << " PID: " << std::hex << cur_dev->product_id << std::endl;
            smsg << "  Path: " << (cur_dev->path?cur_dev->path:"") << std::endl;
            smsg << "  Serial_number:   " << std::dec << sn << std::endl;
            smsg << "  Manufacturer:   " << wstr2str(cur_dev->manufacturer_string) << std::endl;
            smsg << "  Product:   " << wstr2str(cur_dev->product_string) << std::endl;
            smsg << "  Release number:   v" << std::dec << fw_major << "." << fw_minor << std::endl;
            smsg << "***" << std::endl;

            detail::log(mVerbose, VERBOSITY::INFO, LOG_TAG, smsg.str());
        }
    }

    hid_free_enumeration(devs);

    return static_cast<int>(mSlDevPid.size());
}

std::vector<int> SensorCapture::getDeviceList(bool refresh)
{
    if(mSlDevPid.size()==0 || refresh)
        enumerateDevices();

    std::vector<int> sn_vec;

    for(std::map<int,uint16_t>::iterator it = mSlDevPid.begin(); it != mSlDevPid.end(); ++it) {
        sn_vec.push_back(it->first);
    }

    return sn_vec;
}

bool SensorCapture::open( uint16_t pid, int serial_number)
{
    mDevHandle = openHidDevice(pid, serial_number);

    if(mDevHandle) mDevSerial = serial_number;

    return mDevHandle!=nullptr;
}

bool SensorCapture::initializeSensors( int sn )
{
    // A previous connection must be closed before reusing the object
    close();

    // The list can be outdated: refresh it if the requested device is not there
    if( mSlDevPid.empty() || (sn!=-1 && mSlDevPid.find(sn)==mSlDevPid.end()) )
    {
        enumerateDevices();
    }

    if(sn==-1)
    {
        if(mSlDevPid.empty())
        {
            detail::log(mVerbose, VERBOSITY::ERROR, LOG_TAG, "No available ZED Mini, ZED 2 or ZED 2i cameras");
            return false;
        }

        sn = mSlDevPid.begin()->first;
    }

    const std::map<int,uint16_t>::const_iterator dev = mSlDevPid.find(sn);
    if( dev==mSlDevPid.end() )
    {
        detail::log(mVerbose, VERBOSITY::ERROR, LOG_TAG,
                    "Device with sn " + std::to_string(sn) + " not found");
        return false;
    }

    const uint16_t pid = dev->second;

    if(!open( pid,sn))
    {
        detail::log(mVerbose, VERBOSITY::ERROR, LOG_TAG,
                    "Connection to device with sn " + std::to_string(sn) + " failed. " +
                    "Please verify that the udev rules available in the `udev` folder are installed");

        mDevFwVer = -1;
        mDevSerial = -1;

        return false;
    }

    detail::log(mVerbose, VERBOSITY::INFO, LOG_TAG, "Connected to device with sn " + std::to_string(sn));

    mDevFwVer = mSlDevFwVer[sn];
    mDevPid = pid;

    // ----> Clear the data of a previous connection
    {
        const std::lock_guard<std::mutex> lock(mDataMutex);
        mLastIMUData = data::Imu();
        mLastMagData = data::Magnetometer();
        mLastEnvData = data::Environment();
        mLastCamTempData = data::Temperature();
        mNewIMUData = false;
        mNewMagData = false;
        mNewEnvData = false;
        mNewCamTempData = false;
        mImuQueue.clear();
    }
    mTsValid = false;
    mTsScale = 1.0;
    mTsLastOut = 0;
    // <---- Clear the data of a previous connection

    if( !enableDataStream(true) )
    {
        detail::log(mVerbose, VERBOSITY::ERROR, LOG_TAG, "Unable to start the data stream");

        hid_close(mDevHandle);
        mDevHandle = nullptr;
        mDevFwVer = -1;
        mDevSerial = -1;

        return false;
    }

    // The flags must be ready before the thread starts: `close` can be called at any time
    mStopCapture = false;
    mDeviceLost = false;
    mGrabThread = std::thread( &SensorCapture::grabThreadFunc,this );
    mPingThread = std::thread( &SensorCapture::pingThreadFunc,this );
    mInitialized = true;

    return true;
}

bool SensorCapture::isOpened() const
{
    return mInitialized && !mDeviceLost;
}

void SensorCapture::getFirmwareVersion( uint16_t& fw_major, uint16_t& fw_minor )
{
    if(mDevSerial==-1 || mDevFwVer<0)
        return;

    fw_major = static_cast<uint16_t>(mDevFwVer>>8);
    fw_minor = static_cast<uint16_t>(mDevFwVer&0x00FF);
}

int SensorCapture::getSerialNumber()
{
    return mDevSerial;
}

bool SensorCapture::enableDataStream(bool enable) {
    if( !mDevHandle )
        return false;
    unsigned char buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0] = usb::REP_ID_SENSOR_STREAM_STATUS;
    buf[1] = enable?1:0;

    int res = hid_send_feature_report(mDevHandle, buf, 2);
    if (res < 0) {
        detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG,
                    "Unable to set a feature report [SensStreamStatus] - " + wstr2str(hid_error(mDevHandle)));

        return false;
    }

    return true;
}

bool SensorCapture::isDataStreamEnabled() {
    if( !mDevHandle ) {
        return false;
    }

    unsigned char buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0] = usb::REP_ID_SENSOR_STREAM_STATUS;
    int res = hid_get_feature_report(mDevHandle, buf, sizeof(buf));
    if (res < 0)
    {
        detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG,
                    "Unable to get a feature report [SensStreamStatus] - " + wstr2str(hid_error(mDevHandle)));

        return false;
    }

    if( res < static_cast<int>(sizeof(usb::StreamStatus)) )
    {
        detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG, "SensStreamStatus size mismatch [REP_ID_SENSOR_STREAM_STATUS]");
        return false;
    }

    if( buf[0] != usb::REP_ID_SENSOR_STREAM_STATUS )
    {
        detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG, "SensStreamStatus type mismatch [REP_ID_SENSOR_STREAM_STATUS]");
        return false;
    }

    return (buf[1]==1);
}

void SensorCapture::close()
{
    {
        const std::lock_guard<std::mutex> lock(mPingMutex);
        mStopCapture = true;
    }
    mPingCond.notify_all();

    if( mPingThread.joinable() )
    {
        mPingThread.join();
    }

    if( mGrabThread.joinable() )
    {
        mGrabThread.join();
    }

    if( mDevHandle ) {
        if( !mDeviceLost )
            enableDataStream(false);

        hid_close(mDevHandle);
        mDevHandle = nullptr;
    }

    {
        // The frame sync signals of this connection are no longer produced, and its data are no longer valid
        const std::lock_guard<std::mutex> lock(mDataMutex);
        mSyncLink.reset();
        mImuQueue.clear();
        mNewIMUData = false;
        mNewMagData = false;
        mNewEnvData = false;
        mNewCamTempData = false;
    }

    if( mInitialized )
    {
        detail::log(mVerbose, VERBOSITY::INFO, LOG_TAG, "Device closed");
    }

    mInitialized=false;
    mDevSerial = -1;
    mDevFwVer = -1;
}

void SensorCapture::setSyncLink(std::shared_ptr<detail::SyncLink> link)
{
    const std::lock_guard<std::mutex> lock(mDataMutex);
    mSyncLink = std::move(link);
}

uint64_t SensorCapture::updateTimestamp(uint64_t mcu_ts_nsec, uint64_t arrival_ts)
{
    // The MCU clock is not aligned to the host clock and it can run up to 1% faster or slower.
    // The data cannot arrive before being acquired, so the samples with the smallest delay between timestamp and
    // arrival time are used to measure, and slowly remove, the offset and the drift of the MCU clock.

    int64_t step = 0;

    if(mTsValid)
    {
        step = static_cast<int64_t>(mcu_ts_nsec) - static_cast<int64_t>(mTsLastMcu);
        if( step<=0 || step>TS_MAX_STEP_NSEC )
            mTsValid = false; // The MCU has been restarted or data have been lost for a long time
    }

    uint64_t ts = 0;

    if(mTsValid)
    {
        mTsRel += static_cast<double>(step) * (mTsScale + mTsSlew);
        ts = mTsAnchor + static_cast<uint64_t>(mTsRel);

        const int64_t delay = static_cast<int64_t>(arrival_ts) - static_cast<int64_t>(ts);

        if( delay < -TS_MAX_AHEAD_NSEC )
        {
            mTsValid = false;
        }
        else
        {
            if( !mTsWinHasMin || delay<mTsWinMin )
            {
                mTsWinMin = delay;
                mTsWinHasMin = true;
            }

            // Short windows at the beginning for a fast convergence
            const uint64_t winLen = mTsWinCount<2 ? 250000000ULL : (mTsWinCount<4 ? 500000000ULL : 1000000000ULL);

            if( arrival_ts-mTsWinStart >= winLen )
            {
                const double elapsed = static_cast<double>(arrival_ts-mTsWinStart);

                if( mTsWinMin > TS_MAX_DELAY_NSEC )
                {
                    mTsValid = false; // Too far to be recovered smoothly
                }
                else
                {
                    if( mTsWinCount>0 )
                    {
                        // Variation of the delay not explained by the correction applied in the window
                        double drift = static_cast<double>(mTsWinMin - (mTsPrevMin - mTsPrevCorr)) / elapsed;
                        drift = std::max(-TS_MAX_DRIFT, std::min(TS_MAX_DRIFT, drift));

                        mTsScale *= (1.0 + (mTsWinCount<8 ? 0.5 : 0.2)*drift);
                        mTsScale = std::max(0.9, std::min(1.1, mTsScale));
                    }

                    // Remove half of the measured delay during the next window
                    const double nextLen = (mTsWinCount+1)<2 ? 250000000.0 : ((mTsWinCount+1)<4 ? 500000000.0 : 1000000000.0);
                    // Timestamps later than the arrival of the data are impossible: they are recovered faster
                    mTsSlew = std::max(-TS_MAX_SLEW_BACK, std::min(TS_MAX_SLEW, 0.5*static_cast<double>(mTsWinMin)/nextLen));
                    mTsPrevCorr = static_cast<int64_t>(mTsSlew*nextLen);
                    mTsPrevMin = mTsWinMin;
                    mTsWinCount++;

                    mTsWinStart = arrival_ts;
                    mTsWinHasMin = false;
                }
            }
        }
    }

    if(!mTsValid)
    {
        // New reference: the sample is supposed to be just acquired. Timestamps always increase.
        mTsAnchor = std::max(arrival_ts, mTsLastOut+1);
        mTsRel = 0.0;
        mTsSlew = 0.0;
        mTsWinStart = arrival_ts;
        mTsWinHasMin = false;
        mTsPrevMin = 0;
        mTsPrevCorr = 0;
        mTsWinCount = 0;
        mTsValid = true;

        ts = mTsAnchor;
    }

    mTsLastMcu = mcu_ts_nsec;

    if( ts<=mTsLastOut )
        ts = mTsLastOut+1;
    mTsLastOut = ts;

    return ts;
}

void SensorCapture::grabThreadFunc()
{
    unsigned char usbBuf[65];

    uint64_t lastSyncCount = 0;
    bool firstData = true;
    int errorCount = 0;
    int discardCount = STALE_REPORT_COUNT;

    const uint8_t target_struct_id =
            (mDevPid==SL_USB_PROD_MCU_ZED2_REVA || mDevPid==SL_USB_PROD_MCU_ZED2i_REVA) ? usb::REP_ID_SENSOR_DATA : 0;

    while (!mStopCapture)
    {
        memset(usbBuf, 0, sizeof(usbBuf));
        int res = hid_read_timeout( mDevHandle, usbBuf, 64, HID_READ_TIMEOUT_MSEC );
        const uint64_t arrival_ts = getTimestampNow();

        // ----> Device still connected?
        if( res<0 )
        {
            if( ++errorCount >= HID_MAX_CONSECUTIVE_ERRORS )
            {
                detail::log(mVerbose, VERBOSITY::ERROR, LOG_TAG, "Connection lost: the device does not reply");
                mDeviceLost = true;
                break;
            }
            usleep(10000);
            continue;
        }
        errorCount = 0;
        // <---- Device still connected?

        // ----> Data received?
        if( res < static_cast<int>(sizeof(usb::RawData)) )
            continue;

        if( usbBuf[0] != target_struct_id)
        {
            detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG, "REP_ID_SENSOR_DATA - Sensor Data type mismatch");
            continue;
        }

        if( discardCount>0 )
        {
            discardCount--;
            continue;
        }
        // <---- Data received?

        usb::RawData raw;
        memcpy(&raw, usbBuf, sizeof(usb::RawData));

        const bool imuValid = (raw.imu_not_valid!=1);

        // ----> Timestamp update
        uint64_t current_data_ts = 0;
        if( imuValid )
        {
            // Integer conversion: the MCU timestamp does not fit in a floating point value
            const uint64_t mcu_ts_nsec = (raw.timestamp/TS_SCALE_DEN)*TS_SCALE_NUM + ((raw.timestamp%TS_SCALE_DEN)*TS_SCALE_NUM)/TS_SCALE_DEN;
            current_data_ts = updateTimestamp(mcu_ts_nsec, arrival_ts);
        }
        else
        {
            if( mTsLastOut==0 )
                continue; // No reference yet
            current_data_ts = mTsLastOut;
        }
        // <---- Timestamp update

        // ----> Camera/Sensors Synchronization
        // The sync signal is also detected with the counter, in case the data containing it have been lost
        bool framePulse = false;
        if( raw.sync_capabilities!=0 )
        {
            framePulse = !firstData && (raw.frame_sync!=0 || raw.frame_sync_count>lastSyncCount);
        }
        lastSyncCount = raw.frame_sync_count;
        firstData = false;
        // <---- Camera/Sensors Synchronization

        std::shared_ptr<detail::SyncLink> link;

        {
            const std::lock_guard<std::mutex> lock(mDataMutex);

            link = mSyncLink;

            // ----> IMU data
            mLastIMUData.sync = (raw.frame_sync!=0);
            mLastIMUData.valid = imuValid?(data::Imu::NEW_VAL):(data::Imu::OLD_VAL);
            mLastIMUData.timestamp = current_data_ts;
            mLastIMUData.aX = raw.aX*ACC_SCALE;
            mLastIMUData.aY = raw.aY*ACC_SCALE;
            mLastIMUData.aZ = raw.aZ*ACC_SCALE;
            mLastIMUData.gX = raw.gX*GYRO_SCALE;
            mLastIMUData.gY = raw.gY*GYRO_SCALE;
            mLastIMUData.gZ = raw.gZ*GYRO_SCALE;
            mLastIMUData.temp = raw.imu_temp*TEMP_SCALE;
            mNewIMUData = true;

            if( imuValid )
            {
                if( mImuQueue.size()>=IMU_QUEUE_MAX_SIZE )
                    mImuQueue.pop_front();
                mImuQueue.push_back(mLastIMUData);
            }
            // <---- IMU data

            // ----> Magnetometer data
            if(raw.mag_valid == data::Magnetometer::NEW_VAL)
            {
                mLastMagData.valid = data::Magnetometer::NEW_VAL;
                mLastMagData.timestamp = current_data_ts;
                mLastMagData.mY = raw.mY*MAG_SCALE;
                mLastMagData.mZ = raw.mZ*MAG_SCALE;
                mLastMagData.mX = raw.mX*MAG_SCALE;
                mNewMagData = true;
            }
            else
            {
                mLastMagData.valid = (raw.mag_valid==0) ? data::Magnetometer::NOT_PRESENT : data::Magnetometer::OLD_VAL;
            }
            // <---- Magnetometer data

            // ----> Environmental data
            if(raw.env_valid == data::Environment::NEW_VAL)
            {
                mLastEnvData.valid = data::Environment::NEW_VAL;
                mLastEnvData.timestamp = current_data_ts;
                mLastEnvData.temp = raw.temp*TEMP_SCALE;
                if( atLeast(mDevFwVer, ZED_2_FW::FW_3_9))
                {
                    mLastEnvData.press = raw.press*PRESS_SCALE_NEW;
                    mLastEnvData.humid = raw.humid*HUMID_SCALE_NEW;
                }
                else
                {
                    mLastEnvData.press = raw.press*PRESS_SCALE_OLD;
                    mLastEnvData.humid = raw.humid*HUMID_SCALE_OLD;
                }
                mNewEnvData = true;
            }
            else
            {
                mLastEnvData.valid = (raw.env_valid==0) ? data::Environment::NOT_PRESENT : data::Environment::OLD_VAL;
            }
            // <---- Environmental data

            // ----> Camera sensors temperature data
            if(raw.temp_cam_left != TEMP_NOT_VALID &&
                    raw.temp_cam_right != TEMP_NOT_VALID &&
                    raw.env_valid == data::Environment::NEW_VAL ) // Sensor temperature is linked to Environmental data acquisition at FW level
            {
                mLastCamTempData.valid = data::Temperature::NEW_VAL;
                mLastCamTempData.timestamp = current_data_ts;
                mLastCamTempData.temp_left = raw.temp_cam_left*TEMP_SCALE;
                mLastCamTempData.temp_right = raw.temp_cam_right*TEMP_SCALE;
                mNewCamTempData=true;
            }
            else if( mLastCamTempData.valid!=data::Temperature::NOT_PRESENT )
            {
                mLastCamTempData.valid = data::Temperature::OLD_VAL;
            }
            // <---- Camera sensors temperature data
        }

        mDataCond.notify_all();

        if( framePulse && imuValid && link )
        {
            link->addPulse(current_data_ts);
        }
    }
}

void SensorCapture::pingThreadFunc()
{
    // The ping is a blocking request that can take up to one second if the MCU is busy:
    // it has its own thread to not lose the data that arrive in the meantime
    std::unique_lock<std::mutex> lock(mPingMutex);

    while( !mStopCapture && !mDeviceLost )
    {
        if( mPingCond.wait_for(lock, std::chrono::nanoseconds(PING_PERIOD_NSEC), [this]{ return mStopCapture.load(); }) )
            break;

        if( mDeviceLost )
            break;

        lock.unlock();
        sendPing();
        lock.lock();
    }
}

bool SensorCapture::sendPing() {
    if( !mDevHandle )
        return false;

    unsigned char buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0] = usb::REP_ID_REQUEST_SET;
    buf[1] = usb::RQ_CMD_PING;

    int res = hid_send_feature_report(mDevHandle, buf, 2);
    if (res < 0)
    {
        detail::log(mVerbose, VERBOSITY::WARNING, LOG_TAG,
                    "Unable to send ping [REP_ID_REQUEST_SET-RQ_CMD_PING] - " + wstr2str(hid_error(mDevHandle)));

        return false;
    }

    return true;
}

bool SensorCapture::searchForConnectedDev(int* serial_number, unsigned short* found_pid)
{
    const int in_serial_number = (serial_number==nullptr) ? 0 : *serial_number;

    if (hid_init()==-1)
        return false;

    struct hid_device_info* devs = hid_enumerate(SL_USB_VENDOR, 0x0);

    bool found = false;
    bool notSupported = false;
    uint16_t pid = 0;
    int found_serial_number = 0;

    for( struct hid_device_info* cur_dev = devs; cur_dev!=nullptr && !found; cur_dev = cur_dev->next )
    {
        int sn = -1;
        if( !isSensorModule(cur_dev->product_id) || !parseSerialNumber(cur_dev->serial_number, sn) )
            continue;

        if( in_serial_number!=0 && sn!=in_serial_number )
            continue;

        if( supportsReset(cur_dev->product_id) )
        {
            found = true;
            pid = cur_dev->product_id;
            found_serial_number = sn;
        }
        else
        {
            notSupported = true;
        }
    }

    hid_free_enumeration(devs);

    if(!found)
    {
        if(notSupported)
            std::cerr << "The reset function works only for ZED2/ZED2i camera models." << std::endl;
        return false;
    }

    if(serial_number)
        *serial_number = found_serial_number;
    if(found_pid)
        *found_pid = pid;
    return true;
}

bool SensorCapture::resetSensorModule(int serial_number)
{
    int found_sn = serial_number;
    unsigned short pid = 0;
    bool res = searchForConnectedDev(&found_sn, &pid);
    if(!res)
    {
        std::string msg;
        if(serial_number!=0)
        {
            msg = "[sl_oc::sensors::SensorCapture] WARNING: Sensors Module reset failed. Unable to find the Sensor Module with serial number ";
            msg += std::to_string(serial_number);
        }
        else
        {
            msg = "[sl_oc::sensors::SensorCapture] WARNING: Sensors Module reset failed. Unable to find the Sensor Module of a ZED2 camera. Please verify the USB connection.";
        }

        std::cerr << msg << std::endl;

        return false;
    }

    hid_device* devHandle = openHidDevice(pid, found_sn);

    if(!devHandle)
    {
        std::cerr << "Unable to open the MCU HID device" << std::endl;

        return false;
    }

    unsigned char  buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0] = static_cast<unsigned char>(usb::REP_ID_REQUEST_SET);
    buf[1] = static_cast<unsigned char>(usb::RQ_CMD_RST);

    hid_send_feature_report(devHandle, buf, 2);
    // Note: cannot verify the return value of the `hid_send_feature_report` command because the MCU is suddenly reset
    // and it cannot return a valid value

    hid_close(devHandle);

    sleep(2); // Wait for MCU and OV580 to reboot

    std::cerr << "[sl_oc::sensors::SensorCapture] INFO: Sensors Module reset successful" << std::endl;

    return true;
}

bool SensorCapture::resetVideoModule(int serial_number)
{
    int found_sn = serial_number;
    unsigned short pid = 0;
    bool res = searchForConnectedDev(&found_sn, &pid);
    if(!res)
    {
        std::string msg;
        if(serial_number!=0)
        {
            msg = "[sl_oc::sensors::SensorCapture] WARNING: Video Module reset failed. Unable to find the Sensor Module with serial number ";
            msg += std::to_string(serial_number);
        }
        else
        {
            msg = "[sl_oc::sensors::SensorCapture] WARNING: Video Module reset failed. Unable to find the Sensor Module of a ZED2 camera. Please verify the USB connection.";
        }

        std::cerr << msg << std::endl;

        return false;
    }

    hid_device* devHandle = openHidDevice(pid, found_sn);

    if(!devHandle)
    {
        std::cerr << "Unable to open the MCU HID device" << std::endl;

        return false;
    }

    usb::OV580CmdStruct cmd;
    cmd.struct_id = usb::REP_ID_OV580_CMD;
    cmd.cmd = usb::OV580_CMD_RESET;
    cmd.info=0;

    unsigned char buf[65];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &cmd, sizeof(usb::OV580CmdStruct));

    int ret = hid_send_feature_report(devHandle, buf, sizeof(usb::OV580CmdStruct));
    hid_close(devHandle);

    if(ret!=sizeof(usb::OV580CmdStruct)) {
        std::cerr << "[sl_oc::sensors::SensorCapture] INFO: Video Module reset failed" << std::endl;
        return false;
    }

    sleep(2); // Wait for OV580 to reboot

    std::cerr << "[sl_oc::sensors::SensorCapture] INFO: Video Module reset successful" << std::endl;
    return true;
}

const data::Imu& SensorCapture::getLastIMUData(uint64_t timeout_usec)
{
    std::unique_lock<std::mutex> lock(mDataMutex);

    const bool newData = mDataCond.wait_for(lock, std::chrono::microseconds(timeout_usec<100?100:timeout_usec),
                                            [this]{ return mNewIMUData; });

    mUserIMUData = mLastIMUData;
    if(newData)
        mNewIMUData = false;
    else if(mUserIMUData.valid!=data::Imu::NOT_PRESENT)
        mUserIMUData.valid = data::Imu::OLD_VAL;

    return mUserIMUData;
}

std::vector<data::Imu> SensorCapture::getIMUData(uint64_t timeout_usec)
{
    std::unique_lock<std::mutex> lock(mDataMutex);

    mDataCond.wait_for(lock, std::chrono::microseconds(timeout_usec), [this]{ return !mImuQueue.empty(); });

    std::vector<data::Imu> samples(mImuQueue.begin(), mImuQueue.end());
    mImuQueue.clear();

    return samples;
}

const data::Magnetometer& SensorCapture::getLastMagnetometerData(uint64_t timeout_usec)
{
    std::unique_lock<std::mutex> lock(mDataMutex);

    const bool newData = mDataCond.wait_for(lock, std::chrono::microseconds(timeout_usec<100?100:timeout_usec),
                                            [this]{ return mNewMagData; });

    mUserMagData = mLastMagData;
    if(newData)
        mNewMagData = false;
    else if(mUserMagData.valid!=data::Magnetometer::NOT_PRESENT)
        mUserMagData.valid = data::Magnetometer::OLD_VAL;

    return mUserMagData;
}

const data::Environment &SensorCapture::getLastEnvironmentData(uint64_t timeout_usec)
{
    std::unique_lock<std::mutex> lock(mDataMutex);

    const bool newData = mDataCond.wait_for(lock, std::chrono::microseconds(timeout_usec<100?100:timeout_usec),
                                            [this]{ return mNewEnvData; });

    mUserEnvData = mLastEnvData;
    if(newData)
        mNewEnvData = false;
    else if(mUserEnvData.valid!=data::Environment::NOT_PRESENT)
        mUserEnvData.valid = data::Environment::OLD_VAL;

    return mUserEnvData;
}

const data::Temperature& SensorCapture::getLastCameraTemperatureData(uint64_t timeout_usec)
{
    std::unique_lock<std::mutex> lock(mDataMutex);

    const bool newData = mDataCond.wait_for(lock, std::chrono::microseconds(timeout_usec<100?100:timeout_usec),
                                            [this]{ return mNewCamTempData; });

    mUserCamTempData = mLastCamTempData;
    if(newData)
        mNewCamTempData = false;
    else if(mUserCamTempData.valid!=data::Temperature::NOT_PRESENT)
        mUserCamTempData.valid = data::Temperature::OLD_VAL;

    return mUserCamTempData;
}

}

}
