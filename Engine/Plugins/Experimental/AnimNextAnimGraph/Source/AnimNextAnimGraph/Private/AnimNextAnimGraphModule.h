// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UObject/GCObject.h"

// Enable console commands only in development builds when logging is enabled
#define WITH_ANIMNEXT_CONSOLE_COMMANDS (!UE_BUILD_SHIPPING && !NO_LOGGING)

class UAnimNextAnimationGraph;
class UAnimNextAnimGraphSettings;

namespace UE::AnimNext::AnimGraph
{

class FAnimNextAnimGraphModule : public IModuleInterface, public FGCObject
{
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	friend UAnimNextAnimGraphSettings;
	
	// References to loaded default graphs to prevent GC.
	// They must exist here rather than on the UAnimNextAnimGraphSettings CDO to avoid breaking DisregardForGC assumptions
	TArray<TObjectPtr<const UAnimNextAnimationGraph>> LoadedGraphs;

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	void ListNodeTemplates(const TArray<FString>& Args);
	void ListAnimationGraphs(const TArray<FString>& Args);

	TArray<IConsoleObject*> ConsoleCommands;
#endif
};

}
