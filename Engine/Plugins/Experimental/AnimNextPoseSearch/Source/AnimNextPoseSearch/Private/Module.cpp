// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Module/AnimNextModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "RigVMCore/RigVMRegistry.h"

#include "AlphaBlend.h"
#include "AnimNextInteractionIslandDependency.h"
#include "Features/IModularFeatures.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchInteractionAsset.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Animation/TrajectoryTypes.h"

namespace UE::AnimNext::PoseSearch
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UPoseSearchDatabase::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UPoseSearchSchema::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UPoseSearchInteractionAsset::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UMultiAnimAsset::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class }
		};

		static UScriptStruct* const AllowedStructTypes[] =
		{
			FTransformTrajectorySample::StaticStruct(),
			FTransformTrajectory::StaticStruct(),
			FPoseSearchBlueprintResult::StaticStruct(),
			FAlphaBlendArgs::StaticStruct(),
			FPoseSearchInteractionAssetItem::StaticStruct(),
			FPoseSearchInteractionAvailability::StaticStruct(),
			FPoseHistoryReference::StaticStruct(),
		};

		FRigVMRegistry& RigVMRegistry = FRigVMRegistry::Get();
		RigVMRegistry.RegisterObjectTypes(AllowedObjectTypes);
		RigVMRegistry.RegisterStructTypes(AllowedStructTypes);

		IModularFeatures::Get().RegisterModularFeature(UE::PoseSearch::IInteractionIslandDependency::FeatureName, &UE::PoseSearch::FAnimNextInteractionIslandDependency::ModularFeature);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(UE::PoseSearch::IInteractionIslandDependency::FeatureName, &UE::PoseSearch::FAnimNextInteractionIslandDependency::ModularFeature);
	}
};

}

IMPLEMENT_MODULE(UE::AnimNext::PoseSearch::FModule, AnimNextPoseSearch)
