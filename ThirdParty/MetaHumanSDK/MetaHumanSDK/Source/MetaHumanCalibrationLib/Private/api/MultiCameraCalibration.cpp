// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiCameraCalibration.h"
#include "Common.h"

#include <calib/Defs.h>
#include <calib/Image.h>
#include <calib/Object.h>
#include <calib/ObjectDetector.h>
#include <calib/Calibration.h>
#include <calib/CalibContext.h>
#include <calib/DHInputOutput.h>
#include <OpenCVCamera.h>
#include <carbon/data/CameraModelOpenCV.h>
#include <carbon/io/CameraIO.h>

#include <calib/BeforeOpenCvHeaders.h>
CARBON_DISABLE_EIGEN_WARNINGS
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/eigen.hpp>
CARBON_RENABLE_WARNINGS
#include <calib/AfterOpenCvHeaders.h>

#include <Eigen/Core>

using namespace TITAN_NAMESPACE;
using namespace TITAN_NAMESPACE::calib;

namespace TITAN_API_NAMESPACE
{

void EigenToPointsVector(const Eigen::MatrixX<double>& InPoints, std::vector<float>& OutPoints)
{
    int pointsSize = (int)InPoints.size() / 2;
    for (int pointIndex = 0; pointIndex < pointsSize; ++pointIndex)
    {
        double x = InPoints(pointIndex);
        double y = InPoints(pointIndex + pointsSize);
        OutPoints.push_back((float)x);
        OutPoints.push_back((float)y);
    }
}

void PointsVectorToEigen(const std::vector<float>& InPoints, Eigen::MatrixX<double>& OutPoints)
{
    int pointsSize = static_cast<int>(InPoints.size()) / 2;
    OutPoints.resize(pointsSize, 2);
    for (int i = 0; i < pointsSize; i++)
    {
        OutPoints(i, 0) = InPoints[(i * 2)];
        OutPoints(i, 1) = InPoints[(i * 2) + 1];
    }
}

void PointsVectorToCvPoints(const std::vector<float>& InPoints, std::vector<cv::Point2f>& OutPoints)
{
    int NumPoints = static_cast<int>(InPoints.size()) / 2;
    for (int i = 0; i < NumPoints; i++)
    {
        float x = InPoints[(i * 2)];
        float y = InPoints[(i * 2) + 1];
        OutPoints.push_back(cv::Point2f(x, y));
    }
}

std::map<std::string, OpenCVCamera> GetOpenCvCameras(const std::vector<TITAN_NAMESPACE::calib::Camera*>& InCameras)
{
    std::map<std::string, OpenCVCamera> OpenCVCameras;

    bool setOriginInFirstCamera = true;
    for (const auto* calibCamera : InCameras)
    {
        Eigen::Matrix<real_t, 4, 4> transform44 = calibCamera->getWorldPosition().inverse().template cast<real_t>();
        if (setOriginInFirstCamera)
        {
            transform44 = transform44 * InCameras[0]->getWorldPosition().template cast<real_t>();
        }
        Eigen::Matrix<real_t, 3, 4> transform = transform44.template block<3, 4>(0, 0);
        const Eigen::Matrix3<real_t> K = calibCamera->getCameraModel()->getIntrinsicMatrix().template cast<real_t>();
        const Eigen::Vector<real_t, 5> D = calibCamera->getCameraModel()->getDistortionParams().template cast<real_t>();

        const int cameraWidth = static_cast<int>(calibCamera->getCameraModel()->getFrameWidth());
        const int cameraHeight = static_cast<int>(calibCamera->getCameraModel()->getFrameHeight());
        const std::string cameraTag = calibCamera->getTag();

        OpenCVCamera openCVCamera;
        openCVCamera.width = cameraWidth;
        openCVCamera.height = cameraHeight;
        openCVCamera.fx = (float)K(0, 0);
        openCVCamera.fy = (float)K(1, 1);
        openCVCamera.cx = (float)K(0, 2);
        openCVCamera.cy = (float)K(1, 2);
        openCVCamera.k1 = (float)D(0);
        openCVCamera.k2 = (float)D(1);
        openCVCamera.p1 = (float)D(2);
        openCVCamera.p2 = (float)D(3);
        openCVCamera.k3 = (float)D(4);

        Eigen::Matrix<real_t, 4, 4> aff = Eigen::Matrix<real_t, 4, 4>::Identity();
        aff.topLeftCorner(3, 4) = transform;
        for (int i = 0, row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                openCVCamera.Extrinsics[i++] = (float)aff(row, column);
            }
        }

        OpenCVCameras[cameraTag] = openCVCamera;
    }

