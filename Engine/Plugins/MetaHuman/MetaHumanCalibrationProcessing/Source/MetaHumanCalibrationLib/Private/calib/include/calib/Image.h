// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Image class

    Header contains the C++ implementation of the image representation and related structures and functionalities.
 */
#pragma once

#include <carbon/Common.h>


#include <calib/Defs.h>
#include <carbon/Common.h>
#include <optional>
CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/Dense>
CARBON_RENABLE_WARNINGS

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)


/**
   @brief High-level representation of a image

    Image is represented in context of camera calibration pipeline, so it is defined
    with data container, and metadata, for example - camera tag, model tag,...
    Two ways of image loading is supported: raw and proxy. Raw image loading is standard
    loading where data is loaded at the moment of defining the istance of Image class.
    Proxy image loading will load the data only when we want to access the pixels.
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Image
{
public:
    virtual ~Image() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get pixels of a image.

        Get data container with pixel values. In case of raw image instance, this function
        will get internal container of pixels, and in case of proxy image instace, function
        will open the image on specified path and then get its data.

        @return
            trust::Expected<Eigen::MatrixX<real_t>> which, if valid, will contain matrix of
            pixel values with shape of (image width, image height).

     */

    virtual std::optional<Eigen::MatrixX<real_t>> getPixels() = 0;

    /**
        @brief Get name tag for camera model of this image.

        @return
            Array of characters representing model name or ID.
     */

    virtual const char* getModelTag() const = 0;

    /**
        @brief Get name tag for camera of this image.

        @return
            Array of characters representing model name or ID.
     */

    virtual const char* getCameraTag() const = 0;

    /**
        @brief Get tag for frame ID of this image.

        @return
            int value representing frame ID.
     */

    virtual size_t getFrameId() const = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the image described with input parameters. This constructor loads
        the image data from specified path into local data container.

        @param path
            Array of characters defining the path to the image data.

        @param modelTag
            Array of characters representing camera model name or ID of this image.

        @param camTag
            Array of characters representing camera name or ID of this image.

        @param frameId
            int value representing frame ID of this image.

        @warning Created image must always be deallocated using Image::destructor.
     */

    static std::optional<Image*> loadRaw(const char* path, const char* modelTag, const char* camTag, size_t frameId);

    /*!
        @brief Construct the image described with input parameters. This constructor does
        not load image data. Data is loaded when one of the functions Image::getPixels()
        or Image::getPixel() is called.

        @param path
            Array of characters defining the path to the image data.

        @param modelTag
            Array of characters representing camera model name or ID of this image.

        @param camTag
            Array of characters representing camera name or ID of this image.

        @param frameId
            int value representing frame ID of this image.

        @warning Created image must always be deallocated using Image::destructor.
     */

    static std::optional<Image*> loadProxy(const char* path, const char* modelTag, const char* camTag, size_t frameId);

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for images created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(Image* image);
    };
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
