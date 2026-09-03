// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphEntryAssetDefinitions.h"

#define LOCTEXT_NAMESPACE "AnimNextAnimGraphEntryAssetDefinitions"

FText UAssetDefinition_AnimNextAnimationGraphEntry::GetObjectDisplayNameText(UObject* Object) const
{
	UAnimNextRigVMAssetEntry* Parameter = CastChecked<UAnimNextRigVMAssetEntry>(Object);
	return Parameter->GetDisplayName();
}

#undef LOCTEXT_NAMESPACE