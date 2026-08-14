// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Nodes/HyprsenseRealtimeNode.h"



namespace
{
	TAutoConsoleVariable<bool> CVarEnableRealtimeMonoDebug
	{
		TEXT("mh.Pipeline.EnableRealtimeMonoDebug"),
		false,
		TEXT("Enables the realtime mono solve debugging image options"),
		ECVF_Default
	};
}

class FMetaHumanPipelineCoreModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;

private:

	void HandleRealtimeMonoCVarChanged(IConsoleVariable* InRealtimeMonoCVar) const;
};

void FMetaHumanPipelineCoreModule::StartupModule()
{
	// Update if realtime mono CVar is enabled
	HandleRealtimeMonoCVarChanged(CVarEnableRealtimeMonoDebug.AsVariable());

	// Register callback on change to realtime mono CVar
	CVarEnableRealtimeMonoDebug.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FMetaHumanPipelineCoreModule::HandleRealtimeMonoCVarChanged));
}

void FMetaHumanPipelineCoreModule::HandleRealtimeMonoCVarChanged(IConsoleVariable* InRealtimeMonoCVar) const
{
#if WITH_METADATA
	UEnum* Enum = StaticEnum<EHyprsenseRealtimeNodeDebugImage>();

	const TArray<EHyprsenseRealtimeNodeDebugImage> Items =
	{
		EHyprsenseRealtimeNodeDebugImage::None,
		EHyprsenseRealtimeNodeDebugImage::Input,
		EHyprsenseRealtimeNodeDebugImage::FaceDetect,
		EHyprsenseRealtimeNodeDebugImage::Headpose,
		EHyprsenseRealtimeNodeDebugImage::Trackers,
		EHyprsenseRealtimeNodeDebugImage::Solver
	};

	for (const EHyprsenseRealtimeNodeDebugImage Item : Items)
	{
		int64 Index = Enum->GetIndexByValue(static_cast<int64>(Item));

		if (InRealtimeMonoCVar->GetBool() ||
			Item == EHyprsenseRealtimeNodeDebugImage::None ||
			Item == EHyprsenseRealtimeNodeDebugImage::Input ||
			Item == EHyprsenseRealtimeNodeDebugImage::Trackers)
		{
			Enum->RemoveMetaData(TEXT("Hidden"), Index);
		}
		else
		{
			Enum->SetMetaData(TEXT("Hidden"), TEXT("true"), Index);
		}
	}
#endif
}

IMPLEMENT_MODULE(FMetaHumanPipelineCoreModule, MetaHumanPipelineCore)
