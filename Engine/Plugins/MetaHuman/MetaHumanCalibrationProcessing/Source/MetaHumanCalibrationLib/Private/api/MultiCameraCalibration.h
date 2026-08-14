// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"
#include "OpenCVCamera.h"

#include <vector>
#include <map>
#include <string>
#include <utility>

namespace TITAN_API_NAMESPACE
{

class TITAN_API MultiCameraCalibration
{
public:
    MultiCameraCalibration();
    ~MultiCameraCalibration();
    MultiCameraCalibration(MultiCameraCalibration&&) = delete;
    MultiCameraCalibration(const MultiCameraCalibration&) = delete;
    MultiCameraCalibration& operator=(MultiCameraCalibration&&) = delete;
    MultiCameraCalibration& operator=(const MultiCameraCalibration&) = delete;

    /**
     * Initialize multi camera calibration.
     * @param[in] InPatternWidth The number of inner corners on the chessboard pattern wide
     * @param[in] InPatternHeight The number of inner corners on the chessboard pattern high
     * @param[in] InPatternSquareSize The size of the square on the chessboard pattern in cm
     * @returns True if initialization is successful, False otherwise.
     */
    bool Init(size_t InPatternWidth, size_t InPatternHeight, double InPatternSquareSize);

    /**
     * Adds a camera view used for calibration
     * @param[in] InCameraName The name of the camera view
     * @param[in] InWidth The width of the camera image in pixels
     * @param[in] InHeight The height of the camera image in pixels
     * @returns True if successful, false upon any error.
     */
    bool AddCamera(const std::string& InCameraName, int32_t InWidth, int32_t InHeight);

    /**
     * Detects the corner points in the image of a chessboard pattern. Calculates an average sharpness of the corner points in the image
     * @param[in] InCameraName The name of the camera view
     * @param[in] InImage The image of the chessboard pattern
     * @param[out] OutPoints The corner points detected in the image
     * @param[out] OutChessboardSharpness The estimated sharpness of the corner points in the image points detected in the image
     * @returns True if successful, false if chessboard pattern is not detected.
     */
    bool DetectPattern(const std::string& InCameraName, const unsigned char* InImage, std::vector<float>& OutPoints, double& OutChessboardSharpness) const;

    /**
     * Calibrates the intrinsics and extrinsics of the added cameras
     * @param[in] InCornerPointsPerCameraPerFrame The detected corner points per camera per frame
     * @param[out] OutCalibrations The calibrated cameras
     * @param[out] OutMSE The reprojection error from the calibration
     * @returns True if successful, false upon any error.
     */
    bool Calibrate(const std::vector<std::map<std::string, std::vector<float>>>& InCornerPointsPerCameraPerFrame,
                   std::map<std::string, OpenCVCamera>& OutCalibrations,
                   double& OutMSE);

    /**
     * Calibrates the intrinsics and extrinsics of the added cameras
     * @param[in] InImagePerCameraPerFrame The images per camera per frame
     * @param[out] OutCalibrations The calibrated cameras
     * @param[out] OutMSE The reprojection error from the calibration
     * @returns True if successful, false upon any error.
     */
    bool Calibrate(const std::vector<std::map<std::string, const unsigned char*>>& InImagePerCameraPerFrame,
                   std::map<std::string, OpenCVCamera>& OutCalibrations,
                   double& OutMSE);

    /**
     * Exports the results of Calibrate to a json file
     * @param[in] InCalibrations The camera calibrations to be exported
     * @param[in] InExportFilepath The filepath the calibration json will be written to
     * @returns True if successful, false upon any error.
     */
    bool ExportCalibrations(const std::map<std::string, OpenCVCamera>& InCalibrations, const std::string& InExportFilepath) const;

private:
    struct Private;
    Private* m;
};

} // namespace TITAN_API_NAMESPACE
