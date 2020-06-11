///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2020, STEREOLABS.
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

// ----> Includes
#include "videocapture.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
// <---- Includes

// ----> Camera settings control
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
// <---- Camera settings control

// ----> Global functions to control settings
// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res );

// Handle Keyboard
void handleKeyboard( sl_oc::video::VideoCapture &cap, int key );

// Change active control
void setActiveControl( CamControl control );

// Set new value for the active control
void setControlValue( sl_oc::video::VideoCapture &cap, int value );

// '+' or '-' pressed
void changeControlValue( sl_oc::video::VideoCapture &cap, bool increase );

// 'a' or 'A' pressed to enable automatic WhiteBalanse or Gain/Exposure
void toggleAutomaticControl( sl_oc::video::VideoCapture &cap );
// <---- Global functions to control settings

// The main function
int main(int argc, char *argv[])
{
    sl_oc::VERBOSITY verbose = sl_oc::VERBOSITY::INFO;

    // ----> Set Video parameters
    sl_oc::video::VideoParams params;
    params.res = sl_oc::video::RESOLUTION::HD2K;
    params.fps = sl_oc::video::FPS::FPS_15;
    params.verbose = verbose;
    // <---- Set Video parameters

    // ----> Create Video Capture
    sl_oc::video::VideoCapture cap(params);
    if( !cap.initializeVideo(-1) )
    {
        std::cerr << "Cannot open camera video capture" << std::endl;
        std::cerr << "See verbosity level for more details." << std::endl;

        return EXIT_FAILURE;
    }
    std::cout << "Connected to camera sn: " << cap.getSerialNumber() << std::endl;
    // <---- Create Video Capture

    // Set the default camera control setting
    setActiveControl( Brightness );

    uint64_t last_ts=0;

    // Infinite video grabbing loop
    while (1)
    {
        // 3) Get last available frame
        const sl_oc::video::Frame frame = cap.getLastFrame();

        // ----> If the frame is valid we can display it
        if(frame.data!=nullptr && frame.timestamp!=last_ts)
        {
#if 0
            // ----> Video Debug information

            std::cout << std::setprecision(9) << "[" << frame.frame_id << "] Ts: " <<  static_cast<double>(frame.timestamp)/1e9 << " sec" << std::endl;
            if( last_ts!=0 )
            {
                double dt = (frame.timestamp - last_ts)/1e9;
                std::cout << std::setprecision(9) << " * dT: " << dt << " sec - FPS: " << 1./dt <<  std::endl;
            }

            // <---- Video Debug information
#endif
            last_ts = frame.timestamp;

            // ----> Conversion from YUV 4:2:2 to BGR for visualization
            cv::Mat frameYUV = cv::Mat( frame.height, frame.width, CV_8UC2, frame.data );
            cv::Mat frameBGR;
            cv::cvtColor(frameYUV,frameBGR,cv::COLOR_YUV2BGR_YUYV);
            // <---- Conversion from YUV 4:2:2 to BGR for visualization

            // 4.c) Show frame
            showImage( "Stream RGB", frameBGR, params.res );
        }
        // <---- If the frame is valid we can display it

        // ----> Keyboard handling
        int key = cv::waitKey( 5 );

        if( key != -1 )
        {
            if(key=='q' || key=='Q') // Quit
                break;
            else
                handleKeyboard( cap, key );
        }
        // <---- Keyboard handling
    }

    return EXIT_SUCCESS;
}

// Handle Keyboard
void handleKeyboard( sl_oc::video::VideoCapture &cap, int key )
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
        cap.resetBrightness();
        cap.resetSharpness();
        cap.resetContrast();
        cap.resetHue();
        cap.resetSaturation();
        cap.resetGamma();
        cap.resetAECAGC();
        cap.resetAutoWhiteBalance();

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

// Change active control
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

// Set new value for the active control
void setControlValue(sl_oc::video::VideoCapture &cap, int value )
{
    int newValue;
    switch( activeControl )
    {
    case Brightness:
        cap.setBrightness( value );
        newValue = cap.getBrightness();

        std::cout << "New Brightness value: ";
        break;

    case Contrast:
        cap.setContrast( value );
        newValue = cap.getContrast();

        std::cout << "New Contrast value: ";
        break;

    case Hue:
        cap.setHue( value );
        newValue = cap.getHue();

        std::cout << "New Hue value: ";
        break;

    case Saturation:
        cap.setSaturation( value );
        newValue = cap.getSaturation();

        std::cout << "New Saturation value: ";
        break;

    case WhiteBalance:
        cap.setWhiteBalance( 2800+value*411 );
        newValue = cap.getWhiteBalance();

        std::cout << "New White Balance value: ";
        break;

    case Sharpness:
        cap.setSharpness( value );
        newValue = cap.getSharpness();

        std::cout << "New Sharpness value: ";
        break;

    case Gamma:
        cap.setGamma( value);
        newValue = cap.getGamma();

        std::cout << "New Gamma value: ";
        break;

    case Gain:
    case Exposure:
    default:
        // Nothing to do here
        return;
    }

    std::cout << newValue << std::endl;
}

