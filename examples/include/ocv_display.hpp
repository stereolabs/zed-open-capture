/**
 * @file ocv_display.hpp
 *
 * Functions to display OpenCV images.
 */

#ifndef OCV_DISPLAY_HPP
#define OCV_DISPLAY_HPP

#include <opencv2/opencv.hpp>
#include "videocapture.hpp"

namespace sl_oc
{

namespace tools
{

/*!
 * \brief Rescale the OpenCV T-API images [cv::UMat] according to the selected resolution to better display them on screen and show
 * \param name Name of the display window
 * \param img Image to be displayed
 * \param res Image resolution
 * \param change_name Add rescaling information in window name if true
 * \param info optional info string
 */
void showImage( std::string name, cv::UMat& img, sl_oc::video::RESOLUTION res, bool change_name=true, std::string info="" )
{
    cv::UMat resized;
    switch(res)
    {
    default:
    case sl_oc::video::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        if(change_name) name += " [Resize factor 0.6]";
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        if(change_name) name += " [Resize factor 0.4]";
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    if(!info.empty())
    {
        cv::putText( resized, info, cv::Point(20,40),cv::FONT_HERSHEY_SIMPLEX, 0.75,
                     cv::Scalar(100,100,100), 2);
    }

    cv::imshow( name, resized );
}

/*!
 * \brief Rescale the OpenCV images [cv::Mat] according to the selected resolution to better display them on screen and show
 * \param name Name of the display window
 * \param img Image to be displayed
 * \param res Image resolution
 * \param change_name Add rescaling information in window name if true
 * \param info optional info string
 */
void showImage( std::string name, cv::Mat& img, sl_oc::video::RESOLUTION res, bool change_name=true, std::string info="" )
{
    cv::Mat resized;
    switch(res)
    {
    default:
    case sl_oc::video::RESOLUTION::VGA:
        resized = img;
        break;
    case sl_oc::video::RESOLUTION::HD720:
        if(change_name) name += " [Resize factor 0.6]";
        cv::resize( img, resized, cv::Size(), 0.6, 0.6 );
        break;
    case sl_oc::video::RESOLUTION::HD1080:
    case sl_oc::video::RESOLUTION::HD2K:
        if(change_name) name += " [Resize factor 0.4]";
        cv::resize( img, resized, cv::Size(), 0.4, 0.4 );
        break;
    }

    if(!info.empty())
    {
        cv::putText( resized, info, cv::Point(20,40),cv::FONT_HERSHEY_SIMPLEX, 0.75,
                     cv::Scalar(100,100,100), 2);
    }

    cv::imshow( name, resized );
}

} // namespace tools
} // namespace sl_oc
#endif // OCV_DISPLAY_HPP
