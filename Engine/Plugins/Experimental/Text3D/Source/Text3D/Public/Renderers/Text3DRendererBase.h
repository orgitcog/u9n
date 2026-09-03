// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"
#include "Text3DTypes.h"
#include "Text3DRendererBase.generated.h"

class UText3DComponent;

/**
 * Base class for a rendering implementation of Text3D
 * The whole rendering logic should be encapsulated into an instance of this class
 */
UCLASS(MinimalAPI, Abstract)
class UText3DRendererBase : public UObject
{
	GENERATED_BODY()

public:
	/** Allocate renderer resources */
	void Create();

	/** Update rendering state */
	void Update(EText3DRendererFlags InFlags);

	/** Clears the active rendering state */
	void Clear();

	/** Cleanup renderer resources */
	void Destroy();

	/** Get cached bounds from last update */
	FBox GetBounds() const;

	/** Get the implementation name for debug purposes */
	virtual FName GetName() const PURE_VIRTUAL(UText3DRenderingImplementationBase::GetName, return NAME_None; );
	
protected:
	/** Create and setup the implementation components, called on load or creation */
	virtual void OnCreate() PURE_VIRTUAL(UText3DRenderingImplementationBase::OnCreate, );

	/** Update rendering state of text characters, called when render state is outdated */
	virtual void OnUpdate(EText3DRendererFlags InFlags) PURE_VIRTUAL(UText3DRenderingImplementationBase::OnUpdate, );

	/** Clear rendering state and remove all visible characters */
	virtual void OnClear() PURE_VIRTUAL(UText3DRenderingImplementationBase::OnClear, );

	/** Destroy and clean the implementation components, called on destroy or deactivation */
	virtual void OnDestroy() PURE_VIRTUAL(UText3DRenderingImplementationBase::OnDestroy, );

	/** Calculate the bounds of the rendered text */
	virtual FBox OnCalculateBounds() const PURE_VIRTUAL(UText3DRenderingImplementationBase::OnCalculateBounds, return FBox(ForceInitToZero); );

	UText3DComponent* GetText3DComponent() const;

	/** Recalculates bounds when a layout or geometry change happened */
	void RefreshBounds();

private:
	TOptional<FBox> CachedBounds;
	bool bInitialized = false;
};