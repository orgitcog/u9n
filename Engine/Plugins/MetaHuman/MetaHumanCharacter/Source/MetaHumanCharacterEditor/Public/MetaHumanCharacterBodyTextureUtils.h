// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacter.h"

class FMetaHumanFaceTextureSynthesizer;
class UMaterialInstanceDynamic;

// Helper class providing stateless static functions for managing body textures
class FMetaHumanCharacterBodyTextureUtils
{
public:
	static int32 GetSkinToneIndex(const FMetaHumanCharacterSkinProperties& InSkinProperties);

	static int32 GetBodySurfaceMapId(const FMetaHumanCharacterSkinProperties& InSkinProperties);

	static void InitBodyTextureData(const FMetaHumanCharacterSkinProperties& InSkinProperties, const TMap<EBodyTextureType, FMetaHumanCharacterTextureInfo>& InTextureInfo, TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& OutBodyTextures);

	static void UpdateBodyTextureSet(const TOptional<FMetaHumanCharacterSkinSettings>& InCharacterSkinSettings, const FMetaHumanCharacterSkinProperties& InSkinProperties, TMap<EBodyTextureType, FMetaHumanCharacterTextureInfo>& InOutTextureInfo, TMap<EBodyTextureType, TObjectPtr<class UTexture2D>>& InOutBodyTextures);

	static void UpdateBodySkinBiasGain(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, FMetaHumanCharacterSkinProperties& InOutSkinProperties);

	/**
	 * Get the skin tone and update the body material with rgb bias parameters and textures. Updates the rgb bias and textures for the chest in the face material to match
	 */
	static void GetSkinToneAndUpdateMaterials(const FMetaHumanCharacterSkinProperties& InSkinProperties,
											  const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
											  const TMap<EBodyTextureType, TObjectPtr<class UTexture2D>>& InBodyTextures,
											  const struct FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet,
											  TNotNull<UMaterialInstanceDynamic*> InBodyMID);

	static void SetMaterialHiddenFacesTexture(TNotNull<UMaterialInstanceDynamic*> InBodyMID, const TObjectPtr<class UTexture2D>& InHiddenFacesTexture);

	static void SetMaterialHiddenFacesTextureNoOp(TNotNull<UMaterialInstanceDynamic*> InBodyMID);

	// See the similar functions in FMetaHumanCharacterTextureSynthesis 
	static UTexture2D* CreateBodyTextureFromSource(EBodyTextureType InTextureType, FImageView InTextureImage);
};
