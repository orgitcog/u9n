// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"
#include "Retargeter/RetargetOps/AlignPoleVectorOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/IKChainsOp.h"
#include "Retargeter/RetargetOps/RetargetPoseOp.h"
#include "Retargeter/RetargetOps/SpeedPlantingOp.h"
#include "RigEditor/IKRigStructViewer.h"
#include "Templates/SharedPointer.h"
#include "IKRetargetOpDetails.generated.h"

enum class EFKChainRotationMode : uint8;
class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;

class FIKRetargetOpBaseSettingsCustomization : public IPropertyTypeCustomization
{
public:

	// IPropertyTypeCustomization
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		FDetailWidgetRow& InHeaderRow,
		IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override {};
	// End IPropertyTypeCustomization
	
	bool LoadAndValidateStructToCustomize(
		const TSharedRef<IPropertyHandle>& InStructPropertyHandle,
		const IPropertyTypeCustomizationUtils& InStructCustomizationUtils);

	static void AddChildPropertyToGroup(
		IDetailGroup& InGroup,
		const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
		const FName& InChildPropertyName);

	static void AddChildPropertiesToCategoryGroups(
		const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder);

	static void AddChildPropertiesInCategory(
		const TSharedRef<IPropertyHandle>& InParentPropertyHandle,
		IDetailChildrenBuilder& InChildBuilder,
		const FName& InCategoryName,
		const TArray<FName>& InPropertiesToIgnore = {});

protected:

	// the op that owns the settings being customized / edited
	FName OpName;
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	UIKRigStructViewer* StructViewer;
	UIKRetargeter* RetargetAsset;
	UIKRetargeterController* AssetController;
};

//
// FK Chain Op Customization
//

class FChainsFKOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FChainsFKOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

UCLASS(BlueprintType)
class URetargetFKChainSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "FK Chain Settings")
	FRetargetFKChainSettings Settings;
};

//
// Run IK Op Customization
//

class FRunIKRigOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FRunIKRigOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

//
// IK Chains Op Customization
//

class FIKChainOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FIKChainOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

UCLASS(BlueprintType)
class URetargetIKChainSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "IK Chain Settings")
	FRetargetIKChainSettings Settings;
};

//
// Stride Warp Op Customization
//

class FStrideWarpOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FStrideWarpOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

UCLASS(BlueprintType)
class URetargetStrideWarpSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Stride Warp Settings")
	FRetargetStrideWarpChainSettings Settings;
};

//
// Speed Plant Op Customization
//

class FSpeedPlantOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FSpeedPlantOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

UCLASS(BlueprintType)
class URetargetSpeedPlantSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Stride Warp Settings")
	FRetargetSpeedPlantingSettings Settings;
};

//
// Pole Vector Op Customization
//

class FPoleVectorOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FPoleVectorOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization
};

UCLASS(BlueprintType)
class UPoleVectorSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Pole Vector Settings")
	FRetargetPoleVectorSettings Settings;
};

//
// Retarget Pose Op Customization
//

class FAdditivePoseOpCustomization : public FIKRetargetOpBaseSettingsCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FAdditivePoseOpCustomization>();
	}

	// IPropertyTypeCustomization
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End IPropertyTypeCustomization

private:
	
	void UpdatePoseNameOptions();

	TSharedPtr<IPropertyHandle> PoseToApplyProperty;
	
	TSharedPtr<FName> CurrentPoseOption;
	TArray<TSharedPtr<FName>> PoseNameOptions;
};

UCLASS(BlueprintType)
class URetargetPoseOpSettingsWrapper : public UIKRigStructWrapperBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Retarget Pose Settings")
	FIKRetargetAdditivePoseOpSettings Settings;
};
