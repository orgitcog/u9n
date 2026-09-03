// Copyright Epic Games, Inc. All Rights Reserved.

#include "ErrorInternal.h"
//#include <c/Calibration.h>
#include <calib/Calibration.h>
#include <calib/Utilities.h>
#include <optional>
#include <carbon/Common.h>

#include <calib/BeforeOpenCvHeaders.h>
CARBON_DISABLE_EIGEN_WARNINGS
#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
CARBON_RENABLE_WARNINGS
#include <calib/AfterOpenCvHeaders.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

#define CV_REAL CV_64F

CARBON_DISABLE_EIGEN_WARNINGS

Eigen::MatrixX<real_t> convertPointVectorX2Matrix(const std::vector<cv::Point2f>& points);
cv::Point2f getNitroPoint2D(int l, const Eigen::MatrixX<real_t>& m);
cv::Point3f getNitroPoint3D(int l, const Eigen::MatrixX<real_t>& m);
void maskDetectedPattern(cv::Mat& inputOutput, const std::vector<cv::Point2f>& corner_points);
std::vector<cv::Point2f> nitroToCvPoints2(const Eigen::MatrixX<real_t>& m);
std::vector<cv::Point3f> nitroToCvPoints3(const Eigen::MatrixX<real_t>& m);


ImageReliability checkFrameReliability(const Eigen::MatrixX<real_t>& image, int patternWidth, int patternHeight, double sharpnessThreshold)
{
    cv::Mat cv_image;
    cv::eigen2cv(image, cv_image);
    cv::normalize(cv_image, cv_image, 255, 0, cv::NORM_MINMAX, CV_8U);

    std::vector<cv::Point2f> points;

    if (cv::findChessboardCorners(cv_image, cv::Size(patternWidth, patternHeight), points, cv::CALIB_CB_ADAPTIVE_THRESH))
    {
        cv::Scalar sharpness = cv::estimateChessboardSharpness(cv_image, cv::Size((int)patternWidth, (int)patternHeight), points);
        if (sharpness[0] <= sharpnessThreshold)
        {
            return ImageReliability::IMAGE_RELIABLE;
        }
        return ImageReliability::BLURRY_GRID;
    }

    return ImageReliability::NO_GRID_DETECTED;
}

std::optional<Eigen::MatrixX<real_t>> loadImage(const char* path)
{
    cv::Mat image;

    CV_CALL_CATCH(image = cv::imread(path, cv::IMREAD_GRAYSCALE),
                  std::runtime_error("Loading image at " + std::string(path) + " failed."));
    if (image.empty())
    {
        std::cout << "Loading image at " + std::string(path) + " failed." << std::endl;
        return std::nullopt;
    }

    CARBON_ASSERT(image.channels() == 1, "Input must have 1 channel.");

    image.convertTo(image, CV_REAL);
    Eigen::MatrixX<real_t> out;
    cv::cv2eigen(image, out);

    return out;
}

std::optional<Eigen::MatrixX<real_t>> detectPattern(const Eigen::MatrixX<real_t>& image, std::size_t p_w, std::size_t p_h, real_t /*sq_size*/, PatternDetect type)
{
    CARBON_ASSERT(image.size() != 0, "Input image container is empty.");

    int pref_size = 1500;
    float scale = 1;
    cv::Mat cv_image;
    cv::eigen2cv(image, cv_image);
    cv::normalize(cv_image, cv_image, 0, 255, cv::NORM_MINMAX, CV_8U);

    cv::Mat cv_image_r_s;
    std::vector<cv::Point2f> points_cv;

    int d_value = std::min(cv_image.cols, cv_image.rows);
    if (d_value > pref_size)
    {
        scale = (float)pref_size / (float)d_value;
        CV_CALL_CATCH(cv::resize(cv_image, cv_image_r_s,
                                 cv::Size((int)((float)cv_image.cols * scale), (int)((float)cv_image.rows * scale)),
                                 cv::INTER_CUBIC),
                      std::runtime_error("Input image resize failed."));
    }
    else
    {
        cv_image_r_s = cv_image;
    }
    int detect_alg;
    if (type == DETECT_FAST)
    {
        detect_alg = cv::CALIB_CB_FAST_CHECK;
    }
    else
    {
        detect_alg = cv::CALIB_CB_ADAPTIVE_THRESH;
    }
    bool board_found;
    CV_CALL_CATCH(board_found = cv::findChessboardCorners(cv_image_r_s, cv::Size((int)p_w, (int)p_h), points_cv, detect_alg),
                  std::runtime_error("Pattern detection failed."));
    if (!board_found)
    {
        LOG_INFO("Board not found");
        return std::nullopt;
    }

    for (size_t i = 0; i < points_cv.size(); i++) //-V621 //-V654
    {
        points_cv[i].x /= scale;
        points_cv[i].y /= scale;
    }

    CV_CALL_CATCH(cv::cornerSubPix(cv_image, points_cv, cv::Size(5, 5), cv::Size(-1, -1),
                                   cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 10000, 10e-6)),
                  std::runtime_error("Subpixel corner refinement failed."));
    Eigen::MatrixX<real_t> output = convertPointVectorX2Matrix(points_cv);
    return output;
}

