// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPalette.h"

#include "MetaHumanWardrobeItem.generated.h"

class UMetaHumanItemEditorPipeline;
class UMetaHumanItemPipeline;

UCLASS()
class METAHUMANCHARACTERPALETTE_API UMetaHumanWardrobeItem : public UMetaHumanCharacterPalette
{
	GENERATED_BODY()

public:
	// Wardrobe Items should not be created or modified outside the editor
#if WITH_EDITOR
	/** Set the Pipeline for this Wardrobe Item to use. */
	void SetPipeline(TNotNull<UMetaHumanItemPipeline*> InPipeline);

	const UMetaHumanItemEditorPipeline* GetEditorPipeline() const;
	virtual const UMetaHumanCharacterEditorPipeline* GetPaletteEditorPipeline() const override;
#endif
	
	TObjectPtr<const UMetaHumanItemPipeline> GetPipeline() const;
	virtual const UMetaHumanCharacterPipeline* GetPalettePipeline() const override;

	/**
	 * Returns true if this Wardrobe Item is its own asset, false if it's a subobject of a Palette.
	 */
	bool IsExternal() const;

	// Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject interface

	/** 
	 * The main asset this item represents, e.g. a mesh.
	 */
	UPROPERTY(EditAnywhere, Category = "Character")
	TSoftObjectPtr<UObject> PrincipalAsset;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category = Thumbnail)
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;

	/** A selectable texture that will be used as wardrobe item thumbnail in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Thumbnail)
	TSoftObjectPtr<UTexture2D> ThumbnailImage;

	/** An editable text that will be used as thumbnail item name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Thumbnail)
	FText ThumbnailName;
#endif

private:
	/** 
	 * The pipeline that will be used to build this Item.
	 * 
	 * Choose the appropriate pipeline for the type of Principal Asset.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Character")
	TObjectPtr<UMetaHumanItemPipeline> Pipeline;
};
