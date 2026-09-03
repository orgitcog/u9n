// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorBodyEditingTools.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "Async/Async.h"
#include "Editor/EditorEngine.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "ToolTargetManager.h"
#include "Misc/ScopedSlowTask.h"
#include "DNAUtils.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

/**
* Body Tool Command change for undo/redo transactions.
*/
class FMetaHumanCharacterEditorBodyToolCommandChange : public FToolCommandChange
{
public:
	FMetaHumanCharacterEditorBodyToolCommandChange(
		TSharedRef<FMetaHumanCharacterBodyIdentity::FState> InOldState,
		TSharedRef<FMetaHumanCharacterBodyIdentity::FState> InNewState,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldState(InOldState)
		, NewState(InNewState)
		, ToolManager(InToolManager)
	{
	}

	virtual void Apply(UObject* Object) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(Object);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(Character, NewState);
	}

	virtual void Revert(UObject* Object) override
	{
		UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(Object);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(Character, OldState);
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}

private:
	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> OldState;
	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> NewState;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

class FBodyParametricFitDNACommandChange : public FToolCommandChange
{
public:
	FBodyParametricFitDNACommandChange(
		const TArray<uint8>& InOldDNABuffer,
		const TSharedRef<FMetaHumanCharacterBodyIdentity::FState>& InNewState,
		TNotNull<UInteractiveToolManager*> InToolManager)
		: OldDNABuffer{ InOldDNABuffer }
		, NewState{ InNewState }
		, ToolManager{ InToolManager }
	{
	}

	//~Begin FToolCommandChange interface
	virtual void Apply(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);
		UMetaHumanCharacterEditorSubsystem::Get()->RemoveBodyRig(MetaHumanCharacter);
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyState(MetaHumanCharacter, NewState);
	}

	virtual void Revert(UObject* InObject) override
	{
		UMetaHumanCharacter* MetaHumanCharacter = CastChecked<UMetaHumanCharacter>(InObject);

		TArray<uint8> BufferCopy;
		BufferCopy.SetNumUninitialized(OldDNABuffer.Num());
		FMemory::Memcpy(BufferCopy.GetData(), OldDNABuffer.GetData(), OldDNABuffer.Num());
		UMetaHumanCharacterEditorSubsystem::Get()->CommitBodyDNA(MetaHumanCharacter, ReadDNAFromBuffer(&BufferCopy, EDNADataLayer::All).ToSharedRef());
	}

	virtual bool HasExpired(UObject* InObject) const override
	{
		// If the ToolManager is not valid anymore it means the asset editor was closed so mark the transaction as expired
		return !ToolManager.IsValid();
	}
	//~End FToolCommandChange interface

private:
	TArray<uint8> OldDNABuffer;
	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> NewState;
	TWeakObjectPtr<UInteractiveToolManager> ToolManager;
};

UInteractiveTool* UMetaHumanCharacterEditorBodyToolBuilder::BuildTool(const FToolBuilderState& InSceneState) const
{
	UToolTarget* Target = InSceneState.TargetManager->BuildFirstSelectedTargetable(InSceneState, GetTargetRequirements());
	check(Target);

	switch (ToolType)
	{
	case EMetaHumanCharacterBodyEditingTool::Model:
		{
			UMetaHumanCharacterEditorBodyModelTool* BodyModelTool = NewObject<UMetaHumanCharacterEditorBodyModelTool>(InSceneState.ToolManager);
			BodyModelTool->SetTarget(Target);
			return BodyModelTool;
		}
		case EMetaHumanCharacterBodyEditingTool::Blend:
		{
			UMetaHumanCharacterEditorBodyBlendTool* BlendTool = NewObject<UMetaHumanCharacterEditorBodyBlendTool>(InSceneState.ToolManager);
			BlendTool->SetTarget(Target);
			BlendTool->SetWorld(InSceneState.World);
			return BlendTool;
		}
	}

	return nullptr;
}

const FToolTargetTypeRequirements& UMetaHumanCharacterEditorBodyToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(
		TArray<const UClass*>
		{
			UPrimitiveComponentBackedTarget::StaticClass()
		}
	);

	return TypeRequirements;
}

