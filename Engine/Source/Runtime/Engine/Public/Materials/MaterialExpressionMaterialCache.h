// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialExpressionCustomOutput.h"
#include "MaterialExpressionMaterialCache.generated.h"

UCLASS(meta = (DisplayName = "MaterialCache"))
class UMaterialExpressionMaterialCache : public UMaterialExpressionCustomOutput
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Value;
	
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput UV;
	
	virtual FString GetFunctionName() const override { return TEXT("GetMaterialCache"); }
	virtual FString GetDisplayName() const override { return TEXT("MaterialCache"); }

	UMaterialExpressionMaterialCache(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override;
	virtual int32 GetNumOutputs() const override { return 2u; }
	virtual int32 GetMaxOutputs() const override { return 2u; }
#endif // WITH_EDITOR
};
