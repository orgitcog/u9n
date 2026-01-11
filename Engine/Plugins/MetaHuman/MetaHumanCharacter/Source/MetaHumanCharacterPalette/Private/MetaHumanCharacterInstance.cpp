// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterInstance.h"

#include "MetaHumanCharacterPaletteLog.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanPinnedSlotSelection.h"

#include "Algo/Find.h"
#include "Logging/StructuredLog.h"
#include "UObject/Package.h"

void UMetaHumanCharacterInstance::Assemble(EMetaHumanCharacterPaletteBuildQuality Quality, const FMetaHumanCharacterAssembled& OnAssembled)
{
	Assemble(Quality, OnAssembled, FMetaHumanCharacterAssembledNative());
}

void UMetaHumanCharacterInstance::Assemble(EMetaHumanCharacterPaletteBuildQuality Quality, const FMetaHumanCharacterAssembledNative& OnAssembledNative)
{
	Assemble(Quality, FMetaHumanCharacterAssembled(), OnAssembledNative);
}

void UMetaHumanCharacterInstance::Assemble(
	EMetaHumanCharacterPaletteBuildQuality Quality,
	const FMetaHumanCharacterAssembled& OnAssembled,
	const FMetaHumanCharacterAssembledNative& OnAssembledNative)
{
	if (!Collection
		|| !Collection->GetPipeline())
	{
		OnAssembled.ExecuteIfBound(EMetaHumanCharacterAssemblyResult::Failed);
		OnAssembledNative.ExecuteIfBound(EMetaHumanCharacterAssemblyResult::Failed);
		return;
	}

	// All selections are propagated to real slots, so the pipeline doesn't have to deal with any
	// virtual slots.
	const TArray<FMetaHumanPipelineSlotSelectionData> RealSlotSelections = Collection->PropagateVirtualSlotSelections(SlotSelections);

	// Not used yet
	const FInstancedStruct AssemblyInput;

	UObject* OuterForGeneratedObjects = this;
	// we use the transient package for the outer in preview builds, to avoid dirtying the collection
	if (Quality == EMetaHumanCharacterPaletteBuildQuality::Preview)
	{
		OuterForGeneratedObjects = GetTransientPackage();
	}

	Collection->GetPipeline()->AssembleCollection(
		Collection,
		Quality,
		RealSlotSelections,
		FInstancedStruct(),
		OuterForGeneratedObjects,
		UMetaHumanCollectionPipeline::FOnAssemblyComplete::CreateWeakLambda(
			this,
			[this, OnAssembled, OnAssembledNative](FMetaHumanAssemblyOutput&& NewAssemblyOutput)
			{
				AssemblyOutput = MoveTemp(NewAssemblyOutput.PipelineAssemblyOutput);
#if WITH_EDITORONLY_DATA
				AssemblyAssetMetadata = MoveTemp(NewAssemblyOutput.Metadata);
#endif

				// In order to keep the parameter context encapsulated, we have to split the map.
				//
				// It's not ideal, but necessary to prevent the parameter context from becoming an
				// unwanted side channel that will reduce the flexibility we have in future.
				AssemblyInstanceParameters.Empty(NewAssemblyOutput.InstanceParameters.Num());
				AssemblyInstanceParameterContext.Empty(NewAssemblyOutput.InstanceParameters.Num());
				for (TPair<FMetaHumanPaletteItemPath, FMetaHumanInstanceParameterOutput>& Pair : NewAssemblyOutput.InstanceParameters)
				{
					AssemblyInstanceParameters.Add(Pair.Key, MoveTemp(Pair.Value.Parameters));

					if (Pair.Value.ParameterContext.IsValid())
					{
						AssemblyInstanceParameterContext.Add(Pair.Key, MoveTemp(Pair.Value.ParameterContext));
					}
				}

				const EMetaHumanCharacterAssemblyResult Status = AssemblyOutput.IsValid()
					? EMetaHumanCharacterAssemblyResult::Succeeded
					: EMetaHumanCharacterAssemblyResult::Failed;

				if (Status == EMetaHumanCharacterAssemblyResult::Succeeded)
				{
					// TODO: Apply the instance parameters from the pinned slot selections here as well

					// Apply any overridden parameters to the new assembly output
					for (const TPair<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& Pair : OverriddenInstanceParameters)
					{
						ApplyOverriddenInstanceParameters(Pair.Key);
					}
				}

				OnAssembled.ExecuteIfBound(Status);
				OnAssembledNative.ExecuteIfBound(Status);

				if (Status == EMetaHumanCharacterAssemblyResult::Succeeded)
				{
					OnInstanceUpdated.Broadcast();
					OnInstanceUpdatedNative.Broadcast();
				}
			}));
}