void FMetaHumanCharacterClothVisibilityBase::UpdateClothVisibility(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, EMetaHumanClothingVisibilityState InVisibilityState, bool bUpdateMaterialHiddenFaces /*= true*/)
{
	if (UMetaHumanCharacterEditorSubsystem::IsCharacterOutfitSelected(InMetaHumanCharacter))
	{
		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();

		if (InVisibilityState == EMetaHumanClothingVisibilityState::Shown)
		{
			// Reset the stored preview material
			if (SavedPreviewMaterial.IsSet())
			{
				MetaHumanCharacterSubsystem->UpdateCharacterPreviewMaterial(InMetaHumanCharacter, SavedPreviewMaterial.GetValue());
				SavedPreviewMaterial.Reset();
			}
		}
		else
		{
			// Hide any outfit and revert to clay mode if the character has selected outfits
			SavedPreviewMaterial = InMetaHumanCharacter->PreviewMaterialType;
			MetaHumanCharacterSubsystem->UpdateCharacterPreviewMaterial(InMetaHumanCharacter, EMetaHumanCharacterSkinPreviewMaterial::Clay);
		}

		MetaHumanCharacterSubsystem->SetClothingVisibilityState(InMetaHumanCharacter, InVisibilityState, bUpdateMaterialHiddenFaces);
	}
}

void UMetaHumanCharacterParametricBodyProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* BodyModelTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	BodyModelTool->BodyParameterProperties->OnPostEditChangeProperty(PropertyChangedEvent);
}

bool UMetaHumanCharacterParametricBodyProperties::IsFixedBodyType() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	return MetaHumanCharacter->bFixedBodyType;
}

static void UpdateConstraintItem(const FMetaHumanCharacterBodyConstraint& InBodyConstraint, FMetaHumanCharacterBodyConstraintItemPtr& OutConstraintItem)
{
	OutConstraintItem->Name = InBodyConstraint.Name;
	OutConstraintItem->bIsActive = InBodyConstraint.bIsActive;
	OutConstraintItem->TargetMeasurement = InBodyConstraint.TargetMeasurement;
	OutConstraintItem->ActualMeasurement = InBodyConstraint.TargetMeasurement;
	OutConstraintItem->MinMeasurement = InBodyConstraint.MinMeasurement;
	OutConstraintItem->MaxMeasurement = InBodyConstraint.MaxMeasurement;
}

static TArray<FMetaHumanCharacterBodyConstraintItemPtr> BodyConstraintsToConstraintItems(const TArray<FMetaHumanCharacterBodyConstraint>& InBodyConstraints)
{
	TArray<FMetaHumanCharacterBodyConstraintItemPtr> BodyConstraintItems;
	for (const FMetaHumanCharacterBodyConstraint& BodyConstraint : InBodyConstraints)
	{
		FMetaHumanCharacterBodyConstraintItemPtr BodyConstraintItem = MakeShared<FMetaHumanCharacterBodyConstraintItem>();
		UpdateConstraintItem(BodyConstraint, BodyConstraintItem);

		BodyConstraintItems.Add(BodyConstraintItem);
	}
	return BodyConstraintItems;
}

static TArray<FMetaHumanCharacterBodyConstraint> BodyConstraintItemsToConstraints(const TArray<FMetaHumanCharacterBodyConstraintItemPtr>& InBodyConstraintItems)
{
	TArray<FMetaHumanCharacterBodyConstraint> BodyConstraints;
	for (const FMetaHumanCharacterBodyConstraintItemPtr& BodyConstraintItem : InBodyConstraintItems)
	{
		FMetaHumanCharacterBodyConstraint BodyConstraint;
		BodyConstraint.Name = BodyConstraintItem->Name;
		BodyConstraint.bIsActive = BodyConstraintItem->bIsActive;
		BodyConstraint.TargetMeasurement = BodyConstraintItem->TargetMeasurement;
		BodyConstraints.Add(BodyConstraint);
	}
	return BodyConstraints;
}

void UMetaHumanCharacterParametricBodyProperties::OnBeginConstraintEditing()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	UMaterialInterface* TranslucentMaterial = MetaHumanCharacterSubsystem->GetTranslucentClothingMaterial();

	UpdateClothVisibility(MetaHumanCharacter, EMetaHumanClothingVisibilityState::Hidden);
}

