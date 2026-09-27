// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UObject/UObjectGlobals.h"

namespace UE::AnimNext::ControlRig
{

class ANIMNEXTCONTROLRIG_API FAnimNextControlRigModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	using FReplacementObjectMap = FCoreUObjectDelegates::FReplacementObjectMap;
	DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnObjectsReinstanced, const FReplacementObjectMap&);
	static FOnObjectsReinstanced OnObjectsReinstanced;

	FDelegateHandle OnObjectsReinstancedHandle;
#endif
};

} // end namespace
