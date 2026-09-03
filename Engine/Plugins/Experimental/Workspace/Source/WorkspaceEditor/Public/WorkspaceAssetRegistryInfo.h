// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "WorkspaceAssetRegistryInfo.generated.h"

USTRUCT()
struct FWorkspaceOutlinerItemData
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemData() = default;
};

USTRUCT()
struct FOutlinerItemPath
{
	GENERATED_BODY()
	
	friend struct FWorkspaceOutlinerItemExport;
protected:
	UPROPERTY()
	TArray<FName> PathSegments;

public:
	static FOutlinerItemPath MakePath(const FSoftObjectPath& InSoftObjectPath)
	{
		FOutlinerItemPath Path;
		Path.PathSegments.Add(*InSoftObjectPath.ToString());
		return Path;
	}

	FOutlinerItemPath AppendSegment(const FName& InSegment) const
	{
		FOutlinerItemPath Path = *this;
		Path.PathSegments.Add(InSegment);
		return Path;
	}

	FOutlinerItemPath RemoveSegment() const
	{
		FOutlinerItemPath Path = *this;
		if (Path.PathSegments.Num())
		{
			Path.PathSegments.Pop();
		}			
		return Path;
	}

	friend uint32 GetTypeHash(const FOutlinerItemPath& Path)
	{
		uint32 Hash = INDEX_NONE;

		if (Path.PathSegments.Num() == 1)
		{
			Hash = GetTypeHash(Path.PathSegments[0]);
		}
		else if (Path.PathSegments.Num() > 1)
		{
			Hash = GetTypeHash(Path.PathSegments[0]);
			for (int32 Index = 1; Index < Path.PathSegments.Num(); ++Index)
			{
				Hash = HashCombine(Hash, GetTypeHash(Path.PathSegments[Index]));				
			}
			return Hash;
		}
		
		return Hash;
	}
};

USTRUCT()
struct FWorkspaceOutlinerItemExport
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemExport() = default;

	FWorkspaceOutlinerItemExport(const FName InIdentifier, const FSoftObjectPath& InObjectPath)
	{
		Path.PathSegments.Add(*InObjectPath.ToString());
		Path.PathSegments.Add(InIdentifier);
	}
	
	FWorkspaceOutlinerItemExport(const FName InIdentifier, const FWorkspaceOutlinerItemExport& InParent) : Path(InParent.Path.AppendSegment(InIdentifier))
	{
	}

	FWorkspaceOutlinerItemExport(const FName InIdentifier, const FWorkspaceOutlinerItemExport& InParent, const TInstancedStruct<FWorkspaceOutlinerItemData>& InData) : Path(InParent.Path.AppendSegment(InIdentifier)),  Data(InData)
	{
	}

protected:
	/** Full 'path' to item this instance represents, expected to take form of AssetPath follow by a set of identifier names */
	UPROPERTY()
	FOutlinerItemPath Path;

	UPROPERTY()
	TInstancedStruct<FWorkspaceOutlinerItemData> Data;
