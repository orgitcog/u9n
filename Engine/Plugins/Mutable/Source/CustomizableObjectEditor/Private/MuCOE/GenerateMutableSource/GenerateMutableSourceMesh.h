// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/Table.h"
#include "Animation/AnimInstance.h"
#include "UObject/SoftObjectPtr.h"

class FName;
class FString;
class UCustomizableObjectNode;
class UEdGraphPin;
class UObject;
class USkeletalMesh;
class UStaticMesh;
class UStreamableRenderAsset;
struct FMutableGraphGenerationContext;
struct FMutableGraphMeshGenerationData;
struct FMutableCompilationContext;
struct FMutableSourceMeshData;

namespace mu
{
	class FMeshBufferSet;
}


/** Returns the corrected LOD and Section Index when using Automatic LOD From Mesh strategy.
 *
 * Do not confuse Section Index and Material Index, they are not the same.
 * 
 * @param Context 
 * @param Node 
 * @param SkeletalMesh 
 * @param LODIndexConnected Connected pin LOD Index.
 * @param SectionIndexConnected Connected pin Section Index.
 * @param OutLODIndex Corrected Skeletal Mesh LOD Index.
 * @param OutSectionIndex Corrected Skeletal Mesh Section Index.
 * @param bOnlyConnectedLOD Corrected LOD and Section will unconditionally always be the connected ones.
 * @return When using Automatic LOD From Mesh, InOutLODIndex and InOutSectionIndex and will return -1 if the section is not found in the currently compiling LOD. */
void GetLODAndSectionForAutomaticLODs(const FMutableGraphGenerationContext& Context, const UCustomizableObjectNode& Node, const USkeletalMesh& SkeletalMesh,
	const int32 LODIndexConnected, const int32 SectionIndexConnected,
	int32& OutLODIndex, int32& OutSectionIndex,
	bool bOnlyConnectedLOD);


/** Converts an Unreal Skeletal Mesh to Mutable Mesh.
 *
 * @param Source. Mesh data required to generate the mutable mesh
 * @param LODIndexConnected LOD which the pin is connected to.
 * @param SectionIndexConnected Section which the pin is connected to.
 * @param LODIndex LOD we are generating. Will be different from LODIndexConnected only when using Automatic LOD From Mesh. 
 * @param SectionIndex Section we are generating. Will be different from SectionIndexConnected only when using Automatic LOD From Mesh.
 * @param GenerationContext 
 * @param CurrentNode The node being compiled, can be a skeletal mesh or a table node.
 * @return Mutable Mesh. Nullptr if there has been an error. Empty mesh if the Skeletal Mesh does not contain the requested LOD + Section. */
TSharedPtr<mu::FMesh> ConvertSkeletalMeshToMutable(FMutableSourceMeshData& Source,
                                         bool bMeshMustExist,
                                         int32 LODIndex, int32 SectionIndex,
                                         FMutableGraphGenerationContext& GenerationContext,
                                         const UCustomizableObjectNode* CurrentNode, 
										 bool bForceImmediateConversion = false );


TSharedPtr<mu::FMesh> ConvertStaticMeshToMutable(const UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex,
	FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode);


/** 
 * @param Source. Mesh data required to generate the mutable mesh
 * @param LODIndexConnected LOD which the pin is connected to.
 * @param SectionIndexConnected Section which the pin is connected to.
 * @param LODIndex LOD we are generating. Will be different from LODIndexConnected only when using Automatic LOD From Mesh. 
 * @param SectionIndex Section we are generating. Will be different from SectionIndexConnected only when using Automatic LOD From Mesh.
 * @param MeshUniqueTags
 * @param GenerationContext
 * @param CurrentNode The node being compiled, can be a skeletal mesh or a table node.
 * @return Mutable Mesh. Nullptr if there has been an error. Empty mesh if the Skeletal Mesh does not contain the requested LOD + Section. */
TSharedPtr<mu::FMesh> GenerateMutableSkeletalMesh(FMutableSourceMeshData& Source,
                                int32 LODIndexConnected, int32 SectionIndexConnected,
                                int32 LODIndex, int32 SectionIndex, const FString& MeshUniqueTags, 
                                FMutableGraphGenerationContext& GenerationContext, 
								const UCustomizableObjectNode* CurrentNode);

/** */
TSharedPtr<mu::FMesh> GenerateMutableStaticMesh(TSoftObjectPtr<UStreamableRenderAsset> Mesh, const TSoftClassPtr<UAnimInstance>& AnimBp,
	int32 LODIndex, int32 SectionIndex, const FString& MeshUniqueTags,
	FMutableGraphGenerationContext& GenerationContext,
	const UCustomizableObjectNode* CurrentNode, USkeletalMesh* TableReferenceSkeletalMesh,
	bool bIsPassthrough);


TSharedPtr<mu::FMesh> BuildMorphedMutableMesh(const UEdGraphPin* BaseSourcePin, const FString& MorphTargetName,
	FMutableGraphGenerationContext& GenerationContext, bool bOnlyConnectedLOD, const FName& RowName = "");


/** Compiler recursive function. Mutable Node Mesh.
 *
 * @param Pin 
 * @param GenerationContext 
 * @param BaseMeshData 
 * @param bLinkedToExtendMaterial 
 * @param bOnlyConnectedLOD Corrected LOD and Section will unconditionally always be the connected ones.
 * @return  Mutable Mesh Node. */
mu::Ptr<mu::NodeMesh> GenerateMutableSourceMesh(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, const FMutableSourceMeshData& BaseMeshData, bool bLinkedToExtendMaterial, bool bOnlyConnectedLOD);


/** */
extern void SetSurfaceFormat(FMutableCompilationContext&,
	mu::FMeshBufferSet& OutVertexBufferFormat, 
	mu::FMeshBufferSet& OutIndexBufferFormat, 
	const FMutableGraphMeshGenerationData& MeshData);
