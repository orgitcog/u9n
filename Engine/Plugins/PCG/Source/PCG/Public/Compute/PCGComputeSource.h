// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/IPCGNodeSourceTextProvider.h"

#include "ComputeFramework/ComputeSource.h"

#include "PCGComputeSource.generated.h"

#if WITH_EDITOR
class UPCGComputeSource;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGComputeSourceModified, const UPCGComputeSource*);
#endif // WITH_EDITOR

UCLASS()
class PCG_API UPCGComputeSource : public UComputeSource, public IPCGNodeSourceTextProvider
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	//~Begin UObject interface
#endif

	//~ Begin UComputeSource Interface.
	FString GetSource() const override;
	FString GetVirtualPath() const override;
	//~ End UComputeSource Interface.

#if WITH_EDITOR
	//~Begin IPCGNodeSourceTextProvider interface
	FString GetShaderText() const override { return GetSource(); }
	FString GetDeclarationsText() const override { return {}; }
	FString GetShaderFunctionsText() const override { return {}; }
	void SetShaderFunctionsText(const FString& NewFunctionsText) override {}
	void SetShaderText(const FString& NewText) override;
	bool IsShaderTextReadOnly() const override { return false; }
	//~End IPCGNodeSourceTextProvider interface

	static FOnPCGComputeSourceModified OnModifiedDelegate;
#endif

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY(EditAnywhere, Category = "Source", meta = (DisplayAfter = "AdditionalSources"))
	FString Source;
#endif
};
