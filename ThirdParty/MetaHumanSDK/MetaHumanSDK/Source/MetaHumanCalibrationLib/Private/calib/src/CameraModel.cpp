// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
// #include <c/CameraModel.h>
#include <calib/CameraModel.h>
#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

struct CameraModelImpl : CameraModel
{
    Eigen::Matrix3<real_t> intrinsicMatrix;
    Eigen::VectorX<real_t> distortionParams;
    std::vector<ObjectPlaneProjection*> projections;
    std::string modelTag;
    size_t imageWidth;
    size_t imageHeight;

    CameraModelImpl(const char* camModelTag, Eigen::Matrix3<real_t> initIntrinsic, const std::vector<ObjectPlaneProjection*>& projections, size_t imgW, size_t imgH)
    {
        this->modelTag = camModelTag;
        this->intrinsicMatrix = initIntrinsic;
        this->extractModelData(projections);
        this->imageHeight = imgH;
        this->imageWidth = imgW;
    }

    CameraModelImpl(const CameraModelImpl&) = default;
    CameraModelImpl(CameraModelImpl&&) = default;

    ~CameraModelImpl() override = default;

    CameraModelImpl& operator=(const CameraModelImpl&) = default;
    CameraModelImpl& operator=(CameraModelImpl&&) = default;


    const Eigen::Matrix3<real_t>& getIntrinsicMatrix() const override
    {
        return intrinsicMatrix;
    }

    void extractModelData(const std::vector<ObjectPlaneProjection*>& curProjections)
    {
        size_t projections_count = curProjections.size();
        std::vector<ObjectPlaneProjection*> modelProjections;

        for (size_t i = 0; i < projections_count; i++)
        {
            if (std::string(curProjections[i]->getImage()->getModelTag()) == this->modelTag)
            {
                modelProjections.push_back(curProjections[i]);
            }
        }
        this->projections = modelProjections;
    }

    void packDataForCalibration(std::vector<Eigen::MatrixX<real_t>>& points2d, std::vector<Eigen::MatrixX<real_t>>& points3d)
    {
        const size_t proj_count = projections.size();

        for (size_t i = 0; i < proj_count; i++)
        {
            points2d.push_back(projections[i]->getProjectionPoints());
            points3d.push_back(projections[i]->getObjectPlane()->getLocalPoints());
        }
    }

    void setProjectionData(const std::vector<ObjectPlaneProjection*>& curProjections) override
    {
        CARBON_ASSERT(!curProjections.empty(), "Projection array is empty.");
        this->extractModelData(curProjections);
    }

    void setIntrinsicMatrix(const Eigen::Matrix3<real_t>& K) override
    {
        this->intrinsicMatrix = K;
    }

    void setDistortionParams(const Eigen::VectorX<real_t>& D) override
    {
        CARBON_ASSERT((D.rows() == 5) && (D.cols() == 1), "D params shape is not valid.");
        this->distortionParams = D;
    }

    const std::vector<ObjectPlaneProjection*>& getProjectionData() const override
    {
        return projections;
    }

    const Eigen::VectorX<real_t>& getDistortionParams() const override
    {
        return distortionParams;
    }

    std::optional<real_t> calibrateIntrinsics() override
    {
        std::vector<Eigen::MatrixX<real_t>> points2d;
        std::vector<Eigen::MatrixX<real_t>> points3d;
        packDataForCalibration(points2d, points3d);
        std::optional<real_t> mse = TITAN_NAMESPACE::calib::calibrateIntrinsics(points2d,
                                                               points3d,
                                                               intrinsicMatrix,
                                                               distortionParams,
                                                               imageWidth,
                                                               imageHeight);
        return mse;
    }

    const char* getTag() const override
    {
        const char* tag = this->modelTag.c_str();
        return tag;
    }

    size_t getFrameWidth() const override
    {
        return imageWidth;
    }

    size_t getFrameHeight() const override
    {
        return imageHeight;
    }
};

std::optional<CameraModel*> CameraModel::create(const char* camModelTag,
                                                size_t imgW,
                                                size_t imgH,
                                                const std::vector<ObjectPlaneProjection*>& projections,
                                                const Eigen::Matrix3<real_t>& initMatrix)
{
    CameraModelImpl* model = new CameraModelImpl(camModelTag, initMatrix, projections, imgW, imgH);

    return model;
}

void CameraModel::destructor::operator()(CameraModel* model) { delete model; }

CARBON_SUPRESS_MS_WARNING(4324)

struct CameraImpl : Camera
{
    CameraModel* cameraModel;
    std::string camTag;
    std::vector<ObjectPlaneProjection*> projections;
    Eigen::Matrix4<real_t> worldPosition;
    bool hasWorldPosition;

    CameraImpl(const char* camTag, CameraModel* cameraModel, const Eigen::Matrix4<real_t>& position)
    {
        this->cameraModel = cameraModel;
        this->worldPosition = position;
        this->camTag = camTag;
        this->projections = std::vector<ObjectPlaneProjection*>();
        this->hasWorldPosition = false;
    }

    CameraImpl(const CameraImpl&) = default;
    CameraImpl(CameraImpl&&) = default;

    ~CameraImpl() override = default;

    CameraImpl& operator=(const CameraImpl&) = default;
    CameraImpl& operator=(CameraImpl&&) = default;