const FInstancedStruct& UMetaHumanCharacterInstance::GetAssemblyOutput() const
{
	return AssemblyOutput;
}

void UMetaHumanCharacterInstance::ClearAssemblyOutput()
{
	AssemblyOutput.Reset();
}

void UMetaHumanCharacterInstance::SetMetaHumanCollection(UMetaHumanCollection* InCharacterPalette)
{
	if (Collection
		&& OnPaletteBuiltHandle.IsValid())
	{
		Collection->OnPaletteBuilt.Remove(OnPaletteBuiltHandle);
		OnPaletteBuiltHandle.Reset();
	}

	Collection = InCharacterPalette;

	// Ensure we don't keep stale assembly output from a different character.
	//
	// This allows code to safely assume that any Instance belonging to a Character Palette contains
	// assembly output compatible with that Character Palette.
	AssemblyOutput.Reset();

	if (!Collection)
	{
		return;
	}

	OnPaletteBuiltHandle = Collection->OnPaletteBuilt.AddUObject(this, &UMetaHumanCharacterInstance::OnPaletteBuilt);
}

void UMetaHumanCharacterInstance::SetSingleSlotSelection(FName SlotName, const FMetaHumanPaletteItemKey& ItemKey)
{
	SetSingleSlotSelection(FMetaHumanPaletteItemPath(), SlotName, ItemKey);
}

void UMetaHumanCharacterInstance::SetSingleSlotSelection(const FMetaHumanPaletteItemPath& ParentItemPath, FName SlotName, const FMetaHumanPaletteItemKey& ItemKey)
{
	// This is not the most efficient implementation, but it is very simple and this is not a 
	// performance critical function.

	// Remove all existing entries for this slot
	SlotSelections.RemoveAllSwap([&ParentItemPath, SlotName](const FMetaHumanPipelineSlotSelectionData& Element)
		{
			return Element.Selection.ParentItemPath == ParentItemPath
				&& Element.Selection.SlotName == SlotName;
		});

	if (!ItemKey.IsNull())
	{
		// Add a new entry at the end
		SlotSelections.Emplace(FMetaHumanPipelineSlotSelectionData(FMetaHumanPipelineSlotSelection(ParentItemPath, SlotName, ItemKey)));
	}
}

bool UMetaHumanCharacterInstance::TryAddSlotSelection(const FMetaHumanPipelineSlotSelection& Selection)
{
	// TODO: Validation

	FMetaHumanPipelineSlotSelectionData NewSelectionData;
	NewSelectionData.Selection = Selection;

	SlotSelections.Add(NewSelectionData);

	return true;
}

bool UMetaHumanCharacterInstance::TryGetAnySlotSelection(FName SlotName, FMetaHumanPaletteItemKey& OutItemKey) const
{
	return TryGetAnySlotSelection(SlotSelections, FMetaHumanPaletteItemPath(), SlotName, OutItemKey);
}

bool UMetaHumanCharacterInstance::TryGetAnySlotSelection(const FMetaHumanPaletteItemPath& ParentItemPath, FName SlotName, FMetaHumanPaletteItemKey& OutItemKey) const
{
	return TryGetAnySlotSelection(SlotSelections, ParentItemPath, SlotName, OutItemKey);
}

