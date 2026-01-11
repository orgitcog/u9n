// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGModule.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Elements/PCGDifferenceElement.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDifferenceDeterminismTest.h"

#include "ISettingsModule.h"
#include "ShaderCore.h"
#include "ShowFlags.h"
#include "Misc/Paths.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGModule"

namespace PCGModule
{
	FPCGModule* PCGModulePtr = nullptr;

#if WITH_EDITOR
	// @todo_pcg: Temporarily copied from PCGComputeKernel.cpp to avoid changing the ABI for 5.6.1 hotfix. Should be removed in 5.7.
	static const TStaticArray<FSoftObjectPath, 3> DefaultAdditionalSourcePaths =
	{
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_ShaderUtils.PCGCS_ShaderUtils'")),
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_ShaderUtilsInternal.PCGCS_ShaderUtilsInternal'")),

		// Note: PCGDataCollectionDataInterface.ush depends on the quaternion helpers, therefore all kernels also depend on the quaternion helpers.
		// @todo_pcg: In the future quaternion compute source could be opt-in if the kernel does not manipulate point/attribute data.
		FSoftObjectPath(TEXT("/Script/PCG.PCGComputeSource'/PCG/ComputeSources/PCGCS_Quaternion.PCGCS_Quaternion'"))
	};
#endif
}

FPCGModule& FPCGModule::GetPCGModuleChecked()
{
	return PCGModule::PCGModulePtr ? *PCGModule::PCGModulePtr : FModuleManager::GetModuleChecked<FPCGModule>(TEXT("PCG"));
}

bool FPCGModule::IsPCGModuleLoaded()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("PCG"));
}

void FPCGModule::StartupModule()
{
#if WITH_EDITOR
	PCGDeterminismTests::FNativeTestRegistry::Create();

	RegisterNativeElementDeterminismTests();

	FEngineShowFlags::RegisterCustomShowFlag(PCGEngineShowFlags::Debug, /*DefaultEnabled=*/true, EShowFlagGroup::SFG_Developer, LOCTEXT("ShowFlagDisplayName", "PCG Debug"));

	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("PCG"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/PCG"), PluginShaderDir);

	// @todo_pcg: In the future, this could be done in the compute kernel CDO constructor instead, to make kernels more self-contained.
	for (const FSoftObjectPath& AdditionalSourcePath : PCGModule::DefaultAdditionalSourcePaths)
	{
		const UObject* LoadedAdditionalSource = AdditionalSourcePath.TryLoad();

		if (!ensure(LoadedAdditionalSource))
		{
			UE_LOG(LogPCG, Error, TEXT("Failed to load compute source asset '%s'."), *AdditionalSourcePath.ToString());
		}
	}
#endif // WITH_EDITOR

	// Cache for fast access
	check(!PCGModule::PCGModulePtr);
	PCGModule::PCGModulePtr = this;

	// Registering accessor methods
	AttributeAccessorFactory.RegisterDefaultMethods();
	AttributeAccessorFactory.RegisterMethods<UPCGBasePointData>(UPCGBasePointData::GetPointAccessorMethods());
	// @todo_pcg: Eventually remove the UPCGPointData method registration because the UPCGBasePointData accessors are compatible
	AttributeAccessorFactory.RegisterMethods<UPCGPointData>(UPCGPointData::GetPointAccessorMethods());
	AttributeAccessorFactory.RegisterMethods<UPCGSplineData>(UPCGSplineData::GetSplineAccessorMethods());

	// Register onto the PreExit, because we need the class to be still valid to remove them from the mapping
	FCoreDelegates::OnPreExit.AddRaw(this, &FPCGModule::PreExit);

	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FPCGModule::Tick));
}

void FPCGModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	FCoreDelegates::OnPreExit.RemoveAll(this);

	PCGModule::PCGModulePtr = nullptr;
}

void FPCGModule::PreExit()
{
	// Unregistering accessor methods
	AttributeAccessorFactory.UnregisterMethods<UPCGSplineData>();
	AttributeAccessorFactory.UnregisterMethods<UPCGPointData>();
	AttributeAccessorFactory.UnregisterDefaultMethods();
	AttributeAccessorFactory.UnregisterMethods<UPCGBasePointData>();

#if WITH_EDITOR
	DeregisterNativeElementDeterminismTests();

	PCGDeterminismTests::FNativeTestRegistry::Destroy();
#endif // WITH_EDITOR
}

void FPCGModule::ExecuteNextTick(TFunction<void()> TickFunction)
{
	FScopeLock Lock(&ExecuteNextTickLock);
	ExecuteNextTicks.Add(TickFunction);
}

bool FPCGModule::Tick(float DeltaTime)
{
	TArray<TFunction<void()>> LocalExecuteNextTicks;

	{
		FScopeLock Lock(&ExecuteNextTickLock);
		LocalExecuteNextTicks = MoveTemp(ExecuteNextTicks);
	}

	for (TFunction<void()>& LocalExecuteNextTick : LocalExecuteNextTicks)
	{
		LocalExecuteNextTick();
	}

	return true;
}

#if WITH_EDITOR
void FPCGModule::RegisterNativeElementDeterminismTests()
{
	PCGDeterminismTests::FNativeTestRegistry::RegisterTestFunction(UPCGDifferenceSettings::StaticClass(), PCGDeterminismTests::DifferenceElement::RunTestSuite);
	// TODO: Add other native test functions
}

void FPCGModule::DeregisterNativeElementDeterminismTests()
{
	PCGDeterminismTests::FNativeTestRegistry::DeregisterTestFunction(UPCGDifferenceSettings::StaticClass());
}
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FPCGModule, PCG);

PCG_API DEFINE_LOG_CATEGORY(LogPCG);

void PCGLog::LogErrorOnGraph(const FText& InMsg, const FPCGContext* InContext)
{
	if (InContext)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, InMsg);
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("%s"), *InMsg.ToString());
	}
}

void PCGLog::LogWarningOnGraph(const FText& InMsg, const FPCGContext* InContext)
{
	if (InContext)
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, InMsg);
	}
	else
	{
		UE_LOG(LogPCG, Warning, TEXT("%s"), *InMsg.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
