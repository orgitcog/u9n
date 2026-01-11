// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Calibration object related classes

    Header contains the C++ implementation of the calibration object container classes and related structures and functionalities.
 */
#pragma once

#include <carbon/Common.h>

#include <calib/Image.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)


/**
   @brief Three-dimensional calibration object.
    Calibration methods in this library rely on Zhang's calibration technique.
    For that reason the basic calibration object is chessboard pattern. We extend
    basic chessboard detection technique so we can detect multiple pattern planes on
    the same image, and becouse of this modification, the API supports mutliple
    chessboard pattern calibration object.

    Zhang calibration paper: https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/tr98-71.pdf
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/ObjectPlane
{
public:
    virtual ~ObjectPlane() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get pattern shape.

        Get shape of chessboard pattern (number of squares by width, number of squares by height).

        @return
            nitro::Vec2i where the first element represents number of squares by width, and the
            secont element represents number of squares by height.

     */

    virtual Eigen::Vector2i getPatternShape() const = 0;

    /**
        @brief Get local points.

        Get 3D points of a pattern in local coordinate system.

        @return
            nitro::Matrix (number of points, 3) where each matrix row represents one 3D point.
            Point coordinates are stored in collumns as (x, y, z).

     */

    virtual const Eigen::MatrixX<real_t>& getLocalPoints() const = 0;

    /**
       @brief Has projections check.

       The function checks whether the object plane has any projection on 2D plane, ie
       whether it is detected by any camera in the camera system.

       @return
        bool value true if object plane has at least one projection or false if there
        isn't any projection.
     */

    virtual bool hasProjections() const = 0;

    /**
        @brief Get global points.

        Get 3D points of a pattern in global coordinate system.

        @return
            nitro::Matrix (number of points, 3) where each matrix row represents one 3D point.
            Point coordinates are stored in collumns as (x, y, z).

     */
    virtual Eigen::MatrixX<real_t> getGlobalPoints(size_t atFrame = 0) const = 0;

    /**
        @brief Get pattern square size.

        Get length of a square edge that is the basis of the chessboard pattern.

        @return
            Length of a square edge in real value.
     */

    virtual real_t getSquareSize() const = 0;

    /**
        @brief Set transform matrix.

        Set object plane position and orienatation in world coordinate system (4x4 transform matrix relative to world origin).

        @param transform
            Transform matrix with shape 4x4, including rotation matrix and translation vector [R|t].

        @return
            trust::Expected<void> which is invalid if transform matrix shape is not valid.
     */

    virtual void setTransform(const Eigen::Matrix4<real_t>& transform, size_t atFrame = 0) = 0;

    virtual void setNumberOfFrames(size_t numberOfFrames) = 0;

    /**
        @brief Get world position.

        Get object plane position and orienatation in world coordinate system (4x4 transform matrix relative to world origin).

        @return
            Transformation matrix with 4x4 shape [R|t].
     */

    virtual const Eigen::Matrix4<real_t>& getTransform(size_t atFrame = 0) const = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the object plane described with input parameters.

        @param pWidth
            Parameter of pattern shape - number of points by pattern width.

        @param pHeight
            Parameter of pattern shape - number of points by pattern height.

        @param squareSize
            Length of a square edge.

        @warning Created object plane must always be deallocated using ObjectPlane::destructor.
     */

    static
    std::optional<ObjectPlane*> create(size_t pWidth, size_t pHeight, real_t squareSize);

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for object plane created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(ObjectPlane* objectPlane);
    };
};


class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Object
{
public:
    virtual ~Object() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get plane count.

        Get number of planes stored in object.

        @return
            Number of planes in size_t.
     */

    virtual size_t getPlaneCount() const = 0;

    /**
        @brief Add object plane.

        Add object plane to object.

        @param plane
            ObjectPlane object which contains information of unique object plane.

        @return
            trust::Expected<void> which is invalid if ObjectPlane object is invalid.
     */