bool UMetaHumanCharacterInstance::TryGetAnySlotSelection(
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections, 
	FName SlotName, 
	FMetaHumanPaletteItemKey& OutItemKey)
{
	return TryGetAnySlotSelection(SlotSelections, FMetaHumanPaletteItemPath(), SlotName, OutItemKey);
}

bool UMetaHumanCharacterInstance::TryGetAnySlotSelection(
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections, 
	const FMetaHumanPaletteItemPath& ParentItemPath, 
	FName SlotName, 
	FMetaHumanPaletteItemKey& OutItemKey)
{
	const FMetaHumanPipelineSlotSelectionData* Selection = Algo::FindByPredicate(SlotSelections,
		[&ParentItemPath, SlotName](const FMetaHumanPipelineSlotSelectionData& Element)
		{
			return Element.Selection.ParentItemPath == ParentItemPath
				&& Element.Selection.SlotName == SlotName;
		});

	if (!Selection)
	{
		// Initialize this to the null item in case the caller tries to read it
		OutItemKey = FMetaHumanPaletteItemKey();
		return false;
	}

	OutItemKey = Selection->Selection.SelectedItem;
	return true;
}

bool UMetaHumanCharacterInstance::ContainsSlotSelection(const FMetaHumanPipelineSlotSelection& Selection) const
{
	return SlotSelections.ContainsByPredicate(
		[&Selection](const FMetaHumanPipelineSlotSelectionData& Element)
		{
			return Element.Selection == Selection;
		});
}

bool UMetaHumanCharacterInstance::TryRemoveSlotSelection(const FMetaHumanPipelineSlotSelection& Selection)
{
	const int32 Index = SlotSelections.IndexOfByPredicate(
		[&Selection](const FMetaHumanPipelineSlotSelectionData& Element)
		{
			return Element.Selection == Selection;
		});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	SlotSelections.RemoveAtSwap(Index);
	return true;
}

const TArray<FMetaHumanPipelineSlotSelectionData>& UMetaHumanCharacterInstance::GetSlotSelectionData() const
{
	return SlotSelections;
}

TArray<FMetaHumanPinnedSlotSelection> UMetaHumanCharacterInstance::ToPinnedSlotSelections(EMetaHumanUnusedSlotBehavior UnusedSlotBehavior) const
{
	TArray<FMetaHumanPinnedSlotSelection> Result;
	Result.Reserve(SlotSelections.Num());

	TArray<FName> UnusedSlotsToPin;
	if (UnusedSlotBehavior == EMetaHumanUnusedSlotBehavior::PinnedToEmpty)
	{
		if (!Collection
			|| !Collection->GetPipeline())
		{
			UE_LOGFMT(LogMetaHumanCharacterPalette, Error, 
				"ToPinnedSlotSelections: Can't generate empty pinned slot selections for {Instance}, because there is no Collection Pipeline", 
				GetPathName());

			return Result;
		}

		Collection->GetPipeline()->GetSpecification()->Slots.GenerateKeyArray(UnusedSlotsToPin);
	}

	for (const FMetaHumanPipelineSlotSelectionData& SelectionData : SlotSelections)
	{
		FMetaHumanPinnedSlotSelection& PinnedSelection = Result.AddDefaulted_GetRef();
		PinnedSelection.Selection = SelectionData.Selection;

		const FInstancedPropertyBag* Params = OverriddenInstanceParameters.Find(SelectionData.Selection.GetSelectedItemPath());
		if (Params)
		{
			PinnedSelection.InstanceParameters = *Params;
		}

		// TODO: Handle sub-items
		if (SelectionData.Selection.ParentItemPath.IsEmpty())
		{
			// This slot is used, so remove it from the unused list
			UnusedSlotsToPin.Remove(SelectionData.Selection.SlotName);
		}
	}

	// Create empty selections for any unused slots
	for (const FName UnusedSlotName : UnusedSlotsToPin)
	{
		FMetaHumanPinnedSlotSelection& PinnedSelection = Result.AddDefaulted_GetRef();
		PinnedSelection.Selection.SlotName = UnusedSlotName;
	}

	return Result;
}

const TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& UMetaHumanCharacterInstance::GetAssemblyInstanceParameters() const
{
	return AssemblyInstanceParameters;
}

const TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& UMetaHumanCharacterInstance::GetOverriddenInstanceParameters() const
{
	return OverriddenInstanceParameters;
}

// Same as FInstancedPropertyBag::CopyMatchingValuesByID, except that it matches by name.
//
// This function will be moved into FInstancedPropertyBag pending code review
static void CopyMatchingValuesByName(const FInstancedPropertyBag& SourceBag, FInstancedPropertyBag& TargetBag)
{
	const FConstStructView Source = SourceBag.GetValue();
	FStructView Target = TargetBag.GetMutableValue();

	if (!Source.IsValid() || !Target.IsValid())
	{
		return;
	}

	const UPropertyBag* SourceBagStruct = Cast<const UPropertyBag>(Source.GetScriptStruct());
	const UPropertyBag* TargetBagStruct = Cast<const UPropertyBag>(Target.GetScriptStruct());

	if (!SourceBagStruct || !TargetBagStruct)
	{
		return;
	}

	// Iterate over source and copy to target if possible. Source is expected to usually have less items.
	for (const FPropertyBagPropertyDesc& SourceDesc : SourceBagStruct->GetPropertyDescs())
	{
		const FPropertyBagPropertyDesc* PotentialTargetDesc = TargetBagStruct->FindPropertyDescByName(SourceDesc.Name);
		if (PotentialTargetDesc == nullptr
			|| PotentialTargetDesc->CachedProperty == nullptr
			|| SourceDesc.CachedProperty == nullptr)
		{
			continue;
		}

		const FPropertyBagPropertyDesc& TargetDesc = *PotentialTargetDesc;
		void* TargetAddress = Target.GetMemory() + TargetDesc.CachedProperty->GetOffset_ForInternal();
		const void* SourceAddress = Source.GetMemory() + SourceDesc.CachedProperty->GetOffset_ForInternal();
			
		if (TargetDesc.CompatibleType(SourceDesc))
		{
			TargetDesc.CachedProperty->CopyCompleteValue(TargetAddress, SourceAddress);
		}
		else if (TargetDesc.ContainerTypes.Num() == 0
				&& SourceDesc.ContainerTypes.Num() == 0)
		{
			if (TargetDesc.IsNumericType() && SourceDesc.IsNumericType())
			{
				// Try to convert numeric types.
				if (TargetDesc.IsNumericFloatType())
				{
					const TValueOrError<double, EPropertyBagResult> Result = SourceBag.GetValueDouble(SourceDesc);
					if (!Result.HasError() && Result.HasValue())
					{
						TargetBag.SetValueDouble(TargetDesc, Result.GetValue());
					}
				}
				else
				{
					if (TargetDesc.IsUnsignedNumericType())
					{
						const TValueOrError<uint64, EPropertyBagResult> Result = SourceBag.GetValueUInt64(SourceDesc);
						if (!Result.HasError() && Result.HasValue())
						{
							TargetBag.SetValueUInt64(TargetDesc, Result.GetValue());
						}
					}
					else
					{
						const TValueOrError<int64, EPropertyBagResult> Result = SourceBag.GetValueInt64(SourceDesc);
						if (!Result.HasError() && Result.HasValue())
						{
							TargetBag.SetValueInt64(TargetDesc, Result.GetValue());
						}
					}
				}
			}
			else if ((TargetDesc.IsObjectType() && SourceDesc.IsObjectType())
				|| (TargetDesc.IsClassType() && SourceDesc.IsClassType()))
			{
				// Try convert between compatible objects and classes.
				const UClass* TargetObjectClass = Cast<const UClass>(TargetDesc.ValueTypeObject);
				const UClass* SourceObjectClass = Cast<const UClass>(SourceDesc.ValueTypeObject);
				if (SourceObjectClass && TargetObjectClass && SourceObjectClass->IsChildOf(TargetObjectClass))
				{
					const FObjectPropertyBase* TargetProp = CastFieldChecked<FObjectPropertyBase>(TargetDesc.CachedProperty);
					const FObjectPropertyBase* SourceProp = CastFieldChecked<FObjectPropertyBase>(SourceDesc.CachedProperty);
					TargetProp->SetObjectPropertyValue(TargetAddress, SourceProp->GetObjectPropertyValue(SourceAddress));
				}
			}
		}
	}
}

