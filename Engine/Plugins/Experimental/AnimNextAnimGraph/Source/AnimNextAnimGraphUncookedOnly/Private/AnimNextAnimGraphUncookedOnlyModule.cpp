// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Graph/RigVMTrait_AnimNextPublicVariablesUncookedOnly.h"

#define LOCTEXT_NAMESPACE "FAnimNextAnimGraphUncookedOnlyModule"

namespace UE::AnimNext::AnimGraph::UncookedOnly
{

class FAnimNextAnimGraphUncookedOnlyModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		using namespace UE::AnimNext::UncookedOnly;

		FPublicVariablesImpl::Register();
	}
};

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::AnimNext::AnimGraph::UncookedOnly::FAnimNextAnimGraphUncookedOnlyModule, AnimNextAnimGraphUncookedOnly);
