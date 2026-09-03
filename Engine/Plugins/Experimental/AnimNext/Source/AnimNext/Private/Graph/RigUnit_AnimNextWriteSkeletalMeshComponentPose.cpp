// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AnimNextWriteSkeletalMeshComponentPose.h"

#include "AnimNextStats.h"
#include "GenerationTools.h"
#include "Component/SkinnedMeshComponentExtensions.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Logging/StructuredLog.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/AnimNextSkeletalMeshComponentReferenceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextWriteSkeletalMeshComponentPose)

DEFINE_STAT(STAT_AnimNext_Write_Pose);

FRigUnit_AnimNextWriteSkeletalMeshComponentPose_Execute()
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Write_Pose);

	using namespace UE::AnimNext;
	FTaskTagScope TaskTag(ETaskTag::EParallelGameThread);

	USkeletalMeshComponent* OutputComponent = SkeletalMeshComponent;

	// Defer to module component if no component is supplied
	if(OutputComponent == nullptr)
	{
		const FAnimNextModuleContextData& ModuleContextData = ExecuteContext.GetContextData<FAnimNextModuleContextData>();
		FAnimNextModuleInstance& ModuleInstance = ModuleContextData.GetModuleInstance();
		const FAnimNextSkeletalMeshComponentReferenceComponent& ComponentReference = ModuleInstance.GetComponent<FAnimNextSkeletalMeshComponentReferenceComponent>();
		OutputComponent = ComponentReference.GetComponent();
	}

	if(OutputComponent == nullptr)
	{
		return;
	}

	if(!Pose.LODPose.IsValid())
	{
		return;
	}

	USkeletalMesh* SkeletalMesh = OutputComponent->GetSkeletalMeshAsset();
	if(SkeletalMesh == nullptr)
	{
		return;
	}

	// We cannot write to the skeletal mesh component if it is using anim BP
	if(OutputComponent->bEnableAnimation)
	{
		UE_LOGFMT(LogAnimation, Warning, "UAF: Could not write to skeletal mesh component - bEnableAnimation is true [SK: {SkeletalMesh}][Owner: {Owner}]",
			*SkeletalMesh->GetName(), OutputComponent->GetOuter() != nullptr ? *OutputComponent->GetOuter()->GetName() : TEXT("*NO OWNER*"));
		return;
	}

	FDataHandle RefPoseHandle = FDataRegistry::Get()->GetOrGenerateReferencePose(OutputComponent);
	const UE::AnimNext::FReferencePose& RefPose = RefPoseHandle.GetRef<UE::AnimNext::FReferencePose>();

	// We cannot use two different reference poses because we want to avoid an expensive remap operation
	// You should remap the pose explicitly if this is what you need
	if (&RefPose != &Pose.LODPose.GetRefPose())
	{
		UE_LOGFMT(LogAnimation, Warning, "UAF: Could not write to skeletal mesh component - The input pose and the skeletal mesh component use different reference poses [SK: {SkeletalMesh}][Owner: {Owner}]",
			*SkeletalMesh->GetName(), OutputComponent->GetOuter() != nullptr ? *OutputComponent->GetOuter()->GetName() : TEXT("*NO OWNER*"));
		return;
	}

	FMemMark MemMark(FMemStack::Get());
	USkinnedMeshComponent::FRenderStateLockScope LockScope(OutputComponent);

	// TODO: The LOD level we got here is not guaranteed to be stable. We should find another way to get/bind the skeletal mesh component's LOD level.
	const int32 SkeletalMeshComponentLODLevel = OutputComponent->GetPredictedLODLevel();
	const int32 PoseLODLevel = Pose.LODPose.LODLevel;

	TArray<FTransform, TMemStackAllocator<>> LocalSpaceTransforms;

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	LocalSpaceTransforms.SetNumUninitialized(RefSkeleton.GetNum());

#if UE_ENABLE_POSE_DEBUG_FILL
	FMemory::Memset(LocalSpaceTransforms.GetData(), 0xCD, sizeof(FTransform) * LocalSpaceTransforms.Num());
#endif

	// Did we evaluate animation with lower quality than the visual mesh?
	if (SkeletalMeshComponentLODLevel < PoseLODLevel)
	{
		// The given pose is missing transforms that the visual representation needs, initialize these with the reference pose.

		const TArrayView<const FBoneIndexType> PoseBoneToMeshBoneIndexMap = RefPose.GetLODBoneIndexToMeshBoneIndexMap(SkeletalMeshComponentLODLevel);
		const int32 NumPoseBones = Pose.LODPose.LocalTransforms.Num();
		const int32 NumSkeletalMeshBones = PoseBoneToMeshBoneIndexMap.Num();

		for (int32 PoseBoneIndex = NumPoseBones; PoseBoneIndex < NumSkeletalMeshBones; ++PoseBoneIndex)
		{
			const int32 MeshBoneIndex = PoseBoneToMeshBoneIndexMap[PoseBoneIndex];
			LocalSpaceTransforms[MeshBoneIndex] = RefSkeleton.GetRefBonePose()[MeshBoneIndex];
		}
	}

	// Clear our curves and attributes or we'll have leftovers from our last write.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OutputComponent->AnimCurves.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	OutputComponent->GetEditableCustomAttributes().Empty();


	// Map LOD pose into local-space scratch buffer
	FGenerationTools::RemapPose(Pose.LODPose, LocalSpaceTransforms);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OutputComponent->AnimCurves.CopyFrom(Pose.Curves);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Attributes require remapping since the indices are LOD indices and we want mesh indices
	FGenerationTools::RemapAttributes(Pose.LODPose, Pose.Attributes, OutputComponent->GetEditableCustomAttributes());

	// Convert and dispatch to renderer
	UE::Anim::FSkinnedMeshComponentExtensions::CompleteAndDispatch(
		OutputComponent,
		RefPose.GetMeshBoneIndexToParentMeshBoneIndexMap(),
		RefPose.GetLODBoneIndexToMeshBoneIndexMap(SkeletalMeshComponentLODLevel), // Based on the LOD level of the skeletal mesh component
		LocalSpaceTransforms);
}