FInstancedPropertyBag UMetaHumanCharacterInstance::GetCurrentInstanceParametersForItem(const FMetaHumanPaletteItemPath& ItemPath) const
{
	const FInstancedPropertyBag* AssemblyParameters = AssemblyInstanceParameters.Find(ItemPath);
	if (!AssemblyParameters)
	{
		return FInstancedPropertyBag();
	}

	const FInstancedPropertyBag* OverriddenParameters = OverriddenInstanceParameters.Find(ItemPath);
	if (!OverriddenParameters)
	{
		return *AssemblyParameters;
	}

	FInstancedPropertyBag Result = *AssemblyParameters;
	CopyMatchingValuesByName(*OverriddenParameters, Result);
	return Result;
}

void UMetaHumanCharacterInstance::OverrideInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath, const FInstancedPropertyBag& NewParameters)
{
	FInstancedPropertyBag& OverriddenParameters = OverriddenInstanceParameters.FindOrAdd(ItemPath);

	// Merge new parameter values into any existing ones
	if (OverriddenParameters.IsValid())
	{
		if (NewParameters.GetPropertyBagStruct() == OverriddenParameters.GetPropertyBagStruct())
		{
			// The property bags use the exact same struct, so simply copy the data over
			NewParameters.GetPropertyBagStruct()->CopyScriptStruct(
				OverriddenParameters.GetMutableValue().GetMemory(),
				NewParameters.GetValue().GetMemory());
		}
		else
		{
			// Add any properties from NewParameters that don't already exist.
			//
			// Note that any existing properties with the same name but of a different type will be 
			// changed to the new type.
			const EPropertyBagAlterationResult AddResult = OverriddenParameters.AddProperties(NewParameters.GetPropertyBagStruct()->GetPropertyDescs());
			if (AddResult != EPropertyBagAlterationResult::Success)
			{
				UE_LOGFMT(LogMetaHumanCharacterPalette, Error, 
					"OverrideInstanceParameters: Failed to merge the provided parameters with the existing parameters for {ItemPath}: {Reason}", 
					ItemPath.ToDebugString(), 
					StaticEnum<EPropertyBagAlterationResult>()->GetNameStringByValue(static_cast<int64>(AddResult)));

				return;
			}

			// Copy over the property values
			CopyMatchingValuesByName(NewParameters, OverriddenParameters);
		}
	}
	else
	{
		// There is no property bag yet, so just copy the passed-in one
		OverriddenParameters = NewParameters;
	}

	ApplyOverriddenInstanceParameters(ItemPath);
}

void UMetaHumanCharacterInstance::ClearAllOverriddenInstanceParameters()
{
	OverriddenInstanceParameters.Reset();
}

void UMetaHumanCharacterInstance::ClearOverriddenInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath)
{
	OverriddenInstanceParameters.Remove(ItemPath);
}

