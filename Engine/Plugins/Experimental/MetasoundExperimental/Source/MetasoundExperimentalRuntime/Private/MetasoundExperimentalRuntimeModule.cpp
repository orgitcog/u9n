// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExperimentalRuntimeModule.h"

#include "MetasoundCatMixerNode.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

void FMetasoundExperimentalRuntimeModule::StartupModule()
{
	FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();

	// Datatypes.
	//Metasound::Frontend::RegisterDataType<Metasound::FChannelAgnosticType>();
}

void FMetasoundExperimentalRuntimeModule::ShutdownModule()
{
}
IMPLEMENT_MODULE(FMetasoundExperimentalRuntimeModule, MetasoundExperimentalRuntime);

//#if WITH_ENGINE
//#error ("Being linked with Engine")
//#endif //
