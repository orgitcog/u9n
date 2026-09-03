// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Polyline.h"

namespace UE
{
namespace Geometry
{

template<typename T>
using TPolyline2 = TPolyline<T, 2>;

typedef TPolyline2<double> FPolyline2d;
typedef TPolyline2<float> FPolyline2f;

} // end namespace UE::Geometry
} // end namespace UE
