// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Module/AnimNextModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Animation/AnimSequence.h"
#include "Chooser.h"

namespace UE::AnimNext::Chooser
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UChooserTable::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		};

		FRigVMRegistry::Get().RegisterObjectTypes(AllowedObjectTypes);
	}

	virtual void ShutdownModule() override
	{
	}

};

}

IMPLEMENT_MODULE(UE::AnimNext::Chooser::FModule, AnimNextChooser)
