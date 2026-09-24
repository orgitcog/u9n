// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAssetBase.h"
#if WITH_EDITORONLY_DATA
#include "Animation/AnimationAsset.h"
#endif
#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/SkeletalMeshConverterClassProvider.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#if WITH_EDITOR
#include "Features/IModularFeatures.h"
#include "Interfaces/ITargetPlatform.h"
#endif
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothAssetBase)
namespace UE::Chaos::ClothAsset::Private
{

	const TCHAR* MinLodQualityLevelCVarName = TEXT("p.ClothAsset.MinLodQualityLevel");
	const TCHAR* MinLodQualityLevelScalabilitySection = TEXT("ViewDistanceQuality");
	int32 MinLodQualityLevel = -1;
	FAutoConsoleVariableRef CVarClothAssetMinLodQualityLevel(
		MinLodQualityLevelCVarName,
		MinLodQualityLevel,
		TEXT("The quality level for the Min stripping LOD. \n"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* /*Variable*/)
			{
#if WITH_EDITOR || PLATFORM_DESKTOP
				if (GEngine && GEngine->UseClothAssetMinLODPerQualityLevels)
				{
					for (TObjectIterator<UChaosClothAssetBase> It; It; ++It)
					{
						UChaosClothAssetBase* ClothAsset = *It;
						if (ClothAsset && ClothAsset->GetQualityLevelMinLod().PerQuality.Num() > 0)
						{
							FSkinnedMeshComponentRecreateRenderStateContext Context(ClothAsset, false);
						}
					}
				}
#endif
			}),
		ECVF_Scalability);
}

/** Used for locking resources during async building. */
enum class UChaosClothAssetBase::EAsyncProperties : uint32
{
	None = 0,
	RenderData = 1 << 0,
	RefSkeleton = 1 << 1,
	HasVertexColors = 1 << 2,
	OverlayMaterial = 1 << 3,
	OverlayMaterialMaxDrawDistance = 1 << 4,
	All = MAX_uint32
};

UChaosClothAssetBase::UChaosClothAssetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, DataflowInstance(this)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	, MinQualityLevelLOD(0)
	, DisableBelowMinLodStripping(FPerPlatformBool(false))
	, MinLod(0)
{

	// Add the LODInfo for the default LOD 0
	LODInfo.SetNum(1);

	// Set default skeleton (must be done after having added the LOD)
	SetReferenceSkeleton(nullptr);

	MinQualityLevelLOD.SetQualityLevelCVarForCooking(UE::Chaos::ClothAsset::Private::MinLodQualityLevelCVarName, UE::Chaos::ClothAsset::Private::MinLodQualityLevelScalabilitySection);
}

void UChaosClothAssetBase::SetReferenceSkeleton(const FReferenceSkeleton* ReferenceSkeleton)
{
	// Update the reference skeleton
	if (ReferenceSkeleton)
	{
		GetRefSkeleton() = *ReferenceSkeleton;
	}
	else
	{
		// Create a default reference skeleton
		GetRefSkeleton().Empty(1);
		FReferenceSkeletonModifier ReferenceSkeletonModifier(GetRefSkeleton(), nullptr);

		FMeshBoneInfo MeshBoneInfo;
		constexpr const TCHAR* RootName = TEXT("Root");
		MeshBoneInfo.ParentIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
		MeshBoneInfo.ExportName = RootName;
#endif
		MeshBoneInfo.Name = FName(RootName);
		ReferenceSkeletonModifier.Add(MeshBoneInfo, FTransform::Identity);
	}
}

TObjectPtr<UDataflowBaseContent> UChaosClothAssetBase::CreateDataflowContent()
{
	TObjectPtr<UDataflowSkeletalContent> SkeletalContent = UE::DataflowContextHelpers::CreateNewDataflowContent<UDataflowSkeletalContent>(this);

	SkeletalContent->SetDataflowOwner(this);
	SkeletalContent->SetTerminalAsset(this);

	WriteDataflowContent(SkeletalContent);

	return SkeletalContent;
}