    return OpenCVCameras;
}

struct ImageImpl : Image
{
    std::string cameraName;
    size_t frameId;
    cv::Mat image;

    ImageImpl(std::string cameraName, size_t frameId, const cv::Mat& image)
        : cameraName(cameraName)
        , frameId(frameId)
        , image(image)
    {}

    virtual const char* getModelTag() const noexcept override
    {
        return cameraName.c_str();
    }

    virtual const char* getCameraTag() const noexcept override
    {
        return cameraName.c_str();
    }

    virtual size_t getFrameId() const noexcept override
    {
        return frameId;
    }

    std::optional<Eigen::MatrixX<double>> getPixels() noexcept override
    {
        cv::Mat outImage;
        image.convertTo(outImage, CV_64F);
        Eigen::MatrixX<real_t> outPixels;
        cv::cv2eigen(outImage, outPixels);

        return std::optional<Eigen::MatrixX<real_t>>{ outPixels };
    }
};

struct MultiCameraCalibration::Private
{
    std::map<std::string, std::unique_ptr<CameraModel>> cameraModels;

    std::unique_ptr<CalibContext> calibContext;

    std::unique_ptr<ObjectPlane> objectPlane;
};

MultiCameraCalibration::MultiCameraCalibration()
    : m(new Private)
{}

MultiCameraCalibration::~MultiCameraCalibration()
{
    if (m)
    {
        delete m;
        m = nullptr;
    }
}

bool MultiCameraCalibration::Init(size_t InPatternWidth, size_t InPatternHeight, double InPatternSquareSize)
{
    try
    {
        TITAN_RESET_ERROR;

        std::optional<CalibContext*> maybeScene = CalibContext::create();
        TITAN_CHECK_OR_RETURN(maybeScene.has_value(), false, "could not create calibration context");

        std::unique_ptr<CalibContext> calibContext(maybeScene.value());
        m->calibContext = std::move(calibContext);

        std::unique_ptr<ObjectPlane> objectPlane(m->calibContext->addObjectPlane(InPatternWidth, InPatternHeight, InPatternSquareSize).value());
        m->objectPlane = std::move(objectPlane);

        return true;
    }
    catch (std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to initialize: {}", e.what());
    }
}

bool MultiCameraCalibration::AddCamera(const std::string& InCameraName, int32_t InWidth, int32_t InHeight)
{
    try
    {
        TITAN_RESET_ERROR;
        std::optional<CameraModel*> model = CameraModel::create(InCameraName.c_str(), InWidth, InHeight);
        TITAN_CHECK_OR_RETURN(model, false, "could not create camera model");

        m->cameraModels.insert({ InCameraName, std::unique_ptr<CameraModel>(model.value()) });

        m->calibContext->addCameraModel(m->cameraModels[InCameraName].get());
        m->calibContext->addCamera(InCameraName.c_str(), m->cameraModels.size() - 1);

        return true;
    }
    catch (std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to add camera: {}", e.what());
    }
}

