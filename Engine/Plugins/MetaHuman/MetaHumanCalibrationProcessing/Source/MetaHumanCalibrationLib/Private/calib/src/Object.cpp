// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
// #include <c/Object.h>
#include <calib/Calibration.h>
#include <calib/Image.h>
#include <calib/Object.h>
#include <calib/Utilities.h>
#include <carbon/Common.h>

#include "ObjectImpl.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

bool patternSortFunc(ObjectPlane* l, ObjectPlane* r) {
    return (l->getPatternShape()[0] * l->getPatternShape()[1] > r->getPatternShape()[0] * r->getPatternShape()[1]);
}

struct ObjectPlaneImpl : ObjectPlaneInternal
{
    Eigen::Vector2i patternShape;
    Eigen::MatrixX<real_t> patternPoints;
    std::vector<Eigen::Matrix4<real_t>> transforms;
    real_t squareSize;
    bool projectionsFlag;


    ObjectPlaneImpl(size_t pWidth, size_t pHeight, real_t sqSize, Eigen::Matrix4<real_t> transform = Eigen::Matrix4<real_t>::Identity())
    {
        assert(pWidth > 0 && pHeight > 0 && sqSize > 0);
        this->patternShape[0] = static_cast<int>(pWidth);
        this->patternShape[1] = static_cast<int>(pHeight);
        this->squareSize = sqSize;
        this->transforms.resize(1);
        this->transforms[0] = transform;
        this->projectionsFlag = false;
        patternPoints = generate3dPatternPoints(pWidth, pHeight, sqSize);
    }

    ObjectPlaneImpl(const ObjectPlaneImpl&) = default;
    ObjectPlaneImpl(ObjectPlaneImpl&&) = default;

    ~ObjectPlaneImpl() override = default;

    ObjectPlaneImpl& operator=(const ObjectPlaneImpl&) = default;
    ObjectPlaneImpl& operator=(ObjectPlaneImpl&&) = default;

    const Eigen::Matrix4<real_t>& getTransform(size_t atFrame) const override
    {
        return transforms[atFrame];
    }

    bool hasProjections() const override
    {
        return projectionsFlag;
    }

    void setProjectionFlag(bool flag) override
    {
        this->projectionsFlag = flag;
    }

    Eigen::Vector2i getPatternShape() const override
    {
        return patternShape;
    }

    void setNumberOfFrames(size_t numberOfFrames) override
    {
        this->transforms.resize(numberOfFrames);
    }

    void setTransform(const Eigen::Matrix4<real_t>& transform, size_t atFrame) override
    {
        CARBON_ASSERT(atFrame < transforms.size(), "Given frame number exceeds defined number of frames.");
        this->transforms[atFrame] = transform;
    }

    const Eigen::MatrixX<real_t>& getLocalPoints() const override
    {
        return patternPoints;
    }

    real_t getSquareSize() const override
    {
        return squareSize;
    }

    Eigen::MatrixX<real_t> getGlobalPoints(size_t atFrame) const override
    {
        Eigen::MatrixX<real_t> transformedPoints = Eigen::MatrixX<real_t>::Zero(patternPoints.rows(), patternPoints.cols());
        Eigen::Vector4<real_t> point;

        for (int i = 0; i < patternPoints.rows(); i++)
        {
            pointFromRow3dHomogenious(patternPoints, i, point);
            Eigen::Vector4<real_t> point_t = transforms[atFrame] * point;
            rowFromPoint3d(transformedPoints, i, point_t);
        }

        return transformedPoints;
    }
};

std::optional<ObjectPlane*> ObjectPlane::create(size_t pWidth, size_t pHeight, real_t squareSize)
{
    CARBON_ASSERT(pWidth >= 2 && pHeight >= 2 && squareSize >= 0., "Pattern shape parameters are invalid.");
    return new ObjectPlaneImpl(pWidth, pHeight, squareSize);
}

void ObjectPlane::destructor::operator()(ObjectPlane* objectPlane) { delete objectPlane; }

struct ObjectImpl : Object
{
    std::vector<ObjectPlane*> planes;
    Eigen::Matrix4<real_t> transform;

    ObjectImpl(const Eigen::Matrix4<real_t>& transform = Eigen::Matrix4<real_t>::Identity())
    {
        this->transform = transform;
    }

    ObjectImpl(const ObjectImpl&) = default;
    ObjectImpl(ObjectImpl&&) = default;

    ~ObjectImpl() override = default;

    ObjectImpl& operator=(const ObjectImpl&) = default;
    ObjectImpl& operator=(ObjectImpl&&) = default;

    void addObjectPlane(ObjectPlane* plane) override
    {
        this->planes.push_back(plane);
    }

    void sortPlanes() override
    {
        std::sort(planes.begin(), planes.end(), patternSortFunc);
    }

    ObjectPlane* getObjectPlane(size_t planeId) const override
    {
        return planes.at(planeId);
    }

    size_t getPlaneCount() const override
    {
        return planes.size();
    }
};

Object* Object::create(const Eigen::Matrix4<real_t>& transform) { return new ObjectImpl(transform); }

void Object::destructor::operator()(Object* object) { delete object; }

struct ObjectPlaneProjImpl : ObjectPlaneProjection
{
    ObjectPlane* plane;
    Eigen::MatrixX<real_t> points;
    Image* image;
    Eigen::Matrix4<real_t> transform;

    ObjectPlaneProjImpl(ObjectPlane* plane, const Eigen::MatrixX<real_t>& points, Image* image)
    {
        this->plane = plane;
        this->points = points;
        this->image = image;
    }

    ObjectPlaneProjImpl(const ObjectPlaneProjImpl&) = default;
    ObjectPlaneProjImpl(ObjectPlaneProjImpl&&) = default;

    ~ObjectPlaneProjImpl() override = default;

    ObjectPlaneProjImpl& operator=(const ObjectPlaneProjImpl&) = default;
    ObjectPlaneProjImpl& operator=(ObjectPlaneProjImpl&&) = default;

    ObjectPlane* getObjectPlane() const override
    {
        return plane;
    }

    void setImage(Image* curImage) override
    {
        this->image = curImage;
    }

    virtual Image* getImage() const override
    {
        return this->image;
    }

    const Eigen::Matrix4<real_t> getTransform() const override
    {
        return this->transform;
    }

    void setTransform(const Eigen::Matrix4<real_t>& curTransform) override
    {
        this->transform = curTransform;
    }

    void setProjectionPoints(const Eigen::MatrixX<real_t>& curPoints) override
    {
        CARBON_ASSERT(curPoints.size() != 0, "Matrix is empty");
        this->points = curPoints;
    }

    const Eigen::MatrixX<real_t>& getProjectionPoints() const override
    {
        return points;
    }
};

ObjectPlaneProjection* ObjectPlaneProjection::create(ObjectPlane* plane, Image* image, const Eigen::MatrixX<real_t>& points)
{
    CARBON_ASSERT(plane->getLocalPoints().size() != 0, "Input arguments are not valid.");
    return new ObjectPlaneProjImpl(plane, points, image);
}

void ObjectPlaneProjection::destructor::operator()(ObjectPlaneProjection* objectPlaneProj) { delete objectPlaneProj; }

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
