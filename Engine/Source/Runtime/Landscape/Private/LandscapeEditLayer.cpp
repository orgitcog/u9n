// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditLayer.h"
#include "LandscapeEditTypes.h"
#include "Landscape.h"

#define LOCTEXT_NAMESPACE "LandscapeEditLayer"

// ----------------------------------------------------------------------------------

#if WITH_EDITOR
bool ULandscapeEditLayerBase::SupportsAlphaForTargetType(ELandscapeToolTargetType InType) const
{
	return (InType == ELandscapeToolTargetType::Heightmap) || (InType == ELandscapeToolTargetType::Weightmap);
}

void ULandscapeEditLayerBase::SetAlphaForTargetType(ELandscapeToolTargetType InType, float InNewValue, bool bInModify, EPropertyChangeType::Type InChangeType)
{
	check(SupportsAlphaForTargetType(InType));

	const FFloatInterval AlphaInterval = GetAlphaRangeForTargetType(InType);
	const float ClampedNewValue = FMath::Clamp(InNewValue, AlphaInterval.Min, AlphaInterval.Max);
	float& AlphaValueRef = GetAlphaForTargetTypeRef(InType);
	const bool bHasValueChanged = (AlphaValueRef != ClampedNewValue);
	if (bHasValueChanged)
	{
		if (bInModify)
		{
			Modify();
		}

		AlphaValueRef = ClampedNewValue;
	}

	FProperty* AlphaProperty = GetAlphaPropertyForTargetType(InType);
	check(AlphaProperty != nullptr);
	BroadcastOnLayerDataChanged(FName(AlphaProperty->GetName()), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, InChangeType);
}

float ULandscapeEditLayerBase::GetAlphaForTargetType(ELandscapeToolTargetType InType) const
{
	return const_cast<ULandscapeEditLayerBase*>(this)->GetAlphaForTargetTypeRef(InType);
}

float& ULandscapeEditLayerBase::GetAlphaForTargetTypeRef(ELandscapeToolTargetType InType) 
{
	switch (InType)
	{
	case ELandscapeToolTargetType::Heightmap: 
	{
		return HeightmapAlpha;
	}
	case ELandscapeToolTargetType::Weightmap: 
	{
		return WeightmapAlpha;
	}
	default:
	{
		static float DefaultValue = 1.0f;
		return DefaultValue;
	}
	}
}

FProperty* ULandscapeEditLayerBase::GetAlphaPropertyForTargetType(ELandscapeToolTargetType InType) const
{
	switch (InType)
	{
	case ELandscapeToolTargetType::Heightmap:
	{
		return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, HeightmapAlpha));
	}
	case ELandscapeToolTargetType::Weightmap:
	{
		return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapAlpha));
	}
	default:
	{
		return nullptr;
	}
	}
}

FFloatInterval ULandscapeEditLayerBase::GetAlphaRangeForTargetType(ELandscapeToolTargetType InType) const
{
	switch (InType)
	{
	case ELandscapeToolTargetType::Heightmap:
	{
		return FFloatInterval(-1.0f, 1.0f);
	}
	case ELandscapeToolTargetType::Weightmap:
	{
		return FFloatInterval(0.0f, 1.0f);
	}
	default:
	{
		return FFloatInterval(0.0f, 1.0f);
	}
	}
}

void ULandscapeEditLayerBase::SetGuid(const FGuid& InGuid, bool bInModify)
{
	const bool bHasValueChanged = (InGuid != Guid);
	if (bHasValueChanged)
	{
		if (bInModify)
		{
			Modify();
		}

		Guid = InGuid;
	}

	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, Guid), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, /*InChangeType = */EPropertyChangeType::ValueSet);
}

const FGuid& ULandscapeEditLayerBase::GetGuid() const
{
	return Guid;
}

void ULandscapeEditLayerBase::SetName(FName InName, bool bInModify)
{
	check(OwningLandscape.IsValid())
	if (!OwningLandscape->IsLayerNameUnique(InName) || LayerName == InName)
	{
		return;
	}

	const bool bHasValueChanged = (InName != LayerName);
	if (bHasValueChanged)
	{
		if (bInModify)
		{
			Modify();
		}

		LayerName = InName;
	}

	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, LayerName), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, /*InChangeType = */EPropertyChangeType::ValueSet);
}

