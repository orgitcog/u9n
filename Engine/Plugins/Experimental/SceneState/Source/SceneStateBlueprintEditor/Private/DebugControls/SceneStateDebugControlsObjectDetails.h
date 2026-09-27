// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class FReply;
class IPropertyHandle;
class USceneStateDebugControlsObject;
class USceneStateObject;
struct FSceneStateEventTemplate;

namespace UE::SceneState::Editor
{

/** Details customization for USceneStateDebugControlsObject */
class FDebugControlsObjectDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FDebugControlsObjectDetails>();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization

private:
	void CustomizeEventDetails(IDetailLayoutBuilder& InDetailBuilder);

	/** Gets the scene state object within the debug controls object found at the given index */
	USceneStateObject* GetDebuggedObject(int32 InIndex) const;

	/** Adds the debugged object details (if there's only one to see) */
	void CustomizeDebuggedObjectDetails(IDetailLayoutBuilder& InDetailBuilder);

	TSharedPtr<IPropertyHandle> EventsHandle;

	TArray<TWeakObjectPtr<USceneStateDebugControlsObject>> DebugControls;
};

} // UE::SceneState::Editor