std::vector<Eigen::MatrixX<real_t>> detectMultiplePatterns(const Eigen::MatrixX<real_t>& image,
                                                    const std::vector<std::size_t>& p_w,
                                                    const std::vector<std::size_t>& p_h,
                                                    const std::vector<real_t>& sq_size,
                                                    PatternDetect type)
{
    std::vector<Eigen::MatrixX<real_t>> detected_internal;
    const std::size_t pattern_count = sq_size.size();
    Eigen::MatrixX<real_t> image_tmp = image;

    for (size_t i = 0; i < pattern_count; i++)
    {
        std::optional<Eigen::MatrixX<real_t>> maybe_detected = detectPattern(image_tmp,
                                                                      p_w[i],
                                                                      p_h[i],
                                                                      sq_size[i],
                                                                      type);

        if (!maybe_detected.has_value())
        {
            continue;
        }
        else
        {
            std::vector<cv::Point2f> mask_points(4);
            mask_points[0] = getNitroPoint2D(0, maybe_detected.value());
            mask_points[1] = getNitroPoint2D((int)p_w[i] - 1, maybe_detected.value());
            mask_points[2] = getNitroPoint2D((int)p_w[i] * (int)p_h[i] - 1, maybe_detected.value());
            mask_points[3] = getNitroPoint2D((int)p_w[i] * (int)p_h[i] - (int)p_w[i], maybe_detected.value());
            cv::Mat image_cv;
            cv::eigen2cv(image, image_cv);
            maskDetectedPattern(image_cv, mask_points);
            detected_internal.push_back(maybe_detected.value());
            cv::cv2eigen(image_cv, image_tmp);
        }
    }
    return detected_internal;
}

std::optional<real_t> calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                                          const std::vector<Eigen::MatrixX<real_t>>& points3d,
                                          Eigen::Matrix3<real_t>& intrinsics,
                                          Eigen::VectorX<real_t>& distortion,
                                          std::size_t im_w,
                                          std::size_t im_h,
                                          IntrinsicEstimation type)
{
    real_t mse;
    CARBON_ASSERT(points2d.size() == points3d.size(), "Given correspondent points are not of the same size.");

    cv::Mat K;
    cv::Mat D;
    cv::Mat r, t, E, F;
    cv::TermCriteria criteria;
    std::vector<cv::Mat> r1vec, t1vec;
    int USE_INTRINSIC_GUESS = 0;

    if ((intrinsics(2, 2) != 1) || (intrinsics(2, 1) != 0) || (intrinsics(2, 0) != 0))
    {
        K = cv::Mat::eye(3, 3, CV_32F);
    }
    else
    {
        cv::eigen2cv(intrinsics, K);
        USE_INTRINSIC_GUESS = cv::CALIB_USE_INTRINSIC_GUESS;
    }

    if ((distortion.rows() != 5) || (distortion.cols() != 1))
    {
        D = cv::Mat::zeros(5, 1, CV_32F);
    }
    else
    {
        cv::eigen2cv(distortion, D);
    }
    if (K.at<float>(2, 0) == 0)
    {
        USE_INTRINSIC_GUESS = 0;
    }

    std::vector<std::vector<cv::Point2f>> image_points;
    std::vector<std::vector<cv::Point3f>> object_points;

    for (size_t i = 0; i < points2d.size(); i++)
    {
        image_points.push_back(nitroToCvPoints2(points2d[i]));
        object_points.push_back(nitroToCvPoints3(points3d[i]));
    }
    int flags = USE_INTRINSIC_GUESS + cv::CALIB_FIX_ASPECT_RATIO + cv::CALIB_FIX_K4 + cv::CALIB_FIX_K5 + cv::CALIB_FIX_K6 +
        cv::CALIB_FIX_TAUX_TAUY +
        cv::CALIB_FIX_S1_S2_S3_S4;

    switch (type)
    {
        case K_MATRIX:
            flags += cv::CALIB_FIX_K1 + cv::CALIB_FIX_K2 + cv::CALIB_ZERO_TANGENT_DIST +
                cv::CALIB_FIX_K3;
            break;

        case D_PARAMETERS:
            flags += cv::CALIB_FIX_INTRINSIC + cv::CALIB_ZERO_TANGENT_DIST +
                cv::CALIB_FIX_K3;
            break;

        case D_PARAMETERS_FULL:
            flags += cv::CALIB_FIX_INTRINSIC;
            break;

        case K_AND_D:
            flags += cv::CALIB_ZERO_TANGENT_DIST +
                cv::CALIB_FIX_K3;
            break;
    }

    CV_CALL_CATCH(mse = cv::calibrateCamera(object_points, image_points, cv::Size((int)im_w, (int)im_h), K, D, r1vec, t1vec,
                                            flags), std::runtime_error("Internal camera calibration failed."));

    cv::cv2eigen(K, intrinsics);
    cv::cv2eigen(D, distortion);
    return mse;
}

