#ifndef STEREO_HPP
#define STEREO_HPP

#include <iostream>
#include <opencv2/opencv.hpp>
#include "calibration.hpp"

namespace sl_oc {
namespace tools {

/*!
 * \brief STEREO_PAR_FILENAME default stereo parameter configuration file
 */
const std::string STEREO_PAR_FILENAME = "zed_oc_stereo.yaml";

/*!
 * \brief The StereoSgbmPar class is used to store/retrieve the stereo matching parameters
 */
class StereoSgbmPar
{
public:
    /*!
     * \brief Default constructor
     */
    StereoSgbmPar()
    {
        setDefaultValues();
    } 

    /*!
     * \brief load stereo matching parameters
     * \return true if a configuration file exists
     */
    bool load();

    /*!
     * \brief save stereo matching parameters
     * \return true if a configuration file has been correctly created
     */
    bool save();

    /*!
     * \brief set default stereo matching parameters
     */
    void setDefaultValues();

    /*!
     * \brief print the current stereo matching parameters on standard output
     */
    void print();

public:
    int blockSize; //!< [default: 3] Matched block size. It must be an odd number >=1 . Normally, it should be somewhere in the 3..11 range.
    int minDisparity; //!< [default: 0] Minimum possible disparity value. Normally, it is zero but sometimes rectification algorithms can shift images, so this parameter needs to be adjusted accordingly.
    int numDisparities; //!< [default: 96] Maximum disparity minus minimum disparity. The value is always greater than zero. In the current implementation, this parameter must be divisible by 16.
    int mode; //!< Set it to StereoSGBM::MODE_HH to run the full-scale two-pass dynamic programming algorithm. It will consume O(W*H*numDisparities) bytes, which is large for 640x480 stereo and huge for HD-size pictures. By default, it is set to `cv::StereoSGBM::MODE_SGBM_3WAY`.
    int P1; //!< [default: 24*blockSize*blockSize] The first parameter controlling the disparity smoothness. See below.
    int P2; //!< [default: 4*PI]The second parameter controlling the disparity smoothness. The larger the values are, the smoother the disparity is. P1 is the penalty on the disparity change by plus or minus 1 between neighbor pixels. P2 is the penalty on the disparity change by more than 1 between neighbor pixels. The algorithm requires P2 > P1 . See stereo_match.cpp sample where some reasonably good P1 and P2 values are shown (like 8*number_of_image_channels*blockSize*blockSize and 32*number_of_image_channels*blockSize*blockSize , respectively).
    int disp12MaxDiff; //!< [default: 96] Maximum allowed difference (in integer pixel units) in the left-right disparity check. Set it to a non-positive value to disable the check.
    int preFilterCap; //!< [default: 63] Truncation value for the prefiltered image pixels. The algorithm first computes x-derivative at each pixel and clips its value by [-preFilterCap, preFilterCap] interval. The result values are passed to the Birchfield-Tomasi pixel cost function.
    int uniquenessRatio; //!< [default: 5] Margin in percentage by which the best (minimum) computed cost function value should "win" the second best value to consider the found match correct. Normally, a value within the 5-15 range is good enough.
    int speckleWindowSize; //!< [default: 255] Maximum size of smooth disparity regions to consider their noise speckles and invalidate. Set it to 0 to disable speckle filtering. Otherwise, set it somewhere in the 50-200 range.
    int speckleRange; //!< [default: 1] Maximum disparity variation within each connected component. If you do speckle filtering, set the parameter to a positive value, it will be implicitly multiplied by 16. Normally, 1 or 2 is good enough.

