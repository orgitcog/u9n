// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorSubTools.h"
#include "SingleSelectionTool.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterIdentity.h"

#include "MetaHumanCharacterEditorSkinTool.generated.h"

enum class EMetaHumanCharacterAccentRegion : uint8;
enum class EMetaHumanCharacterAccentRegionParameter : uint8;
enum class EMetaHumanCharacterFrecklesParameter : uint8;
class FMetaHumanFaceTextureAttributeMap;
class FMetaHumanFilteredFaceTextureIndices;

UCLASS()
class UMetaHumanCharacterEditorSkinToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

/**
 * Properties for the Skin tool.
 * These are displayed in the details panel of the Skin Tool and are how the user can edit skin parameters
 * and, for now, are the same as the ones stored in UMetaHumanCharacter
 */
UCLASS()
class UMetaHumanCharacterEditorSkinToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	//~End UObject Interface

	/**
	* Delegate that executes on EPropertyChangeType::ValueSet property change event, i.e. when a property
	* value has finished being updated
	*/
	DECLARE_DELEGATE_OneParam(FOnSkinPropertyValueSetDelegate, const FPropertyChangedEvent& PropertyChangedEvent);
	FOnSkinPropertyValueSetDelegate OnSkinPropertyValueSetDelegate;

	/**
	* Utility functions for copying to & from MetaHuman Character Skin Settings and Skin Tool Properties
	*/
	void CopyTo(FMetaHumanCharacterSkinSettings& OutSkinSettings);
	void CopyFrom(const FMetaHumanCharacterSkinSettings& InSkinSettings);

	/**
	* Utility functions for copying to & from MetaHuman Character Evaluation Properties
	*/
	void CopyTo(FMetaHumanCharacterFaceEvaluationSettings& OutFaceEvaluationSettings);
	void CopyFrom(const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings);

public:

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterSkinProperties Skin;

	UPROPERTY(EditAnywhere, Category = "Skin");
	bool bIsSkinFilterEnabled;

	UPROPERTY(EditAnywhere, Category = "Skin");
	TArray<int32> SkinFilterValues;

	UPROPERTY(EditAnywhere, Category = "Skin");
	int32 SkinFilterIndex;

	UPROPERTY(EditAnywhere, Category = "FaceEvaluation", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterFaceEvaluationSettings FaceEvaluationSettings;

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterFrecklesProperties Freckles;

	UPROPERTY(EditAnywhere, Category = "Accents", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterAccentRegions Accents;

	UPROPERTY(EditAnywhere, Category = "Texture Overrides")
	bool bEnableTextureOverrides = false;

	UPROPERTY(EditAnywhere, Category = "Texture Overrides")
	FMetaHumanCharacterSkinTextureSoftSet TextureOverrides;
};

/**
 * The Skin Tool allows the user to edit properties of the MetaHuman Skin
 */
UCLASS()
class UMetaHumanCharacterEditorSkinTool : public USingleSelectionTool
{
	GENERATED_BODY()

public:
	/** Get the Skin Tool properties. */
	UMetaHumanCharacterEditorSkinToolProperties* GetSkinToolProperties() const { return SkinToolProperties; }

	//~Begin USingleTargetWithSelectionTool interface
	virtual void Setup();
	virtual void Shutdown(EToolShutdownType InShutdownType);

	virtual bool HasCancel() const { return true; }
	virtual bool HasAccept() const { return true; }
	virtual bool CanAccept() const { return true; }
	//~End USingleTargetWithSelectionTool interface

	/** Returns true if the filter indices are valid. */
	bool IsFilteredFaceTextureIndicesValid() const;

protected:

	void UpdateSkinState() const;

private:

	const FText GetCommandChangeDescription() const;

	/**
	 * Updates the Skin Texture. Called whenever one of the skin texture parameters changes
	 * Will prompt the user if the character currently has high resolution textures to avoid loss of data
	 * @return true if a change was applied to character and false otherwise.
	 */
	bool UpdateSkinSynthesizedTexture();

	void UpdateSkinToolProperties(TWeakObjectPtr<UInteractiveToolManager> InToolManager, const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings);

	void UpdateFaceTextureFromFilterIndex();

	void SetEnableSkinFilter(bool bInEnableSkinFilter);


private:

	friend class FMetaHumanCharacterEditorSkinToolCommandChange;

	/** Properties of the Skin Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorSkinToolProperties> SkinToolProperties;

	/** Keep track of previously set skin settings */
	FMetaHumanCharacterSkinSettings PreviousSkinSettings;
	FMetaHumanCharacterFaceEvaluationSettings PreviousFaceEvaluationSettings;

	TSharedPtr<FMetaHumanFilteredFaceTextureIndices> FilteredFaceTextureIndices;

	/** Keep track of whether the tool applied any changes */
	bool bActorWasModified = false;
	bool bSkinTextureWasModified = false;

	/** The face state of the actor when the tool was activated */
	TSharedPtr<FMetaHumanCharacterIdentity::FState> FaceState;
};