// Copyright Epic Games, Inc. All Rights Reserved.

#include "Entries/AnimNextRigVMAssetEntry.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "AnimNextScopedCompilerResults.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "AnimNextRigVMAssetEntry"

void UAnimNextRigVMAssetEntry::Initialize(UAnimNextRigVMAssetEditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextRigVMAssetEntry::HandleRigVMGraphModifiedEvent);
}

bool UAnimNextRigVMAssetEntry::IsAsset() const
{
	// Entries are considered assets to allow using the asset logic for save dialogs, etc.
	// Also, they return true even if pending kill, in order to show up as deleted in these dialogs.
	return IsPackageExternal() && !GetPackage()->HasAnyFlags(RF_Transient) && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject);
}

#if WITH_EDITOR

void UAnimNextRigVMAssetEntry::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	BroadcastModified(EAnimNextEditorDataNotifType::PropertyChanged);
}

void UAnimNextRigVMAssetEntry::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		BroadcastModified(EAnimNextEditorDataNotifType::UndoRedo);
	}
}

#endif

void UAnimNextRigVMAssetEntry::BroadcastModified(EAnimNextEditorDataNotifType InType)
{
	if(UAnimNextRigVMAssetEditorData* EditorData = Cast<UAnimNextRigVMAssetEditorData>(GetOuter()))
	{
		// Modifications here can trigger compilation, so add a scope to catch compile batches
		UE::AnimNext::UncookedOnly::FScopedCompilerResults CompilerResults(LOCTEXT("ModifiedAssetsJobName", "Modified Assets"));

		EditorData->BroadcastModified(InType, this);
	}
}

#undef LOCTEXT_NAMESPACE