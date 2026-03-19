// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimDetailsProxyBase.h"

#include "AnimDetailsProxyFloat.generated.h"

/** 
 * A floating point value in anim details  
 * 
 * Note, control rig uses 'float' controls so we call this float though it's a 
 * double internally, so we can use same for non-control rig parameters
 */
USTRUCT(BlueprintType)
struct FAnimDetailsFloat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Interp, Category = "Float", meta = (SliderExponent = "1.0"))
	double Float = 0.0;
};

/** Handles an floating point property bound in sequencer, and the related control if the bound object uses a control rig */
UCLASS(EditInlineNew, CollapseCategories)
class UAnimDetailsProxyFloat 
	: public UAnimDetailsProxyBase
{
	GENERATED_BODY()

public:
	//~ Begin UAnimDetailsProxyBase interface
	virtual FName GetCategoryName() const override;
	virtual void UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder);
	virtual TArray<FName> GetPropertyNames() const override;
	virtual bool PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty) override;
	virtual void UpdateProxyValues() override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& InPropertyName) const override;
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const override;
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) override;
	virtual void SetBindingValueFromCurrent(UObject* InObject, const TSharedPtr<FTrackInstancePropertyBindings>& Binding, const FRigControlModifiedContext& Context, bool bInteractive = false) override;
	//~ End UAnimDetailsProxyBase interface

	UPROPERTY(EditAnywhere, Interp, Category = "Float", meta = (ShowOnlyInnerProperties, Delta = "0.05", SliderExponent = "1", LinearDeltaSensitivity = "1"))
	FAnimDetailsFloat Float;
};
