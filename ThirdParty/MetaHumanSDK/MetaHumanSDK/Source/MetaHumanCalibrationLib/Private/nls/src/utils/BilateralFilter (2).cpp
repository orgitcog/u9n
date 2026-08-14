// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/utils/BilateralFilter.h>
#include <numeric>
#include <algorithm>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

std::vector<Eigen::VectorXf> BilateralFilter::IndependentFilter(const std::vector<Eigen::VectorXf>& InUnfiltered,
                                                                const float& InDomainStdDev,
                                                                const float& InRangeStdDev)
{
    std::vector<Eigen::VectorXf> OutFiltered;

    long long num_dimensions = 0ull;
    if (!InUnfiltered.empty())
    {
        num_dimensions = InUnfiltered.begin()->size();
    }
    OutFiltered.assign(InUnfiltered.size(), Eigen::VectorXf(num_dimensions));

    auto half_width = static_cast<long long>(std::max(std::round(2.0f * InDomainStdDev), 1.0f));


    auto gaussian_filter = CreateGaussianFilter(half_width, InDomainStdDev);

    long long offset = -half_width;
    for (size_t i = 0; i < InUnfiltered.size(); ++i, offset++)
    {
        for (long long k = 0; k < num_dimensions; ++k)
        {
            float sum1 = 0.0f;
            float sum2 = 0.0f;
            for (long long j = 0; j < static_cast<long long>(gaussian_filter.size()); ++j)
            {
                const auto index = std::min(std::max(offset + j, 0ll), static_cast<long long>(InUnfiltered.size()) - 1ll);
                const auto dx = InUnfiltered[index][k] - InUnfiltered[i][k];
                auto gx = std::exp(-(dx * dx) / (2.0f * InRangeStdDev * InRangeStdDev));

                sum1 += gx * gaussian_filter[j];
                sum2 += InUnfiltered[index][k] * gx * gaussian_filter[j];
            }
            OutFiltered[i][k] = sum2 / sum1;
        }
    }

    return OutFiltered;
}

std::vector<Eigen::VectorXf> BilateralFilter::CorrelatedFilter(const std::vector<Eigen::VectorXf>& InUnfiltered,
                                                               const float& InDomainStdDev,
                                                               const float& InRangeStdDev)
{
    std::vector<Eigen::VectorXf> OutFiltered;

    long long num_dimensions = 0ull;
    if (!InUnfiltered.empty())
    {
        num_dimensions = InUnfiltered.begin()->size();
    }
    OutFiltered.assign(InUnfiltered.size(), Eigen::VectorXf(num_dimensions));

    auto half_width = static_cast<long long>(std::max(std::round(2.0f * InDomainStdDev), 1.0f));


    auto gaussian_filter = CreateGaussianFilter(half_width, InDomainStdDev);

    long long offset = -half_width;
    for (size_t i = 0; i < InUnfiltered.size(); ++i, offset++)
    {
        float sum = 0.0f;
        Eigen::VectorXf dimension_sums(num_dimensions);
        dimension_sums.setZero();
        for (long long j = 0; j < static_cast<long long>(gaussian_filter.size()); ++j)
        {
            const auto index = std::min(std::max(offset + j, 0ll), static_cast<long long>(InUnfiltered.size()) - 1ll);
            const auto gx = std::exp(-(InUnfiltered[index] - InUnfiltered[i]).squaredNorm() / (2.0f * InRangeStdDev * InRangeStdDev));

            sum += gx * gaussian_filter[j];
            dimension_sums += InUnfiltered[index] * gx * gaussian_filter[j];
        }
        OutFiltered[i] = dimension_sums / sum;
    }

    return OutFiltered;
}

std::vector<float> BilateralFilter::CreateGaussianFilter(const long long& InHalfWidth, const float& InDomainStdDev)
{
    std::vector<float> OutGaussianFilter(2ll * InHalfWidth + 1ll);
    std::iota(OutGaussianFilter.begin(), OutGaussianFilter.end(), -static_cast<float>(InHalfWidth));
    float sum = 0.0;
    for (auto& elem : OutGaussianFilter)
    {
        elem = std::exp(-(elem * elem) / (2.0f * InDomainStdDev * InDomainStdDev));
        sum += elem;
    }
    for (auto& elem : OutGaussianFilter)
    {
        elem /= sum;
    }
    return OutGaussianFilter;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
