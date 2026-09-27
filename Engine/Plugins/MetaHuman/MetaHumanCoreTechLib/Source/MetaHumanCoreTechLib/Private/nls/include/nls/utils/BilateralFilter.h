// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include <vector>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class BilateralFilter
{
public:
    BilateralFilter() = delete;
    static std::vector<Eigen::VectorXf> CorrelatedFilter(const std::vector<Eigen::VectorXf>& InUnfilteredControls,
                                                         const float& InDomainStdDev,
                                                         const float& InRangeStdDev);
    static std::vector<Eigen::VectorXf> IndependentFilter(const std::vector<Eigen::VectorXf>& InUnfilteredControls,
                                                          const float& InDomainStdDev,
                                                          const float& InRangeStdDev);

private:
    static std::vector<float> CreateGaussianFilter(const long long& half_width, const float& InDomainStdDev);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
