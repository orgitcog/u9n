// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/ImageConstraints.h>

#include <nls/functions/PointSurfaceConstraintFunction.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct ImageConstraints<T>::Private
{
    //! Image constraints data per camera.
    std::map<std::string, std::shared_ptr<const ImageConstraintsData<T>>> imageConstraintsData;

    //! Weight for flow. By default flow is disabled.
    T weight = T(0.0);
};


template <class T>
ImageConstraints<T>::ImageConstraints() : m(new Private)
{}

template <class T> ImageConstraints<T>::~ImageConstraints() = default;
template <class T> ImageConstraints<T>::ImageConstraints(ImageConstraints&& other) = default;
template <class T> ImageConstraints<T>& ImageConstraints<T>::operator=(ImageConstraints&& other) = default;

template <class T>
void ImageConstraints<T>::SetWeight(T weight) { m->weight = weight; }

template <class T>
void ImageConstraints<T>::SetImageConstraintsData(const std::map<std::string, std::shared_ptr<const ImageConstraintsData<T>>>& imageConstraintsData)
{
    m->imageConstraintsData = imageConstraintsData;
}

template <class T>
Cost<T> ImageConstraints<T>::Evaluate(const DiffDataMatrix<T, 3, -1>& vertices,
                                      std::map<std::string, std::shared_ptr<const ImageConstraintsData<T>>>* debugImageConstraints)
{
    Cost<T> cost;

    if (m->weight > 0)
    {
        for (auto&& [cameraName, imageConstraintsDataPtr] : m->imageConstraintsData)
        {
            const auto& imageConstraintsData = *imageConstraintsDataPtr;
            DiffDataMatrix<T, 2, -1> projectedVertices = imageConstraintsData.camera.Project(vertices, /*withExtrinsics=*/true);
            DiffData<T> residual = PointSurfaceConstraintFunction<T, 2>::Evaluate(projectedVertices,
                                                                                  imageConstraintsData.vertexIndices,
                                                                                  imageConstraintsData.targetPositions,
                                                                                  imageConstraintsData.targetNormals,
                                                                                  imageConstraintsData.weights,
                                                                                  m->weight);
            cost.Add(std::move(residual), T(1), cameraName + "_imageConstraint");
        }
    }

    if (debugImageConstraints)
    {
        (*debugImageConstraints) = m->imageConstraintsData;
    }

    return cost;
}

template <class T>
void ImageConstraints<T>::SetupImageConstraints(const Eigen::Transform<T, 3, Eigen::Affine>& rigidTransform,
                                                const Eigen::Matrix<T, 3, -1>& vertices,
                                                VertexConstraints<T, 1, 1>& imageConstraints) const
{
    if (m->weight > 0)
    {
        int numTotalConstraints = 0;
        for (auto&& [cameraName, imageConstraintsData] : m->imageConstraintsData)
        {
            numTotalConstraints += int(imageConstraintsData->vertexIndices.size());
        }

        imageConstraints.ResizeToFitAdditionalConstraints(numTotalConstraints);

        for (auto&& [cameraName, imageConstraintsData] : m->imageConstraintsData)
        {
            const Eigen::Matrix<T, 3, 3> K = imageConstraintsData->camera.Intrinsics();
            const Eigen::Matrix<T, 4, 4> totalTransform = imageConstraintsData->camera.Extrinsics().Matrix() * rigidTransform.matrix();
            const Eigen::Matrix<T, 3, 3> KR = K * totalTransform.block(0, 0, 3, 3);
            const Eigen::Vector<T, 3> Kt = K * totalTransform.block(0, 3, 3, 1);

            for (int i = 0; i < int(imageConstraintsData->vertexIndices.size()); ++i)
            {
                const T weight = imageConstraintsData->weights[i] * sqrt(m->weight);
                if (weight > 0)
                {
                    const int vID = imageConstraintsData->vertexIndices[i];
                    const Eigen::Vector2<T> targetPixelPosition = imageConstraintsData->targetPositions.col(i);
                    const Eigen::Vector2<T> targetNormal = imageConstraintsData->targetNormals.col(i);
                    const Eigen::Vector3<T> pix = KR * vertices.col(vID) + Kt;
                    const T x = pix[0];
                    const T y = pix[1];
                    const T z = pix[2];
                    const T invZ = T(1) / z;
                    const T residual = weight * targetNormal.dot(invZ * pix.template head<2>() - targetPixelPosition);
                    const T nx = targetNormal[0];
                    const T ny = targetNormal[1];
                    Eigen::Matrix<T, 1, 3> drdV;
                    drdV(0, 0) = weight * invZ * (nx * (KR(0, 0) - (x * invZ) * KR(2, 0)) + ny * (KR(1, 0) - (y * invZ) * KR(2, 0)));
                    drdV(0, 1) = weight * invZ * (nx * (KR(0, 1) - (x * invZ) * KR(2, 1)) + ny * (KR(1, 1) - (y * invZ) * KR(2, 1)));
                    drdV(0, 2) = weight * invZ * (nx * (KR(0, 2) - (x * invZ) * KR(2, 2)) + ny * (KR(1, 2) - (y * invZ) * KR(2, 2)));
                    imageConstraints.AddConstraint(vID, residual, drdV);
                }
            }
        }
    }
}

// explicitly instantiate the FlowConstraints classes
template class ImageConstraints<float>;
template class ImageConstraints<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
