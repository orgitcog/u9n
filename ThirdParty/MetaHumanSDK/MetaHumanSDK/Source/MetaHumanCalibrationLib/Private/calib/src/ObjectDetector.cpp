// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
#include "ObjectImpl.h"
// #include <c/ObjectDetector.h>
#include <calib/Calibration.h>
#include <calib/ObjectDetector.h>
#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

struct ObjectDetectorImpl : ObjectDetector
{
    Object* object;
    PatternDetect type;
};


struct ObjectDetectorSingle : ObjectDetectorImpl
{
    friend struct ObjectDetectorMulti;


    Image* image;

    ObjectDetectorSingle(Image* image, Object* object, PatternDetect type)
    {
        this->image = image;
        this->object = object;
        this->type = type;
    }

    std::optional<std::vector<ObjectPlaneProjection*>> tryDetect() override
    {
        std::vector<ObjectPlaneProjection*> projections;
        object->sortPlanes();
        const size_t obj_planes_count = object->getPlaneCount();
        std::optional<Eigen::MatrixX<real_t>> img = image->getPixels();
        CARBON_ASSERT(img.has_value(), "Image container is empty.");

        std::vector<size_t> p_w, p_h;
        std::vector<real_t> sq_size;


        for (size_t i = 0; i < obj_planes_count; i++)
        {
            p_w.push_back(object->getObjectPlane(i)->getPatternShape()[0]);
            p_h.push_back(object->getObjectPlane(i)->getPatternShape()[1]);
            sq_size.push_back(object->getObjectPlane(i)->getSquareSize());
        }
        std::vector<Eigen::MatrixX<real_t>> detectedPatterns = detectMultiplePatterns(img.value(), p_w, p_h, sq_size, type);
        for (size_t i = 0; i < obj_planes_count; i++)
        {
            for (size_t j = 0; j < detectedPatterns.size(); j++)
            {
                auto objectPlane = dynamic_cast<ObjectPlaneInternal*>(object->getObjectPlane(i));
                CARBON_ASSERT(objectPlane, "Invalid dynamic cast between ObjectPlane* and ObjectPlaneInternal*.");
                if (detectedPatterns[j].rows() ==
                    objectPlane->getPatternShape()[0] * object->getObjectPlane(i)->getPatternShape()[1])
                {
                    objectPlane->setProjectionFlag(true);
                    ObjectPlaneProjection* obj_plane = ObjectPlaneProjection::create(object->getObjectPlane(i),
                                                                                     image,
                                                                                     detectedPatterns[j]);

                    projections.push_back(obj_plane);
                }
            }
        }
        return projections;
    }
};


struct ObjectDetectorMulti : ObjectDetectorImpl
{
    std::vector<Image*> images;
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> threadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);


    ObjectDetectorMulti(const std::vector<Image*>& images, Object* object, PatternDetect type)
    {
        this->images = images;
        this->object = object;
        this->type = type;
    }

    std::optional<std::vector<ObjectPlaneProjection*>> tryDetect() override
    {
        std::vector<ObjectPlaneProjection*> projections;
        std::string errorMsg;

        std::atomic<bool> terminate = false;
        std::mutex projectionsMutex;
        int numImages = static_cast<int>(images.size());

        threadPool->AddTaskRangeAndWait(numImages, [this, &projections, &terminate, &projectionsMutex](int start, int end) {
            for (int i = start; i < end; i++)
            {
                ObjectDetectorSingle oDetect(images[i], object, type);
                std::optional<std::vector<ObjectPlaneProjection*>> maybeImgProjections = oDetect.tryDetect();
                std::vector<ObjectPlaneProjection*> imgProjections;
                if (maybeImgProjections.has_value())
                {
                    imgProjections = maybeImgProjections.value();
                }
                else
                {
                    // object detection failed, return empty projections
                    terminate = true;
                }

                if (!terminate)
                {
                    std::scoped_lock lock(projectionsMutex);
                    for (size_t j = 0; j < imgProjections.size(); j++)
                    {
                        projections.push_back(imgProjections[j]);
                    }
                }
            }
        });

        if (terminate)
        {
            projections.clear();
        }

        return projections;
    }
};


ObjectDetector* ObjectDetector::create(Image* image, Object* object, PatternDetect type)
{
    CARBON_ASSERT(image->getPixels().has_value() && (object->getPlaneCount() >= 1), "Input arguments are not valid.");
    return new ObjectDetectorSingle(image, object, type);
}

ObjectDetector* ObjectDetector::create(const std::vector<Image*>& images, Object* object, PatternDetect type)
{
    CARBON_ASSERT((images.size() >= 1) && (object->getPlaneCount() >= 1), "Input arguments are not valid.");
    return new ObjectDetectorMulti(images, object, type);
}

void ObjectDetector::destructor::operator()(ObjectDetector* detector) { delete detector; }

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