std::optional<real_t> calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                                          const std::vector<Eigen::MatrixX<real_t>>& points3d,
                                          Eigen::Matrix3<real_t>& intrinsics,
                                          Eigen::VectorX<real_t>& distortion,
                                          std::size_t im_w,
                                          std::size_t im_h)
{
    IntrinsicEstimation type = K_AND_D;
    real_t mse;

    CARBON_ASSERT(points2d.size() == points3d.size(), "Given correspondent points are not of the same size.");

    std::optional<real_t> maybeMse = calibrateIntrinsics(points2d, points3d, intrinsics, distortion, im_w, im_h, type);
    mse = maybeMse.value();
    return mse;
}

std::optional<real_t> calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d, const Eigen::MatrixX<real_t>& points3d, Eigen::Matrix3<real_t>& intrinsics,
                                          Eigen::VectorX<real_t>& distortion, std::size_t im_w, std::size_t im_h)
{
    const std::size_t points_count = points2d.size();
    std::vector<Eigen::MatrixX<real_t>> points3d_l;
    for (size_t i = 0; i < points_count; i++)
    {
        points3d_l.push_back(points3d);
    }
    std::optional<real_t> mse = calibrateIntrinsics(points2d,
                                                    points3d_l,
                                                    intrinsics,
                                                    distortion,
                                                    im_w,
                                                    im_h);

    return mse;
}

std::optional<real_t> calibrateIntrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d,
                                          const std::vector<Eigen::MatrixX<real_t>>& points3d,
                                          const Eigen::VectorX<std::size_t>& p_idx,
                                          Eigen::Matrix3<real_t>& intrinsics,
                                          Eigen::VectorX<real_t>& distortion,
                                          std::size_t im_w,
                                          std::size_t im_h)
{
    const std::size_t points_count = points2d.size();
    std::vector<Eigen::MatrixX<real_t>> points3d_l;
    for (size_t i = 0; i < points_count; i++)
    {
        points3d_l.push_back(points3d[p_idx[i]]);
    }

    std::optional<real_t> mse = calibrateIntrinsics(points2d,
                                                    points3d_l,
                                                    intrinsics,
                                                    distortion,
                                                    im_w,
                                                    im_h);

    return mse;
}