void UMetaHumanCharacterParametricBodyProperties::OnConstraintItemsChanged(bool bInCommitChange)
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>(); 
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	TArray<FMetaHumanCharacterBodyConstraint> BodyConstraints = BodyConstraintItemsToConstraints(BodyConstraintItems);
	MetaHumanCharacterSubsystem->SetBodyConstraints(MetaHumanCharacter, BodyConstraints);

	if (bInCommitChange && PreviousBodyState.IsValid())
	{
		TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);

		UpdateClothVisibility(MetaHumanCharacter, EMetaHumanClothingVisibilityState::Shown, false);

		MetaHumanCharacterSubsystem->CommitBodyState(MetaHumanCharacter, BodyState.ToSharedRef(), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Minimal);

		const FText CommandChangeDescription = LOCTEXT("BodyParametricCommandChange", "Adjust Parametric Body");
	
		// Creates a command change that allows the user to revert back the state
		TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(PreviousBodyState.ToSharedRef(), BodyState.ToSharedRef(), OwnerTool->GetToolManager());
		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
	
		PreviousBodyState = BodyState;
	}
	else
	{
		// Update measurements
		UpdateMeasurements();
	}
}

void UMetaHumanCharacterParametricBodyProperties::ResetConstraints()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	MetaHumanCharacterSubsystem->ResetParametricBody(MetaHumanCharacter);

	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
	MetaHumanCharacterSubsystem->CommitBodyState(MetaHumanCharacter, BodyState.ToSharedRef(), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Minimal);

	if (PreviousBodyState.IsValid())
	{
		const FText CommandChangeDescription = LOCTEXT("BodyParametricResetCommandChange", "Reset Parametric Body");
	
		// Creates a command change that allows the user to revert back the stateZ
		TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(PreviousBodyState.ToSharedRef(), BodyState.ToSharedRef(), OwnerTool->GetToolManager());
		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);

		PreviousBodyState = BodyState;
	}
}

void UMetaHumanCharacterParametricBodyProperties::PerformParametricFit()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	if (MetaHumanCharacter->HasBodyDNA())
	{
		TArray<uint8> OldDNABuffer = MetaHumanCharacter->GetBodyDNABuffer();
		if (MetaHumanCharacterSubsystem->ParametricFitToDnaBody(MetaHumanCharacter))
		{
			// Creates a command change that allows the user to revert back the body dna
			const FText CommandChangeDescription = LOCTEXT("BodyParametricFitDnaCommandChange", "Parametric Fit From Body Dna");
			TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> NewBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
			TUniquePtr<FBodyParametricFitDNACommandChange> CommandChange = MakeUnique<FBodyParametricFitDNACommandChange>(OldDNABuffer, NewBodyState.ToSharedRef(), OwnerTool->GetToolManager());
			OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
			PreviousBodyState = NewBodyState;
		}
	}
	else
	{
		TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> OldBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
		if (MetaHumanCharacterSubsystem->ParametricFitToCompatibilityBody(MetaHumanCharacter))
		{
			// Creates a command change that allows the user to revert back the state
			const FText CommandChangeDescription = LOCTEXT("BodyParametricFitCompatibilityCommandChange", "Parametric Fit From Fixed Compatibility Body");
			TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> NewBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
			TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(OldBodyState.ToSharedRef(), NewBodyState.ToSharedRef(), OwnerTool->GetToolManager());
			OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
			PreviousBodyState = NewBodyState;
		}
	}
}

TArray<FMetaHumanCharacterBodyConstraintItemPtr> UMetaHumanCharacterParametricBodyProperties::GetConstraintItems(const TArray<FName>& ConstraintNames)
{
	TArray<FMetaHumanCharacterBodyConstraintItemPtr> OutConstraintItems;
	OutConstraintItems.Init(MakeShared<FMetaHumanCharacterBodyConstraintItem>(), ConstraintNames.Num());
	
	for (int32 NameIndex = 0; NameIndex < ConstraintNames.Num(); NameIndex++)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < BodyConstraintItems.Num(); ConstraintIndex++)
		{
			if (BodyConstraintItems[ConstraintIndex]->Name == ConstraintNames[NameIndex])
			{
				OutConstraintItems[NameIndex] = BodyConstraintItems[ConstraintIndex];
				break;
			}
		}
	}
	return OutConstraintItems;
}

