// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Engine/Texture2D.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanFaceTextureSynthesizer.h"

class UTexture2D;
class UMaterialInstanceDynamic;
struct FImage;

// Helper class providing stateless static functions that implement the functionality for synthesizing and updating textures
// It provides for creating and updating synthesized images and textures
class FMetaHumanCharacterTextureSynthesis
{
public:

	/*
	 * Creates a texture from the input image so that it can be used with the MetaHuman face materials.
	 * Sets the image data to the texture source and waits for the texture to be compiled.
	 * 
	 * NOTE: This texture object cannot be used with the texture synthesize APIs below
	 */
	static UTexture2D* CreateFaceTextureFromSource(EFaceTextureType InTextureType, FImageView InTextureImage);


public:
	//
	// Data Initialization API
	//

	/***
	* Load the Texture Synthesis model and initialize all data needed for the OutFaceTextureSynthesizer
	*/
	static void InitFaceTextureSynthesizer(FMetaHumanFaceTextureSynthesizer& OutFaceTextureSynthesizer);

	/**
	 * Initialize the necessary Texture Synthesis data, will not do anything if data have been already populated
	 */
	static void InitSynthesizedFaceData(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
										const TMap<EFaceTextureType, FMetaHumanCharacterTextureInfo>& InTextureInfo,
										TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures,
										TMap<EFaceTextureType, FImage>& OutSynthesizedFaceImages);

	/**
	 * Create a texture of the given face type that is backed with an uncompressed buffer in main memory.
	 * This texture can be used with the Texture Generation API functions.
	 */
	static UTexture2D* CreateFaceTextureEditable(EFaceTextureType InTextureType, int32 InSizeX, int32 InSizeY);

	/**
	 * Create the array of Textures required by the MH Character Face material
	 * OutSynthesizedFaceTextures should be empty
	 */
	static void CreateSynthesizedFaceTextures(int32 InResolution, TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures);

	/** Returns true if the given textures and images are the correct size and format to accept the output of texture synthesis */
	static bool AreTexturesAndImagesSuitableForSynthesis(
		const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
		const TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InSynthesizedFaceTextures,
		const TMap<EFaceTextureType, FImage>& InSynthesizedFaceImages);

	/** Returns the parameters for the FaceTextureSynthesizer that correspond to the input Character Skin Properties */
	static FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams SkinPropertiesToSynthesizerParams(
		const FMetaHumanCharacterSkinProperties& InSkinProperties, 
		const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer);


public:
	//
	// Texture Generation API
	//

	/**
	 * Synthesize any face textures based on the input UV parameters and output the results to the OutCachedImages
	 * Only valid images in OutCachedImages are updated
	 */
	static bool SynthesizeFaceTextures(
		const FMetaHumanCharacterSkinProperties& InSkinProperties, 
		const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, 
		TMap<EFaceTextureType, FImage>& OutCachedImages);

	/**
	 * Synthesize an albedo face texture using a specific HFMap (i.e. overrides the Texture property by using the input HF Map)
	 * InTextureType needs to be one of the base color enum values (Basecolor or Basecolor_Animated_CM1/2/3)
	 * InHFMaps contains the BGR buffer of (Resolution, Resolution, 3) in a flattened layout where Resolution is the OutImage width & height
	 * In the case of the neutral (base) map, then InHFMaps only needs to contain a valid buffer for the first entry in the array
	 * In the case where the texture type is an animated map, then InHFMaps needs to contain a valid buffer for the first entry in the array AND the animated map index
	 */
	static bool SynthesizeFaceAlbedoWithHFMap(EFaceTextureType InTextureType, 
											const FMetaHumanCharacterSkinProperties& InSkinProperties,
											const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, 
											const TStaticArray<TArray<uint8>, 4>& InHFMaps,
											FImageView OutImage);

	/**
	 * Select any face textures based on the High Frequency index and output the results to the OutCachedImages
	 * Only valid images in OutCachedImages are updated
	*/
	static bool SelectFaceTextures(
		const FMetaHumanCharacterSkinProperties& InSkinProperties, 
		const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, 
		TMap<EFaceTextureType, FImage>& OutCachedImages);

	/**
	 * Updates a single texture using data from the InRawData. It assumes the texture has enough space to allocated for the data
	 */
	static void UpdateTexture(TConstArrayView<uint8> InRawData, TNotNull<UTexture2D*> InOutTexture);

	/**
	 * Copies the synthesized cached image data to the output Texture Objects
	 * Copy from the respective cached image for each output texture type, or the neutral map if there is no image
	 * 
	 * @param InCachedImages should contain at least the neutral maps for Basecolor, Normal & Cavity
	 * @param OutSynthesizedFaceTextures should contain a valid texture object for all supported EFaceTextureType types
	 */
	static bool UpdateFaceTextures(
		const TMap<EFaceTextureType, FImage>& InCachedImages, 
		TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures);
};
