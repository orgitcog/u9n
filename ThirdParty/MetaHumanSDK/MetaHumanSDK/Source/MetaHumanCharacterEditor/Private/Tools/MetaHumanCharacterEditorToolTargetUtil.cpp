// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"

#include "MetaHumanCharacterEditorActorInterface.h"
#include "ModelingToolTargetUtil.h"

#include "GameFramework/Actor.h"

namespace UE::ToolTarget
{

UMetaHumanCharacter* GetTargetMetaHumanCharacter(UToolTarget* InTarget)
{
	if (AActor* Actor = UE::ToolTarget::GetTargetActor(InTarget))
	{
		if (Actor->Implements<UMetaHumanCharacterEditorActorInterface>())
		{
			return Cast<IMetaHumanCharacterEditorActorInterface>(Actor)->GetCharacter();
		}
	}

	return nullptr;
}

} // namespace UE::ToolTarget
