// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VectorVM.h"
#include "Math/VectorRegister.h"

struct FVVM_VUI4
{
	union
	{
		VectorRegister4i v;
		uint32 u4[4];
		int32 i4[4];
	};
};