void UMetaHumanCharacterParametricBodyProperties::OnBodyStateChanged()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);

	TArray<FMetaHumanCharacterBodyConstraint> BodyConstraints = BodyState->GetBodyConstraints();
	
	int32 NumConstraints = BodyConstraintItems.Num();
	ActiveContours.Init({}, NumConstraints);
	
	for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ConstraintIndex++)
	{
		// Update constraint item
		UpdateConstraintItem(BodyConstraints[ConstraintIndex], BodyConstraintItems[ConstraintIndex]);
		
		// Update measurements
		BodyConstraintItems[ConstraintIndex]->ActualMeasurement = BodyState->GetMeasurement(ConstraintIndex);
		
		// Update active contour vertices
		if (BodyConstraintItems[ConstraintIndex]->bIsActive)
		{
			ActiveContours.Add(BodyState->GetContourVertices(ConstraintIndex));
		}
	}
}

void UMetaHumanCharacterParametricBodyProperties::UpdateMeasurements()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->GetBodyState(MetaHumanCharacter);

	int32 NumConstraints = BodyState->GetNumberOfConstraints();
	ActiveContours.Init({}, NumConstraints);
	for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ConstraintIndex++)
	{
		// Update measurements
		BodyConstraintItems[ConstraintIndex]->ActualMeasurement = BodyState->GetMeasurement(ConstraintIndex);
		
		// Update active contour vertices
		if (BodyConstraintItems[ConstraintIndex]->bIsActive)
		{
			ActiveContours.Add(BodyState->GetContourVertices(ConstraintIndex));
		}
	}
}

void UMetaHumanCharacterFixedCompatibilityBodyProperties::UpdateHeightFromBodyType()
{
	const FString FixedBodyName = StaticEnum<EMetaHumanBodyType>()->GetAuthoredNameStringByValue(static_cast<int32>(MetaHumanBodyType));
	if (FixedBodyName.Contains(TEXT("srt")))
	{
		Height = EMetaHumanCharacterFixedBodyToolHeight::Short;
	}
	else if (FixedBodyName.Contains(TEXT("tal")))
	{
		Height = EMetaHumanCharacterFixedBodyToolHeight::Tall;
	}
	else
	{
		Height = EMetaHumanCharacterFixedBodyToolHeight::Average;
	}
}


void UMetaHumanCharacterFixedCompatibilityBodyProperties::OnBodyStateChanged()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	MetaHumanBodyType = MetaHumanCharacterSubsystem->GetBodyState(MetaHumanCharacter)->GetMetaHumanBodyType();
}

void UMetaHumanCharacterFixedCompatibilityBodyProperties::OnMetaHumanBodyTypeChanged()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyModelTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyModelTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> PreviousBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
	MetaHumanCharacterSubsystem->SetMetaHumanBodyType(MetaHumanCharacter, MetaHumanBodyType, UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Minimal);
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);

	const FText CommandChangeDescription = LOCTEXT("BodyFixedCompatibilityCommandChange", "Set Fixed Compatibility Body");
	
	// Creates a command change that allows the user to revert back the state
	TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(PreviousBodyState.ToSharedRef(), BodyState.ToSharedRef(), OwnerTool->GetToolManager());
	OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
}

void UMetaHumanCharacterEditorBodyParameterProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnPostEditChangeProperty(PropertyChangedEvent);
}

void UMetaHumanCharacterEditorBodyParameterProperties::OnPostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PreviousBodyState.IsValid())
	{
		UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
		USingleSelectionTool* OwnerTool = GetTypedOuter<USingleSelectionTool>();
		UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

		if (PreviousBodyState->GetGlobalDeltaScale() != GlobalDelta)
		{
			Subsystem->SetBodyGlobalDeltaScale(MetaHumanCharacter, GlobalDelta);
			OnBodyParameterChangedDelegate.Broadcast();

			if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::ResetToDefault)) != 0u && ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0u))
			{
				TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> CurrentState = Subsystem->CopyBodyState(MetaHumanCharacter);
				Subsystem->CommitBodyState(MetaHumanCharacter, CurrentState.ToSharedRef(), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Minimal);

				//Creates a command change that allows the user to revert back the state
				const FText CommandChangeDescription = LOCTEXT("BodyGlobalDeltaCommandChange", "Change Body Global Delta");
				TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(PreviousBodyState.ToSharedRef(), CurrentState.ToSharedRef(), OwnerTool->GetToolManager());
				OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);

				PreviousBodyState = Subsystem->CopyBodyState(MetaHumanCharacter);
			}
		}
	}
}