// '+' or '-' pressed
void changeControlValue( sl_oc::video::VideoCapture &cap, bool increase )
{
    int curValue=0;
    switch( activeControl )
    {
    case Brightness:
        curValue = cap.getBrightness();
        break;

    case Contrast:
        curValue = cap.getContrast();
        break;

    case Hue:
        curValue = cap.getHue();

        std::cout << "New Hue value: ";
        break;

    case Saturation:
        curValue = cap.getSaturation();
        break;

    case Gain:
    {
        int curValueLeft = cap.getGain(sl_oc::video::CAM_SENS_POS::LEFT);
        int curValueRight = cap.getGain(sl_oc::video::CAM_SENS_POS::RIGHT);

        if(increase)
        {
            cap.setGain(sl_oc::video::CAM_SENS_POS::LEFT,++curValueLeft);
            cap.setGain(sl_oc::video::CAM_SENS_POS::RIGHT,++curValueRight);
        }
        else
        {
            cap.setGain(sl_oc::video::CAM_SENS_POS::LEFT,--curValueLeft);
            cap.setGain(sl_oc::video::CAM_SENS_POS::RIGHT,--curValueRight);;
        }

        std::cout << "New Left Gain value: " << cap.getGain(sl_oc::video::CAM_SENS_POS::LEFT) << std::endl;
        std::cout << "New Right Gain value: " << cap.getGain(sl_oc::video::CAM_SENS_POS::RIGHT) << std::endl;
    }
        break;

    case Exposure:
    {
        int curValueLeft = cap.getExposure(sl_oc::video::CAM_SENS_POS::LEFT);
        int curValueRight = cap.getExposure(sl_oc::video::CAM_SENS_POS::RIGHT);

        if(increase)
        {
            cap.setExposure(sl_oc::video::CAM_SENS_POS::LEFT,++curValueLeft);
            cap.setExposure(sl_oc::video::CAM_SENS_POS::RIGHT,++curValueRight);
        }
        else
        {
            cap.setExposure(sl_oc::video::CAM_SENS_POS::LEFT,--curValueLeft);
            cap.setExposure(sl_oc::video::CAM_SENS_POS::RIGHT,--curValueRight);;
        }

        std::cout << "New Left Exposure value: " << cap.getExposure(sl_oc::video::CAM_SENS_POS::LEFT) << std::endl;
        std::cout << "New Right Exposure value: " << cap.getExposure(sl_oc::video::CAM_SENS_POS::RIGHT) << std::endl;
    }
        break;

    case WhiteBalance:
        cap.setAutoWhiteBalance(false);
        curValue = cap.getWhiteBalance();
        break;

    case Sharpness:
        curValue = cap.getSharpness();
        break;

    case Gamma:
        curValue = cap.getGamma();
        break;
    }

    if(activeControl==WhiteBalance)
    {
        if(increase)
            curValue += 100;
        else
            curValue -= 100;

        cap.setWhiteBalance( curValue );

        std::cout << "New White Balance value: " << cap.getWhiteBalance() << std::endl;

    }
    else if(activeControl != Gain && activeControl != Exposure)
    {
        if(increase)
            setControlValue( cap, ++curValue);
        else
            setControlValue( cap, --curValue);
    }
}

// 'a' or 'A' pressed to enable automatic WhiteBalanse or Gain/Exposure
void toggleAutomaticControl( sl_oc::video::VideoCapture &cap )
{
    if(activeControl == WhiteBalance)
    {
        bool curValue = cap.getAutoWhiteBalance();
        cap.setAutoWhiteBalance( !curValue );

        std::cout << "Automatic White Balance control: " << ((!curValue)?"ENABLED":"DISABLED") << std::endl;
    }

    if(activeControl == Exposure || activeControl == Gain )
    {
        bool curValue = cap.getAECAGC();
        cap.setAECAGC( !curValue );

        std::cout << "Automatic Exposure and Gain control: " << ((!curValue)?"ENABLED":"DISABLED") << std::endl;
    }
}

// Rescale the images according to the selected resolution to better display them on screen
void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res )
{
    cv::Mat resized;
    switch(res)
    {
    default:
    case sl_oc::video::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        name += " [Resize factor 0.6]";
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        name += " [Resize factor 0.4]";
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    cv::imshow( name, resized );
}
