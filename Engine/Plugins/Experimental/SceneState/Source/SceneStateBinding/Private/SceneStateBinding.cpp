// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBinding.h"

FConstStructView FSceneStateBinding::GetSourceDataHandleStruct() const
{
	return FConstStructView::Make(SourceDataHandle);
}