    double minDepth_mm; //!< [default: 300] Minimum value of depth for the extracted depth map
    double maxDepth_mm; //!< [default: 10000] Maximum value of depth for the extracted depth map
};

void StereoSgbmPar::setDefaultValues()
{
    blockSize = 3;
    minDisparity = 0;
    numDisparities = 96;
    mode = cv::StereoSGBM::MODE_SGBM_3WAY; // MODE_SGBM = 0, MODE_HH   = 1, MODE_SGBM_3WAY = 2, MODE_HH4  = 3
    P1 = 24*blockSize*blockSize;
    P2 = 4*P1;
    disp12MaxDiff = 96;
    preFilterCap = 63;
    uniquenessRatio = 5;
    speckleWindowSize = 255;
    speckleRange = 1;

    minDepth_mm = 300.;
    maxDepth_mm = 10000.;
}

bool StereoSgbmPar::load()
{
    std::string path = getHiddenDir();
    std::string par_file = path + STEREO_PAR_FILENAME;

    cv::FileStorage fs;
    if(!fs.open(par_file, cv::FileStorage::READ))
    {
        std::cerr << "Error opening stereo parameters file. Using default values." << std::endl << std::endl;
        setDefaultValues();
        return false;
    }

    fs["blockSize"] >> blockSize;
    fs["minDisparity"] >> minDisparity;
    fs["numDisparities"] >> numDisparities;
    fs["mode"] >> mode;
    fs["disp12MaxDiff"] >> disp12MaxDiff;
    fs["preFilterCap"] >> preFilterCap;
    fs["uniquenessRatio"] >> uniquenessRatio;
    fs["speckleWindowSize"] >> speckleWindowSize;
    fs["speckleRange"] >> speckleRange;
    P1 = 24*blockSize*blockSize;
    P2 = 96*blockSize*blockSize;

    fs["minDepth_mm"] >> minDepth_mm;
    fs["maxDepth_mm"] >> maxDepth_mm;

    std::cout << "Stereo parameters load done: " << par_file << std::endl << std::endl;

    return true;
}

bool StereoSgbmPar::save()
{
    std::string path = getHiddenDir();
    std::string par_file = path + STEREO_PAR_FILENAME;

    cv::FileStorage fs;
    if(!fs.open(par_file, cv::FileStorage::WRITE))
    {
        std::cerr << "Error saving stereo parameters. Cannot open file for writing: " << par_file << std::endl << std::endl;
        return false;
    }

    fs << "blockSize" << blockSize;
    fs << "minDisparity" << minDisparity;
    fs << "numDisparities" << numDisparities;
    fs << "mode" << mode;
    fs << "disp12MaxDiff" << disp12MaxDiff;
    fs << "preFilterCap" << preFilterCap;
    fs << "uniquenessRatio" << uniquenessRatio;
    fs << "speckleWindowSize" << speckleWindowSize;
    fs << "speckleRange" << speckleRange;

    fs << "minDepth_mm" << minDepth_mm;
    fs << "maxDepth_mm" << maxDepth_mm;

    std::cout << "Stereo parameters write done: " << par_file << std::endl << std::endl;

    return true;
}

void StereoSgbmPar::print()
{
    std::cout << "Stereo SGBM parameters:" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "blockSize:\t\t" << blockSize << std::endl;
    std::cout << "minDisparity:\t" << minDisparity << std::endl;
    std::cout << "numDisparities:\t" << numDisparities << std::endl;
    std::cout << "mode:\t\t" << mode << std::endl;
    std::cout << "disp12MaxDiff:\t" << disp12MaxDiff << std::endl;
    std::cout << "preFilterCap:\t" << preFilterCap << std::endl;
    std::cout << "uniquenessRatio:\t" << uniquenessRatio << std::endl;
    std::cout << "speckleWindowSize:\t" << speckleWindowSize << std::endl;
    std::cout << "speckleRange:\t" << speckleRange << std::endl;
    std::cout << "P1:\t\t" << P1 << " [Calculated]" << std::endl;
    std::cout << "P2:\t\t" << P2 << " [Calculated]" << std::endl;

    std::cout << "minDepth_mm:\t" << minDepth_mm << std::endl;
    std::cout << "maxDepth_mm:\t" << maxDepth_mm << std::endl;
    std::cout << "------------------------------------------" << std::endl << std::endl;
}

} // namespace tools
} // namespace sl_oc

#endif // STEREO_HPP

