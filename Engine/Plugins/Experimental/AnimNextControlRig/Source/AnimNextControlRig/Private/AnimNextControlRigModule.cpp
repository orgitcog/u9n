// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextControlRigModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace UE::AnimNext::ControlRig
{

#if WITH_EDITOR
FAnimNextControlRigModule::FOnObjectsReinstanced FAnimNextControlRigModule::OnObjectsReinstanced;
#endif

void FAnimNextControlRigModule::StartupModule()
{
#if WITH_EDITOR
	// Register thread safe delegates, which allows objects to safely register to these delegates which may be called from another thread.
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddLambda([this](const FReplacementObjectMap& ObjectMap)
		{
			if (OnObjectsReinstanced.IsBound())
			{
				OnObjectsReinstanced.Broadcast(ObjectMap);
			}
		});
#endif
}

void FAnimNextControlRigModule::ShutdownModule()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
#endif
}

} // end namespace

IMPLEMENT_MODULE(UE::AnimNext::ControlRig::FAnimNextControlRigModule, AnimNextControlRig)