void UMetaHumanCharacterEditorBodyParameterProperties::OnBodyStateChanged()
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	USingleSelectionTool* OwnerTool = GetTypedOuter<USingleSelectionTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());
	GlobalDelta = UMetaHumanCharacterEditorSubsystem::Get()->GetBodyGlobalDeltaScale(MetaHumanCharacter);
}

void UMetaHumanCharacterEditorBodyParameterProperties::ResetBody()
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	USingleSelectionTool* OwnerTool = GetTypedOuter<USingleSelectionTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	MetaHumanCharacterSubsystem->ResetParametricBody(MetaHumanCharacter);

	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
	MetaHumanCharacterSubsystem->CommitBodyState(MetaHumanCharacter, BodyState.ToSharedRef(), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Minimal);

	if (PreviousBodyState.IsValid())
	{
		const FText CommandChangeDescription = LOCTEXT("BodyParametricResetCommandChange", "Reset Parametric Body");
	
		// Creates a command change that allows the user to revert back the stateZ
		TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(PreviousBodyState.ToSharedRef(), BodyState.ToSharedRef(), OwnerTool->GetToolManager());
		OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);

		PreviousBodyState = BodyState;
	}
}

void UMetaHumanCharacterEditorBodyModelTool::Setup()
{
	Super::Setup();

	SetToolDisplayName(LOCTEXT("BodyModelToolName", "Model"));
	
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	// Take a copy of the editing state
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	check(MetaHumanCharacter);
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);

	ParametricBodyProperties = NewObject<UMetaHumanCharacterParametricBodyProperties>(this);
	ParametricBodyProperties->RestoreProperties(this, TEXT("BodyModelToolParametric"));
	ParametricBodyProperties->BodyConstraintItems = BodyConstraintsToConstraintItems(BodyState->GetBodyConstraints());
	ParametricBodyProperties->UpdateMeasurements();	
	ParametricBodyProperties->PreviousBodyState = BodyState;

	FixedCompatibilityBodyProperties = NewObject<UMetaHumanCharacterFixedCompatibilityBodyProperties>(this);
	FixedCompatibilityBodyProperties->MetaHumanBodyType = BodyState->GetMetaHumanBodyType();
	FixedCompatibilityBodyProperties->UpdateHeightFromBodyType();

	BodyParameterProperties = NewObject<UMetaHumanCharacterEditorBodyParameterProperties>(this);
	BodyParameterProperties->GlobalDelta = BodyState->GetGlobalDeltaScale();
	BodyParameterProperties->PreviousBodyState = BodyState;
	AddToolPropertySource(BodyParameterProperties);

	MetaHumanCharacterSubsystem->OnBodyStateChanged(MetaHumanCharacter).	AddWeakLambda(this, [this]
	{
		ParametricBodyProperties->OnBodyStateChanged();
		FixedCompatibilityBodyProperties->OnBodyStateChanged();
		BodyParameterProperties->OnBodyStateChanged();
	});

	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();

	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	if (Settings->bShowCompatibilityModeBodies)
	{
		SubTools->RegisterSubTools(
		{
			{ Commands.BeginBodyModelParametricTool, ParametricBodyProperties },
			{ Commands.BeginBodyFixedCompatibilityTool, FixedCompatibilityBodyProperties },
		});
	}
	else
	{
		SubTools->RegisterSubTools(
		{
			{ Commands.BeginBodyModelParametricTool, ParametricBodyProperties },
		});
	}

}

void UMetaHumanCharacterEditorBodyModelTool::Shutdown(EToolShutdownType InShutdownType)
{
	Super::Shutdown(InShutdownType);

	ParametricBodyProperties->SaveProperties(this, TEXT("BodyModelToolParametric"));
	
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(Target);
	MetaHumanCharacterSubsystem->CommitBodyState(MetaHumanCharacter, MetaHumanCharacterSubsystem->GetBodyState(MetaHumanCharacter), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Full);
}

void UMetaHumanCharacterEditorBodyModelTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (ParametricBodyProperties->bSubToolActive && ParametricBodyProperties->bShowMeasurements)
	{
		FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
		check(PDI);

		for (const TArray<FVector>& Contour : ParametricBodyProperties->ActiveContours)
		{
			for (int32 PointIndex = 0; PointIndex + 1 < Contour.Num(); PointIndex++)
			{
				PDI->DrawLine(Contour[PointIndex], Contour[PointIndex + 1], FLinearColor(0.0, 1.0, 1.0), SDPG_MAX, 0.0f);
			}
		}
	}
}

