// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "FMemoryResource.h"

#include "RigLogic.generated.h"

#define UE_API RIGLOGICMODULE_API

class FRigInstance;
class IDNAReader;

namespace rl4
{

class RigLogic;

}  // namespace rl4

UENUM(BlueprintType)
enum class ERigLogicCalculationType: uint8
{
	Scalar,
	SSE,
	AVX,
	NEON,
	AnyVector
};

UENUM(BlueprintType)
enum class ERigLogicTranslationType : uint8 {
	None,
	Vector = 3
};

UENUM(BlueprintType)
enum class ERigLogicRotationType : uint8 {
	None,
	EulerAngles = 3,
	Quaternions = 4
};

UENUM(BlueprintType)
enum class ERigLogicRotationOrder : uint8 {
	XYZ,
	XZY,
	YXZ,
	YZX,
	ZXY,
	ZYX
};

UENUM(BlueprintType)
enum class ERigLogicScaleType : uint8 {
	None,
	Vector = 3
};

USTRUCT(BlueprintType)
struct FRigLogicConfiguration
{
	GENERATED_BODY()

	FRigLogicConfiguration() :
		CalculationType(ERigLogicCalculationType::AnyVector),
		LoadJoints(true),
		LoadBlendShapes(true),
		LoadAnimatedMaps(true),
		LoadMachineLearnedBehavior(true),
		LoadRBFBehavior(true),
		LoadTwistSwingBehavior(true),
		TranslationType(ERigLogicTranslationType::Vector),
		RotationType(ERigLogicRotationType::Quaternions),
		RotationOrder(ERigLogicRotationOrder::ZYX),
		ScaleType(ERigLogicScaleType::Vector),
		TranslationPruningThreshold(0.0f),
		RotationPruningThreshold(0.0f),
		ScalePruningThreshold(0.0f)
	{
	}

	FRigLogicConfiguration(ERigLogicCalculationType CalculationType,
							bool LoadJoints,
							bool LoadBlendShapes,
							bool LoadAnimatedMaps,
							bool LoadMachineLearnedBehavior,
							bool LoadRBFBehavior,
							bool LoadTwistSwingBehavior,
							ERigLogicTranslationType TranslationType,
							ERigLogicRotationType RotationType,
							ERigLogicRotationOrder RotationOrder,
							ERigLogicScaleType ScaleType,
							float TranslationPruningThreshold,
							float RotationPruningThreshold,
							float ScalePruningThreshold) :
		CalculationType(CalculationType),
		LoadJoints(LoadJoints),
		LoadBlendShapes(LoadBlendShapes),
		LoadAnimatedMaps(LoadAnimatedMaps),
		LoadMachineLearnedBehavior(LoadMachineLearnedBehavior),
		LoadRBFBehavior(LoadRBFBehavior),
		LoadTwistSwingBehavior(LoadTwistSwingBehavior),
		TranslationType(TranslationType),
		RotationType(RotationType),
		RotationOrder(RotationOrder),
		ScaleType(ScaleType),
		TranslationPruningThreshold(TranslationPruningThreshold),
		RotationPruningThreshold(RotationPruningThreshold),
		ScalePruningThreshold(ScalePruningThreshold)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	ERigLogicCalculationType CalculationType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadJoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadBlendShapes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadAnimatedMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadMachineLearnedBehavior;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadRBFBehavior;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	bool LoadTwistSwingBehavior;
	
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	ERigLogicTranslationType TranslationType;
	
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	ERigLogicRotationType RotationType;
	
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	ERigLogicRotationOrder RotationOrder;

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	ERigLogicScaleType ScaleType;

