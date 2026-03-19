// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDExtractedGeometryDataHandle.h"

#include "Chaos/ImplicitObject.h"

void FChaosVDExtractedGeometryDataHandle::SetGeometryKey(const uint32 Key)
{
	GeometryKey = Key;
}

FName FChaosVDExtractedGeometryDataHandle::GetTypeName() const
{
	using namespace Chaos;

	static FName InvalidName = TEXT("Invalid");

	return ImplicitObject ? GetImplicitObjectTypeName(GetInnerType(ImplicitObject->GetType())) : InvalidName;
}