void UChaosClothAssetBase::WriteDataflowContent(const TObjectPtr<UDataflowBaseContent>& DataflowContent) const
{
	if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
		SkeletalContent->SetDataflowAsset(GetDataflowInstance().GetDataflowAsset());
		SkeletalContent->SetDataflowTerminal(GetDataflowInstance().GetDataflowTerminal().ToString());

#if WITH_EDITORONLY_DATA
		SkeletalContent->SetAnimationAsset(GetPreviewSceneAnimation());
		SkeletalContent->SetSkeletalMesh(GetPreviewSceneSkeletalMesh());
#endif
	}
}

void UChaosClothAssetBase::ReadDataflowContent(const TObjectPtr<UDataflowBaseContent>& DataflowContent)
{
	if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
#if WITH_EDITORONLY_DATA
		PreviewSceneAnimation = SkeletalContent->GetAnimationAsset();
		PreviewSceneSkeletalMesh = SkeletalContent->GetSkeletalMesh();
#endif
	}
}

const FDataflowInstance& UChaosClothAssetBase::GetDataflowInstance() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return DataflowInstance;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FDataflowInstance& UChaosClothAssetBase::GetDataflowInstance()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return DataflowInstance;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UChaosClothAssetBase::BeginDestroy()
{
	check(IsInGameThread());

	Super::BeginDestroy();

	// Release the mesh's render resources now
	ReleaseResources();
}

bool UChaosClothAssetBase::IsReadyForFinishDestroy()
{
	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}

	ReleaseResources();

	// see if we have hit the resource flush fence
	return ReleaseResourcesFence.IsFenceComplete();
}

void UChaosClothAssetBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AddClothAssetBase)
	{
		return;
	}
	Ar << GetRefSkeleton();
}

void UChaosClothAssetBase::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	bHasDataflowAsset = (GetDataflowInstance().GetDataflowAsset() != nullptr);
#endif  // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UChaosClothAssetBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UChaosClothAssetBase, OverlayMaterial))
	{
		ReregisterComponents();
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UChaosClothAssetBase, DataflowInstance) && 
		PropertyChangedEvent.GetPropertyName() == FName(TEXT("DataflowAsset")))  // Can't use GET_MEMBER_NAME_CHECKED, because DataflowAsset is private
	{
		bHasDataflowAsset = (GetDataflowInstance().GetDataflowAsset() != nullptr);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// This invalidates the IDataflowContentOwner, not the Dataflow itself
	InvalidateDataflowContents();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // #if WITH_EDITOR

void UChaosClothAssetBase::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (GetResourceForRendering())
	{
		GetResourceForRendering()->GetResourceSizeEx(CumulativeResourceSize);
	}

	for (int32 ModelIndex = 0; ModelIndex < GetNumClothSimulationModels(); ++ModelIndex)
	{
		if (const TSharedPtr<FChaosClothSimulationModel> ClothSimulationModel = ConstCastSharedPtr<FChaosClothSimulationModel>(GetClothSimulationModel(ModelIndex)))
		{
			ClothSimulationModel->GetResourceSizeEx(CumulativeResourceSize);
		}
	}

#if !UE_BUILD_SHIPPING
	FString MemoryReport;
	MemoryReport.Appendf(TEXT("---- Memory report for [%s] [%s] ----"), *StaticClass()->GetName(), *this->GetName());

	FResourceSizeEx RenderDataResourceSize;
	if (GetResourceForRendering())
	{
		for (int32 LodIndex = 0; LodIndex < GetResourceForRendering()->LODRenderData.Num(); ++LodIndex)
		{
			FResourceSizeEx LODRenderDataResourceSize;
			GetResourceForRendering()->LODRenderData[LodIndex].GetResourceSizeEx(LODRenderDataResourceSize);
			MemoryReport.Appendf(TEXT("\n LODRenderData LOD%d size: %ld bytes"), LodIndex, LODRenderDataResourceSize.GetTotalMemoryBytes());
		}

		GetResourceForRendering()->GetResourceSizeEx(RenderDataResourceSize);
	}
	MemoryReport.Appendf(TEXT("\n Total RenderData size: %ld bytes"), RenderDataResourceSize.GetTotalMemoryBytes());

	FResourceSizeEx ClothSimulationModelsResourceSize;
	for (int32 ModelIndex = 0; ModelIndex < GetNumClothSimulationModels(); ++ModelIndex)
	{
		if (const TSharedPtr<FChaosClothSimulationModel> ClothSimulationModel = ConstCastSharedPtr<FChaosClothSimulationModel>(GetClothSimulationModel(ModelIndex)))
		{
			for (int32 LodIndex = 0; LodIndex < ClothSimulationModel->GetNumLods(); ++LodIndex)
			{
				FResourceSizeEx ClothSimulationLodModelResourceSize;
				ClothSimulationModel->ClothSimulationLodModels[LodIndex].GetResourceSizeEx(ClothSimulationLodModelResourceSize);
				MemoryReport.Appendf(TEXT("\n ClothSimulationModel%d LOD%d size: %ld bytes"), ModelIndex, LodIndex, ClothSimulationLodModelResourceSize.GetTotalMemoryBytes());
			}

			ClothSimulationModel->GetResourceSizeEx(ClothSimulationModelsResourceSize);
		}
	}
	MemoryReport.Appendf(TEXT("\n Total ClothSimulationModel(s) size: %ld bytes"), ClothSimulationModelsResourceSize.GetTotalMemoryBytes());

	const int64 TotalResourceSize = RenderDataResourceSize.GetTotalMemoryBytes() + ClothSimulationModelsResourceSize.GetTotalMemoryBytes();
	MemoryReport.Appendf(
		TEXT("\n Total resource size for Cloth Asset [%s]: %ld bytes (%.3f MB)"),
		*this->GetName(),
		TotalResourceSize,
		(float)TotalResourceSize / (1024.f * 1024.f));

	const int64 TotalSize = CumulativeResourceSize.GetTotalMemoryBytes();
	MemoryReport.Appendf(
		TEXT("\n Total size for Cloth Asset [%s]: %ld bytes (%.3f MB)"),
		*this->GetName(),
		TotalSize,
		(float)TotalSize / (1024.f * 1024.f));

	UE_LOG(LogChaosClothAsset, Display, TEXT("\n%s"), *MemoryReport);
#endif
}

