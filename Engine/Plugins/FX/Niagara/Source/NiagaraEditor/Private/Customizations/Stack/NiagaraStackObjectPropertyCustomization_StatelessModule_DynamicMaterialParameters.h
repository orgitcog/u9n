// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticBitArray.h"
#include "Customizations/NiagaraStackObjectPropertyCustomization.h"

class FNiagaraEmitterHandleViewModel;
class FNiagaraEmitterInstance;
class UNiagaraStatelessEmitter;
class UNiagaraStackPropertyRow;
class UMaterial;
class FDetailTreeNode;
struct FExpressionInput;


class NIAGARAEDITOR_API FNiagaraStackObjectPropertyCustomization_StatelessModule_DynamicMaterialParameters
	: public FNiagaraStackObjectPropertyCustomization
{
public:
	FNiagaraStackObjectPropertyCustomization_StatelessModule_DynamicMaterialParameters();

	static TSharedRef<FNiagaraStackObjectPropertyCustomization> MakeInstance();
	
	virtual TOptional<TSharedPtr<SWidget>> GenerateNameWidget(UNiagaraStackPropertyRow* PropertyRow) const override;

private:
	TOptional<FText> TryGetDisplayNameForDynamicMaterialParameter(TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel, int32 ParameterIndex, int32 ParameterChannel) const;
	
	void GetChannelUsedBitMask(FExpressionInput* Input, TStaticBitArray<4>& ChannelUsedMask) const;
	TArray<UMaterial*> GetMaterialsFromEmitter(const UNiagaraStatelessEmitter& InEmitter, const FNiagaraEmitterInstance* InEmitterInstance) const;
	void GetParameterIndexAndChannel(TSharedRef<FDetailTreeNode> DetailTreeNode, int32& OutParameterIndex, int32& OutParameterChannel) const;

private:
	TMap<FName, int32> ParameterIndexMap;
	TMap<FName, int32> ParameterChannelMap;
};