public:
	FName GetIdentifier() const
	{
		// Path needs atleast two segments to contain a valid identifier
		if(Path.PathSegments.Num() > 1)
		{
			return Path.PathSegments.Last();	
		}

		return NAME_None;
	}
	
	FName GetParentIdentifier() const
	{
		// Path needs atleast three segments to contain a valid _parent_ identifier
		const int32 NumSegments = Path.PathSegments.Num();
		if (NumSegments > 2)
		{
			return Path.PathSegments[FMath::Max(NumSegments - 2, 0)];
		}

		return NAME_None;
	}

	template<typename AssetClass>
	AssetClass* GetFirstAssetOfType() const
	{
		if(Path.PathSegments.Num() > 0)
		{
			for (int32 SegmentIndex = Path.PathSegments.Num() - 1; SegmentIndex >= 0; SegmentIndex--)
			{
				FSoftObjectPath ObjectPath = Path.PathSegments[SegmentIndex].ToString();
				if (ObjectPath.IsValid() && ObjectPath.IsAsset())
				{
					if (UObject* Object = ObjectPath.TryLoad())
					{
						if (AssetClass* TypedObject = Cast<AssetClass>(Object))
						{
							return TypedObject;
						}
					}
				}
			}
		}

		return nullptr;
	}

	// Returns the first FSoftObjectPath found in segments, starting from the end. e.g. "SoftObjectPath" - "Foo" - "SoftObjectPathTwo" - "Bar" will return SoftObjectPathTwo
	FSoftObjectPath GetFirstAssetPath() const
	{
		// Path needs atleast one segment to contain a (potentially) valid asset path
		if(Path.PathSegments.Num() > 0)
		{
			for (int32 SegmentIndex = Path.PathSegments.Num() - 1; SegmentIndex >= 0; SegmentIndex--)
			{
				FSoftObjectPath ObjectPath = Path.PathSegments[SegmentIndex].ToString();
				if (ObjectPath.IsValid() && ObjectPath.IsAsset())
				{
					return ObjectPath;	
				}
			}
			
			return FSoftObjectPath(Path.PathSegments[0].ToString());
		}
		
		return FSoftObjectPath();
	}

	// Returns the first path segment as a FSoftObjectPath. e.g:
	//  - "SoftObjectPath" - "Foo" - "SoftObjectPathTwo" - "Bar" will return SoftObjectPath	
	//  - "Foo" - "SoftObjectPath" - "SoftObjectPathTwo" - "Bar" will return FSoftObjectPath()
	FSoftObjectPath GetTopLevelAssetPath() const
	{
		// Path needs atleast one segment to contain a (potentially) valid asset path
		if(Path.PathSegments.Num() > 0)
		{
			return FSoftObjectPath(Path.PathSegments[0].ToString());
		}
		
		return FSoftObjectPath();
	}

	// Returns all valid FSoftObjectPaths found in path segments, starting from the end. e.g:
	//  - "SoftObjectPath" - "Foo" - "SoftObjectPathTwo" - "Bar" will return "SoftObjectPathTwo", "SoftObjectPath"
	//  - "Foo" - "SoftObjectPath" - "SoftObjectPathTwo" - "Bar" will also return "SoftObjectPathTwo", "SoftObjectPath"	
	void GetAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if(Path.PathSegments.Num() > 0)
		{
			for (int32 SegmentIndex = Path.PathSegments.Num() - 1; SegmentIndex >= 0; SegmentIndex--)
			{
				FSoftObjectPath ObjectPath = Path.PathSegments[SegmentIndex].ToString();
				if (ObjectPath.IsValid() && ObjectPath.IsAsset())
				{
					OutAssetPaths.Add(ObjectPath);
				}
			}
		}
	}
	// Returns all valid FWorkspaceOutlinerItemExports found in path segments, starting from the end. e.g:
	//  - "SoftObjectPath" - "Foo" - "SoftObjectPathTwo" - "Bar" will return "SoftObjectPath" - "Bar" , "SoftObjectPathTwo" - "Foo"
	void GetExports(TArray<FWorkspaceOutlinerItemExport>& OutExports) const
	{
		if(Path.PathSegments.Num() > 0)
		{
			bool bFirstExport = true;
			const int32 NumSegments = Path.PathSegments.Num();
			for (int32 SegmentIndex = NumSegments - 1; SegmentIndex >= 0; SegmentIndex--)
			{				
				FSoftObjectPath ObjectPath = Path.PathSegments[SegmentIndex].ToString();
				if (ObjectPath.IsValid() && ObjectPath.IsAsset())
				{
					FWorkspaceOutlinerItemExport Export;
					Export.Path.PathSegments.Append(&Path.PathSegments[0], bFirstExport ? NumSegments : SegmentIndex + 1);
					OutExports.Add(Export);
					bFirstExport = false;
				}
			}
		}
	}

	FString GetFullPath() const
	{
		FString FullPath;
		for (int32 SegmentIndex = 0; SegmentIndex < Path.PathSegments.Num(); SegmentIndex++)
		{
			FullPath.Append(Path.PathSegments[SegmentIndex].ToString());
		}

		return FullPath;
	}
	
	// Remove identifier segment to retrieve parent path hash
	uint32 GetParentHash() const { return GetTypeHash(Path.RemoveSegment()); }

	// Returns whether or not Data has any instanced struct setup
	bool HasData() const { return Data.IsValid(); }
	
	const TInstancedStruct<FWorkspaceOutlinerItemData>& GetData() const { return Data; }
	TInstancedStruct<FWorkspaceOutlinerItemData>& GetData() { return Data; }	

	friend uint32 GetTypeHash(const FWorkspaceOutlinerItemExport& Export)
	{
		return GetTypeHash(Export.Path);
	}

	// Returns the inner ReferredExport from the item data only valid for asset references, otherwise will return *this
	WORKSPACEEDITOR_API FWorkspaceOutlinerItemExport& GetResolvedExport();	
	WORKSPACEEDITOR_API const FWorkspaceOutlinerItemExport& GetResolvedExport() const;
};

USTRUCT()
struct WORKSPACEEDITOR_API FWorkspaceOutlinerAssetReferenceItemData : public FWorkspaceOutlinerItemData
{
	GENERATED_BODY()

	FWorkspaceOutlinerAssetReferenceItemData() = default;

	UPROPERTY()
	FSoftObjectPath ReferredObjectPath;

	UPROPERTY()
	FWorkspaceOutlinerItemExport ReferredExport;

	UPROPERTY()
	bool bRecursiveReference = false;

	static bool IsAssetReference(const FWorkspaceOutlinerItemExport& InExport)
	{
		if (InExport.HasData())
		{
			if (InExport.GetData().GetScriptStruct() == FWorkspaceOutlinerAssetReferenceItemData::StaticStruct())
			{
				return true;
			}			
		}
		
		return false;
	}
};

namespace UE::Workspace
{

static const FLazyName ExportsWorkspaceItemsRegistryTag = TEXT("WorkspaceItemExports");

}

USTRUCT()
struct FWorkspaceOutlinerItemExports
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemExports() = default;

	UPROPERTY()
	TArray<FWorkspaceOutlinerItemExport> Exports;
};
