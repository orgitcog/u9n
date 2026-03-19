// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateSpline.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSplineData.h"
#include "Elements/PCGSplineSampler.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCreateSpline)

#define LOCTEXT_NAMESPACE "PCGCreateSpline"

UPCGCreateSplineSettings::UPCGCreateSplineSettings()
{
	// Change the default for Arrive and Leave tangent
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		ArriveTangentAttribute = PCGSplineSamplerConstants::ArriveTangentAttributeName;
		LeaveTangentAttribute = PCGSplineSamplerConstants::LeaveTangentAttributeName;
	}
}

#if WITH_EDITOR
FText UPCGCreateSplineSettings::GetNodeTooltipText() const
{
	return LOCTEXT("CreateSplineTooltip", "Creates PCG spline data from the input PCG point data, in a sequential order.");
}
#endif

TArray<FPCGPinProperties> UPCGCreateSplineSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);

	return PinProperties;
}

FPCGElementPtr UPCGCreateSplineSettings::CreateElement() const
{
	return MakeShared<FPCGCreateSplineElement>();
}

bool FPCGCreateSplineElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	// Creating the spline component requires to run on the main thread, but if the settings/context aren't available we'll err on the side of caution.
	const UPCGCreateSplineSettings* Settings = Context ? Context->GetInputSettings<UPCGCreateSplineSettings>() : nullptr;
	return !Settings || Settings->Mode == EPCGCreateSplineMode::CreateComponent;
}

bool FPCGCreateSplineElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreateSplineSettings* Settings = Cast<const UPCGCreateSplineSettings>(InSettings);
	return !Settings || Settings->Mode == EPCGCreateSplineMode::CreateDataOnly;
}

