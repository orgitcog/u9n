// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

class UObject;
class USceneComponent;
class UCaptureData;

namespace MetaHumanCaptureDataUtils
{
	METAHUMANCAPTUREDATAEDITOR_API USceneComponent* CreatePreviewComponent(UCaptureData* InCaptureData, UObject* InObject);
}