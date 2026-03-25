// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureSource.h"

void FMetaHumanCaptureVoidResult::SetResult(TResult<void, FMetaHumanCaptureError> InResult)
{
	bIsValid = InResult.IsValid();

	if (!bIsValid)
	{
		FMetaHumanCaptureError Error = InResult.ClaimError();
		Code = Error.GetCode();
		Message = Error.GetMessage();
	}
}

#if WITH_EDITOR
void UMetaHumanCaptureSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	MinDistance = FMath::Clamp(MinDistance, 0.0, MaxDistance);
}
#endif

void UMetaHumanCaptureSource::PostLoad()
{
	Super::PostLoad();

	if (!DeviceAddress_DEPRECATED.IsEmpty())
	{
		DeviceIpAddress.IpAddress = DeviceAddress_DEPRECATED;
	}
}
