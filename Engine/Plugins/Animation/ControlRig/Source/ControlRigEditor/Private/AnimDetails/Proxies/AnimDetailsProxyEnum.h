// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimDetailsProxyBase.h"

#include "AnimDetailsProxyEnum.generated.h"

/** An enum value in anim details */
USTRUCT(BlueprintType)
struct FAnimDetailsEnum
{
	GENERATED_USTRUCT_BODY()

	FAnimDetailsEnum()
	{
		EnumType = nullptr;
		EnumIndex = INDEX_NONE;
	}

	UPROPERTY()
	TObjectPtr<UEnum> EnumType;

	UPROPERTY(EditAnywhere, Category = Enum)
	int32 EnumIndex;
};

/** Handles an enum property bound in sequencer, and the related control if the bound object uses a control rig */
UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailsProxyEnum 
	: public UAnimDetailsProxyBase
{
	GENERATED_BODY()

public:
	//~ Begin UAnimDetailsProxyBase interface
	virtual FName GetCategoryName() const override;
	virtual FName GetDetailRowID() const override;
	virtual void UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder);
	virtual TArray<FName> GetPropertyNames() const override;
	virtual bool PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty) override;
	virtual void UpdateProxyValues() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	//~ End UAnimDetailsProxyBase interface

	UPROPERTY(EditAnywhere, Interp, Meta = (ShowOnlyInnerProperties), Category = "Enum")
	FAnimDetailsEnum Enum;
};
