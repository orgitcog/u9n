// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <dna/Reader.h>

#define UE_API RIGLOGICMODULE_API

class RigLogicDNAReader : public dna::Reader {
public:
	UE_API explicit RigLogicDNAReader(const dna::Reader* DNAReader);

	// Header
	UE_API std::uint16_t getFileFormatGeneration() const override;
	UE_API std::uint16_t getFileFormatVersion() const override;

	// Descriptor
	UE_API dna::StringView getName() const override;
	UE_API dna::Archetype getArchetype() const override;
	UE_API dna::Gender getGender() const override;
	UE_API std::uint16_t getAge() const override;
	UE_API std::uint32_t getMetaDataCount() const override;
	UE_API dna::StringView getMetaDataKey(std::uint32_t index) const override;
	UE_API dna::StringView getMetaDataValue(const char* key) const override;
	UE_API dna::TranslationUnit getTranslationUnit() const override;
	UE_API dna::RotationUnit getRotationUnit() const override;
	UE_API dna::CoordinateSystem getCoordinateSystem() const override;
	UE_API std::uint16_t getLODCount() const override;
	UE_API std::uint16_t getDBMaxLOD() const override;
	UE_API dna::StringView getDBComplexity() const override;
	UE_API dna::StringView getDBName() const override;

	// Definition
	UE_API std::uint16_t getGUIControlCount() const override;
	UE_API dna::StringView getGUIControlName(std::uint16_t index) const override;
	UE_API std::uint16_t getRawControlCount() const override;
	UE_API dna::StringView getRawControlName(std::uint16_t index) const override;
	UE_API std::uint16_t getJointCount() const override;
	UE_API dna::StringView getJointName(std::uint16_t index) const override;
	UE_API std::uint16_t getJointIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointIndicesForLOD(std::uint16_t lod) const override;
	UE_API std::uint16_t getJointParentIndex(std::uint16_t index) const override;
	UE_API std::uint16_t getBlendShapeChannelCount() const override;
	UE_API dna::StringView getBlendShapeChannelName(std::uint16_t index) const override;
	UE_API std::uint16_t getBlendShapeChannelIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getBlendShapeChannelIndicesForLOD(std::uint16_t lod) const override;
	UE_API std::uint16_t getAnimatedMapCount() const override;
	UE_API dna::StringView getAnimatedMapName(std::uint16_t index) const override;
	UE_API std::uint16_t getAnimatedMapIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getAnimatedMapIndicesForLOD(std::uint16_t lod) const override;
	UE_API std::uint16_t getMeshCount() const override;
	UE_API dna::StringView getMeshName(std::uint16_t index) const override;
	UE_API std::uint16_t getMeshIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getMeshIndicesForLOD(std::uint16_t lod) const override;
	UE_API std::uint16_t getMeshBlendShapeChannelMappingCount() const override;
	UE_API dna::MeshBlendShapeChannelMapping getMeshBlendShapeChannelMapping(std::uint16_t index) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getMeshBlendShapeChannelMappingIndicesForLOD(std::uint16_t lod) const override;
	UE_API dna::Vector3 getNeutralJointTranslation(std::uint16_t index) const override;
	UE_API dna::ConstArrayView<float> getNeutralJointTranslationXs() const override;
	UE_API dna::ConstArrayView<float> getNeutralJointTranslationYs() const override;
	UE_API dna::ConstArrayView<float> getNeutralJointTranslationZs() const override;
	UE_API dna::Vector3 getNeutralJointRotation(std::uint16_t index) const override;
	UE_API dna::ConstArrayView<float> getNeutralJointRotationXs() const override;
	UE_API dna::ConstArrayView<float> getNeutralJointRotationYs() const override;
	UE_API dna::ConstArrayView<float> getNeutralJointRotationZs() const override;

