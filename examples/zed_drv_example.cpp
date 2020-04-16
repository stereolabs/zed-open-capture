#include "videocapture.hpp"
#include "sensorcapture.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// Camera settings control
typedef enum _cam_control
{
    Brightness,
    Contrast,
    Hue,
    Saturation,
    Gain,
    Exposure,
    WhiteBalance,
    Sharpness,
    Gamma
} CamControl;

CamControl activeControl = Brightness;

// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, zed::RESOLUTION res );

// Handle Keyboard
void handleKeyboard( zed::VideoCapture &cap, int key );

// Change active control
void setActiveControl( CamControl control );
// Set new value for the active control
void setControlValue( zed::VideoCapture &cap, int value );
// '+' or '-' pressed
void changeControlValue( zed::VideoCapture &cap, bool increase );
// 'a' or 'A' pressed to enable automatic WhiteBalanse or Gain/Exposure
void toggleAutomaticControl( zed::VideoCapture &cap );

int main(int argc, char *argv[])
{
    // ----> Set parameters
    zed::VideoParams params;
    params.res = zed::RESOLUTION::HD1080;
    params.fps = zed::FPS::FPS_30;
    params.verbose = true;
    // <---- Set parameters

    // ----> Create Video Capture
    zed::VideoCapture cap(params);
    if( !cap.init(-1) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        if( !cap.isVerbose() )
            std::cerr << "Try to enable verbose to get more info" << std::endl;

        return EXIT_FAILURE;
    }
    // <---- Create Video Capture

    // Create Sensor Capture
    zed::SensorCapture sens;

    // Default camera control setting
    setActiveControl( Brightness );

    while (1)
    {
        // Get last available frame
        const zed::Frame* frame = cap.getLastFrame();

        // If the frame is valid we can display it
        if(frame != nullptr)
        {
#if 0
            // ----> Video Debug information
            static uint64_t last_ts=0;
            std::cout << std::setprecision(9) << "[" << frame->frame_id << "] Ts: " <<  static_cast<double>(frame->timestamp)/1e9 << " sec" << std::endl;
            if( last_ts!=0 )
            {
                double dt = (frame->timestamp - last_ts)/1e9;
                std::cout << std::setprecision(9) << " * dT: " << dt << " sec - FPS: " << 1./dt <<  std::endl;
            }
            last_ts = frame->timestamp;
            // <---- Video Debug information
#endif

            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame->height, frame->width, CV_8UC2, frame->data );
            cv::Mat frameBGR;
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // Show frame
            showImage( "Stream RGB", frameBGR, params.res );
        }

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );

        if( key != -1 )
        {
             //std::cout << key << std::endl;

            if(key=='q' || key=='Q') // Quit
                break;
            else
                handleKeyboard( cap, key );
        }
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}

void handleKeyboard( zed::VideoCapture &cap, int key )
{
    if(key >= '0' && key <= '9')
    {
        int value = key - '0';
        setControlValue( cap, value );
        return;
    }

    switch(key)
    {
    case 'l':
    {
        bool led;
        cap.toggleLED( &led );
        std::cout << std::string(" * LED STATUS: ") << (led?std::string("ON"):std::string("OFF")) << std::endl;
    }
        break;

    case 'b':
        setActiveControl( Brightness );
        break;

    case 'S':
        setActiveControl( Sharpness );
        break;

    case 'c':
        setActiveControl( Contrast );
        break;

    case 'H':
        setActiveControl( Hue );
        break;

    case 's':
        setActiveControl( Saturation );
        break;

    case 'w':
        setActiveControl( WhiteBalance );
        break;

    case 'g':
        setActiveControl( Gamma );
        break;

    case 'e':
        setActiveControl( Exposure );
        break;

    case 'G':
        setActiveControl( Gain );
        break;

    case 'a':
        toggleAutomaticControl( cap );
        break;

    case 'r':
    case 'R':
        cap.resetBrightnessSetting();
        cap.resetSharpnessSetting();
        cap.resetContrastSetting();
        cap.resetHueSetting();
        cap.resetSaturationSetting();
        cap.resetWhiteBalanceSetting();
        cap.resetGammaSetting();
        cap.resetAECAGC();

        std::cout << "All control settings are reset to default values" << std::endl;
        break;

    case '+':
    case 171:
        changeControlValue(cap,true);
        break;

    case '-':
    case 173:
        changeControlValue(cap,false);
        break;

    case 'h':
    case '?':
        std::cout << "COMMANDS HELP" << std::endl;
        std::cout << " * 'q' or 'Q' -> Quit the example" << std::endl;
        std::cout << " * 'h' or '?' -> This help menu" << std::endl;
        std::cout << " * 'l' -> Toggle camera led" << std::endl;
        std::cout << " * 'b' -> Brightness control" << std::endl;
        std::cout << " * 's' -> Saturation control" << std::endl;
        std::cout << " * 'c' -> Contrast control" << std::endl;
        std::cout << " * 'h' -> Hue control" << std::endl;
        std::cout << " * 'S' -> Sharpness control" << std::endl;
        std::cout << " * 'w' -> White Balance control" << std::endl;
        std::cout << " * 'g' -> Gamma control" << std::endl;
        std::cout << " * 'e' -> Exposure control" << std::endl;
        std::cout << " * 'G' -> Gain control" << std::endl;
        std::cout << " * 'a' -> Toggle automatic for White Balance or Exposure and Gain" << std::endl;
        std::cout << " * 'r' or 'R' -> Reset to default configuration" << std::endl;
        std::cout << " * '+' -> Increase the current control value" << std::endl;
        std::cout << " * '-' -> Decrease the current control value" << std::endl;
        std::cout << " * '0' .. '9' -> Set the current control value" << std::endl;
    }
}

