// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRCControllerId.h"
#include "DataLinkInstance.h"
#include "UObject/Object.h"
#include "AvaDataLinkInstance.generated.h"

class FDataLinkExecutor;
class IAvaSceneInterface;
class URCController;
enum class EDataLinkExecutionResult : uint8;
struct FConstStructView;
struct FStructView;

USTRUCT(BlueprintType, DisplayName="Motion Design Data Link RC Controller Mapping")
struct FAvaDataLinkControllerMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Data Link")
	FString OutputFieldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Data Link")
	FAvaRCControllerId TargetController;
};

UCLASS(MinimalAPI, EditInlineNew)
class UAvaDataLinkInstance : public UObject
{
	GENERATED_BODY()

public:
	void Execute();

	static FName GetDataLinkInstancePropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UAvaDataLinkInstance, DataLinkInstance);
	}

	static FName GetControllerMappingsPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UAvaDataLinkInstance, ControllerMappings);
	}

private:
#if WITH_DATALINK_CONTEXT
	FString BuildContextName() const;
#endif

	void OnExecutionFinished(const FDataLinkExecutor& InExecutor, EDataLinkExecutionResult InResult, FConstStructView InOutputDataView);

	IAvaSceneInterface* GetSceneInterface() const;

	URemoteControlPreset* GetRemoteControlPreset() const;

	struct FResolvedController
	{
		const FAvaDataLinkControllerMapping* Mapping;
		TObjectPtr<URCController> Controller;
		FProperty* TargetProperty;
		uint8* TargetMemory;
	};
	void ForEachResolvedController(const FDataLinkExecutor& InExecutor, URemoteControlPreset* InPreset, FStructView InTargetDataView, TFunctionRef<void(const FResolvedController&)> InFunction);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Design Data Link", meta=(AllowPrivateAccess="true"))
	FDataLinkInstance DataLinkInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Design Data Link", DisplayName="Output Field to Controller Mappings", meta=(AllowPrivateAccess="true"))
	TArray<FAvaDataLinkControllerMapping> ControllerMappings;

	TSharedPtr<FDataLinkExecutor> Executor;
};
