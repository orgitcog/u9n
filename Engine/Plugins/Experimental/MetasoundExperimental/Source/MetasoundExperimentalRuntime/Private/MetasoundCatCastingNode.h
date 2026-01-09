// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "Templates/SharedPointer.h"
#include "TypeFamily/ChannelTypeFamily.h"
#include "UObject/PropertyText.h"

#include "MetasoundCatCastingNode.generated.h"

UCLASS()
class UMetasoundCatCastingOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetCastingOptions();
};

UENUM()
enum class EMetasoundCatCastingMethod : uint8
{
	ChannelDrop = static_cast<uint8>(Audio::EChannelTranscodeMethod::ChannelDrop),
	MixUpOrDown = static_cast<uint8>(Audio::EChannelTranscodeMethod::MixUpOrDown)
};

UENUM()
enum class EMetasoundChannelMapMonoUpmixMethod : uint8
{
	Linear = static_cast<uint8>(Audio::EChannelMapMonoUpmixMethod::Linear),
	EqualPower = static_cast<uint8>(Audio::EChannelMapMonoUpmixMethod::EqualPower),
	FullVolume = static_cast<uint8>(Audio::EChannelMapMonoUpmixMethod::FullVolume)
};

USTRUCT()
struct FMetaSoundCatCastingNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCatCastingNodeConfiguration() =  default;

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions="MetasoundCatCastingOptionsHelper.GetCastingOptions"))
	FName ToType = TEXT("Mono");
	
	UPROPERTY(EditAnywhere, Category = General)
	EMetasoundCatCastingMethod TranscodeMethod = EMetasoundCatCastingMethod::ChannelDrop;

	UPROPERTY(EditAnywhere, Category = General, meta = (EditCondition = "TranscodeMethod == EMetasoundCatCastingMethod::MixUpOrDown", EditCOonditionHides))
	EMetasoundChannelMapMonoUpmixMethod MixMethod = EMetasoundChannelMapMonoUpmixMethod::EqualPower; 

	/* Get the current interface for the class based upon the node extension */
	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InNodeClass) const override;

	/* Pass data down to the operator. */
	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;
};
