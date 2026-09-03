// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/EyeFittingHelper.h>
#include <nls/geometry/GeometryHelpers.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
T DistanceToEllipse(T x, T y, T a, T b, T c, T d, T e) { return T(-1) + a * x + b * y + c * x * y + d * x * x + e * y * y; }


template <class T>
void CalculateEllipses(const Camera<T>& camera,
                       const Eigen::Matrix2X<T>& lowerLid,
                       const Eigen::Matrix2X<T>& upperLid,
                       const Eigen::Matrix2X<T>& iris,
                       Eigen::Vector2<T>& dataCenter,
                       Eigen::VectorX<T>& lidEllipseParameters,
                       Eigen::VectorX<T>& irisEllipseParameters)
{
    dataCenter = Eigen::Vector2<T>(T(camera.Width()) / T(2), T(camera.Height()) / T(2));
    Eigen::Matrix2X<T> lidsCombined = Eigen::Matrix2X<T>(2, lowerLid.cols() + upperLid.cols());
    for (int i = 0; i < lowerLid.cols(); ++i)
    {
        lidsCombined.col(i) = lowerLid.col(i);
    }
    for (int i = 0; i < upperLid.cols(); ++i)
    {
        lidsCombined.col(lowerLid.cols() + i) = upperLid.col(i);
    }

    Eigen::Matrix2<T> linearDataTransform = Eigen::Matrix2<T>::Identity();
    linearDataTransform *= T(1) / T(camera.Width());
    Affine<T, 2, 2> dataTransform;

    dataTransform.SetLinear(linearDataTransform);
    dataTransform.SetTranslation(-linearDataTransform * dataCenter);

    const auto irisPointsTransformed = dataTransform.Transform(iris);
    const auto lidPointsTransformed = dataTransform.Transform(lidsCombined);

    lidEllipseParameters = geoutils::FitEllipse(lidPointsTransformed);
    irisEllipseParameters = geoutils::FitEllipse(irisPointsTransformed);
}

template <class T>
const std::vector<int> EyeFittingHelper<T>::CalculateIrisInliers(const Eigen::Matrix2X<T>& lowerLid,
                                                                 const Eigen::Matrix2X<T>& upperLid,
                                                                 const Eigen::Matrix2X<T>& iris,
                                                                 const Camera<T>& camera)
{
    std::vector<int> inliers;

    Eigen::Vector2<T> dataCenter;
    Eigen::VectorX<T> lidEllipseParameters, irisEllipseParameters;

    CalculateEllipses<T>(camera, lowerLid, upperLid, iris, dataCenter, lidEllipseParameters, irisEllipseParameters);

    for (int i = 0; i < iris.cols(); ++i)
    {
        Eigen::Vector2<T> irisPoint = iris.col(i);
        const T x = (irisPoint(0) - dataCenter(0)) / T(camera.Width());
        const T y = (irisPoint(1) - dataCenter(1)) / T(camera.Width());

        const T distanceToLids = DistanceToEllipse<T>(x,
                                                      y,
                                                      lidEllipseParameters(0),
                                                      lidEllipseParameters(1),
                                                      lidEllipseParameters(2),
                                                      lidEllipseParameters(3),
                                                      lidEllipseParameters(4));
        bool isInsideLids = distanceToLids > 0 ? true : false;
        if (isInsideLids)
        {
            inliers.push_back(i);
        }
    }

    return inliers;
}

template <class T>
bool EyeFittingHelper<T>::UpdateScanMaskBasedOnLandmarks(const Eigen::Matrix2X<T>& lowerLid,
                                                         const Eigen::Matrix2X<T>& upperLid,
                                                         const Eigen::Matrix2X<T>& iris,
                                                         const Camera<T>& camera,
                                                         const std::shared_ptr<const Mesh<T>>& mesh,
                                                         Eigen::VectorX<T>& mask)
{
    CARBON_ASSERT((int)mask.size() == (int)mesh->NumVertices(), "Input mask size does not match number of scan vertices.");

    Eigen::Vector2<T> dataCenter;
    Eigen::VectorX<T> lidEllipseParameters, irisEllipseParameters;

    CalculateEllipses<T>(camera, lowerLid, upperLid, iris, dataCenter, lidEllipseParameters, irisEllipseParameters);
    int inliersCount = 0;

    for (int i = 0; i < mesh->NumVertices(); ++i)
    {
        Eigen::Vector2<T> projection = camera.Project(Eigen::Vector3<T>(mesh->Vertices().col(i)), true);
        const T x = (projection(0) - dataCenter(0)) / T(camera.Width());
        const T y = (projection(1) - dataCenter(1)) / T(camera.Width());

        const T distanceToLids = DistanceToEllipse<T>(x,
                                                      y,
                                                      lidEllipseParameters(0),
                                                      lidEllipseParameters(1),
                                                      lidEllipseParameters(2),
                                                      lidEllipseParameters(3),
                                                      lidEllipseParameters(4));
        const T distanceToIris =
            DistanceToEllipse<T>(x,
                                 y,
                                 irisEllipseParameters(0),
                                 irisEllipseParameters(1),
                                 irisEllipseParameters(2),
                                 irisEllipseParameters(3),
                                 irisEllipseParameters(4));

        bool isInsideIris = distanceToIris > 0 ? true : false;
        bool isInsideLids = distanceToLids > 0 ? true : false;

        if (!isInsideIris && isInsideLids)
        {
            mask(i) = T(1);
            inliersCount++;
        }
    }

    return inliersCount > 0 ? true : false;
}

// explicitly instantiate the mask generator classes
template class EyeFittingHelper<float>;
template class EyeFittingHelper<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