std::optional<real_t> calibrateExtrinsics(const Eigen::MatrixX<real_t>& points2d, const Eigen::MatrixX<real_t>& points3d, const Eigen::Matrix3<real_t>& intrinsics,
                                          const Eigen::VectorX<real_t>& distortion, Eigen::Matrix4<real_t>& T)
{
    real_t mse = 0;
    CARBON_ASSERT(points2d.rows() == points3d.rows(), "Given correspondent points are not of the same size.");
    cv::Mat R, t, Rr, objectPosition, K, D;
    std::vector<cv::Point2f> image_points = nitroToCvPoints2(points2d);
    std::vector<cv::Point3f> object_points = nitroToCvPoints3(points3d);

    cv::eigen2cv(intrinsics, K);
    cv::eigen2cv(distortion, D);
    CV_CALL_CATCH(cv::solvePnP(object_points, image_points, K, D, R, t),
                  std::runtime_error("Calibration of extrinsic camera parameters failed."));
    cv::Rodrigues(R, Rr);
    Eigen::MatrixX<real_t> rot, trans;
    cv::cv2eigen(Rr, rot);
    cv::cv2eigen(t, trans);

    Eigen::MatrixX<real_t> transform = makeTransformationMatrix(rot, trans);
    T = transform;

    Eigen::MatrixX<real_t> projPoints;
    projectPointsOnImagePlane(points3d, intrinsics, distortion, T, projPoints);
    std::optional<real_t> maybeMse = calculateMeanSquaredError(points2d, projPoints);

    if (!maybeMse.has_value())
    {
        LOG_WARNING("Mean squared error is not valid.");
        return std::nullopt;
    }
    mse = maybeMse.value();

    return mse;
}

std::optional<real_t> calibrateStereoExtrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d_1,
                                                const std::vector<Eigen::MatrixX<real_t>>& points2d_2,
                                                const std::vector<Eigen::MatrixX<real_t>>& points3d,
                                                const Eigen::Matrix3<real_t>& intrinsics,
                                                const Eigen::VectorX<real_t>& distortion,
                                                Eigen::Matrix4<real_t>& T,
                                                std::size_t im_w,
                                                std::size_t im_h)
{
    std::optional<real_t> mse = calibrateStereoExtrinsics(points2d_1,
                                                          points2d_2,
                                                          points3d,
                                                          intrinsics,
                                                          distortion,
                                                          intrinsics,
                                                          distortion,
                                                          T,
                                                          im_w,
                                                          im_h);
    return mse.value();
}

std::optional<real_t> calibrateStereoExtrinsics(const std::vector<Eigen::MatrixX<real_t>>& points2d_1,
                                                const std::vector<Eigen::MatrixX<real_t>>& points2d_2,
                                                const std::vector<Eigen::MatrixX<real_t>>& points3d,
                                                const Eigen::Matrix3<real_t>& intrinsics_1,
                                                const Eigen::VectorX<real_t>& distortion_1,
                                                const Eigen::Matrix3<real_t>& intrinsics_2,
                                                const Eigen::VectorX<real_t>& distortion_2,
                                                Eigen::Matrix4<real_t>& T,
                                                std::size_t im_w,
                                                std::size_t im_h)
{
    real_t mse;
    cv::Mat K1, D1, K2, D2;
    cv::Mat R, t, Rr, E, F, Q;


    CARBON_ASSERT(points2d_1.size() == points3d.size(), "Given correspondent points are not of the same size.");
    CARBON_ASSERT(points2d_2.size() == points3d.size(), "Given correspondent points are not of the same size.");
    CARBON_ASSERT(intrinsics_1.rows() == intrinsics_1.cols() && intrinsics_1.rows() == 3 && intrinsics_2.rows() == intrinsics_2.cols() &&
                  intrinsics_2.rows() == 3,
                  "Some of given K matrix is not of correct shape.");

    cv::eigen2cv(intrinsics_1, K1);
    cv::eigen2cv(intrinsics_2, K2);

    CARBON_ASSERT((distortion_1.rows() == 5) && (distortion_1.cols() == 1) && (distortion_2.rows() == 5) && (distortion_2.cols() == 1),
                  "Some of given D vector(matrix) is not of correct shape.");

    cv::eigen2cv(distortion_1, D1);
    cv::eigen2cv(distortion_2, D2);

    std::vector<std::vector<cv::Point2f>> image_points_c1, image_points_c2;
    std::vector<std::vector<cv::Point3f>> object_points;

    for (size_t i = 0; i < points3d.size(); i++)
    {
        image_points_c1.push_back(nitroToCvPoints2(points2d_1[i]));
        image_points_c2.push_back(nitroToCvPoints2(points2d_2[i]));
        object_points.push_back(nitroToCvPoints3(points3d[i]));
    }

    CV_CALL_CATCH(mse = cv::stereoCalibrate(object_points,
                                            image_points_c1,
                                            image_points_c2,
                                            K1,
                                            D1,
                                            K2,
                                            D2,
                                            cv::Size((int)im_w, (int)im_h),
                                            R,
                                            t,
                                            E,
                                            F,
                                            cv::CALIB_FIX_INTRINSIC),
                  std::runtime_error("Calibration of camera external parameters failed."));
    if (isnan(mse))
    {
        LOG_WARNING("Stereo camera calibration failed. Mean squared error is NaN.");
        return std::nullopt;
    }
    Eigen::MatrixX<real_t> rot, trans;
    cv::cv2eigen(R, rot);
    cv::cv2eigen(t, trans);
    Eigen::MatrixX<real_t> maybeTransform = makeTransformationMatrix(rot, trans);

    T = maybeTransform;

    return mse;
}

