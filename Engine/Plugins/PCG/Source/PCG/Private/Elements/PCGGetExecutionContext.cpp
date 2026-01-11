// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGGetExecutionContext.h"

#include "PCGComponent.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGetExecutionContext)

#define LOCTEXT_NAMESPACE "PCGGetExecutionContextElement"

namespace PCGGetExecutionContextConstants
{
	static const FName AttributeName = TEXT("Info");
}

#if WITH_EDITOR
FText UPCGGetExecutionContextSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetExecutionContextTooltip", "Returns some context-specific common information.");
}

EPCGChangeType UPCGGetExecutionContextSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;
}
#endif

TArray<FPCGPinProperties> UPCGGetExecutionContextSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FString UPCGGetExecutionContextSettings::GetAdditionalTitleInformation() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGGetExecutionContextMode>())
	{
		return FText::Format(LOCTEXT("AdditionalTitle", "Get {0}"), EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Mode))).ToString();
	}
	else
	{
		return FString();
	}
}

FPCGElementPtr UPCGGetExecutionContextSettings::CreateElement() const
{
	return MakeShared<FPCGGetExecutionContextElement>();
}

bool FPCGGetExecutionContextElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGetExecutionContextElement::Execute);

	check(Context);

	const UPCGGetExecutionContextSettings* Settings = Context->GetInputSettings<UPCGGetExecutionContextSettings>();
	check(Settings);

	const EPCGGetExecutionContextMode Mode = Settings->Mode;

	UPCGParamData* ParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	check(ParamData && ParamData->Metadata);

	FPCGTaggedData& OutputData = Context->OutputData.TaggedData.Emplace_GetRef();
	OutputData.Data = ParamData;

	const IPCGGraphExecutionSource* ExecutionSource = Context->ExecutionSource.Get();
	const UPCGComponent* SourceComponent = Cast<UPCGComponent>(ExecutionSource);

	bool Value = false;
	const UWorld* SupportingWorld = ExecutionSource ? ExecutionSource->GetExecutionState().GetWorld() : nullptr;

	if (Mode == EPCGGetExecutionContextMode::IsEditor || Mode == EPCGGetExecutionContextMode::IsRuntime)
	{
		const bool bContextIsRuntimeOrOnRuntimeGenerationMode = (SupportingWorld && SupportingWorld->IsGameWorld()) || PCGHelpers::IsRuntimeGeneration(ExecutionSource);
		Value = (bContextIsRuntimeOrOnRuntimeGenerationMode == (Mode == EPCGGetExecutionContextMode::IsRuntime));
	}
	else if (Mode == EPCGGetExecutionContextMode::IsOriginal || Mode == EPCGGetExecutionContextMode::IsLocal)
	{
		Value = SourceComponent && (SourceComponent->IsLocalComponent() == (Mode == EPCGGetExecutionContextMode::IsLocal));
	}
	else if (Mode == EPCGGetExecutionContextMode::IsPartitioned)
	{
		Value = SourceComponent && SourceComponent->IsPartitioned();
	}
	else if (Mode == EPCGGetExecutionContextMode::IsRuntimeGeneration)
	{
		Value = PCGHelpers::IsRuntimeGeneration(ExecutionSource);
	}
	else if (Mode == EPCGGetExecutionContextMode::IsDedicatedServer)
	{
		Value = SupportingWorld && SupportingWorld->IsNetMode(NM_DedicatedServer);
	}
	else if (Mode == EPCGGetExecutionContextMode::HasAuthority)
	{
		Value = ExecutionSource && ExecutionSource->GetExecutionState().HasAuthority();
	}

	FPCGMetadataAttribute<bool>* Attribute = ParamData->Metadata->CreateAttribute<bool>(PCGGetExecutionContextConstants::AttributeName, Value, /*bAllowInterpolation=*/false, /*bOverrideParent=*/false);
	ParamData->Metadata->AddEntry();

	return true;
}

#undef LOCTEXT_NAMESPACE