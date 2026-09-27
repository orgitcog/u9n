// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Renderers/Text3DRendererBase.h"
#include "UObject/ObjectPtr.h"
#include "Text3DStaticMeshesRenderer.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UText3DComponent;

/**
 * Legacy/default renderer for Text3D
 * Each text character is rendered as a StaticMesh within its own StaticMeshComponent
 * Kerning is done through a scene component containing the current character
 * Text3DComponent
 * - Text3DRoot (Root)
 * -- Text3DSceneComponent (Kerning)
 * --- Text3DStaticMeshComponent (Character)
 *
 * Eg: The text "Hello" will be rendered using 5 SceneComponents and 5 StaticMeshComponents
 */
UCLASS(DisplayName="StaticMeshesRenderer", ClassGroup="Text3D")
class UText3DStaticMeshesRenderer : public UText3DRendererBase
{
	GENERATED_BODY()

public:
	/** Gets the number of font glyphs that are currently used */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	int32 GetGlyphCount();

	/** Gets the USceneComponent that a glyph is attached to */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	USceneComponent* GetGlyphKerningComponent(int32 Index);

	/** Gets all the glyph kerning components */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	const TArray<USceneComponent*>& GetGlyphKerningComponents();

	/** Gets the StaticMeshComponent of a glyph */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	UStaticMeshComponent* GetGlyphMeshComponent(int32 Index);

	/** Gets all the glyph meshes */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	const TArray<UStaticMeshComponent*>& GetGlyphMeshComponents();

protected:
	//~ Begin UText3DRendererBase
	virtual void OnCreate() override;
	virtual void OnUpdate(EText3DRendererFlags InFlags) override;
	virtual void OnClear() override;
	virtual void OnDestroy() override;
	virtual FName GetName() const override;
	virtual FBox OnCalculateBounds() const override;
	//~ End UText3DRendererBase

	/** Allocates, or shrinks existing components to match the input number. Returns false if nothing modified. */
	bool AllocateCharacterComponents(int32 Num);

	/** Holds all character components */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TextRoot;

	/** Each character kerning is held in these components */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> CharacterKernings;

	/** Each character mesh is held in these components */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CharacterMeshes;
};