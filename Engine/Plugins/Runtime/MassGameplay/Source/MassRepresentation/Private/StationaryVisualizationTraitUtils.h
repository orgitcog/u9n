// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UMassEntityTraitBase;
struct FMassEntityTemplateBuildContext;
struct FStaticMeshInstanceVisualizationDesc;

namespace UE::Mass::LOD::Private
{
	void SetUpStationaryVisualizationTrait(const UMassEntityTraitBase& Trait, FMassEntityTemplateBuildContext& BuildContext, FStaticMeshInstanceVisualizationDesc& StaticMeshInstanceDesc);
}