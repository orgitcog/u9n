// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/ModuleGuard.h"
#include "AnimNextPool.h"
#include "Module/AnimNextModuleInstance.h"

namespace UE::AnimNext
{

FModuleWriteGuard::FModuleWriteGuard(const FAnimNextModuleInstance* InModuleInstance)
	: ModuleInstance(InModuleInstance)
{
#if ENABLE_MT_DETECTOR
	if(ModuleInstance)
	{
		ModuleInstance->AccessDetector.AcquireWriteAccess();

		if(ModuleInstance->Pool)
		{
			// Cache the handles we acquire for later release
			PrerequisiteHandles.Reserve(ModuleInstance->PrerequisiteRefs.Num());
			for(const FAnimNextModuleInstance::FPrerequisiteReference& PrerequisiteRef : ModuleInstance->PrerequisiteRefs)
			{
				PrerequisiteHandles.Add(PrerequisiteRef.Handle);

				FAnimNextModuleInstance* OtherModuleInstance = ModuleInstance->Pool->TryGet(PrerequisiteRef.Handle);
				ensureAlways(OtherModuleInstance != nullptr);
				if(OtherModuleInstance == nullptr)
				{
					continue;
				}

				OtherModuleInstance->AccessDetector.AcquireWriteAccess();
			}
		}
	}
#endif
}

FModuleWriteGuard::~FModuleWriteGuard()
{
#if ENABLE_MT_DETECTOR
	if(ModuleInstance)
	{
		if(ModuleInstance->Pool)
		{
			// Release only the handles we acquired as we may have change prerequisites in this scope
			for(const FModuleHandle& PrerequisiteHandle : PrerequisiteHandles)
			{
				FAnimNextModuleInstance* OtherModuleInstance = ModuleInstance->Pool->TryGet(PrerequisiteHandle);
				ensureAlways(OtherModuleInstance != nullptr);
				if(OtherModuleInstance == nullptr)
				{
					continue;
				}

				OtherModuleInstance->AccessDetector.ReleaseWriteAccess();
			}
		}

		ModuleInstance->AccessDetector.ReleaseWriteAccess();
	}
#endif
}

}