bool MultiCameraCalibration::DetectPattern(const std::string& InCameraName,
                                           const unsigned char* InImage,
                                           std::vector<float>& OutPoints,
                                           double& OutChessboardSharpness) const
{
    try
    {
        TITAN_RESET_ERROR;

        TITAN_CHECK_OR_RETURN(m->objectPlane, false, "object plane not initialized");
        TITAN_CHECK_OR_RETURN(m->cameraModels.count(InCameraName), false, "camera model does not exist for view");

        int width = static_cast<int>(m->cameraModels.at(InCameraName)->getFrameWidth());
        int height = static_cast<int>(m->cameraModels.at(InCameraName)->getFrameHeight());

        cv::Mat cvImage(height, width, CV_8U, (void*)InImage);
        ImageImpl calibImage(InCameraName, 0, cvImage);

        Object* intrObject = Object::create();
        TITAN_CHECK_OR_RETURN(intrObject, false, "could not create object");
        intrObject->addObjectPlane(m->objectPlane.get());

        // detect pattern
        ObjectDetector* detector = ObjectDetector::create(&calibImage, intrObject, calib::DETECT_DEEP);
        std::optional<std::vector<ObjectPlaneProjection*>> maybeProjections = detector->tryDetect();

        if (maybeProjections.value().empty())
        {
            return false;
        }

        for (auto projection : maybeProjections.value())
        {
            Eigen::MatrixX<double> projectionPoints = projection->getProjectionPoints();
            EigenToPointsVector(projectionPoints, OutPoints);
        }

        std::vector<cv::Point2f> cvPoints;
        PointsVectorToCvPoints(OutPoints, cvPoints);
        cv::normalize(cvImage, cvImage, 255, 0, cv::NORM_MINMAX, CV_8U);

        int patternWidth = m->objectPlane->getPatternShape()(0, 0);
        int patternHeight = m->objectPlane->getPatternShape()(1, 0);
        OutChessboardSharpness = cv::estimateChessboardSharpness(cvImage, cv::Size(patternWidth, patternHeight), cvPoints)[0];

        return true;
    }
    catch (std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to detect pattern: {}", e.what());
    }
}

bool MultiCameraCalibration::Calibrate(const std::vector<std::map<std::string, std::vector<float>>>& InCornerPointsPerCameraPerFrame,
                                       std::map<std::string, OpenCVCamera>& OutCalibrations, double& OutMSE)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->calibContext, false, "calib context not initialised");
        TITAN_CHECK_OR_RETURN(!InCornerPointsPerCameraPerFrame.empty(), false, "no input corner points for calibration");

        std::vector<ObjectPlaneProjection*> projections;
        std::list<ImageImpl> calibImages;

        for (size_t frameNumber = 0; frameNumber < InCornerPointsPerCameraPerFrame.size(); frameNumber++)
        {
            const auto& InCornerPointsPerCamera = InCornerPointsPerCameraPerFrame[frameNumber];

            for (const auto& [cameraName, cornerPoints] : InCornerPointsPerCamera)
            {
                calibImages.emplace_back(cameraName, frameNumber, cv::Mat());
                Image* image = &(calibImages.back());
                Eigen::MatrixX<double> projectionPoints;
                PointsVectorToEigen(cornerPoints, projectionPoints);
                ObjectPlaneProjection* projection = ObjectPlaneProjection::create(m->objectPlane.get(), image, projectionPoints);
                projections.push_back(projection);
            }
        }

        TITAN_CHECK_OR_RETURN(!projections.empty(), false, "no projections set for calibration");

        for (const auto& model : m->calibContext->getCameraModels())
        {
            model->setProjectionData(projections);
        }

        m->calibContext->setCalibrationType(calib::FIXED_PROJECTIONS);

        calib::BAParams params;
        params.iterations = size_t(50);
        params.frameNum = InCornerPointsPerCameraPerFrame.size();
        params.optimizeIntrinsics = false;
        params.fixedIntrinsicIndices = std::vector<int>{ 1, 2 };
        m->calibContext->setBundleAdjustOptimParams(params);

        bool calibSucess = m->calibContext->calibrateScene();

        if (calibSucess)
        {
            OutCalibrations = GetOpenCvCameras(m->calibContext->getCameras());
            OutMSE = m->calibContext->getMse();
        }

        return calibSucess;
    }
    catch (std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to calibrate: {}", e.what());
    }
}

