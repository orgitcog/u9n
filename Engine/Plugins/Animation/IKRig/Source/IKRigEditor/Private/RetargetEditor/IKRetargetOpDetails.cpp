// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetOpDetails.h"

#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "Widgets/Input/SComboBox.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "RetargetEditor/SIKRetargetChainMapList.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/AlignPoleVectorOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/IKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/SpeedPlantingOp.h"
#include "RigEditor/IKRigStructViewer.h"

#define LOCTEXT_NAMESPACE "IKRetargetOpDetails"

bool FIKRetargetOpBaseSettingsCustomization::LoadAndValidateStructToCustomize(
	const TSharedRef<IPropertyHandle>& InStructPropertyHandle,
	const IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	SelectedObjects = InStructCustomizationUtils.GetPropertyUtilities().Get()->GetSelectedObjects();
	if (!ensure(!SelectedObjects.IsEmpty()))
	{
		return false;
	}

	void* StructMemory = nullptr;
	FPropertyAccess::Result Result = InStructPropertyHandle->GetValueData(StructMemory);
	if (!ensure(Result == FPropertyAccess::Success && StructMemory))
	{
		return false;
	}

	FIKRetargetOpSettingsBase* SettingsBeingCustomized = static_cast<FIKRetargetOpSettingsBase*>(StructMemory);
	OpName = SettingsBeingCustomized->OwningOpName;
	if (OpName == NAME_None)
	{
		return false;
	}
	
	StructViewer = Cast<UIKRigStructViewer>(SelectedObjects[0].Get());
	if (StructViewer == nullptr)
	{
		return false;
	}
	RetargetAsset = CastChecked<UIKRetargeter>(StructViewer->GetStructOwner());
	AssetController = UIKRetargeterController::GetController(RetargetAsset);

	return true;
}

void FIKRetargetOpBaseSettingsCustomization::AddChildPropertyToGroup(
	IDetailGroup& InGroup,
	const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
	const FName& InChildPropertyName)
{
	TSharedPtr<IPropertyHandle> PropertyHandle = InParentPropertyHandle->GetChildHandle(InChildPropertyName);
	if (!ensure(PropertyHandle.IsValid()))
	{
		return;
	}
	
	InGroup.AddPropertyRow(PropertyHandle.ToSharedRef());
}

void FIKRetargetOpBaseSettingsCustomization::AddChildPropertiesToCategoryGroups(
	const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder)
{
	uint32 NumChildren;
	InParentPropertyHandle->GetNumChildren(NumChildren);

	// map of category names to property groups
	TMap<FName, IDetailGroup*> CategoryGroups;
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = InParentPropertyHandle->GetChildHandle(ChildIndex);
		if (!ChildHandle.IsValid())
		{
			continue;
		}
		
		FProperty* ChildProperty = ChildHandle->GetProperty();
		if (!ChildProperty)
		{
			continue;
		}

		// get category from metadata
		FString CategoryName = ChildProperty->GetMetaData(TEXT("Category"));
		if (CategoryName.IsEmpty())
		{
			// if no category specified, add directly to builder
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			continue;
		}

		// convert to FName for map key
		FName CategoryFName(*CategoryName);

		// Get or create the group for this category
		IDetailGroup* Group;
		if (IDetailGroup** ExistingGroup = CategoryGroups.Find(CategoryFName))
		{
			Group = *ExistingGroup;
		}
		else
		{
			// create new group
			FText CategoryText = FText::FromString(CategoryName);
			Group = &ChildBuilder.AddGroup(CategoryFName, CategoryText);
			CategoryGroups.Add(CategoryFName, Group);
		}

		// sdd the property to its category group
		Group->AddPropertyRow(ChildHandle.ToSharedRef());
	}
}

