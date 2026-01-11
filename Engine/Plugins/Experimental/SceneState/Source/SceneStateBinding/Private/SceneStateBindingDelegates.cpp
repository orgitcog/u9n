// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBindingDelegates.h"

namespace UE::SceneState
{

#if WITH_EDITOR
	TMulticastDelegate<void(const FStructIdChange&)> OnStructIdChanged;
#endif

} // UE::SceneState