bool FPCGCreateSplineElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateSplineElement::Execute);

	const UPCGCreateSplineSettings* Settings = Context->GetInputSettings<UPCGCreateSplineSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	EPCGCreateSplineMode Mode = Settings->Mode;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
		if (!TargetActor)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor. Ensure TargetActor member is initialized when creating SpatialData."));
			continue;
		}

		const UPCGBasePointData* PointData = SpatialData->ToBasePointData(Context);

		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
			continue;
		}

		const FPCGMetadataAttribute<FVector>* ArriveTangentAttribute = nullptr;
		const FPCGMetadataAttribute<FVector>* LeaveTangentAttribute = nullptr;

		if (Settings->bApplyCustomTangents)
		{
			const UPCGMetadata* PointMetadata = PointData->ConstMetadata();
			check(PointMetadata);

			FName LocalArriveTangentName = ((Settings->ArriveTangentAttribute == NAME_None) ? PointMetadata->GetLatestAttributeNameOrNone() : Settings->ArriveTangentAttribute);
			FName LocalLeaveTangentName = ((Settings->LeaveTangentAttribute == NAME_None) ? PointMetadata->GetLatestAttributeNameOrNone() : Settings->LeaveTangentAttribute);

			const FText AttributeMissingOrNotVector = LOCTEXT("AttributeMissingOrNotVector", "Attribute '{0}' does not exist or is not a vector");

			const FPCGMetadataAttributeBase* ArriveTangentBaseAttribute = PointMetadata->GetConstAttribute(LocalArriveTangentName);
			if (!ArriveTangentBaseAttribute || ArriveTangentBaseAttribute->GetTypeId() != PCG::Private::MetadataTypes<FVector>::Id)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(AttributeMissingOrNotVector, FText::FromString(LocalArriveTangentName.ToString())));
				continue;
			}

			const FPCGMetadataAttributeBase* LeaveTangentBaseAttribute = PointMetadata->GetConstAttribute(LocalLeaveTangentName);
			if (!LeaveTangentBaseAttribute || LeaveTangentBaseAttribute->GetTypeId() != PCG::Private::MetadataTypes<FVector>::Id)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(AttributeMissingOrNotVector, FText::FromString(LocalLeaveTangentName.ToString())));
				continue;
			}

			ArriveTangentAttribute = static_cast<const FPCGMetadataAttribute<FVector>*>(ArriveTangentBaseAttribute);
			LeaveTangentAttribute = static_cast<const FPCGMetadataAttribute<FVector>*>(LeaveTangentBaseAttribute);
		}

		UPCGSplineData* SplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(Context);
		SplineData->InitializeFromData(PointData);
		AActor* SplineActor = TargetActor;

		const int32 NumPoints = PointData->GetNumPoints();
		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(NumPoints);

		TArray<PCGMetadataEntryKey> SplineEntryKeys;
		SplineEntryKeys.Reserve(NumPoints);
		bool bHasAValidEntry = false;

		const FTransform SplineActorTransform = SplineActor->GetTransform();
		
		ESplinePointType::Type PointType = ESplinePointType::Curve;
		if (Settings->bLinear)
		{
			PointType = ESplinePointType::Linear;
		}
		else if (Settings->bApplyCustomTangents)
		{
			PointType = ESplinePointType::CurveCustomTangent;
		}

		FConstPCGPointValueRanges InRanges(PointData);

		for(int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const FTransform& PointTransform = InRanges.TransformRange[PointIndex];
			const FVector LocalPosition = PointTransform.GetLocation() - SplineActorTransform.GetLocation();

			int64 PointMetadataEntry = InRanges.MetadataEntryRange[PointIndex];
			SplinePoints.Emplace(static_cast<float>(PointIndex),
				LocalPosition,
				ArriveTangentAttribute ? ArriveTangentAttribute->GetValueFromItemKey(PointMetadataEntry) : FVector::ZeroVector,
				LeaveTangentAttribute ? LeaveTangentAttribute->GetValueFromItemKey(PointMetadataEntry) : FVector::ZeroVector,
				PointTransform.GetRotation().Rotator(),
				PointTransform.GetScale3D(),
				PointType);

			SplineEntryKeys.Emplace(PointMetadataEntry);
			bHasAValidEntry |= (PointMetadataEntry != PCGInvalidEntryKey);
		}

		if (!bHasAValidEntry)
		{
			SplineEntryKeys.Empty();
		}

		SplineData->Initialize(SplinePoints, Settings->bClosedLoop, FTransform(SplineActorTransform.GetLocation()), std::move(SplineEntryKeys));

		USplineComponent* SplineComponent = nullptr;

		if (Settings->Mode != EPCGCreateSplineMode::CreateDataOnly)
		{
			check(IsInGameThread());
			if (UPCGComponent* SourceComponent = CastChecked<UPCGComponent>(Context->ExecutionSource.Get()))
			{
				SplineComponent = NewObject<USplineComponent>(SplineActor);
				SplineComponent->ComponentTags.Add(SourceComponent->GetFName());
				SplineComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

				SplineComponent->RegisterComponent();
				SplineActor->AddInstanceComponent(SplineComponent);

				if (!SplineComponent->AttachToComponent(SplineActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false)))
				{
					PCGLog::Component::LogComponentAttachmentFailedWarning(Context);
				}

				SplineData->ApplyTo(SplineComponent);

				UPCGManagedComponent* ManagedComponent = NewObject<UPCGManagedComponent>(SourceComponent);
				ManagedComponent->GeneratedComponent = SplineComponent;

				SourceComponent->AddToManagedResources(ManagedComponent);
			}
		}

		// Execute PostProcess Functions
		if (SplineActor)
		{
			// @todo_pcg: for 5.7 move lock inside UPCGFunctionPrototypes::GetPrototypeWithNoParams().
			// for 5.6.1 keeping fix in cpp to preserve abi.
			// This can run off the GameThread and UPCGFunctionPrototypes::GetPrototypeWithNoParams() does a FindObject which can't run while GC is running
			UFunction* FunctionPrototype = nullptr;
			{
				FGCScopeGuard Guard;
				FunctionPrototype = UPCGFunctionPrototypes::GetPrototypeWithNoParams();
			}

			for (UFunction* Function : PCGHelpers::FindUserFunctions(SplineActor->GetClass(), Settings->PostProcessFunctionNames, { FunctionPrototype }, Context))
			{
				SplineActor->ProcessEvent(Function, nullptr);
			}
		}

		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Data = SplineData;
	}

	// Pass-through settings & exclusions
	Context->OutputData.TaggedData.Append(Context->InputData.GetAllSettings());

	return true;
}

#undef LOCTEXT_NAMESPACE