#if WITH_EDITOR
bool UMetaHumanCharacterInstance::TryUnpack(const FString& TargetFolder)
{
	if (!Collection)
	{
		return false;
	}

	const UMetaHumanCollectionEditorPipeline* Pipeline = Collection->GetEditorPipeline();
	if (!Pipeline)
	{
		return false;
	}

	return Pipeline->TryUnpackInstanceAssets(this, AssemblyOutput, AssemblyAssetMetadata, TargetFolder);
}
#endif // WITH_EDITOR

void UMetaHumanCharacterInstance::ApplyOverriddenInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath) const
{
	const FInstancedPropertyBag* OverriddenParameters = OverriddenInstanceParameters.Find(ItemPath);

	if (!OverriddenParameters
		|| !Collection
		|| !AssemblyOutput.IsValid())
	{
		return;
	}

	const FInstancedPropertyBag* AssemblyParameters = AssemblyInstanceParameters.Find(ItemPath);
	if (!AssemblyParameters)
	{
		// This item doesn't have any instance parameters.
		//
		// No error logged, as this is a special case of OverriddenParameters containing parameters 
		// that don't exist in AssemblyInstanceParameters, which we also don't warn about.

		return;
	}
	
	const UMetaHumanCharacterPipeline* ParameterPipeline = nullptr;
	if (!Collection->TryResolvePipeline(ItemPath, ParameterPipeline))
	{
		UE_LOGFMT(LogMetaHumanCharacterPalette, Error, 
			"ItemPath {ItemPath} couldn't be resolved to an item in Collection {Collection} while applying overridden Instance Parameters", 
			ItemPath.ToDebugString(), 
			GetPathNameSafe(Collection));

		return;
	}

	const FInstancedStruct EmptyStruct;
	const FInstancedStruct* AssemblyParameterContext = AssemblyInstanceParameterContext.Find(ItemPath);
	if (!AssemblyParameterContext)
	{
		AssemblyParameterContext = &EmptyStruct;
	}

	// Notify the pipeline that instance parameters have been set, so that it can apply them to
	// whatever it is that they control, e.g. set material parameters from the parameter values.
	if (AssemblyParameters->GetPropertyBagStruct() == OverriddenParameters->GetPropertyBagStruct())
	{
		// Can pass OverriddenParameters directly, as it's the same struct type
		ParameterPipeline->SetInstanceParameters(*AssemblyParameterContext, *OverriddenParameters);
	}
	else
	{
		// The overridden parameters struct is different from the struct that the pipeline is 
		// expecting, so we need to create a temporary property bag and copy the parameters over.

		// If this path gets hit a lot, we could cache this on a transient member variable.
		FInstancedPropertyBag TempParameters = *AssemblyParameters;

		CopyMatchingValuesByName(*OverriddenParameters, TempParameters);
		
		ParameterPipeline->SetInstanceParameters(*AssemblyParameterContext, TempParameters);
	}
}

void UMetaHumanCharacterInstance::RegisterOnInstanceUpdated(const FMetaHumanCharacterInstanceUpdated_Unicast& Delegate)
{
	OnInstanceUpdated.Add(Delegate);
}

void UMetaHumanCharacterInstance::UnregisterOnInstanceUpdated(UObject* Object)
{
	OnInstanceUpdated.RemoveAll(Object);
}

void UMetaHumanCharacterInstance::BeginDestroy()
{
	Super::BeginDestroy();

	if (OnPaletteBuiltHandle.IsValid())
	{
		// If the handle is valid, Collection shouldn't be null, but this can happen if the
		// asset referenced by Collection is forcibly deleted in the editor.
		if (Collection)
		{
			Collection->OnPaletteBuilt.Remove(OnPaletteBuiltHandle);
		}
		OnPaletteBuiltHandle.Reset();
	}
}

void UMetaHumanCharacterInstance::OnPaletteBuilt(EMetaHumanCharacterPaletteBuildQuality Quality)
{
	check(Collection);
	check(OnPaletteBuiltHandle.IsValid());

	Assemble(Quality);
}
