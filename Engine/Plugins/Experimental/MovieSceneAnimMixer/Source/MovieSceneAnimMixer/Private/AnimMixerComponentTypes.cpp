// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimMixerComponentTypes.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieScenePropertyComponentHandler.h"

namespace UE::MovieScene
{

	const UE::Anim::FAttributeId FAnimMixerComponentTypes::RootTransformAttributeId(TEXT("RootTransform"), FCompactPoseBoneIndex(0));
	const UE::Anim::FAttributeId FAnimMixerComponentTypes::RootTransformWeightAttributeId(TEXT("RootTransformWeight"), FCompactPoseBoneIndex(0));
	const UE::Anim::FAttributeId FAnimMixerComponentTypes::RootTransformIsAuthoritativeAttributeId(TEXT("RootTransformIsAuthoritative"), FCompactPoseBoneIndex(0));

	static TUniquePtr<FAnimMixerComponentTypes> GAnimMixerComponentTypes;
	static bool GAnimMixerComponentTypesDestroyed = false;

	FAnimMixerComponentTypes* FAnimMixerComponentTypes::Get()
	{
		if (!GAnimMixerComponentTypes.IsValid())
		{
			check(!GAnimMixerComponentTypesDestroyed);
			GAnimMixerComponentTypes.Reset(new FAnimMixerComponentTypes);
		}
		return GAnimMixerComponentTypes.Get();
	}

	void FAnimMixerComponentTypes::Destroy()
	{
		GAnimMixerComponentTypes.Reset();
		GAnimMixerComponentTypesDestroyed = true;
	}

	FAnimMixerComponentTypes::FAnimMixerComponentTypes()
	{
		FBuiltInComponentTypes* BuiltInTypes = FBuiltInComponentTypes::Get();
		FComponentRegistry* ComponentRegistry = UMovieSceneEntitySystemLinker::GetComponents();

		ComponentRegistry->NewComponentType(&Priority, TEXT("Mixed Animation Priority"));
		ComponentRegistry->NewComponentType(&Target, TEXT("Mixed Animation Target"));
		ComponentRegistry->NewComponentType(&Task, TEXT("Mixed Animation Task"));
		ComponentRegistry->NewComponentType(&MixerTask, TEXT("Mixed Animation Mixer Task"));
		ComponentRegistry->NewComponentType(&RootMotionSettings, TEXT("Root Motion Settings"));
		ComponentRegistry->NewComponentType(&MeshComponent, TEXT("Mixed Animation Mesh Component"));
		ComponentRegistry->NewComponentType(&MixerRootMotion, TEXT("Root Motion"));
		ComponentRegistry->NewComponentType(&MixerEntry, TEXT("MixerEntry"));
		ComponentRegistry->NewComponentType(&RootDestination, TEXT("Root Destination"));


		Tags.RequiresBlending = ComponentRegistry->NewTag(TEXT("Requires Blending"));

		ComponentRegistry->Factories.DuplicateChildComponent(Priority);
		ComponentRegistry->Factories.DuplicateChildComponent(Target);
		ComponentRegistry->Factories.DuplicateChildComponent(Task);
		ComponentRegistry->Factories.DuplicateChildComponent(MixerTask);
		ComponentRegistry->Factories.DuplicateChildComponent(RootMotionSettings);
		ComponentRegistry->Factories.DuplicateChildComponent(MeshComponent);
		ComponentRegistry->Factories.DuplicateChildComponent(MixerRootMotion);
		ComponentRegistry->Factories.DuplicateChildComponent(RootDestination);

		ComponentRegistry->Factories.DefineChildComponent(RootDestination, MixerRootMotion);
		ComponentRegistry->Factories.DefineChildComponent(Task, MixerEntry);

		ComponentRegistry->Factories.DefineChildComponent(Tags.RequiresBlending, Tags.RequiresBlending);

	}

}