FSkeletalMeshLODInfo* UChaosClothAssetBase::GetLODInfo(int32 Index)
{
	return LODInfo.IsValidIndex(Index) ? &LODInfo[Index] : nullptr;
}

const FSkeletalMeshLODInfo* UChaosClothAssetBase::GetLODInfo(int32 Index) const
{
	return LODInfo.IsValidIndex(Index) ? &LODInfo[Index] : nullptr;
}

FReferenceSkeleton& UChaosClothAssetBase::GetRefSkeleton()
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::RefSkeleton);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RefSkeleton;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
const FReferenceSkeleton& UChaosClothAssetBase::GetRefSkeleton() const
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::RefSkeleton, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RefSkeleton;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FSkeletalMeshRenderData* UChaosClothAssetBase::GetResourceForRendering() const
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::RenderData);
	return SkeletalMeshRenderData.Get();
}

UMaterialInterface* UChaosClothAssetBase::GetOverlayMaterial() const
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::OverlayMaterial, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return OverlayMaterial;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

float UChaosClothAssetBase::GetOverlayMaterialMaxDrawDistance() const
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::OverlayMaterialMaxDrawDistance, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return OverlayMaterialMaxDrawDistance;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

const FMeshUVChannelInfo* UChaosClothAssetBase::GetUVChannelData(int32 MaterialIndex) const
{
	if (GetMaterials().IsValidIndex(MaterialIndex))
	{
		// TODO: enable ensure when UVChannelData is setup
		//ensure(GetMaterials()[MaterialIndex].UVChannelData.bInitialized);
		return &GetMaterials()[MaterialIndex].UVChannelData;
	}

	return nullptr;
}

int32 UChaosClothAssetBase::GetMinLodIdx(bool bForceLowestLODIdx) const
{
	if (IsMinLodQualityLevelEnable())
	{
		return bForceLowestLODIdx ? GetQualityLevelMinLod().GetLowestValue() : GetQualityLevelMinLod().GetValue(UE::Chaos::ClothAsset::Private::MinLodQualityLevel);
	}
	else
	{
		return GetMinLod().GetValue();
	}
}

bool UChaosClothAssetBase::GetHasVertexColors() const
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::HasVertexColors, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return bHasVertexColors != 0;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

