// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGDataVisualizationRegistry.h"
#include "Data/PCGGetDataFunctionRegistry.h"
#include "Metadata/Accessors/PCGAttributeAccessorFactory.h"
#include "Utils/PCGLogErrors.h"

#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

// Logs
PCG_API DECLARE_LOG_CATEGORY_EXTERN(LogPCG, Log, All);

struct FPCGContext;
class IPCGDataVisualization;

namespace PCGEngineShowFlags
{
	static constexpr TCHAR Debug[] = TEXT("PCGDebug");
}

// Stats
DECLARE_STATS_GROUP(TEXT("PCG"), STATGROUP_PCG, STATCAT_Advanced);

// CVars

class FPCGModule final : public IModuleInterface
{
public:
	//~ IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override { return true; }
	//~ End IModuleInterface implementation

	void PreExit();

	PCG_API static FPCGModule& GetPCGModuleChecked();
	static const FPCGGetDataFunctionRegistry& ConstGetDataFunctionRegistry() { return GetPCGModuleChecked().GetDataFunctionRegistry; }
	static FPCGGetDataFunctionRegistry& MutableGetDataFunctionRegistry() { return GetPCGModuleChecked().GetDataFunctionRegistry; }

	static const FPCGAttributeAccessorFactory& GetConstAttributeAccessorFactory() { return GetPCGModuleChecked().AttributeAccessorFactory; }
	static FPCGAttributeAccessorFactory& GetMutableAttributeAccessorFactory() { return GetPCGModuleChecked().AttributeAccessorFactory; }
	
	PCG_API static bool IsPCGModuleLoaded();
	
	PCG_API void ExecuteNextTick(TFunction<void()> TickFunction);

private:
	bool Tick(float DeltaTime);
	
	FPCGGetDataFunctionRegistry GetDataFunctionRegistry;
	FPCGAttributeAccessorFactory AttributeAccessorFactory;

	FTSTicker::FDelegateHandle	TickDelegateHandle;

	FCriticalSection ExecuteNextTickLock;
	TArray<TFunction<void()>> ExecuteNextTicks;

#if WITH_EDITOR
private:
	void RegisterNativeElementDeterminismTests();
	void DeregisterNativeElementDeterminismTests();
#endif

#if WITH_EDITOR
public:
	static const FPCGDataVisualizationRegistry& GetConstPCGDataVisualizationRegistry() { return GetPCGModuleChecked().PCGDataVisualizationRegistry; }
	static FPCGDataVisualizationRegistry& GetMutablePCGDataVisualizationRegistry() { return GetPCGModuleChecked().PCGDataVisualizationRegistry; }

private:
	FPCGDataVisualizationRegistry PCGDataVisualizationRegistry;
#endif
};
