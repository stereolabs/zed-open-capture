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

// ----> Includes
#include "videocapture.hpp"

#include <iostream>
#include <iomanip>

#include <opencv2/opencv.hpp>
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
static void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res );

// Handle Keyboard
static void handleKeyboard( sl_oc::video::VideoCapture &cap, int key );

// Handle Mouse events
static void handleMouse(int event, int x, int y, int, void*);

// Change active control
static void setActiveControl(sl_oc::video::VideoCapture &cap, CamControl control );

// Set new value for the active control
static void setControlValue( sl_oc::video::VideoCapture &cap, int value );

// '+' or '-' pressed
static void changeControlValue( sl_oc::video::VideoCapture &cap, bool increase );

// 'a' or 'A' pressed to enable automatic WhiteBalanse or Gain/Exposure
static void toggleAutomaticControl( sl_oc::video::VideoCapture &cap );

// Retrieve all the control values from the camera
static void updateAllCtrlValues(sl_oc::video::VideoCapture &cap);

// Reset all the control values to default
static void resetControls( sl_oc::video::VideoCapture &cap );
// <---- Global functions to control settings

// ----> Global variables
cv::String win_name = "Stream RGB";      // Name of the stream window
bool selectInProgress = false;           // Indicates that an AECAGC ROI is being drawn
bool selectLeft = false;
bool selectRight = false;
cv::Rect aecagc_roi_left = {0,0,0,0};    // The current agcaec ROI rectangle
cv::Rect aecagc_roi_right = {0,0,0,0};   // The current agcaec ROI rectangle
cv::Point origin_roi = {0,0};            // Click point for AECAGC ROI
double img_resize_factor = 1.0;          // Image resize factor for drawing
int img_w = 0;                           // Camera image width
int img_h = 0;                           // Camera image height

bool logging = false; // Indicates if AEG/AGC registers logging is enabled

uint8_t brightness_val;
uint8_t contrast_val;
uint8_t hue_val;
uint8_t saturation_val;
uint8_t gain_val_left;
uint8_t gain_val_right;
uint8_t exposure_val_left;
uint8_t exposure_val_right;
int whiteBalance_val;
uint8_t sharpness_val;
uint8_t gamma_val;
bool autoAECAGC=false;
bool autoWB=false;

bool applyAECAGCrectLeft=false;
bool applyAECAGCrectRight=false;
// <---- Global variables

