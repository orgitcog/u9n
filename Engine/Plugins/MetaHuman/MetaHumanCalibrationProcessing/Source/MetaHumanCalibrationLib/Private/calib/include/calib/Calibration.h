// Copyright Epic Games, Inc. All Rights Reserved.

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
    3Lateral Multiple-View-Geometry Toolkit Module

    Module contains the C++ implementation of the basic camera calibration and
    multiple-view-geometry related functions.
 */

/**
    @brief
        Enumeration of possible algorithms for pattern detection.
 */

enum PatternDetect
{
    /**
        Quick-search algorithm
        which can easily and quick find a pattern in an image with good
        contrast and low noise.
     */

    DETECT_FAST = 0,

    /**
        Includes image normalization
        and adaptive thresholding for more robust pattern detection. This
        algorithm is much slower than DETECT_FAST.
     */

    DETECT_DEEP = 1
};

/**
    @brief
        Enumeration of possible errors with reliability of an image.
 */

enum ImageReliability
{
    NO_GRID_DETECTED = 0,
    BLURRY_GRID = 1,
    IMAGE_RELIABLE = 2
};


/**
    @brief
        Enumeration of possible algorithms for intrinsic parameters calibration.
 */

enum IntrinsicEstimation
{
    /**
        Estimation of K matrix parameters (focal lengths and principal point) while
        distortion parameters are fixed.
     */

    K_MATRIX = 0,

    /**
        Estimation of distortion parameters (radial only) while
        K matrix parameters are fixed.
     */

    D_PARAMETERS = 1,

    /**
        Estimation of distortion parameters (radial and tangential) while
        K matrix parameters are fixed.
     */

    D_PARAMETERS_FULL = 2,

    /**
        Estimation of K matrix parameters and distortion (radial only) parameters.
     */

    K_AND_D = 3
};


/**
    @brief Dummy class for exception throwing when pattern is not found on image.
 */

class PatternNotFound
{};

/**
    @brief Loads image from specified path and stores pixels into
        nitro::Matrix container.

    @return
        trust::Expected<Eigen::MatrixX<real_t> > which is invalid
        if image is not loaded. If it is valid, this is nitro::Matrix that
        contains image pixel values.
 */

enum SceneCalibrationType
{
    FULL_CALIBRATION = 0,
    FIXED_INTRINSICS = 1,
	FIXED_PROJECTIONS = 2,
};

struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/BAParams
{
    std::size_t iterations{ 50 };
    std::size_t frameNum{ 1 };
    bool optimizeIntrinsics{ false };
    bool optimizeDistortion{ false };
    bool optimizePoints{ false };
    std::vector<int> fixedIntrinsicIndices{};
    std::vector<int> fixedDistortionIndices{};
};
/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/ImageReliability
checkFrameReliability(const Eigen::MatrixX<real_t>& image, int patternWidth, int patternHeight, double sharpnessThreshold);

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<Eigen::MatrixX<real_t>> loadImage(const char* path);

/**
    @brief Detect defined pattern on input image.

    @param image
        Input parameter - container matrix that contains pixel
        values stored as real number. Input matrix has one channel
        and pixels are grayscale.

    @param p_w
        Input parameter that defines the width (number of squares per
        width) of the pattern we want to detect.

    @param p_h
        Input parameter that defines the height (number of squares per
        height) of the pattern we want to detect.

    @param sq_size
        Input parameter that defines length of the square edge.

    @note
        if specified pattern is not found on the image, function returns
        PatternNotFound object.

    @return
        trust::Expected<Eigen::MatrixX<real_t> > which returns PatternNotFound
        object if pattern is not detected. If it is valid, this is nitro::Matrix
        that contains detected image points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<Eigen::MatrixX<real_t>> detectPattern(const Eigen::MatrixX<real_t>& image, std::size_t p_w, std::size_t p_h, real_t sq_size, PatternDetect type);

/**
    @brief
        Detect multiple patterns on input image.

    @param image
        Input parameter - container matrix that contains pixel
        values stored as real number. Input matrix has one channel
        and pixels are grayscale.

    @param p_w
        Input parameter that defines the width (number of squares per
        width) for each pattern we want to detect.

    @param p_h
        Input parameter that defines the height (number of squares per
        height) for each pattern we want to detect.

    @param sq_size
        Input parameter that defines length of the square edge for each
        pattern we want to detect.

    @return
        trust::Expected<Eigen::MatrixX<real_t> > which is invalid
        if image is not loaded. If it is valid, this is array nitro::Matrix
        where each matrix contains points of detected pattern.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::vector<Eigen::MatrixX<real_t>> detectMultiplePatterns(const Eigen::MatrixX<real_t>& image, const std::vector<std::size_t>& p_w, const std::vector<std::size_t>& p_h, const std::vector<real_t>& sq_size, PatternDetect type);

/**
   \defgroup calibrateIntrinsics
    @brief Estimate values of camera model intrinsic matrix.

    Intrinsic parameters (fx, fy, cx, cy)
    Distortion parameters as well (k1, k2, p1, p2, k3)

    @param intrinsics
        Output matrix of intrinsic camera parameters. Intrinsics matrix
        can contain initial parameter values that will be further optimized.

        [fx, 0, cx]
        [0, fy, cy]
        [0,  0,  1]

    @param distortion
        Output matrix of distortion parameters [k1, k2, p1, p2, k3].

    @param im_w
        Width of the calibration image.

    @param im_h
        Height of the calibration image.

    @param type
        Which parameters to estimate.

    @return
        Mean squared error between detected and reprojected image points.

 */


