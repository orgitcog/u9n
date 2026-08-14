// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGComputeKernel.h"

#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGComputeSource.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "ShaderCompilerCore.h"
#include "Async/Async.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComputeKernel)

#define LOCTEXT_NAMESPACE "PCGComputeKernel"

namespace PCGComputeKernel
{
	static TAutoConsoleVariable<bool> CVarWarnOnOverridePinUsage(
		TEXT("pcg.Graph.GPU.WarnOnOverridePinUsage"),
		true,
		TEXT("Enables warnings when parameters are overidden on GPU nodes."));

	static const TStaticArray<FSoftObjectPath, 3> DefaultAdditionalSourcePaths =
	{
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_ShaderUtils.PCGCS_ShaderUtils'")),
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_ShaderUtilsInternal.PCGCS_ShaderUtilsInternal'")),

		// Note: PCGDataCollectionDataInterface.ush depends on the quaternion helpers, therefore all kernels also depend on the quaternion helpers.
		// @todo_pcg: In the future quaternion compute source could be opt-in if the kernel does not manipulate point/attribute data.
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_Quaternion.PCGCS_Quaternion'"))
	};
}

#if WITH_EDITOR
void UPCGComputeKernel::Initialize(const FPCGComputeKernelParams& InParams)
{
	ResolvedSettings = InParams.Settings;
	Settings = ResolvedSettings;
	bLogDataDescriptions = InParams.bLogDescriptions;
	bInitialized = true;

	InitializeInternal();

	bHasStaticValidationErrors = !PerformStaticValidation();
}
#endif

const UPCGSettings* UPCGComputeKernel::GetSettings() const
{
	if (!ResolvedSettings)
	{
		FGCScopeGuard Guard;
		ResolvedSettings = Settings.Get();
	}

	return ResolvedSettings;
}

void UPCGComputeKernel::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UPCGComputeKernel* This = CastChecked<UPCGComputeKernel>(InThis);
	Collector.AddReferencedObject(This->ResolvedSettings);
}

bool UPCGComputeKernel::AreKernelSettingsValid(FPCGContext* InContext) const
{
	if (!bInitialized)
	{
		PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), LOCTEXT("UninitializeKernel", "Kernel was not initialized during compilation. Make sure to call Initialize() when creating your kernels."));
		return false;
	}

#if PCG_KERNEL_LOGGING_ENABLED
	for (const FPCGKernelLogEntry& StaticLogEntry : StaticLogEntries)
	{
		if (StaticLogEntry.Verbosity == EPCGKernelLogVerbosity::Error)
		{
			PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), StaticLogEntry.Message);
		}
		else
		{
			PCG_KERNEL_VALIDATION_WARN(InContext, GetSettings(), StaticLogEntry.Message);
		}
	}
#endif

	return !bHasStaticValidationErrors;
}

#if WITH_EDITOR
void UPCGComputeKernel::GatherAdditionalSources(TArray<TObjectPtr<UComputeSource>>& OutAdditionalSources) const
{
	for (const FSoftObjectPath& AdditionalSourcePath : PCGComputeKernel::DefaultAdditionalSourcePaths)
	{
		UPCGComputeSource* AdditionalSource = Cast<UPCGComputeSource>(AdditionalSourcePath.ResolveObject());
		
		if (ensure(AdditionalSource))
		{
			OutAdditionalSources.Add(AdditionalSource);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Default additional compute source '%s' could not be resolved for kernel '%s'."), *AdditionalSourcePath.ToString(), *GetName());
		}
	}
}
#endif