// The main function
int main(int argc, char *argv[])
{
    // ----> Silence unused warning
    (void)argc;
    (void)argv;
    // <---- Silence unused warning

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

    // ----> Create rendering window
    cv::namedWindow(win_name);
    cv::setMouseCallback(win_name, handleMouse);
    // <---- Create rendering window

    // Reset all the controls to default values
    resetControls(cap);

    // Set the default camera control setting
    setActiveControl(cap, Brightness );

    // Update the values for all the controls
    updateAllCtrlValues(cap);

    uint64_t last_ts=0;
    uint16_t not_a_new_frame = 0;
    int frame_timeout_msec = 100;

    // Infinite video grabbing loop
    while (1)
    {
        // 3) Get last available frame
        const sl_oc::video::Frame frame = cap.getLastFrame(frame_timeout_msec);
        img_w = frame.width;
        img_h = frame.height;

        // 3a) Apply AEC AGC ROI if necessary
        if(applyAECAGCrectLeft)
        {
            applyAECAGCrectLeft = false;
            cap.setROIforAECAGC( sl_oc::video::CAM_SENS_POS::LEFT,
                                 aecagc_roi_left.x, aecagc_roi_left.y,
                                 aecagc_roi_left.width, aecagc_roi_left.height);
            selectLeft=false;
            selectRight=false;
        }
        if(applyAECAGCrectRight)
        {
            applyAECAGCrectRight = false;
            cap.setROIforAECAGC( sl_oc::video::CAM_SENS_POS::RIGHT,
                                 aecagc_roi_right.x, aecagc_roi_right.y,
                                 aecagc_roi_right.width, aecagc_roi_right.height);
            selectLeft=false;
            selectRight=false;
        }

        // ----> If the frame is valid we can display it
        if(frame.data!=nullptr && frame.timestamp!=last_ts)
        {
            not_a_new_frame=0;
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
            showImage( win_name, frameBGR, params.res );
        }
        else if(frame.timestamp==last_ts)
        {
            not_a_new_frame++;
            std::cout << "Not a new frame #" << not_a_new_frame << std::endl;

            if( not_a_new_frame>=(3000/frame_timeout_msec)) // Lost connection for 5 seconds
            {
                std::cout << "Camera connection lost. Closing..." << std::endl;
                break;
            }
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
#ifdef SENSOR_LOG_AVAILABLE
    case 'L':
    {
        logging = !logging;
        cap.enableAecAgcSensLogging( logging, 5 );
        std::cout << std::string("*** AEC/AGC registers logging: ") << (logging?std::string("ENABLED"):std::string("DISABLED")) << std::endl;
    }
        break;

    case 'f':
    case 'F':
    {
        bool res = cap.resetAGCAECregisters();
        std::cout << std::string("*** AEC/AGC registers reset: ") << (res?std::string("OK"):std::string("KO")) << std::endl;
    }
        break;
#endif

    case 'l':
    {
        bool led;
        cap.toggleLED( &led );
        std::cout << std::string(" * LED STATUS: ") << (led?std::string("ON"):std::string("OFF")) << std::endl;
    }
        break;

    case 'b':
        setActiveControl( cap, Brightness );
        break;

    case 'S':
        setActiveControl( cap, Sharpness );
        break;

    case 'c':
        setActiveControl( cap, Contrast );
        break;

    case 'h':
        setActiveControl( cap, Hue );
        break;

    case 's':
        setActiveControl( cap, Saturation );
        break;

    case 'w':
        setActiveControl( cap, WhiteBalance );
        break;

    case 'g':
        setActiveControl( cap, Gamma );
        break;

    case 'e':
        setActiveControl( cap, Exposure );
        break;

    case 'G':
        setActiveControl( cap, Gain );
        break;

    case 'a':
        toggleAutomaticControl( cap );
        break;

    case 'r':
    case 'R':
        resetControls(cap);
        updateAllCtrlValues(cap);

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

    case '?':
        std::cout << "COMMANDS HELP" << std::endl;
        std::cout << " * 'q' or 'Q' -> Quit the example" << std::endl;
        std::cout << " * '?' -> This help menu" << std::endl;
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
#ifdef SENSOR_LOG_AVAILABLE
        std::cout << " * 'L' -> Toggle AGC/AEC registers logging" << std::endl;
        std::cout << " * 'f' -> Fix AGC/AEC registers" << std::endl;
#endif
    }
}

void updateAllCtrlValues(sl_oc::video::VideoCapture &cap)
{
    brightness_val = cap.getBrightness();
    sharpness_val = cap.getSharpness();
    contrast_val = cap.getContrast();
    hue_val = cap.getHue();
    saturation_val = cap.getSaturation();
    gamma_val = cap.getGamma();
    autoAECAGC = cap.getAECAGC();
    autoWB = cap.getAutoWhiteBalance();
    gain_val_left = cap.getGain(sl_oc::video::CAM_SENS_POS::LEFT);
    gain_val_right = cap.getGain(sl_oc::video::CAM_SENS_POS::RIGHT);
    whiteBalance_val = cap.getWhiteBalance();

    uint16_t x,y,w,h;
    cap.getROIforAECAGC(sl_oc::video::CAM_SENS_POS::LEFT, x,y,w,h);
    aecagc_roi_left.x = x;
    aecagc_roi_left.y = y;
    aecagc_roi_left.width = w;
    aecagc_roi_left.height = h;
    cap.getROIforAECAGC(sl_oc::video::CAM_SENS_POS::RIGHT, x,y,w,h);
    aecagc_roi_right.x = x;
    aecagc_roi_right.y = y;
    aecagc_roi_right.width = w;
    aecagc_roi_right.height = h;
}

void handleMouse(int event, int x, int y, int, void*)
{
    switch (event)
    {
    case cv::EVENT_LBUTTONDOWN: // AEC AGC ROI drawing started
    {
        if(autoAECAGC && (activeControl==Gain || activeControl==Exposure))
        {
            origin_roi = cv::Point(x, y);
            selectInProgress = true;
        }
        break;
    }

    case cv::EVENT_LBUTTONUP: // AECAGC ROI drawing completed
    {
        selectInProgress = false;

        if(autoAECAGC && (activeControl==Gain || activeControl==Exposure))
        {
            if(selectLeft)
            {
                selectLeft=false;
                applyAECAGCrectLeft = true;
            }
            if(selectRight)
            {
                selectRight=false;
                applyAECAGCrectRight = true;
            }
        }
        break;
    }

    case cv::EVENT_RBUTTONDOWN: // Reset AECAGC ROI with right button
    {
        //Reset selection
        selectInProgress = false;
        aecagc_roi_left = cv::Rect(0,0,img_w/2,img_h);
        aecagc_roi_right = cv::Rect(0,0,img_w/2,img_h);
        applyAECAGCrectLeft = true;
        applyAECAGCrectRight = true;
        break;
    }
    }

    if(selectInProgress) // AECAGC ROI drawing in progress
    {
        x /= img_resize_factor;
        y /= img_resize_factor;

        int or_x = origin_roi.x/img_resize_factor;
        int or_y = origin_roi.y/img_resize_factor;

        y = MAX(y,0);
        y = MIN(y,img_h);
        if(or_x<img_w/2) // AECAGC ROI for the left image
        {
            x = MAX(x,0);
            x = MIN(x,img_w/2-1);
            selectLeft = true;

            aecagc_roi_left.x = MIN(x, or_x);
            aecagc_roi_left.y = MIN(y, or_y);
            aecagc_roi_left.width = abs(x - or_x) + 1;
            aecagc_roi_left.height = abs(y - or_y) + 1;
        }
        else  // AECAGC ROI for the right image
        {
            or_x -= img_w/2;
            x -= img_w/2;
            x = MAX(x,0);
            x = MIN(x,img_w/2-1);
            selectRight = true;

            aecagc_roi_right.x = MIN(x, or_x);
            aecagc_roi_right.y = MIN(y, or_y);
            aecagc_roi_right.width = abs(x - or_x) + 1;
            aecagc_roi_right.height = abs(y - or_y) + 1;
        }
    }
}

// Change active control
void setActiveControl(sl_oc::video::VideoCapture &cap, CamControl control )
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
        brightness_val = newValue;

        std::cout << "New Brightness value: ";
        break;

    case Contrast:
        cap.setContrast( value );
        newValue = cap.getContrast();
        contrast_val = newValue;

        std::cout << "New Contrast value: ";
        break;

    case Hue:
        cap.setHue( value );
        newValue = cap.getHue();
        hue_val = newValue;

        std::cout << "New Hue value: ";
        break;

    case Saturation:
        cap.setSaturation( value );
        newValue = cap.getSaturation();
        saturation_val = newValue;

        std::cout << "New Saturation value: ";
        break;

    case WhiteBalance:
        cap.setWhiteBalance( 2800+value*411 );
        newValue = cap.getWhiteBalance();
        whiteBalance_val = newValue;

        std::cout << "New White Balance value: ";
        break;

    case Sharpness:
        cap.setSharpness( value );
        newValue = cap.getSharpness();
        sharpness_val = newValue;

        std::cout << "New Sharpness value: ";
        break;

    case Gamma:
        cap.setGamma( value);
        newValue = cap.getGamma();
        gamma_val = newValue;

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
        brightness_val = curValue;
        break;

    case Contrast:
        curValue = cap.getContrast();
        contrast_val = curValue;
        break;

    case Hue:
        curValue = cap.getHue();
        hue_val = curValue;
        break;

    case Saturation:
        curValue = cap.getSaturation();
        saturation_val = curValue;
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

        gain_val_left = cap.getGain(sl_oc::video::CAM_SENS_POS::LEFT);
        gain_val_right = cap.getGain(sl_oc::video::CAM_SENS_POS::RIGHT);

        std::cout << "New Left Gain value: " << (int)gain_val_left << std::endl;
        std::cout << "New Right Gain value: " << (int)gain_val_right << std::endl;

        autoAECAGC = cap.getAECAGC();
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

        exposure_val_left = cap.getExposure(sl_oc::video::CAM_SENS_POS::LEFT);
        exposure_val_right = cap.getExposure(sl_oc::video::CAM_SENS_POS::RIGHT);

        std::cout << "New Left Exposure value: " << (int)exposure_val_left << std::endl;
        std::cout << "New Right Exposure value: " << (int)exposure_val_right << std::endl;

        autoAECAGC = cap.getAECAGC();
    }
        break;

    case WhiteBalance:
        cap.setAutoWhiteBalance(false);
        curValue = cap.getWhiteBalance();
        whiteBalance_val = curValue;
        autoWB = cap.getAutoWhiteBalance();
        break;

    case Sharpness:
        curValue = cap.getSharpness();
        sharpness_val = curValue;
        break;

    case Gamma:
        curValue = cap.getGamma();
        gamma_val = curValue;
        break;
    }

    if(activeControl==WhiteBalance)
    {
        if(increase)
            curValue += 100;
        else
            curValue -= 100;

        cap.setWhiteBalance( curValue );

        whiteBalance_val = cap.getWhiteBalance();
        std::cout << "New White Balance value: " << whiteBalance_val << std::endl;
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
        autoWB = cap.getAutoWhiteBalance();

        std::cout << "Automatic White Balance control: " << ((!curValue)?"ENABLED":"DISABLED") << std::endl;
    }

    if(activeControl == Exposure || activeControl == Gain )
    {
        bool curValue = cap.getAECAGC();
        cap.setAECAGC( !curValue );
        autoAECAGC = cap.getAECAGC();

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
        img_resize_factor = 1.0;
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        name += " [Resize factor 0.6]";
        img_resize_factor = 0.6;
        cv::resize( img, resized, cv::Size(), img_resize_factor, img_resize_factor );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        name += " [Resize factor 0.4]";
        img_resize_factor = 0.4;
        cv::resize( img, resized, cv::Size(), img_resize_factor, img_resize_factor );
        break;
    }

    if(autoAECAGC && (activeControl==Gain || activeControl==Exposure))
    {
        // Check that left selection rectangle is valid and draw it on the image
        if ( (aecagc_roi_left.area()>0) &&
             (aecagc_roi_left.width-aecagc_roi_left.x)<=img_w/2 &&
             (aecagc_roi_left.height-aecagc_roi_left.y)<=img_h)
        {
            cv::Rect rescaled_roi;
            rescaled_roi.x = aecagc_roi_left.x*img_resize_factor;
            rescaled_roi.y = aecagc_roi_left.y*img_resize_factor;
            rescaled_roi.width = aecagc_roi_left.width*img_resize_factor;
            rescaled_roi.height = aecagc_roi_left.height*img_resize_factor;
            cv::rectangle(resized, rescaled_roi, cv::Scalar(220, 180, 20), 2);
        }

        // Check that right selection rectangle is valid and draw it on the image
        if ( (aecagc_roi_right.area()>0) &&
             (aecagc_roi_right.width-aecagc_roi_right.x)<=img_w/2 &&
             (aecagc_roi_right.height-aecagc_roi_right.y)<=img_h)
        {
            cv::Rect rescaled_roi;
            rescaled_roi.x = (img_w/2+aecagc_roi_right.x)*img_resize_factor;
            rescaled_roi.y = aecagc_roi_right.y*img_resize_factor;
            rescaled_roi.width = aecagc_roi_right.width*img_resize_factor;
            rescaled_roi.height = aecagc_roi_right.height*img_resize_factor;
            cv::rectangle(resized, rescaled_roi, cv::Scalar(20, 180, 220), 2);
        }
    }

    std::string info;
    switch (activeControl)
    {
    case Brightness:
        info = "Brightness: ";
        info += std::to_string(brightness_val);
        break;
    case Contrast:
        info = "Contrast: ";
        info += std::to_string(contrast_val);
        break;
    case Hue:
        info = "Hue: ";
        info += std::to_string(hue_val);
        break;
    case Saturation:
        info = "Saturation: ";
        info += std::to_string(saturation_val);
        break;
    case Gain:
        info = "Gain: ";
        if(autoAECAGC)
        {
            info += "AUTO";
        }
        else
        {
            info += std::to_string(gain_val_left);
            info += " - ";
            info += std::to_string(gain_val_right);
        }
        break;
    case Exposure:
        info = "Exposure: ";
        if(autoAECAGC)
        {
            info += "AUTO";
        }
        else
        {
            info += std::to_string(exposure_val_left);
            info += " - ";
            info += std::to_string(exposure_val_right);
        }
        break;
    case WhiteBalance:
        info = "WhiteBalance: ";
        if(autoWB)
        {
            info += "AUTO";
        }
        else
        {
            info += std::to_string(whiteBalance_val);
        }
        break;
    case Sharpness:
        info = "Sharpness: ";
        info += std::to_string(sharpness_val);
        break;
    case Gamma:
        info = "Gamma: ";
        info += std::to_string(gamma_val);
        break;
    }

    cv::putText( resized, info, cv::Point(20,40),cv::FONT_HERSHEY_SIMPLEX, 0.75,
                 cv::Scalar(241,240,236), 2);

    cv::imshow( win_name, resized );
}

void resetControls( sl_oc::video::VideoCapture &cap )
{
    cap.resetBrightness();
    cap.resetSharpness();
    cap.resetContrast();
    cap.resetHue();
    cap.resetSaturation();
    cap.resetGamma();
    cap.resetAECAGC();
    cap.resetAutoWhiteBalance();
    cap.resetROIforAECAGC(sl_oc::video::CAM_SENS_POS::LEFT);
    cap.resetROIforAECAGC(sl_oc::video::CAM_SENS_POS::RIGHT);
}