/** @ingroup calibrateIntrinsics
    @param points2d
        Input parameter - array of matrices that contain
        2D object projection points from different views
        or from different patterns.

    @param points3d
        Input parameter - array of matrices that contain
        3D world object points that correspond to 2D points.
        Stride between these points define coordinate system
        scale.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                    const std::vector<Eigen::MatrixX<real_t>>& points3d,
                    Eigen::Matrix3<real_t>& intrinsics,
                    Eigen::VectorX<real_t>& distortion,
                    std::size_t im_w,
                    std::size_t im_h,
                    IntrinsicEstimation type);

/**
   @brief Estimate values of camera model intrinsic matrix.

    Intrinsic parameters (fx, fy, cx, cy)
    Distortion parameters as well (k1, k2, p1, p2, k3)

   @param intrinsics
    Output matrix of intrinsic camera parameters. Intrinsics matrix
    can contain initial parameter values that will be further optimized.

        [fx, 0, cx]
        [0, fy, cy]
        [0,  0,  1]

   @param distortion
    Output matrix of distortion parameters [k1, k2, p1, p2, k3].

   @param im_w
    Width of the calibration image.

   @param im_h
    Height of the calibration image.

   @return
    Mean squared error between detected and reprojected image points.

 */


/** @ingroup calibrateIntrinsics
    @param points2d
        Input parameter - array of matrices that contain
        2D object projection points from different views
        or from different patterns.

    @param points3d
        Input parameter - array of matrices that contain
        3D world object points that correspond to 2D points.
        Stride between these points define coordinate system
        scale.
 */


/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                    const Eigen::MatrixX<real_t>& points3d,
                    Eigen::Matrix3<real_t>& intrinsics,
                    Eigen::VectorX<real_t>& distortion,
                    std::size_t im_w,
                    std::size_t im_h);