void UPCGComputeKernel::ComputeDataDescFromPinProperties(const FPCGPinPropertiesGPU& OutputPinProps, const TArrayView<const FPCGPinProperties>& InInputPinProps, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutPinDesc) const
{
	check(InBinding);

	const FPCGPinPropertiesGPUStruct& Props = OutputPinProps.PropertiesGPU;

	if (Props.InitializationMode == EPCGPinInitMode::FromInputPins)
	{
		TArray<FName> InitPins;
		TArray<FPCGDataCollectionDesc> InputDescs;

		for (FName PinToInitFrom : Props.PinsToInititalizeFrom)
		{
			if (const FPCGPinProperties* InputPinProps = InInputPinProps.FindByPredicate([PinToInitFrom](const FPCGPinProperties& InProps) { return InProps.Label == PinToInitFrom; }))
			{
				InitPins.Emplace(PinToInitFrom);

				FPCGDataCollectionDesc& InputDesc = InputDescs.AddDefaulted_GetRef();
				const FPCGKernelPin KernelPin(KernelIndex, PinToInitFrom, /*bIsInput=*/true);
				ensure(InBinding->ComputeKernelPinDataDesc(KernelPin, &InputDesc));
			}
		}

		const int NumInitPins = InitPins.Num();
		check(NumInitPins == InputDescs.Num());

		// Copies unique (non-reserved) attribute descriptions from 'InDataDesc' to 'OutDataDesc'.
		auto AddAttributesFromData = [](const FPCGDataDesc& InDataDesc, FPCGDataDesc& OutDataDesc)
		{
			for (const FPCGKernelAttributeDesc& InAttrDesc : InDataDesc.AttributeDescs)
			{
				if (InAttrDesc.AttributeId >= PCGComputeConstants::NUM_RESERVED_ATTRS
					&& !OutDataDesc.AttributeDescs.ContainsByPredicate([InAttrDesc](const FPCGKernelAttributeDesc& OutAttrDesc)
						{
							// Note: We shouldn't need to check for uniqueness of attr Index, since attributes should all have unique index via
							// the GlobalAttributeLookupTable
							return InAttrDesc.AttributeKey.Identifier == OutAttrDesc.AttributeKey.Identifier;
						}))
				{
					OutDataDesc.AttributeDescs.Emplace(InAttrDesc);
				}
			}
		};

		// Combines the data for index i of each pin into one data. Creates exactly 'MaxDataCount' datas.
		auto AddDataPairwise = [&](int MaxDataCount)
		{
			for (int DataIndex = 0; DataIndex < MaxDataCount; ++DataIndex)
			{
				// Set element count to 0 for now, but we will overwrite it.
				FPCGDataDesc& DataDesc = OutPinDesc.DataDescs.Emplace_GetRef(OutputPinProps.AllowedTypes, /*SetNumElements=*/0);
				const bool bDomainIs2D = DataDesc.IsDomain2D();

				int TotalNumElements = 0; // Total number of elements computed for this data index.
				FIntPoint TotalNumElements2D = FIntPoint::ZeroValue;

				// For each data index, loop over all the pins and create the uber-data.
				for (int InputPinIndex = 0; InputPinIndex < NumInitPins; ++InputPinIndex)
				{
					const FPCGDataCollectionDesc& InputDesc = InputDescs[InputPinIndex];

					int ClampedDataIndex = DataIndex;

					// If this pin does not have the same number of data, clamp it to the first data.
					if (DataIndex != InputDesc.DataDescs.Num())
					{
						ClampedDataIndex = 0;
					}

					const FPCGDataDesc& InputDataDesc = InputDesc.DataDescs.IsValidIndex(ClampedDataIndex) ? InputDesc.DataDescs[ClampedDataIndex] : FPCGDataDesc(EPCGDataType::Any, 0);
					const bool bSourceDomainIs2D = InputDataDesc.IsDomain2D();

					if (Props.ElementCountMode == EPCGElementCountMode::FromInputData)
					{
						if (Props.ElementMultiplicity == EPCGElementMultiplicity::Product)
						{
							if (bDomainIs2D)
							{
								TotalNumElements2D = TotalNumElements2D.ComponentMax(FIntPoint(1, 1)) * (bSourceDomainIs2D
									? InputDataDesc.ElementCount2D.X * InputDataDesc.ElementCount2D.Y
									: InputDataDesc.ElementCount);
							}
							else
							{
								TotalNumElements = FMath::Max(TotalNumElements, 1) * (bSourceDomainIs2D
									? InputDataDesc.ElementCount2D.X * InputDataDesc.ElementCount2D.Y
									: InputDataDesc.ElementCount);
							}
						}
						else if (Props.ElementMultiplicity == EPCGElementMultiplicity::Sum)
						{
							if (bDomainIs2D)
							{
								TotalNumElements2D += bSourceDomainIs2D ? InputDataDesc.ElementCount2D : InputDataDesc.ElementCount;
							}
							else
							{
								TotalNumElements += bSourceDomainIs2D ? InputDataDesc.ElementCount2D.X * InputDataDesc.ElementCount2D.Y : InputDataDesc.ElementCount;
							}
						}
						else
						{
							checkNoEntry();
						}
					}
					else if (Props.ElementCountMode == EPCGElementCountMode::Fixed)
					{
						TotalNumElements += Props.ElementCount;
						TotalNumElements2D += Props.NumElements2D;
					}
					else
					{
						checkNoEntry();
					}

					if (Props.AttributeInheritanceMode == EPCGAttributeInheritanceMode::CopyAttributeSetup)
					{
						AddAttributesFromData(InputDataDesc, DataDesc);
					}

					// Add unique tags from this data.
					for (const int32 TagStringKey : InputDataDesc.TagStringKeys)
					{
						DataDesc.TagStringKeys.AddUnique(TagStringKey);
					}
				}

				DataDesc.ElementCount = TotalNumElements;
				DataDesc.ElementCount2D = TotalNumElements2D;
			}
		};

		if (Props.DataCountMode == EPCGDataCountMode::FromInputData)
		{
			// If this is the only input pin, we can just copy it.
			if (NumInitPins == 1)
			{
				for (const FPCGDataDesc& InputDataDesc : InputDescs[0].DataDescs)
				{
					FPCGDataDesc& DataDesc = OutPinDesc.DataDescs.Emplace_GetRef(OutputPinProps.AllowedTypes);
					const bool bDomainIs2D = DataDesc.IsDomain2D();
					const bool bSourceDomainIs2D = InputDataDesc.IsDomain2D();

					if (bDomainIs2D)
					{
						DataDesc.ElementCount2D = bSourceDomainIs2D ? InputDataDesc.ElementCount2D : InputDataDesc.ElementCount;
					}
					else
					{
						DataDesc.ElementCount = bSourceDomainIs2D ? InputDataDesc.ElementCount2D.X * InputDataDesc.ElementCount2D.Y : InputDataDesc.ElementCount;
					}

					if (Props.AttributeInheritanceMode == EPCGAttributeInheritanceMode::CopyAttributeSetup)
					{
						AddAttributesFromData(InputDataDesc, DataDesc);
					}

					DataDesc.TagStringKeys = InputDataDesc.TagStringKeys;
				}
			}
			// Take pairs of datas, where the pairs are given by each data of each pin to each data of every other pin.
			else if (Props.DataMultiplicity == EPCGDataMultiplicity::CartesianProduct)
			{
				for (int InputPinIndex = 0; InputPinIndex < NumInitPins; ++InputPinIndex)
				{
					const FPCGDataCollectionDesc& InputDesc = InputDescs[InputPinIndex];

					for (int OtherInputPinIndex = InputPinIndex + 1; OtherInputPinIndex < NumInitPins; ++OtherInputPinIndex)
					{
						const FPCGDataCollectionDesc& OtherInputDesc = InputDescs[OtherInputPinIndex];

						for (const FPCGDataDesc& InputDataDesc : InputDesc.DataDescs)
						{
							for (const FPCGDataDesc& OtherInputDataDesc : OtherInputDesc.DataDescs)
							{
								FPCGDataDesc& DataDesc = OutPinDesc.DataDescs.Emplace_GetRef(OutputPinProps.AllowedTypes);
								const bool bDomainIs2D = DataDesc.IsDomain2D();
								const bool bSourceDomainIs2D = InputDataDesc.IsDomain2D();
								const bool bOtherSourceDomainIs2D = OtherInputDataDesc.IsDomain2D();

								if (Props.ElementCountMode == EPCGElementCountMode::FromInputData)
								{
									if (Props.ElementMultiplicity == EPCGElementMultiplicity::Product)
									{
										if (bDomainIs2D)
										{
											if (bSourceDomainIs2D)
											{
												DataDesc.ElementCount2D = bOtherSourceDomainIs2D ? InputDataDesc.ElementCount2D * OtherInputDataDesc.ElementCount2D : InputDataDesc.ElementCount2D * OtherInputDataDesc.ElementCount;
											}
											else
											{
												DataDesc.ElementCount2D = bOtherSourceDomainIs2D ?  OtherInputDataDesc.ElementCount2D * InputDataDesc.ElementCount : InputDataDesc.ElementCount * OtherInputDataDesc.ElementCount;
											}
										}
										else
										{
											if (bSourceDomainIs2D)
											{
												DataDesc.ElementCount = (bOtherSourceDomainIs2D ? InputDataDesc.ElementCount2D * OtherInputDataDesc.ElementCount2D : InputDataDesc.ElementCount2D * OtherInputDataDesc.ElementCount).X;
											}
											else
											{
												DataDesc.ElementCount = (bOtherSourceDomainIs2D ?  OtherInputDataDesc.ElementCount2D * InputDataDesc.ElementCount : InputDataDesc.ElementCount * OtherInputDataDesc.ElementCount).X;
											}
										}
									}
									else if (Props.ElementMultiplicity == EPCGElementMultiplicity::Sum)
									{
										if (bDomainIs2D)
										{
											if (bSourceDomainIs2D)
											{
												DataDesc.ElementCount2D = bOtherSourceDomainIs2D ? InputDataDesc.ElementCount2D + OtherInputDataDesc.ElementCount2D : InputDataDesc.ElementCount2D + OtherInputDataDesc.ElementCount;
											}
											else
											{
												DataDesc.ElementCount2D = bOtherSourceDomainIs2D ?  OtherInputDataDesc.ElementCount2D + InputDataDesc.ElementCount : InputDataDesc.ElementCount + OtherInputDataDesc.ElementCount;
											}
										}
										else
										{
											if (bSourceDomainIs2D)
											{
												DataDesc.ElementCount = (bOtherSourceDomainIs2D ? InputDataDesc.ElementCount2D + OtherInputDataDesc.ElementCount2D : InputDataDesc.ElementCount2D + OtherInputDataDesc.ElementCount).X;
											}
											else
											{
												DataDesc.ElementCount = (bOtherSourceDomainIs2D ?  OtherInputDataDesc.ElementCount2D + InputDataDesc.ElementCount : InputDataDesc.ElementCount + OtherInputDataDesc.ElementCount).X;
											}
										}
									}
									else
									{
										checkNoEntry();
									}
								}
								else if (Props.ElementCountMode == EPCGElementCountMode::Fixed)
								{
									DataDesc.ElementCount = Props.ElementCount;
									DataDesc.ElementCount2D = Props.NumElements2D;
								}
								else
								{
									checkNoEntry();
								}

								if (Props.AttributeInheritanceMode == EPCGAttributeInheritanceMode::CopyAttributeSetup)
								{
									AddAttributesFromData(InputDataDesc, DataDesc);
									AddAttributesFromData(OtherInputDataDesc, DataDesc);
								}

								// Add unique tags from both input data.
								for (const int32 TagStringKey : InputDataDesc.TagStringKeys)
								{
									DataDesc.TagStringKeys.AddUnique(TagStringKey);
								}

								for (const int32 TagStringKey : OtherInputDataDesc.TagStringKeys)
								{
									DataDesc.TagStringKeys.AddUnique(TagStringKey);
								}
							}
						}
					}
				}
			}
			// Combine elements for each set of datas, where the sets are given by the Nth datas on each pin (or the first data if there is only one data).
			else if (Props.DataMultiplicity == EPCGDataMultiplicity::Pairwise)
			{
				int MaxDataCount = 0;

				// Find the maximum number of data among the init pins. Note, they should all be the same number of data, or only one data.
				for (int I = 0; I < NumInitPins; ++I)
				{
					MaxDataCount = FMath::Max(MaxDataCount, InputDescs[I].DataDescs.Num());
				}

				AddDataPairwise(MaxDataCount);
			}
			else
			{
				checkNoEntry();
			}
		}
		else if (Props.DataCountMode == EPCGDataCountMode::Fixed)
		{
			AddDataPairwise(Props.DataCount);
		}
		else
		{
			checkNoEntry();
		}

		// Apply element count multiplier.
		const uint32 Multiplier = OutputPinProps.GetElementCountMultiplier();
		for (FPCGDataDesc& Desc : OutPinDesc.DataDescs)
		{
			Desc.ElementCount *= Multiplier;
			Desc.ElementCount2D *= Multiplier;
		}
	}
	else if (Props.InitializationMode == EPCGPinInitMode::Custom)
	{
		for (int I = 0; I < Props.DataCount; ++I)
		{
			OutPinDesc.DataDescs.Emplace(OutputPinProps.AllowedTypes, Props.ElementCount, Props.NumElements2D);
		}
	}
	else
	{
		checkNoEntry();
	}
}

