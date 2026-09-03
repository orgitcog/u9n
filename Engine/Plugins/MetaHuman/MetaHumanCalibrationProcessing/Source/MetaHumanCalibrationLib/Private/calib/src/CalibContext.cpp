// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
// #include <c/CalibContext.h>
#include <calib/BundleAdjustmentPerformer.h>
#include <calib/CalibContext.h>
#include <calib/CameraManager.h>
#include <calib/CoordinateSystemAligner.h>
#include <calib/ObjectDetector.h>
#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

struct CalibContextImpl : CalibContext
{
    std::vector<CameraModel*> models;
    std::vector<Camera*> cameras;
    std::vector<Image*> images;
    std::vector<ObjectPlaneProjection*> projections;
    std::vector<ObjectPlane*> planes;
    Object* object;
    PatternDetect patternDetectType = PatternDetect::DETECT_FAST;
    SceneCalibrationType calibType = SceneCalibrationType::FULL_CALIBRATION;
    BAParams params;
    real_t mse;
    const char* metricUnit = "cm";

    CalibContextImpl()
    {}

    CalibContextImpl(const CalibContextImpl&) = default;
    CalibContextImpl(CalibContextImpl&&) = default;

    ~CalibContextImpl() override = default;

    CalibContextImpl& operator=(const CalibContextImpl&) = default;
    CalibContextImpl& operator=(CalibContextImpl&&) = default;

    std::vector<CameraModel*> getCameraModels() const noexcept override
    {
        return models;
    }

    std::vector<Camera*> getCameras() const noexcept override
    {
        return cameras;
    }

    std::vector<Image*> getImages() const noexcept override
    {
        return images;
    }

    Object* getObject() const noexcept override
    {
        return object;
    }

    void setBundleAdjustOptimParams(const BAParams& curParams) noexcept override
    {
        this->params = curParams;
    }

    real_t getMse() const noexcept override
    {
        return this->mse;
    }

    void setPatternDetectorType(PatternDetect type) noexcept override
    {
        this->patternDetectType = type;
    }

    void setCalibrationType(SceneCalibrationType type) noexcept override
    {
        this->calibType = type;
    }

    void prepareObjectPlanes(ObjectPlane* ref, std::vector<ObjectPlane*>& neighbor, std::vector<ObjectPlane*>& parallel) noexcept
    {
        for (size_t i = 0; i < object->getPlaneCount(); i++)
        {
            if ((object->getObjectPlane(i) != ref) && object->getObjectPlane(i)->hasProjections())
            {
                if (CoordinateSystemAligner::neighborhoodCheck(ref, object->getObjectPlane(i), cameras))
                {
                    neighbor.push_back(object->getObjectPlane(i));
                }
                else
                {
                    parallel.push_back(object->getObjectPlane(i));
                }
            }
        }
    }

    bool detectCalibrationObject() noexcept
    {
        ObjectDetector* detector = ObjectDetector::create(images, object, patternDetectType);
        std::optional<std::vector<ObjectPlaneProjection*>> maybeProjections = detector->tryDetect();
        if (!maybeProjections.has_value())
        {
            return false;
        }
        projections = maybeProjections.value();
        return true;
    }

    void collectProjections() const noexcept
    {
        const size_t num_of_models = models.size();
        for (size_t i = 0; i < num_of_models; i++)
        {
            models[i]->setProjectionData(projections);
        }
    }

    bool calibrateIntrinsics() const noexcept
    {
        for (auto model : models)
        {
            std::optional<real_t> intrMse = model->calibrateIntrinsics();
            if (!intrMse.has_value())
            {
                return false;
            }
        }
        return true;
    }

    bool calibrateExtrinsics() const noexcept
    {
        for (auto camera : cameras)
        {
            if (!camera->calibrateExtrinsics())
            {
                std::cout << "Estimation of external camera parameters failed." << std::endl;
                return false;
            }
            ObjectPlane* refplane = object->getObjectPlane(0);
            int projId = camera->isPlaneVisibleOnFrame(*refplane, 0);
            if (projId == -1)
            {
                std::cout << "Referent object plane not visible in first frame." << std::endl;
                return false;
            }
            camera->setWorldPosition(camera->getProjectionData()[projId]->getTransform().inverse());
        }
        return true;
    }

    void alignScene() noexcept
    {
        std::vector<ObjectPlane*> neighbor, parallel;
        ObjectPlane* ref = cameras[0]->getProjectionData()[0]->getObjectPlane();
        this->prepareObjectPlanes(ref, neighbor, parallel);
        CoordinateSystemAligner::transformCamerasGlobal(ref, cameras);
        ObjectPlane* to_be_aligned;

        for (size_t i = 0; i < neighbor.size(); i++)
        {
            to_be_aligned = neighbor[i];
            CoordinateSystemAligner::alignCoordinateSystems(ref, to_be_aligned, cameras);
        }
        for (size_t j = 0; j < parallel.size(); j++)
        {
            to_be_aligned = parallel[j];
            CoordinateSystemAligner::alignCoordinateSystems(ref, to_be_aligned, cameras);
        }
    }

