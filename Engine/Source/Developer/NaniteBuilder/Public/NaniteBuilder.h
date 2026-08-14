// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"
#include "Math/Bounds.h"

struct FMeshBuildVertexView;
struct FMeshNaniteSettings;

namespace Nanite
{

const int32 MaxSectionArraySize = 64;

struct FResources;
struct FIntermediateResources;
using FAssemblyPartResourceRef = TSharedPtr<FIntermediateResources, ESPMode::NotThreadSafe>;
using FMaterialRemapTable = TStaticArray<uint32, MaxSectionArraySize>;

struct FInputAssemblyData
{
	struct FBuiltPartData
	{
		FAssemblyPartResourceRef Resource;
		FMaterialRemapTable MaterialRemap;
	};

	bool IsValid() const { return Nodes.Num() > 0 && Parts.Num() > 0; }

	TArray<FNaniteAssemblyNode> Nodes;
	TArray<FBuiltPartData> Parts;
};

struct FRayTracingFallbackBuildSettings
{
	float FallbackPercentTriangles = 1.0f;
	float FallbackRelativeError = 2.0f;
	float FoliageOverOcclusionBias = 0.0f;

	bool IsFallbackReduced() const
	{
		return FallbackPercentTriangles < 1.0f
			|| FallbackRelativeError > 0.0f
			|| FoliageOverOcclusionBias > 0.0f;
	}
};

class IBuilderModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IBuilderModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IBuilderModule>("NaniteBuilder");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NaniteBuilder");
	}

	virtual const FString& GetVersionString() const = 0;

	struct FInputMeshData
	{
		FMeshBuildVertexData Vertices;
		TArray<uint32> TriangleIndices;
		TArray<uint32> TriangleCounts;
		TArray<int32>  MaterialIndices;
		FMeshDataSectionArray Sections;
		FBounds3f VertexBounds;
		uint8 NumTexCoords = 0;
		uint8 NumBoneInfluences = 0;
		float PercentTriangles = 1.0f;
		float MaxDeviation = 0.0f;
	};

	struct FOutputMeshData
	{
		FMeshBuildVertexData Vertices;
		TArray<uint32> TriangleIndices;
		FMeshDataSectionArray Sections;
	};

	virtual FAssemblyPartResourceRef BuildAssemblyPart(
		FInputMeshData& InputMeshData,
		const FMeshNaniteSettings& Settings)
	{
		return nullptr;
	}

	virtual bool Build(
		FResources& Resources,
		FInputMeshData& InputMeshData,
		FOutputMeshData* OutFallbackMeshData,
		FOutputMeshData* OutRayTracingFallbackMeshData,
		const FRayTracingFallbackBuildSettings* RayTracingFallbackBuildSettings,
		const FMeshNaniteSettings& Settings,
		FInputAssemblyData* AssemblyData = nullptr)
	{
		return false;
	}

	virtual bool BuildMaterialIndices(
		const FMeshDataSectionArray& SectionArray,
		const uint32 TriangleCount,
		TArray<int32>& OutMaterialIndices)
	{
		return false;
	}
};

} // namespace Nanite