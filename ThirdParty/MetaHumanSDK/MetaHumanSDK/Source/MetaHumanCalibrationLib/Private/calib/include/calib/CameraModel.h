// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Camera classes

    Header contains the C++ implementation of the camera model and related structures and functionalities.
 */
#pragma once

#include <carbon/Common.h>

#include <calib/Calibration.h>
#include <calib/Image.h>
#include <calib/Object.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
   @brief Mathematical representation of a camera.
    Mathematical camera model, defined with intrinsic, extrinsic and distortion parameters,
    approximately describes physical camera model and its position in 3D scene. In this
    project, we use pinhole camera model. Projection equation, for mapping 3D point onto
    2D camera plane is:

    @code
    x = K * [R|t] * X
    @endcode

    where K is 3x3 camera intrinsic matrix, R is 3x3 3D rotation matrix, t is 3x1 translation
    vector, X is 3D point in global coordinate system, and x is calculated point on a image
    plane.

    Main difference between physical camera and assumed pinhole mathematical camera model
    is that physical camera model uses lens to focus rays onto sensor. Due to their
    shape, lenses bring distortion into the image. This distortion is a geometric type distortion
    and the consequence is that the straight lines in the real world appear as curves on the
    image. Common geometric distortion types are barrel and pincushion. We expand our camera
    model to support these differences by adding the undistortion equation in projection pipeline.

    For more information check our paper:
    https://drive.google.com/open?id=1a7NCwb24yb3ho099m2dzNpNGcx2b4v51
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/CameraModel
{
public:
    virtual ~CameraModel() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Calibrate intrinsic camera parameters.

        Using 2D -> 3D point correspondences, function estimates camera intrinsic parameters - fx, fy, cx, cy.
        Also function estimates camera distortion parameters k1, k2, k3, p1, p2.
     */

    virtual std::optional<real_t> calibrateIntrinsics() = 0;

    /**
        @brief Get camera intrinsic matrix.

        Intrinsic matrix with shape 3x3, including individual parameters fx, fy, cx, cy.
     */

    virtual const Eigen::Matrix3<real_t>& getIntrinsicMatrix() const = 0;

    /**
        @brief Get camera distortion parameters.

        Distortion parameters stored in a vector of 5 elements - k1, k2, p1, p2, k3.
     */

    virtual const Eigen::VectorX<real_t>& getDistortionParams() const = 0;

    /**
        @brief Set projection data for camera model.

        Assign set of projection points to this camera model. These points are detected checkerboard pattern points,
        stored in ObjectPlaneProjection object.

        @param projections
            Array of ObjectPlaneProjection objects which contains set of detected checkerboard pattern points.

        @return
            trust::Expected<void> which is invalid if projections array is empty.
     */

    virtual void setProjectionData(const std::vector<ObjectPlaneProjection*>& projections) = 0;

    /**
        @brief Set intrinsic matrix (precalculated)

        Assign precalculated intrinsic matrix to the camera model. Initial values of intrisic matrix can
        speed-up the camera-calibration optimization.

        @param K
            Intrinsic matrix with shape 3x3, including individual parameters fx, fy, cx, cy.

        @return
            trust::Expected<void> which is invalid if K matrix shape is not valid.
     */

    virtual void setIntrinsicMatrix(const Eigen::Matrix3<real_t>& K) = 0;

    /**
        @brief Set distortion parameters (precalculated)

        Assign precalculated intrinsic matrix to the camera model. Initial values of intrisic matrix can
        speed-up the camera-calibration optimization.

        @param D
            Distortion parameters stored in a vector of 5 elements - k1, k2, p1, p2, k3.

        @return
            trust::Expected<void> which is invalid if D params shape is not valid.
     */

    virtual void setDistortionParams(const Eigen::VectorX<real_t>& D) = 0;

    /**
        @brief Get projection data of camera model.

        Get projection points assigned to this camera model. These points are detected checkerboard pattern points,
        stored in ObjectPlaneProjection object.

        @return
            Array of ObjectPlaneProjection objects which contains set of detected checkerboard pattern points.
     */

    virtual const std::vector<ObjectPlaneProjection*>& getProjectionData() const = 0;

    /**
        @brief Get name tag for camera model.

        @return
            Array of characters representing model name or ID.
     */

    virtual const char* getTag() const = 0;

    /**
        @brief Get width of the image frame.

        @return
            Width of the image frame.
     */

    virtual size_t getFrameWidth() const = 0;

    /**
        @brief Get height of the image frame.

        @return
        Height of the image frame.
     */

    virtual size_t getFrameHeight() const = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
       @brief Construct the camera model described with input parameters.

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

       @warning Created camera model must always be deallocated using CameraModel::destructor.
     */

    static std::optional<CameraModel*> create(const char* camModelTag,
                                              size_t imgW,
                                              size_t imgH,
                                              const std::vector<ObjectPlaneProjection*>& projections = std::vector<ObjectPlaneProjection*>(),
                                              const Eigen::Matrix3<real_t>& initK = Eigen::Matrix3<real_t>::Identity());

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for camera models created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(CameraModel* model);
    };
};


