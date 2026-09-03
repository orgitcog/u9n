// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
// #include <c/Image.h>
#include <calib/Calibration.h>
#include <calib/Image.h>
#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

struct ImageImpl : Image
{
    const char* camTag;
    const char* modelTag;
    std::string cTagPy;
    std::string mTagPy;
    size_t frameId;

    virtual const char* getModelTag() const override
    {
        return modelTag;
    }

    virtual const char* getCameraTag() const override
    {
        return camTag;
    }

    virtual size_t getFrameId() const override
    {
        return frameId;
    }
};

struct ImageRaw : ImageImpl
{
    Eigen::MatrixX<real_t> image;
    ImageRaw(const std::string& path, const char* camTag, const char* modelTag, size_t frameId)
    {
        this->camTag = camTag;
        this->modelTag = modelTag;
        this->frameId = frameId;
        this->cTagPy = camTag;
        this->mTagPy = modelTag;
        this->image = loadImage(path.c_str()).value();
    }

    std::optional<Eigen::MatrixX<real_t>> getPixels() override
    {
        CARBON_ASSERT(image.size() != 0, "Image container is empty.");
        return image;
    }
};

struct ImageProxy : ImageImpl
{
    std::string path;

    ImageProxy(const std::string& path, const char* camTag, const char* modelTag, size_t frameId)
    {
        this->camTag = camTag;
        this->modelTag = modelTag;
        this->frameId = frameId;
        this->cTagPy = camTag;
        this->mTagPy = modelTag;
        this->path = path.c_str();
    }

    std::optional<Eigen::MatrixX<real_t>> getPixels() override
    {
        std::optional<Eigen::MatrixX<real_t>> maybeImage = loadImage(this->path.c_str());
        return maybeImage;
    }
};

std::optional<Image*> Image::loadRaw(const char* path, const char* modelTag, const char* camTag, size_t frameId) {
    return new ImageRaw(path, camTag, modelTag, frameId);
}

std::optional<Image*> Image::loadProxy(const char* path, const char* modelTag, const char* camTag, size_t frameId) {
    return new ImageProxy(path, camTag, modelTag, frameId);
}

void Image::destructor::operator()(Image* image) { delete image; }

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
