// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigLogic.h"

#include "DNAReader.h"
#include "FMemoryResource.h"
#include "RigInstance.h"
#include "RigLogicModule.h"
#include "Stats/Stats.h"
#include "Stats/StatsHierarchical.h"

#include "riglogic/RigLogic.h"

#if STATS

DEFINE_STAT(STAT_RigLogic_CalculationType);
DEFINE_STAT(STAT_RigLogic_FloatingPointType);
DEFINE_STAT(STAT_RigLogic_LOD);
DEFINE_STAT(STAT_RigLogic_RBFSolverCount);
DEFINE_STAT(STAT_RigLogic_NeuralNetworkCount);
DEFINE_STAT(STAT_RigLogic_PSDCount);
DEFINE_STAT(STAT_RigLogic_JointCount);
DEFINE_STAT(STAT_RigLogic_JointDeltaValueCount);
DEFINE_STAT(STAT_RigLogic_BlendShapeChannelCount);
DEFINE_STAT(STAT_RigLogic_AnimatedMapCount);
DEFINE_STAT(STAT_RigLogic_MapGUIToRawControlsTime);
DEFINE_STAT(STAT_RigLogic_MapRawToGUIControlsTime);
DEFINE_STAT(STAT_RigLogic_CalculateRBFControlsTime);
DEFINE_STAT(STAT_RigLogic_CalculateMLControlsTime);
DEFINE_STAT(STAT_RigLogic_CalculateControlsTime);
DEFINE_STAT(STAT_RigLogic_CalculateJointsTime);
DEFINE_STAT(STAT_RigLogic_CalculateBlendShapesTime);
DEFINE_STAT(STAT_RigLogic_CalculateAnimatedMapsTime);

#endif  // STATS

void FRigLogic::FRigLogicDeleter::operator()(rl4::RigLogic* Pointer)
{
	rl4::RigLogic::destroy(Pointer);
}

static rl4::Configuration AdaptToRigLogicConfig(const FRigLogicConfiguration& Config)
{
	rl4::Configuration Copy = {};
	Copy.calculationType = static_cast<rl4::CalculationType>(Config.CalculationType);
	Copy.loadJoints = Config.LoadJoints;
	Copy.loadBlendShapes = Config.LoadBlendShapes;
	Copy.loadAnimatedMaps = Config.LoadAnimatedMaps;
	Copy.loadMachineLearnedBehavior = Config.LoadMachineLearnedBehavior;
	Copy.loadRBFBehavior = Config.LoadRBFBehavior;
	Copy.loadTwistSwingBehavior = Config.LoadTwistSwingBehavior;
	Copy.translationType = static_cast<rl4::TranslationType>(Config.TranslationType);
	Copy.rotationType = static_cast<rl4::RotationType>(Config.RotationType);
	Copy.rotationOrder = static_cast<rl4::RotationOrder>(Config.RotationOrder);
	Copy.scaleType = static_cast<rl4::ScaleType>(Config.ScaleType);
	Copy.translationPruningThreshold = Config.TranslationPruningThreshold;
	Copy.rotationPruningThreshold = Config.RotationPruningThreshold;
	Copy.scalePruningThreshold = Config.ScalePruningThreshold;
	return Copy;
}

static FRigLogicConfiguration AdaptFromRigLogicConfig(const rl4::Configuration& Config)
{
	FRigLogicConfiguration Copy = {};
	Copy.CalculationType = static_cast<ERigLogicCalculationType>(Config.calculationType);
	Copy.LoadJoints = Config.loadJoints;
	Copy.LoadBlendShapes = Config.loadBlendShapes;
	Copy.LoadAnimatedMaps = Config.loadAnimatedMaps;
	Copy.LoadMachineLearnedBehavior = Config.loadMachineLearnedBehavior;
	Copy.LoadRBFBehavior = Config.loadRBFBehavior;
	Copy.LoadTwistSwingBehavior = Config.loadTwistSwingBehavior;
	Copy.TranslationType = static_cast<ERigLogicTranslationType>(Config.translationType);
	Copy.RotationType = static_cast<ERigLogicRotationType>(Config.rotationType);
	Copy.RotationOrder = static_cast<ERigLogicRotationOrder>(Config.rotationOrder);
	Copy.ScaleType = static_cast<ERigLogicScaleType>(Config.scaleType);
	Copy.TranslationPruningThreshold = Config.translationPruningThreshold;
	Copy.RotationPruningThreshold = Config.rotationPruningThreshold;
	Copy.ScalePruningThreshold = Config.scalePruningThreshold;
	return Copy;
}

FRigLogic::FRigLogic(const IDNAReader* Reader, const FRigLogicConfiguration& Config) :
	MemoryResource{FMemoryResource::SharedInstance()},
	RigLogic{rl4::RigLogic::create(Reader->Unwrap(), AdaptToRigLogicConfig(Config), FMemoryResource::Instance())},
	Configuration{AdaptFromRigLogicConfig(RigLogic->getConfiguration())}
{
}

FRigLogic::~FRigLogic() = default;

const FRigLogicConfiguration& FRigLogic::GetConfiguration() const
{
	return Configuration;
}

uint16 FRigLogic::GetLODCount() const
{
	return RigLogic->getLODCount();
}

