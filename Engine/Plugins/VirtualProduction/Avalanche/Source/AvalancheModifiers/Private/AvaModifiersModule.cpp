// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaModifiersModule.h"

#include "AvaModifiersSceneTreeResolver.h"
#include "Engine/Level.h"
#include "Extensions/ActorModifierSceneTreeUpdateExtension.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FAvalancheModifiersModule, AvalancheModifiers)

void FAvalancheModifiersModule::StartupModule()
{
	FActorModifierSceneTreeUpdateExtension::OnGetSceneTreeResolver().BindRaw(this, &FAvalancheModifiersModule::GetSceneTreeResolver);
}

void FAvalancheModifiersModule::ShutdownModule()
{
	FActorModifierSceneTreeUpdateExtension::OnGetSceneTreeResolver().Unbind();
}

TSharedPtr<IActorModifierSceneTreeCustomResolver> FAvalancheModifiersModule::GetSceneTreeResolver(ULevel* InLevel)
{
	if (IsValid(InLevel))
	{
		return MakeShared<FAvaModifiersSceneTreeResolver>(InLevel);
	}

	return nullptr;
}