void UMetaHumanCharacterEditorBodyModelTool::SetEnabledSubTool(UMetaHumanCharacterBodyModelSubToolBase* InSubTool, bool bInEnabled)
{
	if (InSubTool)
	{
		InSubTool->SetEnabled(bInEnabled);
	}
}


// -----------------------------------------------------
// BodyStateChangeTransactor implementation ------------
// -----------------------------------------------------

FSimpleMulticastDelegate& UBodyStateChangeTransactor::GetStateChangedDelegate(UMetaHumanCharacter* InMetaHumanCharacter)
{
	return UMetaHumanCharacterEditorSubsystem::Get()->OnBodyStateChanged(InMetaHumanCharacter);
}

void UBodyStateChangeTransactor::CommitShutdownState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, EToolShutdownType InShutdownType, const FText& InCommandChangeDescription)
{
	// If BeginDragState is valid it means the user has made some changes so we create a transaction
	// that can be reversed
	if (BeginDragState.IsValid())
	{
		UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();

		Subsystem->CommitBodyState(InMetaHumanCharacter, Subsystem->GetBodyState(InMetaHumanCharacter));

		const FText CommandChangeDescription = FText::Format(LOCTEXT("BodyEditingCommandChangeTransaction", "{0} {1}"),
															 UEnum::GetDisplayValueAsText(InShutdownType),
															 InCommandChangeDescription);

		// Creates a command change that allows the user to revert back the state
		TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(BeginDragState.ToSharedRef(), Subsystem->CopyBodyState(InMetaHumanCharacter), InToolManager);
		InToolManager->GetContextTransactionsAPI()->AppendChange(InMetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);

		UpdateClothVisibility(InMetaHumanCharacter, EMetaHumanClothingVisibilityState::Shown);
	}
}
	
void UBodyStateChangeTransactor::StoreBeginDragState(UMetaHumanCharacter* InMetaHumanCharacter)
{
	// Stores the face state when the drag start to allow it to be undone while the tool is active
	BeginDragState = UMetaHumanCharacterEditorSubsystem::Get()->CopyBodyState(InMetaHumanCharacter);

	UpdateClothVisibility(InMetaHumanCharacter, EMetaHumanClothingVisibilityState::Hidden);
}

void UBodyStateChangeTransactor::CommitEndDragState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, const FText& InCommandChangeDescription)
{
	UMetaHumanCharacterEditorSubsystem* Subsystem = UMetaHumanCharacterEditorSubsystem::Get();
	TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(
		BeginDragState.ToSharedRef(), 
		Subsystem->CopyBodyState(InMetaHumanCharacter), 
		InToolManager);

	InToolManager->GetContextTransactionsAPI()->AppendChange(InMetaHumanCharacter, MoveTemp(CommandChange), InCommandChangeDescription);

	// We cannot simply update the cloth visibility here since the body state is not committed and we need to explicitly update the body
	// This code should be in sync with UMetaHumanCharacterEditorSubsystem::CommitBodyState
	//UpdateClothVisibility(InMetaHumanCharacter, EMetaHumanClothingVisibilityState::Shown);
	if (UMetaHumanCharacterEditorSubsystem::IsCharacterOutfitSelected(InMetaHumanCharacter))
	{
		FScopedSlowTask RefitClothingSlowTask{ 2.0f, LOCTEXT("RefitClothingSlowTask", "Fitting outfit to body mesh") };

		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();

		// Outfit refit requires a full body skel mesh update
		MetaHumanCharacterSubsystem->ApplyBodyState(InMetaHumanCharacter, Subsystem->CopyBodyState(InMetaHumanCharacter), UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Full);

		RefitClothingSlowTask.EnterProgressFrame();

		MetaHumanCharacterSubsystem->SetClothingVisibilityState(InMetaHumanCharacter, EMetaHumanClothingVisibilityState::Shown, false);

		MetaHumanCharacterSubsystem->RunCharacterEditorPipelineForPreview(InMetaHumanCharacter);

		if (SavedPreviewMaterial.IsSet())
		{
			// Reset the stored preview material
			MetaHumanCharacterSubsystem->UpdateCharacterPreviewMaterial(InMetaHumanCharacter, SavedPreviewMaterial.GetValue());
			SavedPreviewMaterial.Reset();
		}
	}
}

