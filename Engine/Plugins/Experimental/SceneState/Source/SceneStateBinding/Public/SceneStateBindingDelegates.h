// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Misc/Guid.h"
#include "UObject/ObjectPtr.h"

class UObject;

namespace UE::SceneState
{

#if WITH_EDITOR
	struct FStructIdChange
	{
		/** Owner of the binding collection */
		TObjectPtr<UObject> BindingOwner;
		/** Map of the old struct id to the new id */
		TMap<FGuid, FGuid> OldToNewStructIdMap;
	};
	/** Delegate called when a struct id has changed */
	extern SCENESTATEBINDING_API TMulticastDelegate<void(const FStructIdChange&)> OnStructIdChanged;
#endif

} // UE::SceneState