class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Camera
{
public:
    virtual ~Camera() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get camera model object for this camera.

        Each camera object must have its camera model which defines camera internal parameters.
        Function returns camera model which is assigned to this camera.

        @return
            Pointer to CameraModel object.
     */

    virtual CameraModel* getCameraModel() const = 0;

    /**
        @brief Get name tag for camera.

        @return
            Array of characters representing camera name or ID.
     */

    virtual const char* getTag() const = 0;

    /**
        @brief Calibrate extrinsic camera parameters.

        Using 2D -> 3D point correspondences, function estimates 4x4 transform matrix that describes
        camera translation and rotation relative to world coordinate system.
     */

    virtual bool calibrateExtrinsics() = 0;

    /**
        @brief Set camera model for this camera.

        If camera model parameter is not provided when constructing the camera, camera model can
        be assigned to camera using this function.

        @param model
            CameraModel object which contains information of camera intrinsic parameters.

        @return
            trust::Expected<void> which is invalid if camera model object is not valid.
     */

    virtual Eigen::Matrix4<real_t> calibrateExtrinsicsStereo(Camera* other) = 0;

    virtual void setCameraModel(CameraModel& model) = 0;

    /**
        @brief Set world position.

        Set camera position and orienatation in world coordinate system (4x4 transform matrix relative to world origin).

        @param transform
            Transform matrix with shape 4x4, including rotation matrix and translation vector [R|t].

        @return
            trust::Expected<void> which is invalid if matrix is not of valid shape.
     */

    virtual void setWorldPosition(const Eigen::Matrix4<real_t>& transform) = 0;

    /**
        @brief Is plane visible check.

        Check if is given plane visible on camera.

        @param plane
            ObjectPlane for which function performs a check whether it is visible on image plane of the camera.

        @return
            int value 1 if it is visible or 0 if it isn't visible.
     */

    virtual int isPlaneVisible(ObjectPlane& plane) const = 0;
    virtual int isPlaneVisibleOnFrame(ObjectPlane& plane, size_t frame) const = 0;
    /**
        @brief Get world position.

        Get camera position and orienatation in world coordinate system (4x4 transform matrix relative to world origin).

        @return
            Transformation matrix with 4x4 shape [R|t].
     */

    virtual const Eigen::Matrix4<real_t>& getWorldPosition() const = 0;

    /**
        @brief Get projection data of camera.

        Get projection points assigned to this camera. These points are detected checkerboard pattern points,
        stored in ObjectPlaneProjection object.

        @return
            Array of ObjectPlaneProjection objects which contains set of detected checkerboard pattern points.
     */

    virtual const std::vector<ObjectPlaneProjection*>& getProjectionData() const = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the camera described with input parameters.

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

        @warning Created camera model must always be deallocated using Camera::destructor.
     */

    static std::optional<Camera*> create(const char* camTag,
                                         CameraModel* cameraModel = nullptr,
                                         const Eigen::Matrix<real_t, 4, 4>& position = Eigen::Matrix<real_t,
                                                                                                     4,
                                                                                                     4>
                                         ::Identity());

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for camera models created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(Camera* camera);
    };
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
