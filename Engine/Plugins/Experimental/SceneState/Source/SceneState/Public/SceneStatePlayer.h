// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "SceneStatePlayer.generated.h"

class USceneStateObject;

/**
 * Scene State Players is the layer between the Context Object and the rest of Scene State
 * It instances a Scene State Object from a given class.
 * These players exist to keep shared logic re-usable across multiple possible implementers
 */
UCLASS(MinimalAPI)
class USceneStatePlayer : public UObject
{
	GENERATED_BODY()

public:
	TSubclassOf<USceneStateObject> GetSceneStateClass() const
	{
		return SceneStateClass;
	}

	SCENESTATE_API void SetSceneStateClass(TSubclassOf<USceneStateObject> InSceneStateClass);

	USceneStateObject* GetSceneState() const
	{
		return RootState;
	}

	/** Returns the context name for this player, for debugging purposes */
	SCENESTATE_API FString GetContextName() const;

	/** Returns the context object for this player */
	SCENESTATE_API UObject* GetContextObject() const;

	SCENESTATE_API void Setup();

	SCENESTATE_API void Begin();

	SCENESTATE_API void Tick(float InDeltaTime);

	SCENESTATE_API void End();

	//~ Begin UObject
#if WITH_EDITOR
	SCENESTATE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	static FName GetSceneStateClassName()
	{
		return GET_MEMBER_NAME_CHECKED(USceneStatePlayer, SceneStateClass);
	}

	static FName GetRootStateName()
	{
		return GET_MEMBER_NAME_CHECKED(USceneStatePlayer, RootState);
	}

protected:
	/** Returns the context name for this player */
	virtual bool OnGetContextName(FString& OutContextName) const
	{
		return false;
	}

	/** Returns the context object for this player */
	virtual bool OnGetContextObject(UObject*& OutContextObject) const
	{
		return false;
	}

	/** Scene State class used to instantiate the Scene State */
	UPROPERTY(EditAnywhere, Category="Scene State", meta=(NoBinding, EditCondition="bEditableSceneStateClass", EditConditionHides, HideEditConditionToggle))
	TSubclassOf<USceneStateObject> SceneStateClass;

	/** Root Scene State object that this Component will run */
	UPROPERTY(VisibleAnywhere, Instanced, Category="Scene State", meta=(AllowPrivateAccess="true", ShowInnerProperties))
	TObjectPtr<USceneStateObject> RootState;

#if WITH_EDITORONLY_DATA
	/** Whether the scene state class is editable in Details */
	UPROPERTY()
	bool bEditableSceneStateClass = true;
#endif
};
