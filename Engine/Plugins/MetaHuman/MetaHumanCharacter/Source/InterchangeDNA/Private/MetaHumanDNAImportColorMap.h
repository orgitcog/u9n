// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "MetaHumanDNAImportColorMap.generated.h"

USTRUCT(BlueprintType)
struct INTERCHANGEDNA_API FMeshVertexColorData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertex Colors")
	FString MeshName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertex Colors")
	TArray<FLinearColor> Colors;
};

UCLASS(BlueprintType)
class INTERCHANGEDNA_API UDNAMeshVertexColorDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vertex Colors")
	TArray<FMeshVertexColorData> MeshColorEntries;

	UFUNCTION(BlueprintCallable, Category = "Vertex Colors")
	FLinearColor GetColorByMeshAndIndex(const FString& InMeshName, int32 InVertexId) const
	{
		for (const FMeshVertexColorData& Entry : MeshColorEntries)
		{
			if (Entry.MeshName == InMeshName && Entry.Colors.IsValidIndex(InVertexId))
			{
				return Entry.Colors[InVertexId];
			}
		}

		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // Default white
	}
};