bool MultiCameraCalibration::Calibrate(const std::vector<std::map<std::string, const unsigned char*>>& InImagePerCameraPerFrame,
                                       std::map<std::string, OpenCVCamera>& OutCalibrations, double& OutMSE)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->calibContext, false, "calib context not initialised");
        TITAN_CHECK_OR_RETURN(!InImagePerCameraPerFrame.empty(), false, "no input images for calibration");

        std::vector<std::map<std::string, ImageImpl>> calibrationImages;
        calibrationImages.reserve(InImagePerCameraPerFrame.size());

        for (size_t frameNumber = 0; frameNumber < InImagePerCameraPerFrame.size(); frameNumber++)
        {
            std::map<std::string, ImageImpl>& calibImagesPerCamera = calibrationImages.emplace_back();

            for (const auto& [cameraName, imagePtr] : InImagePerCameraPerFrame[frameNumber])
            {
                int width = static_cast<int>(m->cameraModels.at(cameraName)->getFrameWidth());
                int height = static_cast<int>(m->cameraModels.at(cameraName)->getFrameHeight());

                cv::Mat cvImage(height, width, CV_8U, (void*)imagePtr);
                calibImagesPerCamera.insert({ cameraName, ImageImpl(cameraName, frameNumber, cvImage) });
            }
        }

        for (size_t frameNumber = 0; frameNumber < calibrationImages.size(); frameNumber++)
        {
            for (auto& [cameraName, calibImage] : calibrationImages[frameNumber])
            {
                m->calibContext->addImage(&calibImage);
            }
        }

        m->calibContext->setPatternDetectorType(calib::DETECT_DEEP);
        calib::BAParams params;
        params.iterations = size_t(50);
        params.frameNum = calibrationImages.size();
        params.optimizeIntrinsics = false;
        params.fixedIntrinsicIndices = std::vector<int>{ 1, 2 };
        m->calibContext->setBundleAdjustOptimParams(params);

        bool calibSucess = m->calibContext->calibrateScene();

        if (calibSucess)
        {
            OutCalibrations = GetOpenCvCameras(m->calibContext->getCameras());
            OutMSE = m->calibContext->getMse();
        }

        return calibSucess;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to calibrate: {}", e.what());
    }
}

bool MultiCameraCalibration::ExportCalibrations(const std::map<std::string, OpenCVCamera>& InCalibrations, const std::string& InExportFilepath) const
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(!InCalibrations.empty(), false, "camera calibrations empty");

        std::vector<TITAN_NAMESPACE::CameraModelOpenCV<real_t>> carbonCameras;
        for (const auto& [cameraName, openCVCamera] : InCalibrations)
        {
            Eigen::Vector<real_t, 5> distortionParams;
            distortionParams(0) = openCVCamera.k1;
            distortionParams(1) = openCVCamera.k2;
            distortionParams(2) = openCVCamera.p1;
            distortionParams(3) = openCVCamera.p2;
            distortionParams(4) = openCVCamera.k3;

            Eigen::Matrix3<real_t> intrinsics = Eigen::Matrix3<real_t>::Identity();
            intrinsics(0, 0) = openCVCamera.fx;
            intrinsics(1, 1) = openCVCamera.fy;
            intrinsics(0, 2) = openCVCamera.cx;
            intrinsics(1, 2) = openCVCamera.cy;

            Eigen::Matrix<real_t, 4, 4> transform44;
            for (int i = 0, column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    transform44(row, column) = openCVCamera.Extrinsics[i++];
                }
            }
            Eigen::Matrix<real_t, 3, 4> transform = transform44.block<3, 4>(0, 0);

            TITAN_NAMESPACE::CameraModelOpenCV<real_t> cameraToWrite;

            cameraToWrite.SetDistortionParams(distortionParams);
            cameraToWrite.SetIntrinsics(intrinsics);
            cameraToWrite.SetExtrinsics(transform);
            cameraToWrite.SetWidth(openCVCamera.width);
            cameraToWrite.SetHeight(openCVCamera.height);
            cameraToWrite.SetLabel(cameraName.c_str());
            cameraToWrite.SetModel(cameraName.c_str());
            carbonCameras.push_back(cameraToWrite);
        }

        TITAN_NAMESPACE::WriteOpenCvModelJson<real_t>(InExportFilepath, carbonCameras);
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to write calib json: {}", e.what());
    }
}

} // namespace TITAN_API_NAMESPACE