FName ULandscapeEditLayerBase::GetName() const
{
	return LayerName;
}

void ULandscapeEditLayerBase::SetVisible(bool bInVisible, bool bInModify)
{
	SetVisible(bInVisible, bInModify, /*bInBroadcastDataChange = */true);
}

void ULandscapeEditLayerBase::SetVisible(bool bInVisible, bool bInModify, bool bInBroadcastDataChange)
{
	const bool bHasValueChanged = (bInVisible != bVisible);
	if (bHasValueChanged)
	{
		if (bInModify)
		{
			Modify();
		}

		bVisible = bInVisible;
	}

	if (bInBroadcastDataChange)
	{
		BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, bVisible), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, /*InChangeType = */EPropertyChangeType::ValueSet);
	}
}

bool ULandscapeEditLayerBase::IsVisible() const
{
	return bVisible;
}

void ULandscapeEditLayerBase::SetLocked(bool bInLocked, bool bInModify)
{
	const bool bHasValueChanged = (bInLocked != bLocked);
	if (bHasValueChanged)
	{
		if (bInModify)
		{
			Modify();
		}

		bLocked = bInLocked;
	}
	
	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, bLocked), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, /*InChangeType = */EPropertyChangeType::ValueSet);
}

bool ULandscapeEditLayerBase::IsLocked() const
{
	return bLocked;
}

ELandscapeBlendMode ULandscapeEditLayerBase::GetBlendMode() const
{
	return ELandscapeBlendMode::LSBM_AdditiveBlend;
}

bool ULandscapeEditLayerBase::RemoveAndCopyWeightmapAllocationLayerBlend(TObjectPtr<ULandscapeLayerInfoObject> InKey, bool& bOutValue, bool bInModify)
{
	bool bHasValueChanged = false;

	if (WeightmapLayerAllocationBlend.RemoveAndCopyValue(InKey, bOutValue))
	{
		Modify(bInModify);

		bHasValueChanged = true;
	}

	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapLayerAllocationBlend), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, bHasValueChanged, /*InChangeType = */EPropertyChangeType::ValueSet);

	// return true if and only if RemoveAndCopy returns true
	return bHasValueChanged;
}

void ULandscapeEditLayerBase::AddOrUpdateWeightmapAllocationLayerBlend(TObjectPtr<ULandscapeLayerInfoObject> InKey, bool bInValue, bool bInModify)
{ 
	const bool* bValueExists = WeightmapLayerAllocationBlend.Find(InKey);
	bool& bFoundValue = WeightmapLayerAllocationBlend.FindOrAdd(InKey, bInValue);

	// changed if existing value has been toggled or a new entry is added to the map
	const bool bHasValueChanged = ((bValueExists == nullptr) || (bFoundValue != bInValue));

	bFoundValue = bInValue;
	
	if (bInModify)
	{
		Modify();
	}

	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapLayerAllocationBlend), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, /*bHasValueChanged =*/ true, /*InChangeType = */EPropertyChangeType::ValueSet);
}

const TMap<TObjectPtr<ULandscapeLayerInfoObject>, bool>& ULandscapeEditLayerBase::GetWeightmapLayerAllocationBlend() const
{
	return WeightmapLayerAllocationBlend;
}

void ULandscapeEditLayerBase::SetWeightmapLayerAllocationBlend(const TMap<TObjectPtr<ULandscapeLayerInfoObject>, bool>& InWeightmapLayerAllocationBlend, bool bInModify)
{
	WeightmapLayerAllocationBlend = InWeightmapLayerAllocationBlend;

	if (bInModify)
	{
		Modify();
	}

	BroadcastOnLayerDataChanged(GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapLayerAllocationBlend), /*bInUserTriggered = */true, /*bInRequiresLandscapeUpdate =*/ true, /*bHasValueChanged =*/ true, /*InChangeType = */EPropertyChangeType::ValueSet);
}

void ULandscapeEditLayerBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, Guid))
	{
		SetGuid(Guid, /*bInModify = */true);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, LayerName))
	{
		SetName(LayerName, /*bInModify = */true);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, HeightmapAlpha))
	{
		SetAlphaForTargetType(ELandscapeToolTargetType::Heightmap, HeightmapAlpha, /*bInModify = */true, PropertyChangedEvent.ChangeType);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapAlpha))
	{
		SetAlphaForTargetType(ELandscapeToolTargetType::Weightmap, WeightmapAlpha, /*bInModify = */true, PropertyChangedEvent.ChangeType);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, bLocked))
	{
		SetLocked(bLocked, /*bInModify = */true);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, bVisible))
	{
		SetVisible(bVisible, /*bInModify = */true);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapLayerAllocationBlend))
	{
		SetWeightmapLayerAllocationBlend(WeightmapLayerAllocationBlend, /*bInModify = */true);
	}
}

void ULandscapeEditLayerBase::PostEditUndo()
{
	Super::PostEditUndo();

	BroadcastOnLayerDataChanged(/*InPropertyName = */NAME_None, /*bInUserTriggered = */false, /*bInRequiresLandscapeUpdate =*/ true, /*bInHasValueChanged = */true, /*InChangeType = */EPropertyChangeType::ValueSet);
}

bool ULandscapeEditLayerBase::CanEditChange(const FProperty* InProperty) const
{
	bool bCanEdit = Super::CanEditChange(InProperty);

	const FName MemberPropertyName = InProperty ? InProperty->GetFName() : NAME_None;

	// Always able to update locked property
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, bLocked))
	{
		return true;
	}

	// All other properties disabled when layer is locked
	if (IsLocked())
	{
		return false;
	}

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, HeightmapAlpha))
	{
		bCanEdit &= SupportsAlphaForTargetType(ELandscapeToolTargetType::Heightmap);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ULandscapeEditLayerBase, WeightmapAlpha))
	{
		bCanEdit &= SupportsAlphaForTargetType(ELandscapeToolTargetType::Weightmap);
	}

	return bCanEdit;
}

void ULandscapeEditLayerBase::PostLoad()
{
	Super::PostLoad();

	// Needed because we might have saved some layers before we realized we were missing this flag
	SetFlags(RF_Transactional);
}

void ULandscapeEditLayerBase::BroadcastOnLayerDataChanged(FName InPropertyName, bool bInUserTriggered, bool bInRequiresLandscapeUpdate, bool bInHasValueChanged, EPropertyChangeType::Type InChangeType)
{
	FProperty* Property = nullptr;
	if (InPropertyName != NAME_None)
	{
		Property = FindFProperty<FProperty>(StaticClass(), InPropertyName);
		check(Property != nullptr);
	}

	FOnLandscapeEditLayerDataChangedParams OnLayerDataChangedParams(FPropertyChangedEvent(Property, InChangeType));
	OnLayerDataChangedParams.bUserTriggered = bInUserTriggered;
	OnLayerDataChangedParams.bRequiresLandscapeUpdate = bInRequiresLandscapeUpdate;
	OnLayerDataChangedParams.bHasValueChanged = bInHasValueChanged;
	OnLayerDataChangedDelegate.Broadcast(OnLayerDataChangedParams);
}

ELandscapeToolTargetTypeFlags ULandscapeEditLayerBase::GetEnabledTargetTypeMask() const
{
	// Compute the default state of each target type : 
	ELandscapeToolTargetTypeFlags EnabledTargetTypeMask = ELandscapeToolTargetTypeFlags::None;
	if (IsVisible())
	{
		//  HeightmapAlpha might still be set to a different value)
		if (GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap) != 0.0f)
		{
			EnabledTargetTypeMask |= ELandscapeToolTargetTypeFlags::Heightmap;
		}
		if (GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap) > 0.0f)
		{
			EnabledTargetTypeMask |= ELandscapeToolTargetTypeFlags::Weightmap;
		}
		// If the layer is visible, visibility is always considered enabled since it does not depend on WeightmapAlpha 
		EnabledTargetTypeMask |= ELandscapeToolTargetTypeFlags::Visibility;
	}
	return EnabledTargetTypeMask;
}