Eigen::MatrixX<real_t> generate3dPatternPoints(std::size_t p_w, std::size_t p_h, real_t sq_size)
{
    Eigen::MatrixX<real_t> points = Eigen::MatrixX<real_t>(p_w * p_h,
                                             3);
    for (size_t j = 0; j < p_h; j++)
    {
        for (size_t i = 0; i < p_w; i++)
        {
            points(j * p_w + i, 0) = i * sq_size;
            points(j * p_w + i, 1) = j * sq_size;
            points(j * p_w + i, 2) = 0.0;
        }
    }
    return points;
}

bool estimateRelativePatternTransform(const Eigen::MatrixX<real_t>& p1_points_2d,
                                      const Eigen::MatrixX<real_t>& p1_points_3d,
                                      const Eigen::MatrixX<real_t>& p2_points_2d,
                                      const Eigen::MatrixX<real_t>& p2_points_3d,
                                      const Eigen::Matrix3<real_t>& intrinsics,
                                      const Eigen::VectorX<real_t>& distortion,
                                      Eigen::Matrix4<real_t>& T)
{
    Eigen::Matrix4<real_t> transform_p1, transform_p1i;
    std::optional<real_t> mse1 = calibrateExtrinsics(p1_points_2d, p1_points_3d, intrinsics, distortion, transform_p1);
    if (!mse1.has_value())
    {
        return false;
    }
    Eigen::Matrix4<real_t> transform_p2, transform_p2i;
    std::optional<real_t> mse2 = calibrateExtrinsics(p2_points_2d, p2_points_3d, intrinsics, distortion, transform_p2);
    if (!mse2.has_value())
    {
        return false;
    }

    transform_p1i = transform_p1.inverse();
    transform_p2i = transform_p2.inverse();

    cv::Mat t1_cv, t2_cv;
    cv::eigen2cv(transform_p1i, t1_cv);
    cv::eigen2cv(transform_p2i, t2_cv);
    cv::Mat t2t1_cv = t2_cv * t1_cv.inv();
    cv::cv2eigen(t2t1_cv,
                 T);

    return true;
}

Eigen::Vector2<real_t> projectPointOnImagePlane(const Eigen::VectorX<real_t>& point3d, const Eigen::Matrix3<real_t>& intrinsics, const Eigen::VectorX<real_t>& distortion,
                                         const Eigen::Matrix4<real_t>& T)
{
    Eigen::Vector4<real_t> p_h;
    p_h(0) = point3d[0];
    p_h(1) = point3d[1];
    p_h(2) = point3d[2];
    p_h(3) = 1.;

    cv::Mat pCV, tCV;
    cv::eigen2cv(p_h, pCV);
    cv::eigen2cv(T, tCV);
    cv::Mat pTransfCV = tCV * pCV;
    cv::cv2eigen(pTransfCV, p_h);

    real_t vx = p_h(0);
    real_t vy = p_h(1);
    real_t vz = p_h(2);

    real_t k1 = distortion(0);
    real_t k2 = distortion(1);
    real_t k3 = distortion(4);
    real_t p1 = distortion(2);
    real_t p2 = distortion(3);

    real_t fx = intrinsics(0, 0);
    real_t fy = intrinsics(1, 1);
    real_t cx = intrinsics(0, 2);
    real_t cy = intrinsics(1, 2);

    // undistort in camera space
    real_t x = vx / vz;
    real_t y = vy / vz;
    real_t xx = x * x;
    real_t xy = x * y;
    real_t yy = y * y;
    real_t r2 = xx + yy;
    real_t r4 = r2 * r2;
    real_t r6 = r4 * r2;
    real_t radial = (real_t(1) + k1 * r2 + k2 * r4 + k3 * r6);
    real_t tangentialX = p1 * (r2 + real_t(2) * xx) + real_t(2) * p2 * xy;
    real_t tangentialY = p2 * (r2 + real_t(2) * yy) + real_t(2) * p1 * xy;
    real_t xdash = x * radial + tangentialX;
    real_t ydash = y * radial + tangentialY;

    Eigen::Vector2<real_t> img_p;
    img_p[0] = fx * xdash + cx;
    img_p[1] = fy * ydash + cy;

    return img_p;
}

