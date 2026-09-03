// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"

#include "NaniteAssemblyData.generated.h"

class UStaticMesh;

/** A single instance of a given mesh in a Nanite Assembly. */
USTRUCT()
struct FNaniteAssemblyNode
{
	GENERATED_USTRUCT_BODY()

	/** The index of this node's parent node */
	UPROPERTY()
	int32 ParentIndex = INDEX_NONE;
	
	/** The index of the assembly part mesh this node instances */
	UPROPERTY()
	int32 PartIndex = INDEX_NONE;

	/** The local transform of the node */
	UPROPERTY()
	FMatrix44f Transform = FMatrix44f::Identity;

	bool operator==(const FNaniteAssemblyNode& Other) const
	{
		return ParentIndex == Other.ParentIndex &&
			PartIndex == Other.PartIndex &&
			Transform.Equals(Other.Transform);
	}
	bool operator!=(const FNaniteAssemblyNode& Other) const { return !(*this == Other); }
};

/** A mesh to be instanced as a part of a Nanite Assembly */
USTRUCT()
struct FNaniteAssemblyPart
{
	GENERATED_USTRUCT_BODY()

	/** The static mesh to render for the part */
	UPROPERTY(VisibleAnywhere, Category = General)
	FSoftObjectPath MeshObjectPath;

	/** The mapping of the part's materials to the final material list (Empty means material indices map 1:1) */
	UPROPERTY()
	TArray<int32> MaterialRemap;

	bool operator==(const FNaniteAssemblyPart& Other) const
	{
		return MeshObjectPath == Other.MeshObjectPath &&
			MaterialRemap == Other.MaterialRemap;
	}
	bool operator!=(const FNaniteAssemblyPart& Other) const { return !(*this == Other); }
};

/** Data to describe a Nanite Assembly */
USTRUCT()
struct FNaniteAssemblyData
{
	GENERATED_USTRUCT_BODY()

	/** The list of assembly parts */
	UPROPERTY(VisibleAnywhere, Category = General)
	TArray<FNaniteAssemblyPart> Parts;

	/** The list of assembly part instance nodes */
	UPROPERTY()
	TArray<FNaniteAssemblyNode> Nodes;

	const bool IsValid() const { return Parts.Num() > 0 && Nodes.Num() > 0; }
	bool operator==(const FNaniteAssemblyData& Other) const {  return Parts == Other.Parts && Nodes == Other.Nodes; }
	bool operator!=(const FNaniteAssemblyData& Other) const { return !(*this == Other); }

	void SerializeForDDC(FArchive& Ar)
	{
		bool bValid = IsValid();
		Ar << bValid;
		if (bValid)
		{
			for (FNaniteAssemblyPart& Part : Parts)
			{
				// TODO: Nanite-Assemblies: Changes to referenced static meshes currently won't invalidate DDC
				FString Path = Part.MeshObjectPath.ToString();
				Ar << Path;
				Ar << Part.MaterialRemap;
			}
			Ar << Nodes;
		}

	}
};

/** Serializes a FNaniteAssemblyNode */
inline FArchive& operator<<(FArchive& Ar, FNaniteAssemblyNode& Node)
{
	Ar << Node.ParentIndex;
	Ar << Node.PartIndex;
	Ar << Node.Transform;

	return Ar;
}

/** Serializes a FNaniteAssemblyPart */
inline FArchive & operator<<(FArchive& Ar, FNaniteAssemblyPart& Part)
{
	Ar << Part.MeshObjectPath;
	Ar << Part.MaterialRemap;

	return Ar;
}

/** Serializes a FNaniteAssemblyData */
inline FArchive& operator<<(FArchive& Ar, FNaniteAssemblyData& Data)
{
	Ar << Data.Parts;
	Ar << Data.Nodes;
	
	return Ar;
}