void FIKRetargetOpBaseSettingsCustomization::AddChildPropertiesInCategory(
	const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
	IDetailChildrenBuilder& InChildBuilder,
	const FName& InCategoryName,
	const TArray<FName>& InPropertiesToIgnore)
{
	uint32 NumChildren;
	InParentPropertyHandle->GetNumChildren(NumChildren);

	IDetailGroup* CategoryGroup = nullptr;
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = InParentPropertyHandle->GetChildHandle(ChildIndex);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		FProperty* ChildProperty = ChildHandle->GetProperty();
		if (!ChildProperty)
		{
			continue;
		}

		if (InPropertiesToIgnore.Contains(ChildProperty->GetName()))
		{
			continue; // filtered out by name
		}
		
		FString CategoryName = ChildProperty->GetMetaData(TEXT("Category"));
		if (CategoryName.IsEmpty() || InCategoryName != CategoryName)
		{
			continue;  // not in the category we're looking for
		}

		// create group if not created yet
		if (!CategoryGroup)
		{
			static bool bStartExpanded = true;
			CategoryGroup = &InChildBuilder.AddGroup(InCategoryName, FText::FromName(InCategoryName), bStartExpanded);
		}

		// Add the property to the category group
		IDetailPropertyRow& NewRow = CategoryGroup->AddPropertyRow(ChildHandle.ToSharedRef());
		NewRow.ShouldAutoExpand(true);
	}

	if (CategoryGroup)
	{
		CategoryGroup->ToggleExpansion(true);
	}
}

void FChainsFKOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}

	// add the IK Rig asset input field
	TSharedPtr<IPropertyHandle> IKRigHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FIKRetargetFKChainsOpSettings, IKRigAsset));
	ChildBuilder.AddProperty(IKRigHandle.ToSharedRef());

	// configure the chain map list view for FK chains
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = OpName;
	ChainListConfig.OpWithChainMapping = OpName;
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = false;
	ChainListConfig.bEnableChainMapping = true;
	ChainListConfig.ChainSettingsGetterFunc = [this](const FName InTargetChainName) -> UObject*
	{
		// this lambda fetches the FK chain settings struct for the given target chain in the given op (by name)
		auto MemoryProvider = [this, InTargetChainName]() -> uint8*
		{
			if (FIKRetargetOpBase* Op = AssetController->GetRetargetOpByName(OpName))
			{
				FIKRetargetFKChainsOpSettings* OpSettings = reinterpret_cast<FIKRetargetFKChainsOpSettings*>(Op->GetSettings());
				for (FRetargetFKChainSettings& ChainToRetarget : OpSettings->ChainsToRetarget)
				{
					if (ChainToRetarget.TargetChainName == InTargetChainName)
					{
						return reinterpret_cast<uint8*>(&ChainToRetarget);
					}
				}
					
			}
			return nullptr;
		};
			
		FIKRigStructToView StructToView;
		StructToView.Owner = AssetController->GetAsset();
		StructToView.Type = FRetargetFKChainSettings::StaticStruct();
		StructToView.MemoryProvider = MemoryProvider;
		StructToView.UniqueName = OpName;
		
		URetargetFKChainSettingsWrapper* Wrapper = NewObject<URetargetFKChainSettingsWrapper>(GetTransientPackage(), NAME_None, RF_Standalone);
		const FName SettingsPropertyName = GET_MEMBER_NAME_CHECKED(URetargetFKChainSettingsWrapper, Settings);
		Wrapper->InitializeWithRetargeter(StructToView, SettingsPropertyName, AssetController);
		Wrapper->SetPropertyHidden(GET_MEMBER_NAME_CHECKED(FRetargetFKChainSettings, TargetChainName), true);
		return Wrapper;
	};

	// add the chain mapping list
	const FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];

	// add the debug properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Debug"));
}

void FRunIKRigOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}

	// add the IK Rig asset input field
	TSharedPtr<IPropertyHandle> IKRigHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FIKRetargetFKChainsOpSettings, IKRigAsset));
	ChildBuilder.AddProperty(IKRigHandle.ToSharedRef());

	// show only IK chains by default
	FChainMapFilterOptions ShowOnlyIKFilter;
	ShowOnlyIKFilter.bNeverShowChainsWithoutIK = true;

	// configure chain map list for the IK op
	// NOTE: no settings memory provider because this op only maps chains, it doesn't store any settings for them
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = NAME_None;
	ChainListConfig.OpWithChainMapping = OpName;
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = true;
	ChainListConfig.bEnableChainMapping = true;
	ChainListConfig.Filter = ShowOnlyIKFilter;

	// add the chain mapping list
	FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];

	// add the debug properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Debug"));
}

void FIKChainOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}
	
	// show only IK chains
	FChainMapFilterOptions ShowOnlyIKFilter;
	ShowOnlyIKFilter.bNeverShowChainsWithoutIK = true;

	// configure chain map list for the IK op
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = OpName;
	ChainListConfig.OpWithChainMapping = AssetController->GetParentOpByName(OpName); // use chain mapping from parent
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = true;
	ChainListConfig.bEnableChainMapping = false;
	ChainListConfig.Filter = ShowOnlyIKFilter;
	ChainListConfig.ChainSettingsGetterFunc = [this](const FName InTargetChainName) -> UObject*
	{
		// this lambda fetches the chain settings struct for the given target chain in the given op (by name)
		auto MemoryProvider = [this, InTargetChainName]() -> uint8*
		{
			if (FIKRetargetOpBase* Op = AssetController->GetRetargetOpByName(OpName))
			{
				FIKRetargetIKChainsOpSettings* OpSettings = reinterpret_cast<FIKRetargetIKChainsOpSettings*>(Op->GetSettings());
				for (FRetargetIKChainSettings& ChainToRetarget : OpSettings->ChainsToRetarget)
				{
					if (ChainToRetarget.TargetChainName == InTargetChainName)
					{
						return reinterpret_cast<uint8*>(&ChainToRetarget);
					}
				}
			}
			return nullptr;
		};
			
		FIKRigStructToView StructToView;
		StructToView.Owner = AssetController->GetAsset();
		StructToView.Type = FRetargetIKChainSettings::StaticStruct();
		StructToView.MemoryProvider = MemoryProvider;
		StructToView.UniqueName = OpName;
		
		URetargetIKChainSettingsWrapper* Wrapper = NewObject<URetargetIKChainSettingsWrapper>(GetTransientPackage(), NAME_None, RF_Standalone);
		const FName SettingsPropertyName = GET_MEMBER_NAME_CHECKED(URetargetIKChainSettingsWrapper, Settings);
		Wrapper->InitializeWithRetargeter(StructToView, SettingsPropertyName, AssetController);
		Wrapper->SetPropertyHidden(GET_MEMBER_NAME_CHECKED(FRetargetIKChainSettings, TargetChainName), true);
		return Wrapper;
	};

	// add the chain mapping list
	FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];

	// add the debug properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Debug"));
}

void FStrideWarpOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}
	
	// show only IK chains
	FChainMapFilterOptions ShowOnlyIKFilter;
	ShowOnlyIKFilter.bNeverShowChainsWithoutIK = true;

	// configure chain map list for the IK op
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = OpName;
	ChainListConfig.OpWithChainMapping = AssetController->GetParentOpByName(OpName); // use chain mapping from parent
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = true;
	ChainListConfig.bEnableChainMapping = false;
	ChainListConfig.Filter = ShowOnlyIKFilter;
	ChainListConfig.ChainSettingsGetterFunc = [this](const FName InTargetChainName) -> UObject*
	{
		// this lambda fetches the chain settings struct for the given target chain in the given op (by name)
		auto MemoryProvider = [this, InTargetChainName]() -> uint8*
		{
			if (FIKRetargetOpBase* Op = AssetController->GetRetargetOpByName(OpName))
			{
				FIKRetargetStrideWarpingOpSettings* OpSettings = reinterpret_cast<FIKRetargetStrideWarpingOpSettings*>(Op->GetSettings());
				for (FRetargetStrideWarpChainSettings& ChainToWarp : OpSettings->ChainSettings)
				{
					if (ChainToWarp.TargetChainName == InTargetChainName)
					{
						return reinterpret_cast<uint8*>(&ChainToWarp);
					}
				}
			}
			return nullptr;
		};
			
		FIKRigStructToView StructToView;
		StructToView.Owner = AssetController->GetAsset();
		StructToView.Type = FRetargetStrideWarpChainSettings::StaticStruct();
		StructToView.MemoryProvider = MemoryProvider;
		StructToView.UniqueName = OpName;
		
		URetargetStrideWarpSettingsWrapper* Wrapper = NewObject<URetargetStrideWarpSettingsWrapper>(GetTransientPackage(), NAME_None, RF_Standalone);
		const FName SettingsPropertyName = GET_MEMBER_NAME_CHECKED(URetargetStrideWarpSettingsWrapper, Settings);
		Wrapper->InitializeWithRetargeter(StructToView, SettingsPropertyName, AssetController);
		Wrapper->SetPropertyHidden(GET_MEMBER_NAME_CHECKED(FRetargetStrideWarpChainSettings, TargetChainName), true);
		return Wrapper;
	};

	// add the chain mapping list
	FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];

	// add the warping properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Warping"));
	// add the debug properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Debug"));
}

void FSpeedPlantOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}
	
	// show only IK chains
	FChainMapFilterOptions ShowOnlyIKFilter;
	ShowOnlyIKFilter.bNeverShowChainsWithoutIK = true;

	// configure chain map list for the IK op
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = OpName;
	ChainListConfig.OpWithChainMapping = AssetController->GetParentOpByName(OpName); // use chain mapping from parent
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = true;
	ChainListConfig.bEnableChainMapping = false;
	ChainListConfig.Filter = ShowOnlyIKFilter;
	ChainListConfig.ChainSettingsGetterFunc = [this](const FName InTargetChainName) -> UObject*
	{
		// this lambda fetches the chain settings struct for the given target chain in the given op (by name)
		auto MemoryProvider = [this, InTargetChainName]() -> uint8*
		{
			if (FIKRetargetOpBase* Op = AssetController->GetRetargetOpByName(OpName))
			{
				FIKRetargetSpeedPlantingOpSettings* OpSettings = reinterpret_cast<FIKRetargetSpeedPlantingOpSettings*>(Op->GetSettings());
				for (FRetargetSpeedPlantingSettings& ChainToWarp : OpSettings->ChainsToSpeedPlant)
				{
					if (ChainToWarp.TargetChainName == InTargetChainName)
					{
						return reinterpret_cast<uint8*>(&ChainToWarp);
					}
				}
			}
			return nullptr;
		};
			
		FIKRigStructToView StructToView;
		StructToView.Owner = AssetController->GetAsset();
		StructToView.Type = FRetargetSpeedPlantingSettings::StaticStruct();
		StructToView.MemoryProvider = MemoryProvider;
		StructToView.UniqueName = OpName;
		
		URetargetSpeedPlantSettingsWrapper* Wrapper = NewObject<URetargetSpeedPlantSettingsWrapper>(GetTransientPackage(), NAME_None, RF_Standalone);
		const FName SettingsPropertyName = GET_MEMBER_NAME_CHECKED(URetargetSpeedPlantSettingsWrapper, Settings);
		Wrapper->InitializeWithRetargeter(StructToView, SettingsPropertyName, AssetController);
		Wrapper->SetPropertyHidden(GET_MEMBER_NAME_CHECKED(FRetargetSpeedPlantingSettings, TargetChainName), true);
		return Wrapper;
	};

	// add the chain mapping list
	FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];

	// add speed planting properties
	AddChildPropertiesInCategory(StructPropertyHandle, ChildBuilder, FName("Speed Planting"));
}

void FPoleVectorOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}

	// add the IK Rig asset input field
	TSharedPtr<IPropertyHandle> IKRigHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FIKRetargetAlignPoleVectorOpSettings, IKRigAsset));
	ChildBuilder.AddProperty(IKRigHandle.ToSharedRef());

	// configure chain map list for the IK op
	FChainMapListConfig ChainListConfig;
	ChainListConfig.OpWithChainSettings = OpName;
	ChainListConfig.OpWithChainMapping = OpName;
	ChainListConfig.Controller = AssetController;
	ChainListConfig.bEnableGoalColumn = true;
	ChainListConfig.bEnableChainMapping = true;
	ChainListConfig.ChainSettingsGetterFunc = [this](const FName InTargetChainName) -> UObject*
	{
		// this lambda fetches the chain settings struct for the given target chain in the given op (by name)
		auto MemoryProvider = [this, InTargetChainName]() -> uint8*
		{
			if (FIKRetargetOpBase* Op = AssetController->GetRetargetOpByName(OpName))
			{
				FIKRetargetAlignPoleVectorOpSettings* OpSettings = reinterpret_cast<FIKRetargetAlignPoleVectorOpSettings*>(Op->GetSettings());
				for (FRetargetPoleVectorSettings& ChainToAlign : OpSettings->ChainsToAlign)
				{
					if (ChainToAlign.TargetChainName == InTargetChainName)
					{
						return reinterpret_cast<uint8*>(&ChainToAlign);
					}
				}
			}
			return nullptr;
		};
			
		FIKRigStructToView StructToView;
		StructToView.Owner = AssetController->GetAsset();
		StructToView.Type = FRetargetPoleVectorSettings::StaticStruct();
		StructToView.MemoryProvider = MemoryProvider;
		StructToView.UniqueName = OpName;
		
		UPoleVectorSettingsWrapper* Wrapper = NewObject<UPoleVectorSettingsWrapper>(GetTransientPackage(), NAME_None, RF_Standalone);
		const FName SettingsPropertyName = GET_MEMBER_NAME_CHECKED(UPoleVectorSettingsWrapper, Settings);
		Wrapper->InitializeWithRetargeter(StructToView, SettingsPropertyName, AssetController);
		Wrapper->SetPropertyHidden(GET_MEMBER_NAME_CHECKED(FRetargetPoleVectorSettings, TargetChainName), true);
		return Wrapper;
	};

	// add the chain mapping list
	FText ChainMapRowName = FText::FromString("Chain Map");
	ChildBuilder.AddCustomRow(ChainMapRowName)
	.WholeRowContent()
	[
		SNew(SIKRetargetChainMapList).InChainMapListConfig(ChainListConfig)
	];
}

void FAdditivePoseOpCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!LoadAndValidateStructToCustomize(StructPropertyHandle, StructCustomizationUtils))
	{
		return;
	}

	// store property handles for callbacks
	PoseToApplyProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FIKRetargetAdditivePoseOpSettings, PoseToApply));

	UpdatePoseNameOptions();

	// add dropdown menu to select retarget pose
	ChildBuilder.AddCustomRow(LOCTEXT("CurrentPoseLabel", "Pose To Apply"))
	.NameContent()
	[
		PoseToApplyProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200)
	[
		SNew(SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&PoseNameOptions)
		.InitiallySelectedItem(CurrentPoseOption)
		.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
		{
			return SNew(STextBlock).Text(FText::FromName(*InItem));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
		{
			if (NewSelection.IsValid())
			{
				PoseToApplyProperty->SetValue(*NewSelection);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]() {
				FName Value;
				PoseToApplyProperty->GetValue(Value);
				return FText::FromName(Value);
			})
		]
	];

	// add the alpha property
	TSharedPtr<IPropertyHandle> AlphaProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FIKRetargetAdditivePoseOpSettings, Alpha));
	ChildBuilder.AddProperty(AlphaProperty.ToSharedRef());
}

void FAdditivePoseOpCustomization::UpdatePoseNameOptions()
{
	// determine currently selected pose
	FName CurrentPoseName;
	PoseToApplyProperty->GetValue(CurrentPoseName);
	
	// get all the retarget poses
	const TMap<FName, FIKRetargetPose>& RetargetPoses = AssetController->GetRetargetPoses(ERetargetSourceOrTarget::Target);
	TArray<FName> PoseNames;
	RetargetPoses.GetKeys(PoseNames);

	// reset list of names
	PoseNameOptions.Reset(PoseNames.Num());

	// add all the other poses
	for (const FName& PoseName : PoseNames)
	{
		TSharedPtr<FName> PostNameOption = MakeShared<FName>(PoseName);
		if (PoseName == CurrentPoseName)
		{
			CurrentPoseOption = PostNameOption;
		}
		PoseNameOptions.Emplace(PostNameOption);
	}

	// default to first pose if stored pose no longer available
	if (!CurrentPoseOption.IsValid())
	{
		CurrentPoseOption = PoseNameOptions[0];
	}
}

#undef LOCTEXT_NAMESPACE
