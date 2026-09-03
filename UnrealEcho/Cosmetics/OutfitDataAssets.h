// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Outfit and Accessory Data Assets

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OutfitDataAssets.generated.h"

/**
 * EOutfitCategory
 * 
 * Categories of outfits for organization and filtering.
 */
UENUM(BlueprintType)
enum class EOutfitCategory : uint8
{
	Casual,
	Formal,
	Athletic,
	Sleepwear,
	Swimwear,
	Fantasy,
	SciFi,
	Cosplay,
	Seasonal,
	Special
};

/**
 * EAccessorySlotType
 * 
 * Types of accessory slots available.
 */
UENUM(BlueprintType)
enum class EAccessorySlotType : uint8
{
	Head,           // Hats, headbands, hair accessories
	Face,           // Glasses, masks
	Ears,           // Earrings
	Neck,           // Necklaces, scarves
	Shoulders,      // Shoulder pads, capes
	Back,           // Wings, backpacks
	Chest,          // Brooches, badges
	Waist,          // Belts
	LeftHand,       // Rings, bracelets, gloves
	RightHand,      // Rings, bracelets, gloves
	LeftFoot,       // Shoes, anklets
	RightFoot,      // Shoes, anklets
	Tail,           // Tail accessories
	Custom
};

/**
 * FOutfitMaterialOverride
 * 
 * Material override for outfit customization.
 */
USTRUCT(BlueprintType)
struct FOutfitMaterialOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	int32 MaterialSlotIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName ColorParameterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FLinearColor DefaultColor;

	FOutfitMaterialOverride()
		: MaterialSlotIndex(0)
		, ColorParameterName(TEXT("BaseColor"))
		, DefaultColor(FLinearColor::White)
	{}
};

/**
 * FOutfitColorScheme
 * 
 * A predefined color scheme for an outfit.
 */
USTRUCT(BlueprintType)
struct FOutfitColorScheme
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color")
	FName SchemeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color")
	FLinearColor PrimaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color")
	FLinearColor SecondaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color")
	FLinearColor AccentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color")
	FLinearColor MetallicColor;

	FOutfitColorScheme()
		: SchemeName(TEXT("Default"))
		, PrimaryColor(FLinearColor::White)
		, SecondaryColor(FLinearColor::Gray)
		, AccentColor(FLinearColor::Red)
		, MetallicColor(FLinearColor(0.8f, 0.8f, 0.8f))
	{}
};

/**
 * UOutfitDataAsset
 * 
 * Data asset defining a complete outfit configuration.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UOutfitDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Identification
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Identity")
	FName OutfitId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Identity")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Identity")
	EOutfitCategory Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Identity")
	TSoftObjectPtr<UTexture2D> ThumbnailIcon;

	// Mesh Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Mesh")
	TSoftObjectPtr<USkeletalMesh> OutfitMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Mesh")
	TArray<FOutfitMaterialOverride> MaterialOverrides;

	// Color Schemes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Colors")
	TArray<FOutfitColorScheme> AvailableColorSchemes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Colors")
	int32 DefaultColorSchemeIndex;

	// Personality Matching
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Personality")
	float Formality; // 0 = casual, 1 = formal

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Personality")
	float Expressiveness; // 0 = subtle, 1 = bold

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Personality")
	float Warmth; // 0 = cool colors, 1 = warm colors

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Personality")
	float Complexity; // 0 = simple, 1 = complex

	// Compatibility
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Compatibility")
	TArray<EAccessorySlotType> BlockedAccessorySlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Compatibility")
	FGameplayTagContainer OutfitTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Compatibility")
	FGameplayTagContainer RequiredTags;

	// Animation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Animation")
	TSoftObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Animation")
	TSoftObjectPtr<UAnimMontage> UnequipMontage;

	// Physics
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Physics")
	bool bHasClothPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Physics")
	float ClothStiffness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outfit|Physics")
	float ClothDamping;

	UOutfitDataAsset()
		: Category(EOutfitCategory::Casual)
		, DefaultColorSchemeIndex(0)
		, Formality(0.5f)
		, Expressiveness(0.5f)
		, Warmth(0.5f)
		, Complexity(0.5f)
		, bHasClothPhysics(false)
		, ClothStiffness(0.5f)
		, ClothDamping(0.5f)
	{}
};

/**
 * UAccessoryDataAsset
 * 
 * Data asset defining an accessory item.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UAccessoryDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Identification
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Identity")
	FName AccessoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Identity")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Identity")
	EAccessorySlotType SlotType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Identity")
	TSoftObjectPtr<UTexture2D> ThumbnailIcon;

	// Mesh Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Mesh")
	TSoftObjectPtr<UStaticMesh> AccessoryMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalAccessoryMesh; // For animated accessories

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Mesh")
	TArray<FOutfitMaterialOverride> MaterialOverrides;

	// Attachment
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Attachment")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Attachment")
	FTransform AttachOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Attachment")
	bool bSnapToSocket;

	// Color Schemes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Colors")
	TArray<FOutfitColorScheme> AvailableColorSchemes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Colors")
	bool bInheritOutfitColors;

	// Compatibility
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Compatibility")
	TArray<EAccessorySlotType> BlockedSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Compatibility")
	FGameplayTagContainer AccessoryTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Compatibility")
	FGameplayTagContainer IncompatibleOutfitTags;

	// Animation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Animation")
	TSoftObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnimation; // For animated accessories

	// Physics
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Physics")
	bool bHasPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accessory|Physics")
	float PhysicsWeight;

	UAccessoryDataAsset()
		: SlotType(EAccessorySlotType::Head)
		, bSnapToSocket(true)
		, bInheritOutfitColors(true)
		, bHasPhysics(false)
		, PhysicsWeight(1.0f)
	{}
};

/**
 * UOutfitCollectionDataAsset
 * 
 * A collection of outfits and accessories that go together.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UOutfitCollectionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	FName CollectionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	FText CollectionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	TSoftObjectPtr<UTexture2D> CollectionIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	TArray<TSoftObjectPtr<UOutfitDataAsset>> Outfits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	TArray<TSoftObjectPtr<UAccessoryDataAsset>> Accessories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collection")
	FGameplayTagContainer CollectionTags;
};

/**
 * UOutfitPresetDataAsset
 * 
 * A preset combination of outfit and accessories.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UOutfitPresetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName PresetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FText PresetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	TSoftObjectPtr<UOutfitDataAsset> Outfit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	int32 ColorSchemeIndex;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	TMap<EAccessorySlotType, TSoftObjectPtr<UAccessoryDataAsset>> Accessories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	TMap<EAccessorySlotType, int32> AccessoryColorSchemes;

	// Personality matching for auto-selection
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Personality")
	float ExtraversionMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Personality")
	float OpennessMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Personality")
	float ConscientiousnessMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Personality")
	float AgreeablenessMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Personality")
	float NeuroticismMatch;

	UOutfitPresetDataAsset()
		: ColorSchemeIndex(0)
		, ExtraversionMatch(0.5f)
		, OpennessMatch(0.5f)
		, ConscientiousnessMatch(0.5f)
		, AgreeablenessMatch(0.5f)
		, NeuroticismMatch(0.5f)
	{}
};
