// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneInterrogation.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

namespace UE::MovieScene
{


TEntitySystemLinkerExtensionID<IInterrogationExtension> IInterrogationExtension::GetExtensionID()
{
	static TEntitySystemLinkerExtensionID<IInterrogationExtension> ID = UMovieSceneEntitySystemLinker::RegisterExtension<IInterrogationExtension>();
	return ID;
}


} // namespace UE::MovieScene
