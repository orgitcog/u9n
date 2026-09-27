// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitUID.h"

#define UE_API ANIMNEXTANIMGRAPH_API

namespace UE::AnimNext
{
	struct FNodeTemplate;

	/**
	  * FNodeTemplateBuilder
	  * 
	  * Utility to help construct node templates.
	  */
	struct FNodeTemplateBuilder
	{
		FNodeTemplateBuilder() = default;

		// Adds the specified trait type to the node template
		UE_API void AddTrait(FTraitUID TraitUID);

		// Returns a node template for the provided list of traits
		// The node template will be built into the provided buffer and a pointer to it is returned
		UE_API FNodeTemplate* BuildNodeTemplate(TArray<uint8>& NodeTemplateBuffer) const;

		// Returns a node template for the provided list of traits
		// The node template will be built into the provided buffer and a pointer to it is returned
		static UE_API FNodeTemplate* BuildNodeTemplate(const TArray<FTraitUID>& InTraitUIDs, TArray<uint8>& NodeTemplateBuffer);

		// Resets the node template builder
		UE_API void Reset();

	private:
		static UE_API uint32 GetNodeTemplateUID(const TArray<FTraitUID>& InTraitUIDs);
		static UE_API void AppendTemplateTrait(
			const TArray<FTraitUID>& InTraitUIDs, int32 TraitIndex,
			TArray<uint8>& NodeTemplateBuffer);

		TArray<FTraitUID> TraitUIDs;	// The list of traits to use when building the node template
	};
}

#undef UE_API
