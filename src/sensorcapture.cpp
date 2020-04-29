#include "sensorcapture.hpp"

#include <sstream>

#include <unistd.h>           // for usleep, close

namespace sl_drv {

SensorCapture::SensorCapture(bool verbose )
{
    mVerbose = verbose;

    if( mVerbose )
    {
        std::string ver =
                "ZED Driver - Sensors module - Version: "
                + std::to_string(mDrvMajorVer) + "."
                + std::to_string(mDrvMinorVer) + "."
                + std::to_string(mDrvPatchVer);
        INFO_OUT( ver );
    }
}

SensorCapture::~SensorCapture()
{
    reset();
}

int SensorCapture::enumerateDevices()
{
    mSlDevPid.clear();
    mSlDevFwVer.clear();

    struct hid_device_info *devs, *cur_dev;

    if (hid_init()==-1)
        return 0;

    devs = hid_enumerate(SL_USB_VENDOR, 0x0);
    cur_dev = devs;
    while (cur_dev) {
        int fw_major = cur_dev->release_number>>8;
        int fw_minor = cur_dev->release_number&0x00FF;
        uint16_t pid = cur_dev->product_id;
        std::string sn_str = wstr2str( cur_dev->serial_number );
        int sn = std::stoi( sn_str );

        mSlDevPid[sn]=pid;
        mSlDevFwVer[sn]=cur_dev->release_number;

        if(mVerbose)
        {
            std::ostringstream smsg;

            smsg << "Device Found: " << std::endl;
            smsg << "  VID: " << std::hex << cur_dev->vendor_id << " PID: " << std::hex << cur_dev->product_id << std::endl;
            smsg << "  Path: " << cur_dev->path << std::endl;
            smsg << "  Serial_number:   " << sn_str << std::endl;
            smsg << "  Manufacturer:   " << wstr2str(cur_dev->manufacturer_string) << std::endl;
            smsg << "  Product:   " << wstr2str(cur_dev->product_string) << std::endl;
            smsg << "  Release number:   v" << fw_major << "." << fw_minor << std::endl;
            smsg << "***" << std::endl;

            INFO_OUT(smsg.str());
        }

        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);

    return mSlDevPid.size();
}

std::vector<int> SensorCapture::getDeviceList()
{
    if(mSlDevPid.size()==0)
        enumerateDevices();

    std::vector<int> sn_vec;

    for(std::map<int,uint16_t>::iterator it = mSlDevPid.begin(); it != mSlDevPid.end(); ++it) {
      sn_vec.push_back(it->first);
    }

    return sn_vec;
}

bool SensorCapture::init( int sn )
{
    std::string sn_str;

    if(sn!=-1)
        sn_str = std::to_string(sn);
    else
    {
        if(mSlDevPid.size()==0)
        {
            enumerateDevices();
        }

        if(mSlDevPid.size()==0)
        {
            ERROR_OUT("No available ZED Mini or ZED2 cameras");
            return false;
        }

        sn = mSlDevPid.begin()->first;
        sn_str = std::to_string(sn);
    }

    mDevSerial = sn;

    std::wstring wide_string = std::wstring(sn_str.begin(), sn_str.end());
    const wchar_t* wsn = wide_string.c_str();

    uint16_t pid = mSlDevPid[sn];

    mDevHandle = hid_open(SL_USB_VENDOR, pid, wsn );

    if (!mDevHandle)
    {
        std::string msg = "Connection to device with sn ";
        msg += sn_str;
        msg += " failed";

        ERROR_OUT(msg);

        mDevFwVer = -1;
        mDevSerial = -1;

        return false;
    }

    if(mVerbose)
    {
        std::string msg = "Connected to device with sn ";
        msg += sn_str;

        INFO_OUT(msg);
    }

    mDevFwVer = mSlDevFwVer[sn];

    mInitialized = startCapture();

    return true;
}

void SensorCapture::getFwVersion( uint16_t& fw_major, uint16_t& fw_minor )
{
    if(mDevSerial==-1)
        return;

    uint16_t release = mSlDevFwVer[mDevSerial];

    fw_major = release>>8;
    fw_minor = release&0x00FF;
}

bool SensorCapture::enableDataStream(bool enable) {
    if( !mDevHandle )
        return false;
    unsigned char buf[65];
    buf[0] = REP_ID_SENSOR_STREAM_STATUS;
    buf[1] = enable?1:0;

    int res = hid_send_feature_report(mDevHandle, buf, 2);
    if (res < 0) {
        if(mVerbose)
        {
            std::string msg = "Unable to set a feature report [SensStreamStatus] - ";
            msg += wstr2str(hid_error(mDevHandle));

            WARNING_OUT(msg);
        }

        return false;
    }

    return true;
}

bool SensorCapture::isDataStreamEnabled() {
    if( !mDevHandle ) {
        return false;
    }

    unsigned char buf[65];
    buf[0] = REP_ID_SENSOR_STREAM_STATUS;
    int res = hid_get_feature_report(mDevHandle, buf, sizeof(buf));
    if (res < 0)
    {
        std::string msg = "Unable to get a feature report [SensStreamStatus] - ";
        msg += wstr2str(hid_error(mDevHandle));

        WARNING_OUT( msg );

        return false;
    }

    if( res < static_cast<int>(sizeof(SensStreamStatus)) )
    {
        WARNING_OUT( std::string("SensStreamStatus size mismatch [REP_ID_SENSOR_STREAM_STATUS]"));
        return false;
    }

    if( buf[0] != REP_ID_SENSOR_STREAM_STATUS )
    {
        WARNING_OUT( std::string("SensStreamStatus type mismatch [REP_ID_SENSOR_STREAM_STATUS]") );

        return false;
    }

    bool enabled = (buf[1]==1);

    return enabled;
}

bool SensorCapture::startCapture()
{
    if( !enableDataStream(true) )
    {
        return false;
    }

    mGrabThread = std::thread( &SensorCapture::grabThreadFunc,this );

    return true;
}

void SensorCapture::reset()
{
    mStopCapture = true;

    if( mGrabThread.joinable() )
    {
        mGrabThread.join();
    }

    enableDataStream(false);

    if( mDevHandle ) {
        hid_close(mDevHandle);
        mDevHandle = nullptr;
    }

    if( mVerbose && mInitialized)
    {
        std::string msg = "Device closed";
        INFO_OUT( msg );
    }

    mInitialized=false;
}

void SensorCapture::grabThreadFunc()
{
    mStopCapture = false;
    mGrabRunning = false;

    mNewIMUData=false;
    mNewMagData=false;
    mNewEnvData=false;
    mNewCamTempData=false;

    // Read sensor data
    unsigned char buf[65];

    int ping_data_count = 0;


    while (!mStopCapture)
    {
        // ----> Keep data stream alive
        // sending a ping once per second
        if(ping_data_count>=400) {
            ping_data_count=0;
            sendPing();
        };
        ping_data_count++;
        // <---- Keep data stream alive

        mGrabRunning=true;

        buf[1]=1;
        int res = hid_read_timeout( mDevHandle, buf, 64, 500 );

        // TODO count timeout and stop thread

        if( res < static_cast<int>(sizeof(SensData)) )  {
            hid_set_nonblocking( mDevHandle, 0 );
            continue;
        }

        if( buf[0] != REP_ID_SENSOR_DATA )
        {
            if(mVerbose)
            {
                WARNING_OUT( std::string("REP_ID_SENSOR_DATA - Sensor Data type mismatch") );
            }

            hid_set_nonblocking( mDevHandle, 0 );
            continue;
        }

        SensData* data = (SensData*)buf;

        // ----> IMU data
        mIMUMutex.lock();
        mLastIMUData.valid = data->imu_not_valid!=1;
        mLastIMUData.timestamp = data->timestamp*TS_SCALE;
        mLastIMUData.aX = data->aX*ACC_SCALE;
        mLastIMUData.aY = data->aY*ACC_SCALE;
        mLastIMUData.aZ = data->aZ*ACC_SCALE;
        mLastIMUData.gX = data->gX*GYRO_SCALE;
        mLastIMUData.gY = data->gY*GYRO_SCALE;
        mLastIMUData.gZ = data->gZ*GYRO_SCALE;
        mLastIMUData.temp = data->imu_temp*TEMP_SCALE;
        mNewIMUData = true;
        mIMUMutex.unlock();

        //std::string msg = std::to_string(mLastMAGData.timestamp);
        //INFO_OUT(msg);

        // <---- IMU data

        // ----> Magnetometer data
        if(data->mag_valid == SensMagData::NEW_VAL)
        {
            mMagMutex.lock();
            mLastMagData.valid = SensMagData::NEW_VAL;
            mLastMagData.timestamp = data->timestamp*TS_SCALE;
            mLastMagData.mY = data->mY*MAG_SCALE;
            mLastMagData.mZ = data->mZ*MAG_SCALE;
            mLastMagData.mX = data->mX*MAG_SCALE;
            mNewMagData = true;
            mMagMutex.unlock();

            //std::string msg = std::to_string(mLastMAGData.timestamp);
            //INFO_OUT(msg);
        }
        else
        {
            mLastIMUData.valid = static_cast<SensMagData::MagStatus>(data->mag_valid);
        }
        // <---- Magnetometer data

        // ----> Environmental data
        if(data->env_valid == SensEnvData::NEW_VAL)
        {
            mEnvMutex.lock();
            mLastEnvData.valid = SensEnvData::NEW_VAL;
            mLastEnvData.timestamp = data->timestamp*TS_SCALE;
            mLastEnvData.temp = data->temp*TEMP_SCALE;
            if( atLeast(mDevFwVer, ZED_2_FW::FW_2_3_9))
            {
                mLastEnvData.press = data->press*PRESS_SCALE_NEW;
                mLastEnvData.humid = data->humid*HUMID_SCALE_NEW;
            }
            else
            {
                mLastEnvData.press = data->press*PRESS_SCALE_OLD;
                mLastEnvData.humid = data->humid*HUMID_SCALE_OLD;
            }
            mNewEnvData = true;
            mEnvMutex.unlock();

            //std::string msg = std::to_string(mLastENVData.timestamp);
            //INFO_OUT(msg);
        }
        else
        {
            mLastIMUData.valid = static_cast<SensMagData::MagStatus>(data->mag_valid);
        }
        // <---- Environmental data

        // ----> Camera sensors temperature data
        if(data->temp_cam_left != TEMP_NOT_VALID &&
           data->temp_cam_left != TEMP_NOT_VALID &&
                data->env_valid == SensEnvData::NEW_VAL ) // Sensor temperature is linked to Environmental data acquisition at FW level
        {
            mCamTempMutex.lock();
            mLastCamTempData.valid = true;
            mLastCamTempData.timestamp = data->timestamp*TS_SCALE;
            mLastCamTempData.temp_left = data->temp_cam_left*TEMP_SCALE;
            mLastCamTempData.temp_right = data->temp_cam_right*TEMP_SCALE;
            mNewCamTempData=true;
            mCamTempMutex.unlock();

            //std::string msg = std::to_string(mLastCamTempData.timestamp);
            //INFO_OUT(msg);
        }
        else
        {
            mLastCamTempData.valid = false;
        }
        // <---- Camera sensors temperature data


    }

    mGrabRunning = false;
}

bool SensorCapture::sendPing() {
    if( !mDevHandle )
        return false;

    unsigned char buf[65];
    buf[0] = REP_ID_REQUEST_SET;
    buf[1] = RQ_CMD_PING;

    int res = hid_send_feature_report(mDevHandle, buf, 2);
    if (res < 0)
    {
        std::string msg = "Unable to send ping [REP_ID_REQUEST_SET-RQ_CMD_PING] - ";
        msg += wstr2str(hid_error(mDevHandle));

        WARNING_OUT(msg);

        return false;
    }

    return true;
}

const SensImuData* SensorCapture::getLastIMUData(uint64_t timeout_msec)
{
    // ----> Wait for a new frame
    uint64_t time_count = timeout_msec*10;
    while( !mNewIMUData )
    {
        if(time_count==0)
        {
            return nullptr;
        }
        time_count--;
        usleep(100);
    }
    // <---- Wait for a new frame

    // Get the frame mutex
    const std::lock_guard<std::mutex> lock(mIMUMutex);
    mNewIMUData = false;
    return &mLastIMUData;
}

const SensMagData* SensorCapture::getLastMagData(uint64_t timeout_msec)
{
    // ----> Wait for a new frame
    uint64_t time_count = timeout_msec*10;
    while( !mNewMagData )
    {
        if(time_count==0)
        {
            return nullptr;
        }
        time_count--;
        usleep(100);
    }
    // <---- Wait for a new frame

    // Get the frame mutex
    const std::lock_guard<std::mutex> lock(mMagMutex);
    mNewMagData = false;
    return &mLastMagData;
}

const SensEnvData* SensorCapture::getLastEnvData(uint64_t timeout_msec)
{
    // ----> Wait for a new frame
    uint64_t time_count = timeout_msec*10;
    while( !mNewEnvData )
    {
        if(time_count==0)
        {
            return nullptr;
        }
        time_count--;
        usleep(100);
    }
    // <---- Wait for a new frame

    // Get the frame mutex
    const std::lock_guard<std::mutex> lock(mEnvMutex);
    mNewEnvData = false;
    return &mLastEnvData;
}

const SensCamTempData* SensorCapture::getLastCamTempData(uint64_t timeout_msec)
{
    // ----> Wait for a new frame
    uint64_t time_count = timeout_msec*10;
    while( !mNewCamTempData )
    {
        if(time_count==0)
        {
            return nullptr;
        }
        time_count--;
        usleep(100);
    }
    // <---- Wait for a new frame

    // Get the frame mutex
    const std::lock_guard<std::mutex> lock(mCamTempMutex);
    mNewCamTempData = false;
    return &mLastCamTempData;
}

}
