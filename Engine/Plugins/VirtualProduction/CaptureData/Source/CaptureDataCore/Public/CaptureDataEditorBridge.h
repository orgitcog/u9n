// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UObject;

namespace UE::CaptureManager
{

class ICaptureDataEditorBridge : public IModuleInterface
{
public:
	virtual void DeferMarkDirty(TWeakObjectPtr<UObject> InObject) = 0;
};

} // namespace UE::CaptureManager

