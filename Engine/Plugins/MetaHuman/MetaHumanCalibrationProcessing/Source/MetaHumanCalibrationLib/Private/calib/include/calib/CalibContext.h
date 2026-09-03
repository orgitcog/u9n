// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - CalibContext class

    Header contains the C++ implementation of the calibration context and related structures and functionalities.
 */
#pragma once

#include <calib/CameraModel.h>
#include <calib/Defs.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

enum ImageLoadType
{
    LOAD_RAW,
    LOAD_PROXY
};

/**
   @brief
    CalibContext is high-level API class which unifies objects and functionalities
    and makes it easy to create a scene for calibration, projection, reprojection,
    and other multiple-view-geometry functions.
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/CalibContext
{
public:
    virtual ~CalibContext() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get all camera models on the CalibContext scene.

        @return
            Array of CameraModel objects.
     */

    virtual std::vector<CameraModel*> getCameraModels() const noexcept = 0;

    /**
        @brief Get all cameras on the CalibContext scene.

        @return
            Array of Camera objects.
     */

    virtual std::vector<Camera*> getCameras() const noexcept = 0;

    /**
        @brief Get all images on the CalibContext scene.

        @return
            Array of Image objects.
     */

    virtual std::vector<Image*> getImages() const noexcept = 0;

    /**
        @brief Get calibration object on the scene.

        @return
            Pointer to calibration Object.
     */

    virtual Object* getObject() const noexcept = 0;

    /**
        @brief Set the type of pattern detector.

        @param type
            PatternDetect enumerator which contains possible pattern detect types.
     */

    virtual void setPatternDetectorType(PatternDetect type) noexcept = 0;

    /**
        @brief Set the type of calibration procedure.

        @param type
            SceneCalibrationType enumerator which contains possible calibration procedure types.
     */

    virtual void setCalibrationType(SceneCalibrationType type) noexcept = 0;

    /**
        @brief Calibrate current scene

            This method performs full pipeline which contain the following subprocesses:
            1) Calibration object detection (on all input images)
            2) Intrinsic calibration (for all camera models)
            3) Extrinsic calibration (for all cameras and its projections)
            4) Coordinate system alignment (for all detected object planes)
            5) Bundle adjustment

        @return
            trust::Expected<void> which is invalid if at least one of the subprocesses is not valid.
     */

    virtual bool calibrateScene() noexcept = 0;

    /**
        @brief Set the parameters for bundle adjustment optimization.

        @param params
            BAParams object - Set of bundle adjustment params that are leading the optimization process.
     */

    virtual void setBundleAdjustOptimParams(const BAParams& params) noexcept = 0;

    /**
        @brief Reprojection error (in pixels).

        @return
            Reprojection error (in pixels).
     */

    virtual real_t getMse() const noexcept = 0;

    /**
        @brief Constructs and adds to the scene the camera model described with input parameters.

        @param camModelTag
            Array of characters representing camera model name or ID.

        @param imgW
            Image width of camera model (in pixels).

        @param imgH
            Image height of camera model (in pixels).

        @param projections
            If non-nullptr is given, this is array of ObjectPlaneProjection objects which
            contains set of detected checkerboard pattern points. This data can be assigned
            later, using CameraModel::setProjection.

        @param initK
            If non-nullptr is given, this is initial camera intrinsic matrix. This matrix can
            be assigned later, using CameraModel::setIntrinsicMatrix.
     */

    virtual std::optional<CameraModel*> addCameraModel(const char* camModelTag,
                                                       size_t imgW,
                                                       size_t imgH,
                                                       const std::vector<ObjectPlaneProjection*>& projections = std::vector<ObjectPlaneProjection*>(),
                                                       const Eigen::Matrix3<real_t>& initK =
                                                       Eigen::Matrix3<real_t>::Identity()) noexcept = 0;

    virtual void addCameraModel(CameraModel* model) noexcept = 0;

    /*!
        @brief Constructs and adds to the scene the camera described with input parameters.

        @param camTag
            Array of characters representing camera name or ID.

        @param cameraModel
            Pointer to CameraModel object which holds cameras intrinsic parameters.

        @param projections
            If non-nullptr is given, this is array of ObjectPlaneProjection objects which
            contains set of detected checkerboard pattern points. This data can be assigned
            later, using Camera::setProjection.

        @param position
            If given, this is initial camera transform matrix.
     */

    virtual std::optional<Camera*> addCamera(const char* camTag, size_t modelIndex, const Eigen::Matrix4<real_t>& position =
                                             Eigen::Matrix4<real_t>::Identity()) noexcept = 0;

    virtual void addCamera(Camera* camera) noexcept = 0;

    /*!
        @brief Constructs and adds to the scene image described with input parameters.

        @param path
            Array of characters defining the path to the image data.

        @param modelTag
            Array of characters representing camera model name or ID of this image.

        @param camTag
            Array of characters representing camera name or ID of this image.

        @param frameId
            int value representing frame ID of this image.
     */

    virtual std::optional<Image*> addImage(const char* path, const char* modelTag, const char* camTag, int frameId, ImageLoadType loadType) noexcept = 0;

    virtual void addImage(Image* image) noexcept = 0;

    /*!
        @brief Constructs and adds to the scene the object plane described with input parameters.

        @param pWidth
            Parameter of pattern shape - number of points by pattern width.

        @param pHeight
            Parameter of pattern shape - number of points by pattern height.

        @param squareSize
            Length of a square edge.
     */

    virtual std::optional<ObjectPlane*> addObjectPlane(size_t pWidth, size_t pHeight, real_t squareSize) noexcept = 0;

    virtual void addObjectPlane(ObjectPlane* plane) noexcept = 0;

    virtual void setObject(Object* object) noexcept = 0;

    virtual void setMetricUnit(const char* metricUnit) noexcept = 0;

    virtual const char* getMetricUnit() const noexcept = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the initial CalibContext scene.
     */

    static std::optional<CalibContext*> create() noexcept;

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for calibration context scenes created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(CalibContext* scene) noexcept;
    };
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