/** @ingroup calibrateIntrinsics
    @param points2d
        Input parameter - array of matrices that contain
        2D object projection points from different views.

    @param points3d
        Input parameter - matrix that contain
        3D world object points that correspond to all 2D points.
        Stride between these points define coordinate system
        scale. This function is used when there is only one
        pattern used for calibration.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                    const std::vector<Eigen::MatrixX<real_t>>& points3d,
                    Eigen::Matrix3<real_t>& intrinsics,
                    Eigen::VectorX<real_t>& distortion,
                    std::size_t im_w,
                    std::size_t im_h);

/** @ingroup calibrateIntrinsics
    @param points2d
        Input parameter - array of matrices that contain
        2D object projection points from different views
        or from different patterns.

    @param points3d
        Input parameter - array of matrices that contain
        all possible 3D world object points on the scene.
        Stride between these points define coordinate system
        scale.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                    const std::vector<Eigen::MatrixX<real_t>>& points3d,
                    const Eigen::VectorX<std::size_t>& p_idx,
                    Eigen::Matrix3<real_t>& intrinsics,
                    Eigen::VectorX<real_t>& distortion,
                    std::size_t im_w,
                    std::size_t im_h);

/**
    @brief
        Estimate values of camera rotation matrix and
        translation vector, relative to detected pattern.

        Rotation matrix and translation vector are packed
        in a 4x4 transformation matrix, for convenience.

    @param points2d
        Input parameter - matrix that contain
        2D object projection points from detected
        pattern.

    @param points3d
        Input parameter - matrix that contain correspondent
        3D points of detected pattern.

    @param intrinsics
        Input matrix of intrinsic camera parameters.

    @param distortion
        Input matrix of distortion parameters.

    @param T
        Output 4x4 transformation matrix [R|t]. It contains
        rotation matrix and translation vector of a camera
        relative to given 3D pattern points.

            [r11, r12, r13, t1]
            [r21, r22, r23, t1]
            [r31, r32, r33, t1]
            [  0,   0,   0,  1]

    @return
        Mean squared error between detected and reprojected image points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateExtrinsics(const Eigen::MatrixX<real_t>& points2d,
                    const Eigen::MatrixX<real_t>& points3d,
                    const Eigen::Matrix3<real_t>& intrinsics,
                    const Eigen::VectorX<real_t>& distortion,
                    Eigen::Matrix4<real_t>& T);

/**
    @brief
        Estimate values of camera rotation matrix and
        translation vector, relative to other camera
        in the stereo system.

        Rotation matrix and translation vector are packed in a 4x4
        transformation matrix, for convenience.

    @note It is assumed that cameras have same intrinsic parameters.

    @param points2d_1
        Input parameter - array of matrices that contain
        2D object projection points of the first camera
        from detected patterns in multiple frames.

    @param points2d_2
        Input parameter - array of matrices that contain
        2D object projection points of the second camera
        from detected patterns in multiple frames.

    @param points3d
        Input parameter - array of matrices that contain
        3D object points from detected patterns in multiple
        frames.

    @param intrinsics
        Input matrix of intrinsic camera parameters.

    @param distortion
        Input matrix of distortion parameters.

    @param T
        Output 4x4 transformation matrix [R|t]. It contains
        rotation matrix and translation vector of a camera
        relative to other camera -> camera2 relative to camera1.

                [r11, r12, r13, t1]
                [r21, r22, r23, t1]
                [r31, r32, r33, t1]
                [  0,   0,   0,  1]

    @return
        Mean squared error between detected and reprojected image points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateStereoExtrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d_1,
                          const std::vector<Eigen::MatrixX<real_t>>& points2d_2,
                          const std::vector<Eigen::MatrixX<real_t>>& points3d,
                          const Eigen::Matrix3<real_t>& intrinsics,
                          const Eigen::VectorX<real_t>& distortion,
                          Eigen::Matrix4<real_t>& T,
                          std::size_t im_w,
                          std::size_t im_h);

/**
    @brief
        Estimate values of camera rotation matrix and
        translation vector, relative to other camera
        in the stereo system. Rotation matrix and
        translation vector are packed in a 4x4
        transformation matrix, for convenience.

    @note Cameras can have different internal parameters.

    @param points2d_1
        Input parameter - array of matrices that contain
        2D object projection points of the first camera
        from detected patterns in multiple frames.

    @param points2d_2
        Input parameter - array of matrices that contain
        2D object projection points of the second camera
        from detected patterns in multiple frames.

    @param points3d
        Input parameter - array of matrices that contain
        3D object points from detected patterns in multiple
        frames.

    @param intrinsics_1
        Input matrix of intrinsic camera parameters for the
        first camera.

    @param intrinsics_2
        Input matrix of intrinsic camera parameters for the
        second camera.

    @param distortion_1
        Input matrix of distortion parameters for the
        first camera.

    @param distortion_2
        Input matrix of distortion parameters for the
        second camera.

    @param T
        Output 4x4 transformation matrix [R|t]. It contains
        rotation matrix and translation vector of a camera
        relative to other camera -> camera2 relative to camera1.

            [r11, r12, r13, t1]
            [r21, r22, r23, t1]
            [r31, r32, r33, t1]
            [  0,   0,   0,  1]

    @return
        Mean squared error between detected and reprojected image points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
calibrateStereoExtrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d_1,
                          const std::vector<Eigen::MatrixX<real_t>>& points2d_2,
                          const std::vector<Eigen::MatrixX<real_t>>& points3d,
                          const Eigen::Matrix3<real_t>& intrinsics_1,
                          const Eigen::VectorX<real_t>& distortion_1,
                          const Eigen::Matrix3<real_t>& intrinsics_2,
                          const Eigen::VectorX<real_t>& distortion_2,
                          Eigen::Matrix4<real_t>& T,
                          std::size_t im_w,
                          std::size_t im_h);

/**
    @brief
        Generates array of 3D points (where Z = 0 for each point)
        based on input parameters.

    @param p_w
        Input parameter that defines the width (number of squares per
        width) of the pattern.

    @param p_h
        Input parameter that defines the height (number of squares per
        height) of the pattern.

    @param sq_size
        Input parameter that defines length of the square edge
        (stride between points).

    @return
        Eigen::MatrixX<real_t> matrix that contains generated 3D points.
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::MatrixX<real_t>
generate3dPatternPoints(std::size_t p_w, std::size_t p_h, real_t sq_size);

/**
    @brief
        Estimates relative transformation between two patterns
        detected on same image.

    @param p1_points_2d
        Input parameter - 2D projection points of the first
        pattern.

    @param p1_points_3d
        Input parameter - 3D points of the first pattern.

    @param p2_points_2d
        Input parameter - 2D projection points of the second
        pattern.

    @param p2_points_3d
        Input parameter - 3D points of the second pattern.

    @param intrinsics
        Input matrix of intrinsic camera parameters.

    @param distortion
        Input matrix of distortion parameters.

    @param T
        Output 4x4 transformation matrix [R|t]. It contains
        rotation matrix and translation vector of one pattern
        plane relative to other pattern plane.

            [r11, r12, r13, t1]
            [r21, r22, r23, t1]
            [r31, r32, r33, t1]
            [  0,   0,   0,  1]
 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/bool
estimateRelativePatternTransform(const Eigen::MatrixX<real_t>& p1_points_2d,
                                 const Eigen::MatrixX<real_t>& p1_points_3d,
                                 const Eigen::MatrixX<real_t>& p2_points_2d,
                                 const Eigen::MatrixX<real_t>& p2_points_3d,
                                 const Eigen::Matrix3<real_t>& intrinsics,
                                 const Eigen::VectorX<real_t>& distortion,
                                 Eigen::Matrix4<real_t>& T);

/**
   @brief
    Projects point on image plane.

   @param point3d
    Input parameter - 3D point in world coordinate system.

   @param intrinsics
    Input matrix of intrinsic camera parameters.

   @param distortion
    Input matrix of distortion parameters.

   @param T
    Input 4x4 camera transformation matrix [R|t].

   @return
    Coordinates of projected 2D point on image plane.

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/Eigen::Vector2<real_t>
projectPointOnImagePlane(const Eigen::VectorX<real_t>& point3d,
                         const Eigen::Matrix3<real_t>& intrinsics,
                         const Eigen::VectorX<real_t>& distortion,
                         const Eigen::Matrix4<real_t>& T);


/**
   @brief
    Projects points on image plane.

   @param points3d
    Input parameter - 3D points in world coordinate system.

   @param intrinsics
    Input matrix of intrinsic camera parameters.

   @param distortion
    Input matrix of distortion parameters.

   @param T
    Input 4x4 camera transformation matrix [R|t].

   @param outPoints2d
    Output parameter of 2D points obtained by projecting
    3D points on camera image plane.

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/void projectPointsOnImagePlane(
    const Eigen::MatrixX<real_t>& points3d,
    const Eigen::Matrix3<real_t>& intrinsics,
    const Eigen::VectorX<real_t>& distortion,
    const Eigen::Matrix4<real_t>& T,
    Eigen::MatrixX<real_t>& outPoints2d);

/**
   @brief
    Bundle adjustment of current scene in order to reduce reprojection error of cameras.

   @param points
    Input/Output parameter - positions of points in global coordinate system.

   @param imagePoints
    Projections of 3d points for each camera

   @param visibility
    Visibility of 3d points for each camera

   @param cameraMatrix
    Input/Output intrinsic matrices of all cameras

   @param R
    Input/Output - Rotation matrices of all cameras

   @param T
    Input/Output - Translation vectors of all cameras

   @param distCoeffs
    Input/Output - Distortion coefficients of all cameras

   @param params
    Input - set of instructions for Bundle adjustment algorithm.

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/std::optional<real_t>
bundleAdjustment(const Eigen::MatrixX<real_t>& points,
                 std::vector<Eigen::Matrix4<real_t>>& objTransform,
                 const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                 const std::vector<std::vector<bool>>& visibility,
                 std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                 std::vector<Eigen::Matrix4<real_t>>& cameraTransform,
                 std::vector<Eigen::VectorX<real_t>>& distCoeffs,
                 const BAParams& params);

/**
   @brief
    Bundle adjustment of current scene in order to reduce reprojection error of cameras.

   @param points
    Input/Output parameter - positions of points in global coordinate system.

   @param imagePoints
    Projections of 3d points for each camera

   @param visibility
    Visibility of 3d points for each camera

   @param cameraMatrix
    Input/Output intrinsic matrices of all cameras

   @param R
    Input/Output - Rotation matrices of all cameras

   @param T
    Input/Output - Translation vectors of all cameras

   @param distCoeffs
    Input/Output - Distortion coefficients of all cameras

   @param params
    Input - set of instructions for Bundle adjustment algorithm.

 */

/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/
std::optional<real_t> featureBundleAdjustment(Eigen::MatrixX<real_t>& points,
                                              const std::vector<std::vector<Eigen::MatrixX<real_t>>>& imagePoints,
                                              const std::vector<std::vector<std::vector<bool>>>& visibility,
                                              std::vector<Eigen::Matrix3<real_t>>& cameraMatrix,
                                              std::vector<Eigen::Matrix4<real_t>>& cameraTransform,
                                              std::vector<Eigen::VectorX<real_t>>& distCoeffs,
                                              const BAParams& params);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
