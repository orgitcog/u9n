// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

// Generic axis enum (mirrored for property use in Object.h)
namespace EAxis
{
	enum Type
	{
		None,
		X,
		Y,
		Z,
	};
}


// Extended axis enum for more specialized usage
namespace EAxisList
{
	enum Type
	{
		None		= 0,
		X			= 1 << 0,
		Y			= 1 << 1,
		Z			= 1 << 2,

		Screen		= 1 << 3,
		XY			= X | Y,
		XZ			= X | Z,
		YZ			= Y | Z,
		XYZ			= X | Y | Z,
		All			= XYZ | Screen,

		//alias over Axis YZ since it isn't used when the z-rotation widget is being used
		ZRotation	= YZ,
		
		// alias over Screen since it isn't used when the 2d translate rotate widget is being used
		Rotate2D	= Screen,

		Left			= 1 << 4,
		Up				= 1 << 5,
		Forward			= 1 << 6,

		LU = Left | Up,
		LF = Left | Forward,
		UF = Up | Forward,
		LeftUpForward	= Left | Up | Forward,
	};
}
