#include "sensorcapture.hpp"

#include <sstream>

namespace sl_drv {

SensorCapture::SensorCapture( SensorParams params )
{
    memcpy( &mParams, &params, sizeof(SensorParams) );
}

SensorCapture::~SensorCapture()
{

}

int SensorCapture::enumerateDevices()
{
    mSlDevPid.clear();

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

        if(mParams.verbose)
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

        return false;
    }

    if(mParams.verbose)
    {
        std::string msg = "Connected to device with sn ";
        msg += sn_str;

        INFO_OUT(msg);
    }

    return true;
}

}