TArrayView<const uint16> FRigLogic::GetRBFSolverIndicesForLOD(uint16_t LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getRBFSolverIndicesForLOD(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

TArrayView<const uint32> FRigLogic::GetNeuralNetworkIndicesForLOD(uint16_t LOD) const
{
	rl4::ConstArrayView<uint32> Indices = RigLogic->getNeuralNetworkIndicesForLOD(LOD);
	return TArrayView<const uint32>{Indices.data(), static_cast<int32>(Indices.size())};
}

TArrayView<const uint16> FRigLogic::GetBlendShapeChannelIndicesForLOD(uint16_t LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getBlendShapeChannelIndicesForLOD(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

TArrayView<const uint16> FRigLogic::GetAnimatedMapIndicesForLOD(uint16_t LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getAnimatedMapIndicesForLOD(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

TArrayView<const uint16> FRigLogic::GetJointIndicesForLOD(uint16_t LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getJointIndicesForLOD(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

TArrayView<const float> FRigLogic::GetNeutralJointValues() const
{
	rl4::ConstArrayView<float> Values = RigLogic->getNeutralJointValues();
	return TArrayView<const float>{Values.data(), static_cast<int32>(Values.size())};
}

TArrayView<const uint16> FRigLogic::GetJointVariableAttributeIndices(uint16 LOD) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getJointVariableAttributeIndices(LOD);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

uint16 FRigLogic::GetJointGroupCount() const
{
	return RigLogic->getJointGroupCount();
}

uint16 FRigLogic::GetNeuralNetworkCount() const
{
	return RigLogic->getNeuralNetworkCount();
}

uint16 FRigLogic::GetRBFSolverCount() const
{
	return RigLogic->getRBFSolverCount();
}

uint16 FRigLogic::GetMeshCount() const
{
	return RigLogic->getMeshCount();
}

uint16 FRigLogic::GetMeshRegionCount(uint16 MeshIndex) const
{
	return RigLogic->getMeshRegionCount(MeshIndex);
}

TArrayView<const uint16> FRigLogic::GetNeuralNetworkIndices(uint16 MeshIndex, uint16 RegionIndex) const
{
	rl4::ConstArrayView<uint16> Indices = RigLogic->getNeuralNetworkIndices(MeshIndex, RegionIndex);
	return TArrayView<const uint16>{Indices.data(), static_cast<int32>(Indices.size())};
}

void FRigLogic::MapGUIToRawControls(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_MapGUIToRawControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::MapGUIToRawControls");
#endif  // CPUPROFILERTRACE_ENABLED
	RigLogic->mapGUIToRawControls(Instance->Unwrap());
}

void FRigLogic::MapRawToGUIControls(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_MapRawToGUIControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::MapRawToGUIControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->mapRawToGUIControls(Instance->Unwrap());
}

void FRigLogic::CalculateControls(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateControls(Instance->Unwrap());
}

void FRigLogic::CalculateMachineLearnedBehaviorControls(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateMLControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateMachineLearnedBehaviorControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateMachineLearnedBehaviorControls(Instance->Unwrap());
}

void FRigLogic::CalculateMachineLearnedBehaviorControls(FRigInstance* Instance, uint16 NeuralNetIndex) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateMLControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateMachineLearnedBehaviorControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateMachineLearnedBehaviorControls(Instance->Unwrap(), NeuralNetIndex);
}

void FRigLogic::CalculateRBFControls(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateRBFControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateRBFControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateRBFControls(Instance->Unwrap());
}

void FRigLogic::CalculateRBFControls(FRigInstance* Instance, uint16 SolverIndex) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateRBFControlsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateRBFControls");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateRBFControls(Instance->Unwrap(), SolverIndex);
}

void FRigLogic::CalculateJoints(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateJointsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateJoints");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateJoints(Instance->Unwrap());
}

void FRigLogic::CalculateJoints(FRigInstance* Instance, uint16 JointGroupIndex) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateJointsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateJoints");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateJoints(Instance->Unwrap(), JointGroupIndex);
}

void FRigLogic::CalculateBlendShapes(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateBlendShapesTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateBlendShapes");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateBlendShapes(Instance->Unwrap());
}

void FRigLogic::CalculateAnimatedMaps(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	SCOPE_CYCLE_COUNTER(STAT_RigLogic_CalculateAnimatedMapsTime);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::CalculateAnimatedMaps");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculateAnimatedMaps(Instance->Unwrap());
}

void FRigLogic::Calculate(FRigInstance* Instance) const
{
#if STATS
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigLogic_Calculate);
#endif  // STATS
#if CPUPROFILERTRACE_ENABLED
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FRigLogic::Calculate");
#endif  // CPUPROFILERTRACE_ENABLED

	RigLogic->calculate(Instance->Unwrap());
}

void FRigLogic::CollectCalculationStats(FRigInstance* Instance) const
{
#if STATS
	rl4::Stats Stats = {};
	RigLogic->collectCalculationStats(Instance->Unwrap(), &Stats);
	SET_DWORD_STAT(STAT_RigLogic_CalculationType, Stats.calculationType);
	SET_DWORD_STAT(STAT_RigLogic_FloatingPointType, Stats.floatingPointType);
	SET_DWORD_STAT(STAT_RigLogic_LOD, Instance->GetLOD());
	SET_DWORD_STAT(STAT_RigLogic_RBFSolverCount, Stats.rbfSolverCount);
	SET_DWORD_STAT(STAT_RigLogic_NeuralNetworkCount, Stats.neuralNetworkCount);
	SET_DWORD_STAT(STAT_RigLogic_PSDCount, Stats.psdCount);
	SET_DWORD_STAT(STAT_RigLogic_JointCount, Stats.jointCount);
	SET_DWORD_STAT(STAT_RigLogic_JointDeltaValueCount, Stats.jointDeltaValueCount);
	SET_DWORD_STAT(STAT_RigLogic_BlendShapeChannelCount, Stats.blendShapeChannelCount);
	SET_DWORD_STAT(STAT_RigLogic_AnimatedMapCount, Stats.animatedMapCount);
#endif  // STATS
}

rl4::RigLogic* FRigLogic::Unwrap() const
{
	return RigLogic.Get();
}