TSharedRef<FMetaHumanCharacterBodyIdentity::FState> UBodyStateChangeTransactor::GetBeginDragState() const
{
	return BeginDragState.ToSharedRef();
}

// -----------------------------------------------------
// BodyBlendTool implementation ------------------------
// -----------------------------------------------------

bool UMetaHumanCharacterEditorBodyBlendToolProperties::IsFixedBodyType() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyBlendTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyBlendTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	return MetaHumanCharacter->bFixedBodyType;
}

void UMetaHumanCharacterEditorBodyBlendToolProperties::PerformParametricFit() const
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyBlendTool* OwnerTool = GetTypedOuter<UMetaHumanCharacterEditorBodyBlendTool>();
	UMetaHumanCharacter* MetaHumanCharacter = UE::ToolTarget::GetTargetMetaHumanCharacter(OwnerTool->GetTarget());

	if (MetaHumanCharacter->HasBodyDNA())
	{
		TArray<uint8> OldDNABuffer = MetaHumanCharacter->GetBodyDNABuffer();
		if (MetaHumanCharacterSubsystem->ParametricFitToDnaBody(MetaHumanCharacter))
		{
			// Creates a command change that allows the user to revert back the body dna
			const FText CommandChangeDescription = LOCTEXT("BodyParametricFitDnaCommandChange", "Parametric Fit From Body Dna");
			TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> NewBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
			TUniquePtr<FBodyParametricFitDNACommandChange> CommandChange = MakeUnique<FBodyParametricFitDNACommandChange>(OldDNABuffer, NewBodyState.ToSharedRef(), OwnerTool->GetToolManager());
			OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
		}
	}
	else
	{
		TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> OldBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
		if (MetaHumanCharacterSubsystem->ParametricFitToCompatibilityBody(MetaHumanCharacter))
		{
			// Creates a command change that allows the user to revert back the state
			const FText CommandChangeDescription = LOCTEXT("BodyParametricFitCompatibilityCommandChange", "Parametric Fit From Fixed Compatibility Body");
			TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> NewBodyState = MetaHumanCharacterSubsystem->CopyBodyState(MetaHumanCharacter);
			TUniquePtr<FMetaHumanCharacterEditorBodyToolCommandChange> CommandChange = MakeUnique<FMetaHumanCharacterEditorBodyToolCommandChange>(OldBodyState.ToSharedRef(), NewBodyState.ToSharedRef(), OwnerTool->GetToolManager());
			OwnerTool->GetToolManager()->GetContextTransactionsAPI()->AppendChange(MetaHumanCharacter, MoveTemp(CommandChange), CommandChangeDescription);
		}
	}
}

void UMetaHumanCharacterEditorBodyBlendTool::InitStateChangeTransactor()
{
	UBodyStateChangeTransactor* BodyStateChangeTransactor = NewObject<UBodyStateChangeTransactor>(this);
	if (BodyStateChangeTransactor && BodyStateChangeTransactor->GetClass()->ImplementsInterface(UMeshStateChangeTransactorInterface::StaticClass()))
	{
		MeshStateChangeTransactor.SetInterface(Cast<IMeshStateChangeTransactorInterface>(BodyStateChangeTransactor));
		MeshStateChangeTransactor.SetObject(BodyStateChangeTransactor);
	}
}

void UMetaHumanCharacterEditorBodyBlendTool::Setup()
{
	Super::Setup();
	BlendProperties = NewObject<UMetaHumanCharacterEditorBodyBlendToolProperties>(this);
	BlendProperties->RestoreProperties(this, GetCommandChangeDescription().ToString());
	AddToolPropertySource(BlendProperties);

	BodyParameterProperties = NewObject<UMetaHumanCharacterEditorBodyParameterProperties>(this);
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> OriginalState = UMetaHumanCharacterEditorSubsystem::Get()->CopyBodyState(MetaHumanCharacter);
	BodyParameterProperties->GlobalDelta = OriginalState->GetGlobalDeltaScale();
	BodyParameterProperties->PreviousBodyState = OriginalState;
	AddToolPropertySource(BodyParameterProperties);

	BodyParameterProperties->OnBodyParameterChangedDelegate.AddWeakLambda(this, [this] ()
	{
		UpdateManipulatorPositions();
	});
	
	UMetaHumanCharacterEditorSubsystem::Get()->OnBodyStateChanged(MetaHumanCharacter).AddWeakLambda(this, [this]
	{
		BodyParameterProperties->OnBodyStateChanged();
	});
}