    const char* getTag() const override
    {
        const char* tag = this->camTag.c_str();
        return tag;
    }

    virtual const std::vector<ObjectPlaneProjection*>& getProjectionData() const override
    {
        return this->projections;
    }

    void extractCameraProjectionData()
    {
        std::vector<ObjectPlaneProjection*> camera_projs;
        std::vector<ObjectPlaneProjection*> model_projs = this->getCameraModel()->getProjectionData();

        const size_t proj_count = model_projs.size();

        for (size_t i = 0; i < proj_count; i++)
        {
            if (std::string(model_projs[i]->getImage()->getCameraTag()) == std::string(this->getTag()))
            {
                ObjectPlaneProjection* projection;
                projection = model_projs[i];
                camera_projs.push_back(projection);
            }
        }
        this->projections = camera_projs;
    }

    void setWorldPosition(const Eigen::Matrix4<real_t>& transform) override
    {
        this->worldPosition = transform;
        if (!hasWorldPosition)
        {
            hasWorldPosition = true;
        }
    }

    const Eigen::Matrix4<real_t>& getWorldPosition() const override
    {
        return worldPosition;
    }

    CameraModel* getCameraModel() const override
    {
        return cameraModel;
    }

    int isPlaneVisible(ObjectPlane& plane) const override
    {
        for (size_t i = 0; i < this->getProjectionData().size(); i++)
        {
            if (&plane == this->getProjectionData()[i]->getObjectPlane())
            {
                return (int)i;
            }
        }
        return -1;
    }

    int isPlaneVisibleOnFrame(ObjectPlane& plane, size_t frame) const override
    {
        for (size_t i = 0; i < this->getProjectionData().size(); i++)
        {
            ObjectPlane* currPlane = this->getProjectionData()[i]->getObjectPlane();
            size_t currFrame = this->getProjectionData()[i]->getImage()->getFrameId();
            if ((&plane == currPlane) && (currFrame == frame))
            {
                return (int)i;
            }
        }
        return -1;
    }

    void setCameraModel(CameraModel& model) override
    {
        cameraModel = &model;
    }

    virtual bool calibrateExtrinsics() override
    {
        Eigen::Matrix3<real_t> K = this->getCameraModel()->getIntrinsicMatrix();
        Eigen::VectorX<real_t> D = this->getCameraModel()->getDistortionParams();
        Eigen::MatrixX<real_t> points2d, points3d;
        Eigen::Matrix4<real_t> transform = Eigen::Matrix4<real_t>::Identity();
        this->extractCameraProjectionData();

        for (auto projection : projections)
        {
            points2d = projection->getProjectionPoints();
            points3d = projection->getObjectPlane()->getLocalPoints();
            std::optional<real_t> maybeMse = TITAN_NAMESPACE::calib::calibrateExtrinsics(points2d, points3d, K, D, transform);
            if (!maybeMse.has_value())
            {
                return false;
            }
            projection->setTransform(transform);
        }
        return true;
    }

    void packDataForStereoCalibration(CameraModel* model1,
                                      CameraModel* model2,
                                      std::vector<Eigen::MatrixX<real_t>>& points2d1,
                                      std::vector<Eigen::MatrixX<real_t>>& points2d2,
                                      std::vector<Eigen::MatrixX<real_t>>& points3d)
    {
        auto projections1 = model1->getProjectionData();
        const size_t proj_count1 = projections1.size();

        auto projections2 = model2->getProjectionData();
        const size_t proj_count2 = projections2.size();

        for (size_t i = 0; i < proj_count1; i++)
        {
            for (size_t j = 0; j < proj_count2; j++)
            {
                if (projections1[i]->getImage()->getFrameId() == projections2[j]->getImage()->getFrameId())
                {
                    points2d1.push_back(projections1[i]->getProjectionPoints());
                    points2d2.push_back(projections2[j]->getProjectionPoints());
                    points3d.push_back(projections1[i]->getObjectPlane()->getLocalPoints());
                }
            }
        }
    }

    virtual Eigen::Matrix4<real_t> calibrateExtrinsicsStereo(Camera* other) override
    {
        size_t w = this->getCameraModel()->getFrameWidth();
        size_t h = this->getCameraModel()->getFrameHeight();
        Eigen::Matrix3<real_t> K2 = other->getCameraModel()->getIntrinsicMatrix();
        Eigen::VectorX<real_t> D2 = other->getCameraModel()->getDistortionParams();

        Eigen::Matrix3<real_t> K1 = this->getCameraModel()->getIntrinsicMatrix();
        Eigen::VectorX<real_t> D1 = this->getCameraModel()->getDistortionParams();
        Eigen::Matrix4<real_t> T;

        std::vector<Eigen::MatrixX<real_t>> points2d1, points2d2, points3d;

        packDataForStereoCalibration(this->getCameraModel(), other->getCameraModel(), points2d1, points2d2, points3d);

        /*std::optional<real_t> mse =*/calibrateStereoExtrinsics(points2d1, points2d2, points3d, K1, D1, K2, D2, T, w, h);

        return T;
    }
};

std::optional<Camera*> Camera::create(const char* camTag, CameraModel* cameraModel, const Eigen::Matrix4<real_t>& position)
{
    CameraImpl* camera = new CameraImpl(camTag, cameraModel, position);

    return camera;
}

void Camera::destructor::operator()(Camera* camera) { delete camera; }

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
