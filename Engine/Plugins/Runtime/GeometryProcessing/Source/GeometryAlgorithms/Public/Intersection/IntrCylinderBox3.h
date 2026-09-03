// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoxTypes.h"

namespace UE
{
namespace Geometry
{

/**
 * Compute intersection between a cylinder and a 3D axis-aligned box.
 */
template <typename RealType>
bool DoesCylinderIntersectBox(
	const TAxisAlignedBox3<RealType>& Box,
	const TVector<RealType>& CylinderCenter,
	const TVector<RealType>& CylinderDirection,
	RealType CylinderRadius,
	RealType CylinderHeight);

} // end namespace UE::Geometry
} // end namespace UE