	/** The joint translation pruning threshold is used to eliminate joint translation deltas below
	  * the specified threshold from the joint matrix when the RigLogic instance is initialized.
	  * Use it with caution, as while it may reduce the amount of compute to be done, it may also erase
	  * important deltas that could introduce artifacts into the rig.
	  * A reasonably safe starting value to try translation pruning would be 0.0001f
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	float TranslationPruningThreshold;

	/** The joint rotation pruning threshold is used to eliminate joint rotation deltas below
	  * the specified threshold from the joint matrix when the RigLogic instance is initialized.
	  * Use it with caution, as while it may reduce the amount of compute to be done, it may also erase
	  * important deltas that could introduce artifacts into the rig.
	  * A reasonably safe starting value to try rotation pruning would be 0.1f
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	float RotationPruningThreshold;

	/** The joint scale pruning threshold is used to eliminate joint scale deltas below
	  * the specified threshold from the joint matrix when the RigLogic instance is initialized.
	  * Use it with caution, as while it may reduce the amount of compute to be done, it may also erase
	  * important deltas that could introduce artifacts into the rig.
	  * A reasonably safe starting value to try scale pruning would be 0.001f
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigLogic")
	float ScalePruningThreshold;
};

class FRigLogic
{
public:
	UE_API explicit FRigLogic(const IDNAReader* Reader, const FRigLogicConfiguration& Config = FRigLogicConfiguration());
	UE_API ~FRigLogic();

	FRigLogic(const FRigLogic&) = delete;
	FRigLogic& operator=(const FRigLogic&) = delete;

	FRigLogic(FRigLogic&&) = default;
	FRigLogic& operator=(FRigLogic&&) = default;

	UE_API const FRigLogicConfiguration& GetConfiguration() const;
	UE_API uint16 GetLODCount() const;
	UE_API TArrayView<const uint16> GetRBFSolverIndicesForLOD(uint16_t LOD) const;
	UE_API TArrayView<const uint32> GetNeuralNetworkIndicesForLOD(uint16_t LOD) const;
	UE_API TArrayView<const uint16> GetBlendShapeChannelIndicesForLOD(uint16_t LOD) const;
	UE_API TArrayView<const uint16> GetAnimatedMapIndicesForLOD(uint16_t LOD) const;
	UE_API TArrayView<const uint16> GetJointIndicesForLOD(uint16_t LOD) const;
	UE_API TArrayView<const float> GetNeutralJointValues() const;
	UE_API TArrayView<const uint16> GetJointVariableAttributeIndices(uint16 LOD) const;
	UE_API uint16 GetJointGroupCount() const;
	UE_API uint16 GetNeuralNetworkCount() const;
	UE_API uint16 GetRBFSolverCount() const;
	UE_API uint16 GetMeshCount() const;
	UE_API uint16 GetMeshRegionCount(uint16 MeshIndex) const;
	UE_API TArrayView<const uint16> GetNeuralNetworkIndices(uint16 MeshIndex, uint16 RegionIndex) const;

	UE_API void MapGUIToRawControls(FRigInstance* Instance) const;
	UE_API void MapRawToGUIControls(FRigInstance* Instance) const;
	UE_API void CalculateControls(FRigInstance* Instance) const;
	UE_API void CalculateMachineLearnedBehaviorControls(FRigInstance* Instance) const;
	UE_API void CalculateMachineLearnedBehaviorControls(FRigInstance* Instance, uint16 NeuralNetIndex) const;
	UE_API void CalculateRBFControls(FRigInstance* Instance) const;
	UE_API void CalculateRBFControls(FRigInstance* Instance, uint16 SolverIndex) const;
	UE_API void CalculateJoints(FRigInstance* Instance) const;
	UE_API void CalculateJoints(FRigInstance* Instance, uint16 JointGroupIndex) const;
	UE_API void CalculateBlendShapes(FRigInstance* Instance) const;
	UE_API void CalculateAnimatedMaps(FRigInstance* Instance) const;
	UE_API void Calculate(FRigInstance* Instance) const;
	UE_API void CollectCalculationStats(FRigInstance* Instance) const;

private:
	friend FRigInstance;
	UE_API rl4::RigLogic* Unwrap() const;

private:
	TSharedPtr<FMemoryResource> MemoryResource;

	struct FRigLogicDeleter
	{
		void operator()(rl4::RigLogic* Pointer);
	};
	TUniquePtr<rl4::RigLogic, FRigLogicDeleter> RigLogic;
	FRigLogicConfiguration Configuration;
};

#undef UE_API