int32 UChaosClothAssetBase::GetPlatformMinLODIdx(const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	check(TargetPlatform);
	if (IsMinLodQualityLevelEnable())
	{
		// get all supported quality level from scalability + engine ini files
		return GetQualityLevelMinLod().GetValueForPlatform(TargetPlatform);
	}
	else
	{
		return GetMinLod().GetValueForPlatform(*TargetPlatform->IniPlatformName());
	}
#else
	return 0;
#endif
}

bool UChaosClothAssetBase::IsMinLodQualityLevelEnable() const
{
	return (GEngine && GEngine->UseClothAssetMinLODPerQualityLevels);
}

void UChaosClothAssetBase::SetOverlayMaterial(UMaterialInterface* NewOverlayMaterial)
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::OverlayMaterial);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OverlayMaterial = NewOverlayMaterial;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UChaosClothAssetBase::SetOverlayMaterialMaxDrawDistance(float InMaxDrawDistance)
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::OverlayMaterialMaxDrawDistance);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OverlayMaterialMaxDrawDistance = InMaxDrawDistance;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UChaosClothAssetBase::SetDataflow(UDataflow* InDataflow)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DataflowInstance.SetDataflowAsset(InDataflow);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITORONLY_DATA
	bHasDataflowAsset = (GetDataflowInstance().GetDataflowAsset() != nullptr);
#endif
}

UDataflow* UChaosClothAssetBase::GetDataflow()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return DataflowInstance.GetDataflowAsset();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

const UDataflow* UChaosClothAssetBase::GetDataflow() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return DataflowInstance.GetDataflowAsset();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UChaosClothAssetBase::InitResources()
{
	LLM_SCOPE_BYNAME(TEXT("ClothAsset/InitResources"));

	// Build the material channel data used by the texture streamer
	UpdateUVChannelData(false);

	if (SkeletalMeshRenderData.IsValid())
	{
		TArray<UMorphTarget*> DummyMorphTargets;  // Not even used by InitResources() at the moment
		SkeletalMeshRenderData->InitResources(GetHasVertexColors(), DummyMorphTargets, this);
	}
}

void UChaosClothAssetBase::ReleaseResources()
{
	if (SkeletalMeshRenderData && SkeletalMeshRenderData->IsInitialized())
	{
		if (GIsEditor && !GIsPlayInEditorWorld)
		{
			// Flush the rendering command to be sure there is no command left that can create/modify a rendering resource
			FlushRenderingCommands();
		}

		SkeletalMeshRenderData->ReleaseResources();

		// Insert a fence to signal when these commands completed
		ReleaseResourcesFence.BeginFence();
	}
}

void UChaosClothAssetBase::CalculateInvRefMatrices()
{
	auto GetRefPoseMatrix = [this](int32 BoneIndex)->FMatrix
		{
			check(BoneIndex >= 0 && BoneIndex < GetRefSkeleton().GetRawBoneNum());
			FTransform BoneTransform = GetRefSkeleton().GetRawRefBonePose()[BoneIndex];
			BoneTransform.NormalizeRotation();  // Make sure quaternion is normalized!
			return BoneTransform.ToMatrixWithScale();
		};

	const int32 NumRealBones = GetRefSkeleton().GetRawBoneNum();

	RefBasesInvMatrix.Empty(NumRealBones);
	RefBasesInvMatrix.AddUninitialized(NumRealBones);

	// Reset cached mesh-space ref pose
	TArray<FMatrix> ComposedRefPoseMatrices;
	ComposedRefPoseMatrices.SetNumUninitialized(NumRealBones);

	// Precompute the Mesh.RefBasesInverse
	for (int32 BoneIndex = 0; BoneIndex < NumRealBones; ++BoneIndex)
	{
		// Render the default pose
		ComposedRefPoseMatrices[BoneIndex] = GetRefPoseMatrix(BoneIndex);

		// Construct mesh-space skeletal hierarchy
		if (BoneIndex > 0)
		{
			int32 Parent = GetRefSkeleton().GetRawParentIndex(BoneIndex);
			ComposedRefPoseMatrices[BoneIndex] = ComposedRefPoseMatrices[BoneIndex] * ComposedRefPoseMatrices[Parent];
		}

		FVector XAxis, YAxis, ZAxis;
		ComposedRefPoseMatrices[BoneIndex].GetScaledAxes(XAxis, YAxis, ZAxis);
		if (XAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
			YAxis.IsNearlyZero(UE_SMALL_NUMBER) &&
			ZAxis.IsNearlyZero(UE_SMALL_NUMBER))
		{
			// This is not allowed, warn them
			UE_LOG(
				LogChaosClothAsset,
				Warning,
				TEXT("Reference Pose for asset %s for joint (%s) includes NIL matrix. Zero scale isn't allowed on ref pose."),
				*GetPathName(),
				*GetRefSkeleton().GetBoneName(BoneIndex).ToString());
		}

		// Precompute inverse so we can use from-refpose-skin vertices
		RefBasesInvMatrix[BoneIndex] = FMatrix44f(ComposedRefPoseMatrices[BoneIndex].Inverse());
	}
}