#if WITH_EDITOR
bool UPCGComputeKernel::PerformStaticValidation()
{
	return ValidatePCGNode(StaticLogEntries);
}

bool UPCGComputeKernel::ValidatePCGNode(TArray<FPCGKernelLogEntry>& OutLogEntries) const
{
	if (!Settings)
	{
		return true;
	}

	if (PCGComputeKernel::CVarWarnOnOverridePinUsage.GetValueOnAnyThread())
	{
		for (const FPCGSettingsOverridableParam& Param : Settings->OverridableParams())
		{
			if (ensure(!Param.PropertiesNames.IsEmpty()))
			{
				const FName PropertyName = Param.PropertiesNames[0];

				if (Settings->IsPropertyOverriddenByPin(PropertyName))
				{
#if PCG_KERNEL_LOGGING_ENABLED
					OutLogEntries.Emplace(FText::Format(LOCTEXT("ParamOverrideGPU", "Tried to override pin '{0}', but overrides are not supported on GPU nodes."), FText::FromName(PropertyName)), EPCGKernelLogVerbosity::Warning);
#endif
				}
			}
		}
	}

	// Validate types of incident edges to make sure we catch invalid cases like Spatial -> Point.
	return AreInputEdgesValid(OutLogEntries);
}

bool UPCGComputeKernel::AreInputEdgesValid(TArray<FPCGKernelLogEntry>& OutLogEntries) const
{
	bool bAllEdgesValid = true;

	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		for (const UPCGPin* InputPin : Node->GetInputPins())
		{
			if (!InputPin)
			{
				continue;
			}

			for (const UPCGEdge* InputEdge : InputPin->Edges)
			{
				const UPCGPin* UpstreamPin = InputEdge ? InputEdge->GetOtherPin(InputPin) : nullptr;
				if (UpstreamPin && InputPin->GetRequiredTypeConversion(UpstreamPin) != EPCGTypeConversion::NoConversionRequired)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					OutLogEntries.Emplace(FText::Format(
						LOCTEXT("InvalidInputPinEdge", "Unsupported connected upstream pin '{0}' on node '{1}' with type {2}. Recreate the edge to add required conversion nodes."),
						FText::FromName(UpstreamPin->Properties.Label),
						Node->GetNodeTitle(EPCGNodeTitleType::ListView),
						StaticEnum<EPCGDataType>() ? StaticEnum<EPCGDataType>()->GetDisplayNameTextByValue(static_cast<int64>(UpstreamPin->Properties.AllowedTypes)) : FText::FromString(TEXT("MISSING"))),
						EPCGKernelLogVerbosity::Error);
#endif

					bAllEdgesValid = false;
				}
			}
		}
	}

	return bAllEdgesValid;
}
#endif

#undef LOCTEXT_NAMESPACE