    std::optional<real_t> bundleAdjustment() noexcept
    {
        std::optional<real_t> maybeMse = BundleAdjustmentPerformer::bundleAdjustScene(object, cameras, params);
        return maybeMse;
    }

    void createObject() noexcept
    {
        Object* maybeObject = Object::create();
        object = maybeObject;
        for (size_t i = 0; i < planes.size(); i++)
        {
            object->addObjectPlane(planes[i]);
        }
    }

    bool calibrateScene() noexcept override
    {
        this->createObject();

        if (calibType != SceneCalibrationType::FIXED_PROJECTIONS)
        {
            bool detectObject = this->detectCalibrationObject();
            if (!detectObject)
            {
                std::cout << "Object detection failed" << std::endl;
                return false;
            }
            this->collectProjections();
        }

        if (calibType != SceneCalibrationType::FIXED_INTRINSICS)
        {
            bool calibIntr = this->calibrateIntrinsics();
            if (!calibIntr)
            {
                std::cout << "Internal camera calibration failed." << std::endl;
                return false;
            }
        }

        bool calibExtr = this->calibrateExtrinsics();
        if (!calibExtr)
        {
            return false;
        }

        this->alignScene();
        std::optional<real_t> bundleAdjustErr = this->bundleAdjustment();
        if (!bundleAdjustErr.has_value())
        {
            std::cout << "Bundle adjustment failed." << std::endl;
            return false;
        }
        mse = bundleAdjustErr.value();
        return true;
    }

    void addImage(Image* image) noexcept override
    {
        images.push_back(image);
    }

    void addCameraModel(CameraModel* model) noexcept override
    {
        models.push_back(model);
    }

    void addCamera(Camera* camera) noexcept override
    {
        cameras.push_back(camera);
    }

    void addObjectPlane(ObjectPlane* plane) noexcept override
    {
        object->addObjectPlane(plane);
    }

    void setObject(Object* curObject) noexcept override
    {
        this->object = curObject;
    }

    std::optional<Image*> addImage(const char* path, const char* modelTag, const char* camTag, int frameId,
                                   ImageLoadType loadType = ImageLoadType::LOAD_RAW) noexcept override
    {
        Image* image = nullptr;
        if (loadType == ImageLoadType::LOAD_RAW)
        {
            std::optional<Image*> maybeImage = Image::loadRaw(path, modelTag, camTag, frameId);
            image = maybeImage.value();
        }
        else if (loadType == ImageLoadType::LOAD_PROXY)
        {
            std::optional<Image*> maybeImage = Image::loadProxy(path, modelTag, camTag, frameId);
            image = maybeImage.value();
        }
        else
        {
            LOG_WARNING("Unsupported load type");
            return std::nullopt;
        }

        images.push_back(image);

        return image;
    }

    std::optional<CameraModel*> addCameraModel(const char* camModelTag,
                                               size_t imgW,
                                               size_t imgH,
                                               const std::vector<ObjectPlaneProjection*>& curProjections,
                                               const Eigen::Matrix3<real_t>& initK) noexcept override
    {
        std::optional<CameraModel*> maybeModel = CameraModel::create(camModelTag, imgW, imgH, curProjections, initK);
        if (maybeModel.has_value())
        {
            models.push_back(maybeModel.value());
        }
        return maybeModel;
    }

    std::optional<Camera*> addCamera(const char* camTag, size_t modelIndex, const Eigen::Matrix4<real_t>&/*position*/) noexcept override // TODO is this correct?
    {
        std::optional<Camera*> maybeCamera = Camera::create(camTag, models[modelIndex]);
        if (maybeCamera.has_value())
        {
            cameras.push_back(maybeCamera.value());
        }
        return maybeCamera;
    }

    std::optional<ObjectPlane*> addObjectPlane(size_t pWidth, size_t pHeight, real_t squareSize) noexcept override
    {
        std::optional<ObjectPlane*> maybePlane = ObjectPlane::create(pWidth, pHeight, squareSize);
        if (maybePlane.has_value())
        {
            planes.push_back(maybePlane.value());
        }

        return maybePlane;
    }

    void setMetricUnit(const char* curMetricUnit) noexcept override
    {
        this->metricUnit = curMetricUnit;
    }

    const char* getMetricUnit() const noexcept override
    {
        const char* curMetricUnit = this->metricUnit;
        return curMetricUnit;
    }
};


std::optional<CalibContext*> CalibContext::create() noexcept
{
    return new CalibContextImpl();
}

void CalibContext::destructor::operator()(CalibContext* scene) noexcept
{
    delete scene;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
