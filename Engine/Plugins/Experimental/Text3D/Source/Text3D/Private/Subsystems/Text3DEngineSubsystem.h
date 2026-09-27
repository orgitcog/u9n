// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "GeometryBuilders/Text3DGlyphContourNode.h"
#include "Subsystems/EngineSubsystem.h"
#include "Text3DModule.h"
#include "Text3DTypes.h"

#include "Text3DEngineSubsystem.generated.h"

class FFreeTypeFace;
class UFont;
class UMaterial;
class UStaticMesh;
class UWorld;
struct FFontFaceData;

USTRUCT()
struct FGlyphMeshParameters
{
	GENERATED_BODY()

	UPROPERTY()
	float Extrude = 5.0f;

	UPROPERTY()
	float Bevel = 0.0f;

	UPROPERTY()
	EText3DBevelType BevelType = EText3DBevelType::Convex;

	UPROPERTY()
	int32 BevelSegments = 8;

	UPROPERTY()
	bool bOutline = false;

	UPROPERTY()
	float OutlineExpand = 0.5f;

	UPROPERTY()
	uint32 TypefaceIndex = 0;

	UPROPERTY()
	FVector PivotOffset = FVector::ZeroVector;

	friend uint32 GetTypeHash(const FGlyphMeshParameters& InElement)
	{
		uint32 HashParameters = 0;
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.Extrude));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.Bevel));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.BevelType));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.BevelSegments));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.bOutline));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.OutlineExpand));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.TypefaceIndex));
		HashParameters = HashCombine(HashParameters, GetTypeHash(InElement.PivotOffset));
		return HashParameters;
	}
};

USTRUCT()
struct FText3DCachedMesh
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY()
	FBox MeshBounds = FBox(ForceInitToZero);

	UPROPERTY()
	FVector MeshOffset = FVector::ZeroVector;
};

USTRUCT()
struct FCachedFontMeshes
{
	GENERATED_BODY()

	FCachedFontMeshes();

	int32 GetCacheCount() const;
	TSharedPtr<int32> GetCacheCounter();

	UPROPERTY()
	TMap<uint32, FText3DCachedMesh> Glyphs;

private:
	TSharedPtr<int32> CacheCounter;
};

USTRUCT()
struct FTypefaceFontData
{
	GENERATED_BODY()

	FTypefaceFontData();

	int32 GetCacheCount() const
	{
		const int32 Count = CacheCounter.GetSharedReferenceCount();
		return Count;
	}

#if WITH_FREETYPE
	FT_Face GetTypeFace() const { return TypeFace; }
	void SetTypeFace(FT_Face InTypeFace);

	TSharedPtr<const FFontFaceData> GetTypeFaceData() const;
	void SetTypeFaceData(const TSharedPtr<const FFontFaceData>& InTypeFaceData);
#endif

	FName GetTypefaceName() const { return TypefaceName; }
	void SetTypefaceName(FName InTypefaceName)
	{
		TypefaceName = InTypefaceName;
	}

	uint32 GetTypefaceFontDataHash() const { return TypefaceFontDataHash; }
	void SetTypefaceFontDataHash(uint32 InDataHash)
	{
		TypefaceFontDataHash = InDataHash;
	}

	TMap<uint32, FCachedFontMeshes>& GetMeshes() { return Meshes; }

	TSharedPtr<int32> GetCacheCounter() { return CacheCounter; }

	FCachedFontMeshes& FindOrAddMeshes(uint32 InHashParameters)
	{
		return Meshes.FindOrAdd(InHashParameters);
	}

private:
	UPROPERTY()
	TMap<uint32, FCachedFontMeshes> Meshes;

	FName TypefaceName = NAME_None;
	TSharedPtr<int32> CacheCounter;
	uint32 TypefaceFontDataHash = 0;

#if WITH_FREETYPE
	TWeakPtr<const FFontFaceData> TypeFaceDataWeak;
	FT_Face TypeFace = nullptr;
#endif
};

USTRUCT()
struct FCachedFontData
{
	GENERATED_BODY()

	FCachedFontData();
	~FCachedFontData();

#if WITH_FREETYPE
	FT_Face GetFreeTypeFace(uint32 InTypefaceIndex);
#endif

	FString GetFontName() const;

	UFont* GetFont() { return Font; }
	void SetFont(UFont* InFont)
	{
		Font = InFont;
	}

	void LoadFreeTypeFace(uint32 InTypefaceIndex);
	void ClearFreeTypeFace();

	bool Cleanup();
	bool CleanupTypeface(uint32 InTypefaceIndex);

	uint32 GetTypefaceFontDataHash(int32 InTypefaceEntryIndex) const;
	const FTypefaceFontData* GetTypefaceFontData(int32 InTypefaceEntryIndex) const;
	FTypefaceFontData* GetTypefaceFontData(int32 InTypefaceEntryIndex);
	TSharedPtr<int32> GetCacheCounter(int32 InTypefaceEntryIndex);
	TSharedPtr<int32> GetMeshesCacheCounter(const FGlyphMeshParameters& InParameters);

	FText3DCachedMesh* GetGlyphMesh(uint32 InGlyphIndex, const FGlyphMeshParameters& InParameters);
	TText3DGlyphContourNodeShared GetGlyphContours(uint32 InGlyphIndex, int32 InTypefaceEntryIndex);

	void PrintCache() const;

private:
	UPROPERTY()
	TObjectPtr<UFont> Font;

	UPROPERTY()
	TMap<uint32, FTypefaceFontData> TypefaceFontDataMap;
};

UCLASS()
class UText3DEngineSubsystem : public UEngineSubsystem, public FSelfRegisteringExec
{
	GENERATED_BODY()

public:
	static UText3DEngineSubsystem* Get();

	UText3DEngineSubsystem();

	// ~Begin UEngineSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEngineSubsystem

	// ~Begin FSelfRegisteringExec
	virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* InCmd, FOutputDevice& InAr) override;
	// ~End FSelfRegisteringExec

	void PrintCache() const;

	void Reset();
	void Cleanup();
	FCachedFontData& GetCachedFontData(UFont* InFont, int32 InTypefaceEntryIndex = 0);

	UPROPERTY()
	TObjectPtr<UMaterial> DefaultMaterial;

private:
	bool CleanupTimerCallback(float DeltaTime);

	UPROPERTY()
	TMap<uint32, FCachedFontData> CachedFonts;

	FTSTicker::FDelegateHandle CleanupTickerHandle;
};
