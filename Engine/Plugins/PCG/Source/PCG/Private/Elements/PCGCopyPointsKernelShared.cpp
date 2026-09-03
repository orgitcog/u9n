// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCopyPointsKernelShared.h"

#include "PCGContext.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Elements/PCGCopyPoints.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#define LOCTEXT_NAMESPACE "PCGCopyPointsKernel"

bool PCGCopyPointsKernel::IsKernelDataValid(const UPCGComputeKernel* InKernel, const UPCGCopyPointsSettings* InCopyPointSettings, const FPCGComputeGraphContext* InContext)
{
	check(InCopyPointSettings);
	check(InContext);

	if (InCopyPointSettings->bMatchBasedOnAttribute)
	{
		if (const UPCGDataBinding* DataBinding = InContext->DataBinding.Get(); ensure(DataBinding))
		{
			auto ValidateAttributeExists = [InContext, DataBinding, InKernel, InCopyPointSettings](FName InputPin) -> bool
			{
				const FPCGDataCollectionDesc* PinDataDesc = DataBinding->GetCachedKernelPinDataDesc(InKernel, InputPin, /*bIsInputPin=*/true);

				if (!ensure(PinDataDesc))
				{
					return false;
				}

				const FName MatchAttributeName = InCopyPointSettings->MatchAttribute;

				// todo_pcg: Can generalize this to any type?
				constexpr EPCGKernelAttributeType MatchAttributeType = EPCGKernelAttributeType::Int;

				for (const FPCGDataDesc& DataDesc : PinDataDesc->DataDescs)
				{
					if (!DataDesc.ContainsAttribute(MatchAttributeName, MatchAttributeType))
					{
						PCG_KERNEL_VALIDATION_ERR(InContext, InCopyPointSettings, FText::Format(
							LOCTEXT("MatchAttributeMissing", "Match attribute '{0}' not found, this attribute must be present on all input data, and be of type Integer."),
							FText::FromName(MatchAttributeName)));

						return false;
					}
				}

				// Valid for execution if we have some data to process.
				return !PinDataDesc->DataDescs.IsEmpty();
			};

			if (!ValidateAttributeExists(PCGCopyPointsConstants::SourcePointsLabel) || !ValidateAttributeExists(PCGCopyPointsConstants::TargetPointsLabel))
			{
				return false;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
