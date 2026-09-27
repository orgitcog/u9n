// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Text3DTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "Text3DProjectSettings.generated.h"

enum class EText3DMaterialBlendMode : uint8;
class UFontFace;
class UFont;
class UMaterial;

/** Settings for Text3D plugin */
UCLASS(MinimalAPI, Config=Text3D, meta=(DisplayName="Text3D"))
class UText3DProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	friend class UText3DEditorFontSubsystem;

public:
	static TEXT3D_API const UText3DProjectSettings* Get();
	static TEXT3D_API UText3DProjectSettings* GetMutable();

	struct FMaterialParameters
	{
		static const FLazyName Mode;

		// Solid
		static const FLazyName SolidColor;
		static const FLazyName Opacity;
		
		// Gradient
		static const FLazyName GradientOffset;
		static const FLazyName GradientColorA;
		static const FLazyName GradientColorB;
		static const FLazyName GradientRotation;
		static const FLazyName GradientSmoothness;

		// Texture
		static const FLazyName MainTexture;
		static const FLazyName TexturedUTiling;
		static const FLazyName TexturedVTiling;

		// Bounds
		static const FLazyName BoundsOrigin;
		static const FLazyName BoundsSize;
		static const FLazyName TextPosition;

		// Mask
		static const FLazyName MaskEnabled;
		static const FLazyName MaskRotation;
		static const FLazyName MaskOffset;
		static const FLazyName MaskSmoothness;
	};

	UText3DProjectSettings();

	TEXT3D_API UMaterial* GetDefaultMaterial() const;

	TEXT3D_API UFont* GetFallbackFont() const;
	TEXT3D_API UFontFace* GetFallbackFontFace() const;
	TEXT3D_API const FString& GetFontDirectory() const;

	TEXT3D_API UMaterial* GetBaseMaterial(const FText3DMaterialKey& InKey) const;

#if WITH_EDITOR
	void OpenEditorSettingsWindow() const;
	TEXT3D_API TConstArrayView<FString> GetFavoriteFonts() const;
	TEXT3D_API bool GetShowOnlyMonospaced() const;
	TEXT3D_API bool GetShowOnlyBold() const;
	TEXT3D_API bool GetShowOnlyItalic() const;

	TEXT3D_API void AddFavoriteFont(const FString& InFontName);
	TEXT3D_API void RemoveFavoriteFont(const FString& InFontName);
	TEXT3D_API void SetShowOnlyMonospaced(bool InShowOnlyMonospaced);
	TEXT3D_API void SetShowOnlyBold(bool InShowOnlyBold);
	TEXT3D_API void SetShowOnlyItalic(bool InShowOnlyItalic);
#endif

protected:
	void AddMaterial(FText3DMaterialKey&& InKey, TSoftObjectPtr<UMaterial>&& InMaterial);

#if WITH_EDITOR
	//~ Begin UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InChangeEvent) override;
	//~ End UObject

	void Save();
#endif

	/** Default custom material used on Text */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	TSoftObjectPtr<UMaterial> DefaultMaterial;
	
	/** Font used when the selected font is unavailable */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	TSoftObjectPtr<UFont> FallbackFont;

	/** Font face used as fallback when no font faces are found */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	TSoftObjectPtr<UFontFace> FallbackFontFace;

	/** Default project directory where system fonts will be imported and stored as UFont */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	FString FontDirectory;

#if WITH_EDITORONLY_DATA
	/** Favorites fonts pinned in the font viewer dropdown */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	TArray<FString> FavoriteFonts;

	/** Only show monospaced fonts in font viewer */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	bool bShowOnlyMonospaced = false;

	/** Only show fonts with bold support in font viewer */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	bool bShowOnlyBold = false;

	/** Only show fonts with italic support in font viewer */
	UPROPERTY(Config, EditAnywhere, Category="Text3D")
	bool bShowOnlyItalic = false;

	/** System font names retrieved from the current platform */
	UPROPERTY(VisibleAnywhere, Transient, AdvancedDisplay, Category="Text3D")
	TArray<FString> SystemFontNames;
#endif

	/** Base parent material */
	UPROPERTY(Transient)
	TMap<FText3DMaterialKey, TSoftObjectPtr<UMaterial>> BaseMaterials;
};