void UChaosClothAssetBase::SetResourceForRendering(TUniquePtr<FSkeletalMeshRenderData>&& InSkeletalMeshRenderData)
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::RenderData);
	SkeletalMeshRenderData = MoveTemp(InSkeletalMeshRenderData);
}

#if WITH_EDITORONLY_DATA
void UChaosClothAssetBase::SetPreviewSceneSkeletalMesh(USkeletalMesh* Mesh)
{
	PreviewSceneSkeletalMesh = Mesh;
}

USkeletalMesh* UChaosClothAssetBase::GetPreviewSceneSkeletalMesh() const
{
	// Load the SkeletalMesh asset if it's not already loaded
	return PreviewSceneSkeletalMesh.LoadSynchronous();
}

void UChaosClothAssetBase::SetPreviewSceneAnimation(UAnimationAsset* Animation)
{
	PreviewSceneAnimation = Animation;
}

UAnimationAsset* UChaosClothAssetBase::GetPreviewSceneAnimation() const
{
	// Load the animation asset if it's not already loaded
	return PreviewSceneAnimation.LoadSynchronous();
}
#endif  // #if WITH_EDITORONLY_DATA

void UChaosClothAssetBase::SetHasVertexColors(bool InbHasVertexColors)
{
	WaitUntilAsyncPropertyReleased(EAsyncProperties::HasVertexColors);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bHasVertexColors = InbHasVertexColors;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR
bool UChaosClothAssetBase::ExportToSkeletalMesh(USkeletalMesh& SkeletalMesh) const
{
	using namespace UE::Chaos::ClothAsset;
	using namespace UE::Geometry;

	const TArray<IClothAssetSkeletalMeshConverterClassProvider*> ClassProviders =
		IModularFeatures::Get().GetModularFeatureImplementations<IClothAssetSkeletalMeshConverterClassProvider>(IClothAssetSkeletalMeshConverterClassProvider::FeatureName);
	if (const IClothAssetSkeletalMeshConverterClassProvider* const ClassProvider = ClassProviders.Num() ? ClassProviders[0] : nullptr)
	{
		if (const TSubclassOf<UClothAssetSkeletalMeshConverter> ClothAssetSkeletalMeshConverterClass = ClassProvider->GetClothAssetSkeletalMeshConverter())
		{
			if (UClothAssetSkeletalMeshConverter* const ClothAssetSkeletalMeshConverter = ClothAssetSkeletalMeshConverterClass->GetDefaultObject<UClothAssetSkeletalMeshConverter>())
			{
				return ClothAssetSkeletalMeshConverter->ExportToSkeletalMesh(*this, SkeletalMesh);
			}
		}
	}
	else
	{
		UE_LOG(LogChaosClothAsset, Error, TEXT("The export to SkeletalMesh has failed: Cannot find a SkeletalMesh converter. Make sure to enable the ChaosClothAssetEditor plugin."));
	}
	return false;
}
#endif  // #if WITH_EDITOR

void UChaosClothAssetBase::ReregisterComponents()
{
	// Recreate the simulation proxies with the updated physics asset
	for (TObjectIterator<UChaosClothComponent> ObjectIterator; ObjectIterator; ++ObjectIterator)
	{
		if (UChaosClothComponent* const Component = *ObjectIterator)
		{
			if (Component->GetAsset() == this)
			{
				const FComponentReregisterContext Context(Component);  // Context goes out of scope, causing the Component to be re-registered
			}
		}
	}
}
