// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ToolTargets/SkeletalMeshComponentToolTarget.h"
#include "SKMMorphTargetBackedTarget.h"
#include "ToolTargets/ToolTarget.h"
#include "MeshDescription.h"

#include "SKMMorphTargetToolTarget.generated.h"

/**
 * A tool target backed by a skeletal mesh component that can provide and take a mesh
 * description.
 */
UCLASS(Transient)
class USkeletalMeshMorphTargetToolTarget :
	public USkeletalMeshComponentReadOnlyToolTarget,
	public IDynamicMeshCommitter,
	public ISkeletalMeshMorphTargetBackedTarget
{
	GENERATED_BODY()

public:
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh() override;
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh(const FGetMeshParameters& InGetMeshParams) override;
	
	// IDynamicMeshCommitter
	virtual void CommitDynamicMesh(const UE::Geometry::FDynamicMesh3& Mesh, const FDynamicMeshCommitInfo& CommitInfo) override;
	using IDynamicMeshCommitter::CommitDynamicMesh; // unhide the other overload


	virtual void SetDataToCommit(FMeshDescription&& InMeshDescription, const FName& InToolMorphTargetName) override;
	virtual void SetEditingMorphTargetName(const FName& InName) override;
	virtual TArray<FName> GetEditableMorphTargetNames() override;
	virtual FName GetValidNameForNewMorphTarget(const FName& InName) override;
protected:
	// So that the tool target factory can poke into Component.
	friend class USkeletalMeshMorphTargetToolTargetFactory;

	FMeshDescription* GetEditingMeshDescription();
	void CommitEditedMeshDescription();
	
	UPROPERTY()
	FName EditingMorphTargetName;

	static constexpr int32 LodIndex0 = 0;

	FMeshDescription ToolMeshDescription;
	FName ToolMorphTargetName;
};




UCLASS(Transient)
class USkeletalMeshMorphTargetToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};


