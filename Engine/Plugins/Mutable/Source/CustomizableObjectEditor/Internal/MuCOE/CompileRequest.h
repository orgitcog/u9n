// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"

class UCustomizableObject;


struct CUSTOMIZABLEOBJECTEDITOR_API FCompilationRequest
{
	FCompilationRequest(UCustomizableObject& CustomizableObject);

	UCustomizableObject* GetCustomizableObject();
	
	void SetDerivedDataCachePolicy(UE::DerivedData::ECachePolicy InCachePolicy);
	UE::DerivedData::ECachePolicy GetDerivedDataCachePolicy() const;

	void BuildDerivedDataCacheKey();
	UE::DerivedData::FCacheKey GetDerivedDataCacheKey() const;

	void SetCompilationState(ECompilationStatePrivate InState, ECompilationResultPrivate InResult);

	ECompilationStatePrivate GetCompilationState() const;
	ECompilationResultPrivate GetCompilationResult() const;
	
	bool operator==(const FCompilationRequest& Other) const;

private:
	TWeakObjectPtr<UCustomizableObject> CustomizableObject;

	ECompilationStatePrivate State = ECompilationStatePrivate::None;
	ECompilationResultPrivate Result = ECompilationResultPrivate::Unknown;
	
	UE::DerivedData::ECachePolicy DDCPolicy = UE::DerivedData::ECachePolicy::None;
	UE::DerivedData::FCacheKey DDCKey;

public:
	FCompilationOptions Options;

	TArray<FText> Warnings;
	TArray<FText> Errors;

	FCompileDelegate Callback;
	FCompileNativeDelegate CallbackNative;

	bool bAsync = true;
	bool bSkipIfCompiled = false;
	bool bSkipIfOutOfDate = false;
	bool bSilentCompilation = true;
};