const FText UMetaHumanCharacterEditorBodyBlendTool::GetDescription() const
{
	return LOCTEXT("BodyBlendToolName", "Blend");
}

const FText UMetaHumanCharacterEditorBodyBlendTool::GetCommandChangeDescription() const
{
	return LOCTEXT("BodyBlendToolCommandChange", "Body Blend Tool");
}

const FText UMetaHumanCharacterEditorBodyBlendTool::GetCommandChangeIntermediateDescription() const
{
	return LOCTEXT("BodyBlendToolIntermediateCommandChange", "Move Body Blend Manipulator");
}

float UMetaHumanCharacterEditorBodyBlendTool::GetManipulatorScale() const
{
	return 0.006f;
}

float UMetaHumanCharacterEditorBodyBlendTool::GetAncestryCircleRadius() const
{
	return 9.f;
}

TArray<FVector3f> UMetaHumanCharacterEditorBodyBlendTool::GetManipulatorPositions() const
{
	return UMetaHumanCharacterEditorSubsystem::Get()->GetBodyGizmos(MetaHumanCharacter);
}

TArray<FVector3f> UMetaHumanCharacterEditorBodyBlendTool::BlendPresets(int32 InManipulatorIndex, const TArray<float>& Weights)
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyBlendToolProperties* BlendToolProperties = Cast<UMetaHumanCharacterEditorBodyBlendToolProperties>(BlendProperties);
	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> BeginDragState = Cast<UBodyStateChangeTransactor>(MeshStateChangeTransactor.GetObject())->GetBeginDragState();
	return MetaHumanCharacterEditorSubsystem->BlendBodyRegion(MetaHumanCharacter, InManipulatorIndex, BlendToolProperties->BlendOptions, BeginDragState, PresetStates, Weights);
}

void UMetaHumanCharacterEditorBodyBlendTool::AddMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset, int32 InItemIndex)
{
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> PresetState = UMetaHumanCharacterEditorSubsystem::Get()->CopyBodyState(MetaHumanCharacter);
	PresetState->Deserialize(InCharacterPreset->GetBodyStateData());
	if (PresetStates.Num() <= InItemIndex)
	{
		PresetStates.AddDefaulted(InItemIndex - PresetStates.Num() + 1);
	}
	PresetStates[InItemIndex] = PresetState;
}

void UMetaHumanCharacterEditorBodyBlendTool::RemoveMetaHumanCharacterPreset(int32 InItemIndex)
{
	if (InItemIndex < PresetStates.Num())
	{
		PresetStates[InItemIndex].Reset();
	}
}

void UMetaHumanCharacterEditorBodyBlendTool::BlendToMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset)
{
	// set drag state to enable undo of selecting preset
	MeshStateChangeTransactor->StoreBeginDragState(MetaHumanCharacter);

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	UMetaHumanCharacterEditorBodyBlendToolProperties* BlendToolProperties = Cast<UMetaHumanCharacterEditorBodyBlendToolProperties>(BlendProperties);
	TSharedPtr<const FMetaHumanCharacterBodyIdentity::FState> InitState = MetaHumanCharacterEditorSubsystem->GetBodyState(MetaHumanCharacter);
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> State = MetaHumanCharacterEditorSubsystem->CopyBodyState(MetaHumanCharacter);
	State->Deserialize(InCharacterPreset->GetBodyStateData());
	TArray<TSharedPtr<const FMetaHumanCharacterBodyIdentity::FState>> States = { State };
	TArray<float> Weights = { 1.0f };
	TArray<FVector3f> ManipulatorPositions = MetaHumanCharacterEditorSubsystem->BlendBodyRegion(MetaHumanCharacter, INDEX_NONE, BlendToolProperties->BlendOptions, InitState, States, Weights);
	UpdateManipulatorPositions(ManipulatorPositions);

	MeshStateChangeTransactor->CommitEndDragState(GetToolManager(), MetaHumanCharacter, GetCommandChangeIntermediateDescription());
}

#undef LOCTEXT_NAMESPACE