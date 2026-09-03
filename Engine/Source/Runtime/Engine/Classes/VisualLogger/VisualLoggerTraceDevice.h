// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"

#if ENABLE_VISUAL_LOG

DECLARE_DELEGATE_TwoParams(FImmediateRenderDelegate, const UObject*, const FVisualLogEntry&);

class FVisualLoggerTraceDevice : public FVisualLogDevice
{
public:
	static ENGINE_API FVisualLoggerTraceDevice& Get();

	ENGINE_API FVisualLoggerTraceDevice();
	ENGINE_API virtual void Cleanup(bool bReleaseMemory = false) override;
	ENGINE_API virtual void StartRecordingToFile(double TimeStamp) override;
	ENGINE_API virtual void StopRecordingToFile(double TimeStamp) override;
	ENGINE_API virtual void DiscardRecordingToFile() override;
	ENGINE_API virtual void SetFileName(const FString& InFileName) override;
	ENGINE_API virtual void Serialize(const UObject* InLogOwner, const FName& InOwnerName, const FName& InOwnerDisplayName, const FName& InOwnerClassName, const FVisualLogEntry& InLogEntry) override;
	virtual bool HasFlags(int32 InFlags) const override { return !!(InFlags & (EVisualLoggerDeviceFlags::CanSaveToFile | EVisualLoggerDeviceFlags::StoreLogsLocally)); }

	FImmediateRenderDelegate ImmediateRenderDelegate;
};

#endif