	// Behavior
	UE_API dna::ConstArrayView<std::uint16_t> getGUIToRawInputIndices() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getGUIToRawOutputIndices() const override;
	UE_API dna::ConstArrayView<float> getGUIToRawFromValues() const override;
	UE_API dna::ConstArrayView<float> getGUIToRawToValues() const override;
	UE_API dna::ConstArrayView<float> getGUIToRawSlopeValues() const override;
	UE_API dna::ConstArrayView<float> getGUIToRawCutValues() const override;
	UE_API std::uint16_t getPSDCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getPSDRowIndices() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getPSDColumnIndices() const override;
	UE_API dna::ConstArrayView<float> getPSDValues() const override;
	UE_API std::uint16_t getJointRowCount() const override;
	UE_API std::uint16_t getJointColumnCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointVariableAttributeIndices(std::uint16_t lod) const override;
	UE_API std::uint16_t getJointGroupCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointGroupLODs(std::uint16_t jointGroupIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointGroupInputIndices(std::uint16_t jointGroupIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointGroupOutputIndices(std::uint16_t jointGroupIndex) const override;
	UE_API dna::ConstArrayView<float> getJointGroupValues(std::uint16_t jointGroupIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getJointGroupJointIndices(std::uint16_t jointGroupIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getBlendShapeChannelLODs() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getBlendShapeChannelInputIndices() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getBlendShapeChannelOutputIndices() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getAnimatedMapLODs() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getAnimatedMapInputIndices() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getAnimatedMapOutputIndices() const override;
	UE_API dna::ConstArrayView<float> getAnimatedMapFromValues() const override;
	UE_API dna::ConstArrayView<float> getAnimatedMapToValues() const override;
	UE_API dna::ConstArrayView<float> getAnimatedMapSlopeValues() const override;
	UE_API dna::ConstArrayView<float> getAnimatedMapCutValues() const override;

	// Geometry
	UE_API std::uint32_t getVertexPositionCount(std::uint16_t meshIndex) const override;
	UE_API dna::Position getVertexPosition(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexPositionXs(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexPositionYs(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexPositionZs(std::uint16_t meshIndex) const override;
	UE_API std::uint32_t getVertexTextureCoordinateCount(std::uint16_t meshIndex) const override;
	UE_API dna::TextureCoordinate getVertexTextureCoordinate(std::uint16_t meshIndex, std::uint32_t textureCoordinateIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexTextureCoordinateUs(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexTextureCoordinateVs(std::uint16_t meshIndex) const override;
	UE_API std::uint32_t getVertexNormalCount(std::uint16_t meshIndex) const override;
	UE_API dna::Normal getVertexNormal(std::uint16_t meshIndex, std::uint32_t normalIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexNormalXs(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexNormalYs(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getVertexNormalZs(std::uint16_t meshIndex) const override;
	UE_API std::uint32_t getVertexLayoutCount(std::uint16_t meshIndex) const override;
	UE_API dna::VertexLayout getVertexLayout(std::uint16_t meshIndex, std::uint32_t layoutIndex) const override;
	UE_API dna::ConstArrayView<std::uint32_t> getVertexLayoutPositionIndices(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<std::uint32_t> getVertexLayoutTextureCoordinateIndices(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<std::uint32_t> getVertexLayoutNormalIndices(std::uint16_t meshIndex) const override;
	UE_API std::uint32_t getFaceCount(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<std::uint32_t> getFaceVertexLayoutIndices(std::uint16_t meshIndex, std::uint32_t faceIndex) const override;
	UE_API std::uint16_t getMaximumInfluencePerVertex(std::uint16_t meshIndex) const override;
	UE_API std::uint32_t getSkinWeightsCount(std::uint16_t meshIndex) const override;
	UE_API dna::ConstArrayView<float> getSkinWeightsValues(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getSkinWeightsJointIndices(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override;
	UE_API std::uint16_t getBlendShapeTargetCount(std::uint16_t meshIndex) const override;
	UE_API std::uint16_t getBlendShapeChannelIndex(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;
	UE_API std::uint32_t getBlendShapeTargetDeltaCount(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;
	UE_API dna::Delta getBlendShapeTargetDelta(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex, std::uint32_t deltaIndex) const override;
	UE_API dna::ConstArrayView<float> getBlendShapeTargetDeltaXs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;
	UE_API dna::ConstArrayView<float> getBlendShapeTargetDeltaYs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;
	UE_API dna::ConstArrayView<float> getBlendShapeTargetDeltaZs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;
	UE_API dna::ConstArrayView<std::uint32_t> getBlendShapeTargetVertexIndices(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override;

	// Machine Learned Behavior
	UE_API std::uint16_t getMLControlCount() const override;
	UE_API dna::StringView getMLControlName(std::uint16_t index) const override;
	UE_API std::uint16_t getNeuralNetworkCount() const override;
	UE_API std::uint16_t getNeuralNetworkIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getNeuralNetworkIndicesForLOD(std::uint16_t lod) const override;
	UE_API std::uint16_t getMeshRegionCount(std::uint16_t meshIndex) const override;
	UE_API dna::StringView getMeshRegionName(std::uint16_t meshIndex, std::uint16_t regionIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getNeuralNetworkIndicesForMeshRegion(std::uint16_t meshIndex, std::uint16_t regionIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getNeuralNetworkInputIndices(std::uint16_t netIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getNeuralNetworkOutputIndices(std::uint16_t netIndex) const override;
	UE_API std::uint16_t getNeuralNetworkLayerCount(std::uint16_t netIndex) const override;
	UE_API dna::ActivationFunction getNeuralNetworkLayerActivationFunction(std::uint16_t netIndex, std::uint16_t layerIndex) const override;
	UE_API dna::ConstArrayView<float> getNeuralNetworkLayerActivationFunctionParameters(std::uint16_t netIndex, std::uint16_t layerIndex) const override;
	UE_API dna::ConstArrayView<float> getNeuralNetworkLayerBiases(std::uint16_t netIndex, std::uint16_t layerIndex) const override;
	UE_API dna::ConstArrayView<float> getNeuralNetworkLayerWeights(std::uint16_t netIndex, std::uint16_t layerIndex) const override;

	// RBFBehaviorReader methods
	UE_API std::uint16_t getRBFPoseCount() const override;
	UE_API dna::StringView getRBFPoseName(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFPoseJointOutputIndices(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFPoseBlendShapeChannelOutputIndices(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFPoseAnimatedMapOutputIndices(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<float> getRBFPoseJointOutputValues(std::uint16_t poseIndex) const override;
	UE_API float getRBFPoseScale(std::uint16_t poseIndex) const override;
	UE_API std::uint16_t getRBFPoseControlCount() const override;
	UE_API dna::StringView getRBFPoseControlName(std::uint16_t poseControlIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFPoseInputControlIndices(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFPoseOutputControlIndices(std::uint16_t poseIndex) const override;
	UE_API dna::ConstArrayView<float> getRBFPoseOutputControlWeights(std::uint16_t poseIndex) const override;
	UE_API std::uint16_t getRBFSolverCount() const override;
	UE_API std::uint16_t getRBFSolverIndexListCount() const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFSolverIndicesForLOD(std::uint16_t lod) const override;
	UE_API dna::StringView getRBFSolverName(std::uint16_t solverIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFSolverRawControlIndices(std::uint16_t solverIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getRBFSolverPoseIndices(std::uint16_t solverIndex) const override;
	UE_API dna::ConstArrayView<float> getRBFSolverRawControlValues(std::uint16_t solverIndex) const override;
	UE_API dna::RBFSolverType getRBFSolverType(std::uint16_t solverIndex) const override;
	UE_API float getRBFSolverRadius(std::uint16_t solverIndex) const override;
	UE_API dna::AutomaticRadius getRBFSolverAutomaticRadius(std::uint16_t solverIndex) const override;
	UE_API float getRBFSolverWeightThreshold(std::uint16_t solverIndex) const override;
	UE_API dna::RBFDistanceMethod getRBFSolverDistanceMethod(std::uint16_t solverIndex) const override;
	UE_API dna::RBFNormalizeMethod getRBFSolverNormalizeMethod(std::uint16_t solverIndex) const override;
	UE_API dna::RBFFunctionType getRBFSolverFunctionType(std::uint16_t solverIndex) const override;
	UE_API dna::TwistAxis getRBFSolverTwistAxis(std::uint16_t solverIndex) const override;

	// JointBehaviorMetadataReader methods
	UE_API dna::TranslationRepresentation getJointTranslationRepresentation(std::uint16_t jointIndex) const override;
	UE_API dna::RotationRepresentation getJointRotationRepresentation(std::uint16_t jointIndex) const override;
	UE_API dna::ScaleRepresentation getJointScaleRepresentation(std::uint16_t jointIndex) const override;

	// TwistSwingBehaviorReader methods
	UE_API std::uint16_t getTwistCount() const override;
	UE_API dna::TwistAxis getTwistSetupTwistAxis(std::uint16_t twistIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getTwistInputControlIndices(std::uint16_t twistIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getTwistOutputJointIndices(std::uint16_t twistIndex) const override;
	UE_API dna::ConstArrayView<float> getTwistBlendWeights(std::uint16_t twistIndex) const override;
	UE_API std::uint16_t getSwingCount() const override;
	UE_API dna::TwistAxis getSwingSetupTwistAxis(std::uint16_t swingIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getSwingInputControlIndices(std::uint16_t swingIndex) const override;
	UE_API dna::ConstArrayView<std::uint16_t> getSwingOutputJointIndices(std::uint16_t swingIndex) const override;
	UE_API dna::ConstArrayView<float> getSwingBlendWeights(std::uint16_t swingIndex) const override;

	// Reader
	UE_API void unload(dna::DataLayer Layer) override;
	static UE_API void destroy(dna::Reader* Pointer);

private:
	void CacheNeutralJointTranslations() const;
	void CacheNeutralJointRotations() const;
	void CacheJointGroup(std::uint16_t JointGroupIndex) const;
	void CacheRBFSolverRawControlValues(std::uint16_t SolverIndex) const;
	void CacheRBFPoseJointOutputValues(std::uint16_t PoseIndex) const;

private:
	enum class CachedDataKey
	{
		None,
		NeutralJointTranslations,
		NeutralJointRotations,
		JointGroup,
		RBFSolverRawControlValues,
		RBFPoseJointOutputValues
	};

private:
	const dna::Reader* Reader;
	mutable TArray<float> Values;
	mutable TArray<float> RBFValues;
	mutable CachedDataKey Key;
	mutable std::uint16_t Id;
};

#undef UE_API