void ULandscapeEditLayerBase::SetBackPointer(ALandscape* Landscape)
{
	OwningLandscape = Landscape;
}
#endif //WITH_EDITOR

#if WITH_EDITORONLY_DATA
void ULandscapeEditLayerBase::SetHeightmapAlphaInternal(float InNewValue)
{
	SetAlphaForTargetType(ELandscapeToolTargetType::Heightmap, InNewValue, /*bInModify = */true, /*InChangeType = */EPropertyChangeType::ValueSet);
}

void ULandscapeEditLayerBase::SetWeightmapAlphaInternal(float InNewValue)
{
	SetAlphaForTargetType(ELandscapeToolTargetType::Weightmap, InNewValue, /*bInModify = */true, /*InChangeType = */EPropertyChangeType::ValueSet);
}

void ULandscapeEditLayerBase::SetGuidInternal(const FGuid& InGuid)
{
	SetGuid(InGuid, /*bInModify = */true);
}

void ULandscapeEditLayerBase::SetNameInternal(FName InName)
{
	SetName(InName, /*bInModify = */true);
}

void ULandscapeEditLayerBase::SetVisibleInternal(bool bInVisible)
{
	SetVisible(bInVisible, /*bInModify = */true);
}

void ULandscapeEditLayerBase::SetLockedInternal(bool bInLocked)
{
	SetLocked(bInLocked, /*bInModify = */true);
}

void ULandscapeEditLayerBase::SetWeightmapLayerAllocationBlendInternal(const TMap<TObjectPtr<ULandscapeLayerInfoObject>, bool>& InWeightmapLayerAllocationBlend)
{
	SetWeightmapLayerAllocationBlend(InWeightmapLayerAllocationBlend, /*bInModify = */true);
}

#endif // WITH_EDITORONLY_DATA

// ----------------------------------------------------------------------------------

bool ULandscapeEditLayer::SupportsTargetType(ELandscapeToolTargetType InType) const
{
	return (InType == ELandscapeToolTargetType::Heightmap) || (InType == ELandscapeToolTargetType::Weightmap) || (InType == ELandscapeToolTargetType::Visibility);
}

// ----------------------------------------------------------------------------------

bool ULandscapeEditLayerSplines::SupportsTargetType(ELandscapeToolTargetType InType) const
{
	return (InType == ELandscapeToolTargetType::Heightmap) || (InType == ELandscapeToolTargetType::Weightmap) || (InType == ELandscapeToolTargetType::Visibility);
}

TArray<ULandscapeEditLayerSplines::FEditLayerAction> ULandscapeEditLayerSplines::GetActions() const
{
	TArray<FEditLayerAction> Actions;

#if WITH_EDITOR
	// Register an "Update Splines" action :
	Actions.Add(FEditLayerAction(
		LOCTEXT("LandscapeEditLayerSplines_UpdateSplines", "Update Splines"),
		FEditLayerAction::FExecuteDelegate::CreateWeakLambda(this, [](const FEditLayerAction::FExecuteParams& InParams)
		{
			InParams.GetLandscape()->UpdateLandscapeSplines(FGuid(), /*bUpdateOnlySelection = */false, /*bForceUpdate =*/true);
			return FEditLayerAction::FExecuteResult(/*bInSuccess = */true);
		}),
		FEditLayerAction::FCanExecuteDelegate::CreateWeakLambda(this, [](const FEditLayerAction::FExecuteParams& InParams, FText& OutReason)
		{
			const ULandscapeEditLayerBase* EditLayer = InParams.GetEditLayer();
			check(EditLayer != nullptr);
			if (EditLayer->IsLocked())
			{
				OutReason = FText::Format(LOCTEXT("LandscapeEditLayerSplines_CannotUpdateSplinesOnLockedLayer", "Cannot update splines on layer '{0}' : the layer is currently locked"), FText::FromName(EditLayer->GetName()));
				return false;
			}

			OutReason = LOCTEXT("LandscapeEditLayerSplines_UpdateSplines_Tooltip", "Update Landscape Splines");
			return true;
		})));
#endif // WITH_EDITOR

	return Actions;
}

#undef LOCTEXT_NAMESPACE