    virtual void addObjectPlane(ObjectPlane* plane) = 0;

    /**
        @brief Get object plane.

        Get object plane from object.

        @param planeId
            Object plane position in object.

        @return
            ObjectPlane object which contains information of unique object plane.
     */

    virtual ObjectPlane* getObjectPlane(size_t planeId) const = 0;

    /**
        @brief Sort planes.

        Sort object planes inside object. This function sorts object planes by dimensions,
        in decreasing order, from largest to smallest. Main purpose of this function is
        preparation for object detection on images.

        @return
            trust::Expected<void> which is invalid if there is an internal error in the
            function.
     */

    virtual void sortPlanes() = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the object described with input parameter.

        @param transform
            Transformation matrix with 4x4 shape [R|t]. If given, this is initial object transform matrix,
            relative to world origin.

        @warning Created camera model must always be deallocated using Camera::destructor.
     */
    static
    Object* create(const Eigen::Matrix4<real_t>& transform = Eigen::Matrix4<real_t>::Identity());

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for calibration objects created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(Object* object);
    };
};


class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/ObjectPlaneProjection
{
public:
    virtual ~ObjectPlaneProjection() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Get object plane for this projection.

        Each object plane projection (2D representation) must have its corresponding object plane
        (3D representation). Function returns corresponding object plane.

        @return
            Pointer to ObjectPlane object.
     */

    virtual ObjectPlane* getObjectPlane() const = 0;

    /**
        @brief Set projection points (2D).

        Assign set of projection points to this object plane projection.

        @param points
            nitro::Matrix (number of points, 2) where each matrix row represents one 2D point.
            Point coordinates are stored in collumns as (x, y).

        @return
            trust::Expected<void> which is invalid if points container is empty.
     */

    virtual void setProjectionPoints(const Eigen::MatrixX<real_t>& points) = 0;

    /**
        @brief Get projection points (2D).

        Get projection points of this object plane projection.

        @return
            nitro::Matrix (number of points, 2) where each matrix row represents one 2D point.
            Point coordinates are stored in collumns as (x, y).
     */

    virtual const Eigen::MatrixX<real_t>& getProjectionPoints() const = 0;

    /**
        @brief Set image for this projection.

        Set image that corresponds to this projection.

        @param image
            Pointer to Image object.

        @return
            trust::Expected<void> which is invalid if image pointer is invalid.
     */

    virtual void setImage(Image* image) = 0;

    /**
        @brief Get image for this projection.

        Each object plane projection (2D representation) must have its corresponding image.
        Function returns corresponding image.

        @return
            Pointer to Image object.
     */

    virtual Image* getImage() const = 0;

    /**
        @brief Get transform.

        Get object plane position and orienatation in corresponding camera coordinate system.

        @return
            Transformation matrix with 4x4 shape [R|t].
     */

    virtual const Eigen::Matrix4<real_t> getTransform() const = 0;

    /**
        @brief Set transform.

        Set object plane position and orienatation in corresponding camera coordinate system.

        @return
            Transformation matrix with 4x4 shape [R|t].
     */

    virtual void setTransform(const Eigen::Matrix4<real_t>& transform) = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the object plane projection described with input parameters.

        @param plane
            Pointer to ObjectPlane object which corresponds to this object plane projection.

       @param image
            Pointer to Image object which corresponds to this object plane projection.

        @param points
            If non-nullptr is given, this is nitro::Matrix (number of points, 2) container
            where each matrix row represents one 2D point. Point coordinates are stored
            in collumns as (x, y).

        @warning Created object plane projection must always be deallocated using
        ObjectPlaneProjection::destructor.
     */

    static ObjectPlaneProjection* create(ObjectPlane* plane, Image* image = nullptr, const Eigen::MatrixX<real_t>& points =
                                         Eigen::Matrix2<real_t>::Zero());

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for object plane projections created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(ObjectPlaneProjection* objectPlaneProj);
    };
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