void showImage( std::string name, cv::Mat& img, zed::RESOLUTION res )
{
    cv::Mat resized;
    switch(res)
    {
    default:
    case zed::RESOLUTION::VGA:
        resized = img;
        break;
    case zed::RESOLUTION::HD720:
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case zed::RESOLUTION::HD1080:
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    case zed::RESOLUTION::HD2K:
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    cv::imshow( name, resized );
}

void setActiveControl( CamControl control )
{
    activeControl = control;

    std::string ctrlStr;

    switch( activeControl )
    {
    case Brightness:
        ctrlStr = "Brightness";
        break;
    case Contrast:
        ctrlStr = "Contrast";
        break;
    case Hue:
        ctrlStr = "Hue";
        break;
    case Saturation:
        ctrlStr = "Saturation";
        break;
    case Gain:
        ctrlStr = "Gain";
        break;
    case WhiteBalance:
        ctrlStr = "White Balance";
        break;
    case Sharpness:
        ctrlStr = "Sharpness";
        break;
    case Gamma:
        ctrlStr = "Gamma";
        break;
    case Exposure:
        ctrlStr = "Exposure";
        break;
    }

    std::cout << "Active camera control: " << ctrlStr << std::endl;
}

void setControlValue(zed::VideoCapture &cap, int value )
{
    int newValue;
    switch( activeControl )
    {
    case Brightness:
        cap.setBrightnessSetting( value );
        newValue = cap.getBrightnessSetting();

        std::cout << "New Brightness value: ";
        break;

    case Contrast:
        cap.setContrastSetting( value );
        newValue = cap.getContrastSetting();

        std::cout << "New Contrast value: ";
        break;

    case Hue:
        cap.setHueSetting( value );
        newValue = cap.getHueSetting();

        std::cout << "New Hue value: ";
        break;

    case Saturation:
        cap.setSaturationSetting( value );
        newValue = cap.getSaturationSetting();

        std::cout << "New Saturation value: ";
        break;

    case Gain:
        break;

    case WhiteBalance:
        cap.setWhiteBalanceSetting( 2800+value*411 );
        newValue = cap.getWhiteBalanceSetting();

        std::cout << "New White Balance value: ";
        break;

    case Sharpness:
        cap.setSharpnessSetting( value );
        newValue = cap.getSharpnessSetting();

        std::cout << "New Sharpness value: ";
        break;

    case Gamma:
        cap.setGammaSetting( value);
        newValue = cap.getGammaSetting();

        std::cout << "New Gamma value: ";
        break;
    }

    std::cout << newValue << std::endl;
}

void changeControlValue( zed::VideoCapture &cap, bool increase )
{
    int curValue;
    switch( activeControl )
    {
    case Brightness:
        curValue = cap.getBrightnessSetting();
        break;

    case Contrast:
        curValue = cap.getContrastSetting();
        break;

    case Hue:
        curValue = cap.getHueSetting();

        std::cout << "New Hue value: ";
        break;

    case Saturation:
        curValue = cap.getSaturationSetting();
        break;

    case Gain:
    {
        int curValueLeft = cap.getGainSetting(zed::CAM_SENS_POS::LEFT);
        int curValueRight = cap.getGainSetting(zed::CAM_SENS_POS::RIGHT);

        if(increase)
        {
            cap.setGainSetting(zed::CAM_SENS_POS::LEFT,++curValueLeft);
            cap.setGainSetting(zed::CAM_SENS_POS::RIGHT,++curValueRight);
        }
        else
        {
            cap.setGainSetting(zed::CAM_SENS_POS::LEFT,--curValueLeft);
            cap.setGainSetting(zed::CAM_SENS_POS::RIGHT,--curValueRight);;
        }

        std::cout << "New Left Gain value: " << cap.getGainSetting(zed::CAM_SENS_POS::LEFT) << std::endl;
        std::cout << "New Right Gain value: " << cap.getGainSetting(zed::CAM_SENS_POS::RIGHT) << std::endl;
    }
        break;

    case WhiteBalance:
        cap.setAutoWhiteBalanceSetting(false);
        curValue = cap.getWhiteBalanceSetting();
        break;

    case Sharpness:
        curValue = cap.getSharpnessSetting();
        break;

    case Gamma:
        curValue = cap.getGammaSetting();
        break;
    }

    if(activeControl==WhiteBalance)
    {
        if(increase)
            curValue += 100;
        else
            curValue -= 100;

        cap.setWhiteBalanceSetting( curValue );

        std::cout << "New White Balance value: " << cap.getWhiteBalanceSetting() << std::endl;

    }
    else if(activeControl != Gain)
    {
        if(increase)
            setControlValue( cap, ++curValue);
        else
            setControlValue( cap, --curValue);
    }
}

void toggleAutomaticControl( zed::VideoCapture &cap )
{
    if(activeControl == WhiteBalance)
    {
        bool curValue = cap.getAutoWhiteBalanceSetting();
        cap.setAutoWhiteBalanceSetting( !curValue );

        std::cout << "Automatic White Balance control: " << ((!curValue)?"ENABLED":"DISABLED") << std::endl;
    }

    if(activeControl == Exposure || activeControl == Gain )
    {
        bool curValue = cap.getAECAGC();
        cap.setAECAGC( !curValue );

        std::cout << "Automatic Exposure and Gain control: " << ((!curValue)?"ENABLED":"DISABLED") << std::endl;
    }
}
