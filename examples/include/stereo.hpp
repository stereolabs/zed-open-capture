#ifndef STEREO_HPP
#define STEREO_HPP

#include <iostream>
#include <opencv2/opencv.hpp>
#include "calibration.hpp"

namespace sl_oc {
namespace tools {

const std::string STEREO_PAR_FILENAME = "zed_oc_stereo.yaml";

class StereoSgbmPar
{
public:
    StereoSgbmPar()
    {
        setDefaultValues();
    }

    virtual ~StereoSgbmPar(){;}

    bool load();
    bool save();
    void setDefaultValues();

    void print();

public:
    int blockSize;
    int minDisparity;
    int numDisparities;
    int mode;
    int P1;
    int P2;
    int disp12MaxDiff;
    int preFilterCap;
    int uniquenessRatio;
    int speckleWindowSize;
    int speckleRange;

    float minDepth_mm;
    float maxDepth_mm;
};

void StereoSgbmPar::setDefaultValues()
{
    blockSize = 3;
    minDisparity = 0;
    numDisparities = 96;
    mode = cv::StereoSGBM::MODE_SGBM_3WAY; // MODE_SGBM = 0, MODE_HH   = 1, MODE_SGBM_3WAY = 2, MODE_HH4  = 3
    P1 = 24*blockSize*blockSize;
    P2 = 4*P1;
    disp12MaxDiff = 0;
    preFilterCap = 63;
    uniquenessRatio = 5;
    speckleWindowSize = 255;
    speckleRange = 1;

    minDepth_mm = 300.f;
    maxDepth_mm = 10000.f;
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

    std::cout << "minDepth_mm:\t\t" << minDepth_mm << std::endl;
    std::cout << "maxDepth_mm:\t\t" << maxDepth_mm << std::endl;
    std::cout << "------------------------------------------" << std::endl << std::endl;
}

}
}

#endif // STEREO_HPP