void projectPointsOnImagePlane(const Eigen::MatrixX<real_t>& points3d,
                               const Eigen::Matrix3<real_t>& intrinsics,
                               const Eigen::VectorX<real_t>& distortion,
                               const Eigen::Matrix4<real_t>& T,
                               Eigen::MatrixX<real_t>& outPoints2d)
{
    Eigen::Vector3<real_t> point3d;
    Eigen::Vector2<real_t> point2d;
    CARBON_ASSERT((distortion.rows() == 5) && (distortion.cols() == 1), "Given camera distortion parameters is not of correct shape.");
    CARBON_ASSERT(points3d.cols() == 3, "Input points are not of correct shape.");

    outPoints2d = Eigen::MatrixX<real_t>(points3d.rows(), 2);

    for (int i = 0; i < points3d.rows(); i++)
    {
        point3d[0] = points3d(i, 0);
        point3d[1] = points3d(i, 1);
        point3d[2] = points3d(i, 2);

        point2d = projectPointOnImagePlane(point3d, intrinsics, distortion, T);
        outPoints2d(i, 0) = point2d(0);
        outPoints2d(i, 1) = point2d(1);
    }
}


Eigen::MatrixX<real_t> convertPointVectorX2Matrix(const std::vector<cv::Point2f>& points)
{
    Eigen::MatrixX<real_t> output = Eigen::MatrixX<real_t>(points.size(), 2);
    for (size_t i = 0; i < points.size(); i++)
    {
        output(i, 0) = points[i].x;
        output(i, 1) = points[i].y;
    }

    return output;
}

void maskDetectedPattern(cv::Mat& inputOutput, const std::vector<cv::Point2f>& corner_points)
{
    CARBON_ASSERT(!inputOutput.empty() && (corner_points.size() == 4), "Input arguments are invalid.");

    std::size_t width = inputOutput.cols;
    std::size_t height = inputOutput.rows;
    cv::Mat black = cv::Mat::zeros((int)height, (int)width, inputOutput.type());
    std::vector<std::vector<cv::Point>> contour_coords(1);
    contour_coords[0].resize(corner_points.size());
    contour_coords[0][0] = (cv::Point)corner_points[0];
    contour_coords[0][1] = (cv::Point)corner_points[1];
    contour_coords[0][2] = (cv::Point)corner_points[2];
    contour_coords[0][3] = (cv::Point)corner_points[3];
    cv::Mat mask10((int)height, (int)width, CV_8U, cv::Scalar(0));
    cv::Mat mask0((int)height, (int)width, CV_8U, cv::Scalar(0));
    drawContours(mask10, contour_coords, 0, cv::Scalar(255), cv::FILLED);
    bitwise_not(mask10, mask0);
    cv::Mat excluded = inputOutput.clone();
    black.copyTo(excluded, mask10);
    inputOutput = excluded;
}

cv::Point2f getNitroPoint2D(int l, const Eigen::MatrixX<real_t>& m) { return cv::Point2f((float)m(l, 0), (float)m(l, 1)); }

cv::Point3f getNitroPoint3D(int l, const Eigen::MatrixX<real_t>& m) { return cv::Point3f((float)m(l, 0), (float)m(l, 1), (float)m(l, 2)); }

std::vector<cv::Point3f> nitroToCvPoints3(const Eigen::MatrixX<real_t>& m)
{
    std::vector<cv::Point3f> points;
    for (int i = 0; i < m.rows(); i++)
    {
        points.push_back(getNitroPoint3D(i, m));
    }
    return points;
}

std::vector<cv::Point2f> nitroToCvPoints2(const Eigen::MatrixX<real_t>& m)
{
    std::vector<cv::Point2f> points;
    for (int i = 0; i < m.rows(); i++)
    {
        points.push_back(getNitroPoint2D(i, m));
    }
    return points;
}

std::vector<cv::Point2f> nitroToCvPoints2const(const Eigen::MatrixX<real_t>& m)
{
    std::vector<cv::Point2f> points;
    for (int i = 0; i < m.rows(); i++)
    {
        points.push_back(getNitroPoint2D(i, m));
    }
    return points;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
