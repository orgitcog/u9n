// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSpeech2FaceModule.h"

#include "AudioDrivenAnimationConfig.h"
#include "AudioDrivenAnimationCustomizations.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#endif // WITH_EDTIOR

void FMetaHumanSpeech2FaceModule::StartupModule()
{
#if WITH_EDITOR
	using namespace UE::MetaHuman::Private;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FAudioDrivenAnimationSolveOverrides::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAudioSolveOverridesPropertyTypeCustomization::MakeInstance)
	);

	PropertyModule.NotifyCustomizationModuleChanged();
#endif // WITH_EDTIOR
}

void FMetaHumanSpeech2FaceModule::ShutdownModule()
{
#if WITH_EDITOR
	FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");

	if (PropertyModule)
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FAudioDrivenAnimationSolveOverrides::StaticStruct()->GetFName());
		PropertyModule->NotifyCustomizationModuleChanged();
	}
#endif // WITH_EDTIOR
}

IMPLEMENT_MODULE(FMetaHumanSpeech2FaceModule, MetaHumanSpeech2Face)
