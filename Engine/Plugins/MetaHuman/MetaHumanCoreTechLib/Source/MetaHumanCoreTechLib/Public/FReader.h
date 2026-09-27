// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "dna/BinaryStreamReader.h"
#include "DNAAsset.h"
#include "DNAReader.h"

namespace dna
{
	// Wrapper stream reader for accessing asset DNA data via the BinaryStreamReader interface
	// The code is similar to the FSkelMeshDNAReader but implements the dna::BinaryStreamReader interface instead of the IDNAReader one
	class METAHUMANCORETECHLIB_API FReader : public BinaryStreamReader
	{
	public:

		explicit FReader(UDNAAsset* DNAAsset) : GeometryReader{ nullptr }
		{
			BehaviorReader = DNAAsset->GetBehaviorReader()->Unwrap();
#if WITH_EDITORONLY_DATA
			GeometryReader = DNAAsset->GetGeometryReader()->Unwrap();
#endif
		}

		const Reader* GeometryReader;
		const Reader* BehaviorReader;

		// geometry
		virtual std::uint32_t getVertexPositionCount(std::uint16_t meshIndex) const override { return GeometryReader->getVertexPositionCount(meshIndex); }
		virtual Position getVertexPosition(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override { return GeometryReader->getVertexPosition(meshIndex, vertexIndex); }
		virtual ConstArrayView<float> getVertexPositionXs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexPositionXs(meshIndex); }
		virtual ConstArrayView<float> getVertexPositionYs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexPositionYs(meshIndex); }
		virtual ConstArrayView<float> getVertexPositionZs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexPositionZs(meshIndex); }
		virtual std::uint32_t getVertexTextureCoordinateCount(std::uint16_t meshIndex) const override { return GeometryReader->getVertexTextureCoordinateCount(meshIndex); }
		virtual TextureCoordinate getVertexTextureCoordinate(std::uint16_t meshIndex, std::uint32_t textureCoordinateIndex) const override { return GeometryReader->getVertexTextureCoordinate(meshIndex, textureCoordinateIndex); }
		virtual ConstArrayView<float> getVertexTextureCoordinateUs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexTextureCoordinateUs(meshIndex); }
		virtual ConstArrayView<float> getVertexTextureCoordinateVs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexTextureCoordinateVs(meshIndex); }
		virtual std::uint32_t getVertexNormalCount(std::uint16_t meshIndex) const override { return GeometryReader->getVertexNormalCount(meshIndex); }
		virtual Normal getVertexNormal(std::uint16_t meshIndex, std::uint32_t normalIndex) const override { return GeometryReader->getVertexNormal(meshIndex, normalIndex); }
		virtual ConstArrayView<float> getVertexNormalXs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexNormalXs(meshIndex); }
		virtual ConstArrayView<float> getVertexNormalYs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexNormalYs(meshIndex); }
		virtual ConstArrayView<float> getVertexNormalZs(std::uint16_t meshIndex) const override { return GeometryReader->getVertexNormalZs(meshIndex); }
		virtual std::uint32_t getVertexLayoutCount(std::uint16_t meshIndex) const override { return GeometryReader->getVertexLayoutCount(meshIndex); }
		virtual VertexLayout getVertexLayout(std::uint16_t meshIndex, std::uint32_t layoutIndex) const override { return GeometryReader->getVertexLayout(meshIndex, layoutIndex); }
		virtual ConstArrayView<std::uint32_t> getVertexLayoutPositionIndices(std::uint16_t meshIndex) const override { return GeometryReader->getVertexLayoutPositionIndices(meshIndex); }
		virtual ConstArrayView<std::uint32_t> getVertexLayoutTextureCoordinateIndices(std::uint16_t meshIndex) const override { return GeometryReader->getVertexLayoutTextureCoordinateIndices(meshIndex); }
		virtual ConstArrayView<std::uint32_t> getVertexLayoutNormalIndices(std::uint16_t meshIndex) const override { return GeometryReader->getVertexLayoutNormalIndices(meshIndex); }
		virtual std::uint32_t getFaceCount(std::uint16_t meshIndex) const override { return GeometryReader->getFaceCount(meshIndex); }
		virtual ConstArrayView<std::uint32_t> getFaceVertexLayoutIndices(std::uint16_t meshIndex, std::uint32_t faceIndex) const override { return GeometryReader->getFaceVertexLayoutIndices(meshIndex, faceIndex); }
		virtual std::uint16_t getMaximumInfluencePerVertex(std::uint16_t meshIndex) const override { return GeometryReader->getMaximumInfluencePerVertex(meshIndex); }
		virtual std::uint32_t getSkinWeightsCount(std::uint16_t meshIndex) const override { return GeometryReader->getSkinWeightsCount(meshIndex); }
		virtual ConstArrayView<float> getSkinWeightsValues(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override { return GeometryReader->getSkinWeightsValues(meshIndex, vertexIndex); }
		virtual ConstArrayView<std::uint16_t> getSkinWeightsJointIndices(std::uint16_t meshIndex, std::uint32_t vertexIndex) const override { return GeometryReader->getSkinWeightsJointIndices(meshIndex, vertexIndex); }
		virtual std::uint16_t getBlendShapeTargetCount(std::uint16_t meshIndex) const override { return GeometryReader->getBlendShapeTargetCount(meshIndex); }
		virtual std::uint16_t getBlendShapeChannelIndex(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeChannelIndex(meshIndex, blendShapeTargetIndex); }
		virtual std::uint32_t getBlendShapeTargetDeltaCount(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeTargetDeltaCount(meshIndex, blendShapeTargetIndex); }
		virtual Delta getBlendShapeTargetDelta(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex, std::uint32_t deltaIndex) const override { return GeometryReader->getBlendShapeTargetDelta(meshIndex, blendShapeTargetIndex, deltaIndex); }
		virtual ConstArrayView<float> getBlendShapeTargetDeltaXs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeTargetDeltaXs(meshIndex, blendShapeTargetIndex); }
		virtual ConstArrayView<float> getBlendShapeTargetDeltaYs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeTargetDeltaYs(meshIndex, blendShapeTargetIndex); }
		virtual ConstArrayView<float> getBlendShapeTargetDeltaZs(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeTargetDeltaZs(meshIndex, blendShapeTargetIndex); }
		virtual ConstArrayView<std::uint32_t> getBlendShapeTargetVertexIndices(std::uint16_t meshIndex, std::uint16_t blendShapeTargetIndex) const override { return GeometryReader->getBlendShapeTargetVertexIndices(meshIndex, blendShapeTargetIndex); }

		// behaviour
		virtual ConstArrayView<std::uint16_t> getGUIToRawInputIndices() const override { return BehaviorReader->getGUIToRawInputIndices(); }
		virtual ConstArrayView<std::uint16_t> getGUIToRawOutputIndices() const override { return BehaviorReader->getGUIToRawOutputIndices(); }
		virtual ConstArrayView<float> getGUIToRawFromValues() const override { return BehaviorReader->getGUIToRawFromValues(); }
		virtual ConstArrayView<float> getGUIToRawToValues() const override { return BehaviorReader->getGUIToRawToValues(); }
		virtual ConstArrayView<float> getGUIToRawSlopeValues() const override { return BehaviorReader->getGUIToRawSlopeValues(); }
		virtual ConstArrayView<float> getGUIToRawCutValues() const override { return BehaviorReader->getGUIToRawCutValues(); }
		virtual std::uint16_t getPSDCount() const override { return BehaviorReader->getPSDCount(); }
		virtual ConstArrayView<std::uint16_t> getPSDRowIndices() const override { return BehaviorReader->getPSDRowIndices(); }
		virtual ConstArrayView<std::uint16_t> getPSDColumnIndices() const override { return BehaviorReader->getPSDColumnIndices(); }
		virtual ConstArrayView<float> getPSDValues() const override { return BehaviorReader->getPSDValues(); }
		virtual std::uint16_t getJointRowCount() const override { return BehaviorReader->getJointRowCount(); }
		virtual std::uint16_t getJointColumnCount() const override { return BehaviorReader->getJointColumnCount(); }
		virtual ConstArrayView<std::uint16_t> getJointVariableAttributeIndices(std::uint16_t lod) const override { return BehaviorReader->getJointVariableAttributeIndices(lod); }
		virtual std::uint16_t getJointGroupCount() const override { return BehaviorReader->getJointGroupCount(); }
		virtual ConstArrayView<std::uint16_t> getJointGroupLODs(std::uint16_t jointGroupIndex) const override { return BehaviorReader->getJointGroupLODs(jointGroupIndex); }
		virtual ConstArrayView<std::uint16_t> getJointGroupInputIndices(std::uint16_t jointGroupIndex) const override { return BehaviorReader->getJointGroupInputIndices(jointGroupIndex); }
		virtual ConstArrayView<std::uint16_t> getJointGroupOutputIndices(std::uint16_t jointGroupIndex) const override { return BehaviorReader->getJointGroupOutputIndices(jointGroupIndex); }
		virtual ConstArrayView<float> getJointGroupValues(std::uint16_t jointGroupIndex) const override { return BehaviorReader->getJointGroupValues(jointGroupIndex); }
		virtual ConstArrayView<std::uint16_t> getJointGroupJointIndices(std::uint16_t jointGroupIndex) const override { return BehaviorReader->getJointGroupJointIndices(jointGroupIndex); }
		virtual ConstArrayView<std::uint16_t> getBlendShapeChannelLODs() const override { return BehaviorReader->getBlendShapeChannelLODs(); }
		virtual ConstArrayView<std::uint16_t> getBlendShapeChannelInputIndices() const override { return BehaviorReader->getBlendShapeChannelInputIndices(); }
		virtual ConstArrayView<std::uint16_t> getBlendShapeChannelOutputIndices() const override { return BehaviorReader->getBlendShapeChannelOutputIndices(); }
		virtual ConstArrayView<std::uint16_t> getAnimatedMapLODs() const override { return BehaviorReader->getAnimatedMapLODs(); }
		virtual ConstArrayView<std::uint16_t> getAnimatedMapInputIndices() const override { return BehaviorReader->getAnimatedMapInputIndices(); }
		virtual ConstArrayView<std::uint16_t> getAnimatedMapOutputIndices() const override { return BehaviorReader->getAnimatedMapOutputIndices(); }
		virtual ConstArrayView<float> getAnimatedMapFromValues() const override { return BehaviorReader->getAnimatedMapFromValues(); }
		virtual ConstArrayView<float> getAnimatedMapToValues() const override { return BehaviorReader->getAnimatedMapToValues(); }
		virtual ConstArrayView<float> getAnimatedMapSlopeValues() const override { return BehaviorReader->getAnimatedMapSlopeValues(); }
		virtual ConstArrayView<float> getAnimatedMapCutValues() const override { return BehaviorReader->getAnimatedMapCutValues(); }

		// definition
		virtual std::uint16_t getGUIControlCount() const override { return BehaviorReader->getGUIControlCount(); }
		virtual StringView getGUIControlName(std::uint16_t index) const override { return BehaviorReader->getGUIControlName(index); }
		virtual std::uint16_t getRawControlCount() const override { return BehaviorReader->getRawControlCount(); }
		virtual StringView getRawControlName(std::uint16_t index) const override { return BehaviorReader->getRawControlName(index); }
		virtual std::uint16_t getJointCount() const override { return BehaviorReader->getJointCount(); }
		virtual StringView getJointName(std::uint16_t index) const override { return BehaviorReader->getJointName(index); }
		virtual std::uint16_t getJointIndexListCount() const override { return BehaviorReader->getJointIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getJointIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getJointIndicesForLOD(lod); }
		virtual std::uint16_t getJointParentIndex(std::uint16_t index) const override { return BehaviorReader->getJointParentIndex(index); }
		virtual std::uint16_t getBlendShapeChannelCount() const override { return BehaviorReader->getBlendShapeChannelCount(); }
		virtual StringView getBlendShapeChannelName(std::uint16_t index) const override { return BehaviorReader->getBlendShapeChannelName(index); }
		virtual std::uint16_t getBlendShapeChannelIndexListCount() const override { return BehaviorReader->getBlendShapeChannelIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getBlendShapeChannelIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getBlendShapeChannelIndicesForLOD(lod); }
		virtual std::uint16_t getAnimatedMapCount() const override { return BehaviorReader->getAnimatedMapCount(); }
		virtual StringView getAnimatedMapName(std::uint16_t index) const override { return BehaviorReader->getAnimatedMapName(index); }
		virtual std::uint16_t getAnimatedMapIndexListCount() const override { return BehaviorReader->getAnimatedMapIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getAnimatedMapIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getAnimatedMapIndicesForLOD(lod); }
		virtual std::uint16_t getMeshCount() const override { return BehaviorReader->getMeshCount(); }
		virtual StringView getMeshName(std::uint16_t index) const override { return BehaviorReader->getMeshName(index); }
		virtual std::uint16_t getMeshIndexListCount() const override { return BehaviorReader->getMeshIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getMeshIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getMeshIndicesForLOD(lod); }
		virtual std::uint16_t getMeshBlendShapeChannelMappingCount() const override { return BehaviorReader->getMeshBlendShapeChannelMappingCount(); }
		virtual MeshBlendShapeChannelMapping getMeshBlendShapeChannelMapping(std::uint16_t index) const override { return BehaviorReader->getMeshBlendShapeChannelMapping(index); }
		virtual ConstArrayView<std::uint16_t> getMeshBlendShapeChannelMappingIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getMeshBlendShapeChannelMappingIndicesForLOD(lod); }
		virtual Vector3 getNeutralJointTranslation(std::uint16_t index) const override { return BehaviorReader->getNeutralJointTranslation(index); }
		virtual ConstArrayView<float> getNeutralJointTranslationXs() const override { return BehaviorReader->getNeutralJointTranslationXs(); }
		virtual ConstArrayView<float> getNeutralJointTranslationYs() const override { return BehaviorReader->getNeutralJointTranslationYs(); }
		virtual ConstArrayView<float> getNeutralJointTranslationZs() const override { return BehaviorReader->getNeutralJointTranslationZs(); }
		virtual Vector3 getNeutralJointRotation(std::uint16_t index) const override { return BehaviorReader->getNeutralJointRotation(index); }
		virtual ConstArrayView<float> getNeutralJointRotationXs() const override { return BehaviorReader->getNeutralJointRotationXs(); }
		virtual ConstArrayView<float> getNeutralJointRotationYs() const override { return BehaviorReader->getNeutralJointRotationYs(); }
		virtual ConstArrayView<float> getNeutralJointRotationZs() const override { return BehaviorReader->getNeutralJointRotationZs(); }

		// description
		virtual StringView getName() const override { return BehaviorReader->getName(); }
		virtual Archetype getArchetype() const override { return BehaviorReader->getArchetype(); }
		virtual Gender getGender() const override { return BehaviorReader->getGender(); }
		virtual std::uint16_t getAge() const override { return BehaviorReader->getAge(); }
		virtual std::uint32_t getMetaDataCount() const override { return BehaviorReader->getMetaDataCount(); }
		virtual StringView getMetaDataKey(std::uint32_t index) const override { return BehaviorReader->getMetaDataKey(index); }
		virtual StringView getMetaDataValue(const char* key) const override { return BehaviorReader->getMetaDataValue(key); }
		virtual TranslationUnit getTranslationUnit() const override { return BehaviorReader->getTranslationUnit(); }
		virtual RotationUnit getRotationUnit() const override { return BehaviorReader->getRotationUnit(); }
		virtual CoordinateSystem getCoordinateSystem() const override { return BehaviorReader->getCoordinateSystem(); }
		virtual std::uint16_t getLODCount() const override { return BehaviorReader->getLODCount(); }
		virtual std::uint16_t getDBMaxLOD() const override { return BehaviorReader->getDBMaxLOD(); }
		virtual StringView getDBComplexity() const override { return BehaviorReader->getDBComplexity(); }
		virtual StringView getDBName() const override { return BehaviorReader->getDBName(); }

		// Machine Learned Behavior
		virtual std::uint16_t getFileFormatGeneration() const override { return BehaviorReader->getFileFormatGeneration(); }
		virtual std::uint16_t getFileFormatVersion() const override { return BehaviorReader->getFileFormatVersion(); }
		virtual std::uint16_t getMLControlCount() const override { return BehaviorReader->getMLControlCount(); }
		virtual StringView getMLControlName(std::uint16_t index) const override { return BehaviorReader->getMLControlName(index); }
		virtual std::uint16_t getNeuralNetworkCount() const override { return BehaviorReader->getNeuralNetworkCount(); }
		virtual std::uint16_t getNeuralNetworkIndexListCount() const override { return BehaviorReader->getNeuralNetworkIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getNeuralNetworkIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getNeuralNetworkIndicesForLOD(lod); }
		virtual std::uint16_t getMeshRegionCount(std::uint16_t meshIndex) const override { return BehaviorReader->getMeshRegionCount(meshIndex); }
		virtual StringView getMeshRegionName(std::uint16_t meshIndex, std::uint16_t regionIndex) const override { return BehaviorReader->getMeshRegionName(meshIndex, regionIndex); }
		virtual ConstArrayView<std::uint16_t> getNeuralNetworkIndicesForMeshRegion(std::uint16_t meshIndex, std::uint16_t regionIndex) const override { return BehaviorReader->getNeuralNetworkIndicesForMeshRegion(meshIndex, regionIndex); }
		virtual ConstArrayView<std::uint16_t> getNeuralNetworkInputIndices(std::uint16_t netIndex) const override { return BehaviorReader->getNeuralNetworkInputIndices(netIndex); }
		virtual ConstArrayView<std::uint16_t> getNeuralNetworkOutputIndices(std::uint16_t netIndex) const override { return BehaviorReader->getNeuralNetworkOutputIndices(netIndex); }
		virtual std::uint16_t getNeuralNetworkLayerCount(std::uint16_t netIndex) const override { return BehaviorReader->getNeuralNetworkLayerCount(netIndex); }
		virtual ActivationFunction getNeuralNetworkLayerActivationFunction(std::uint16_t netIndex, std::uint16_t layerIndex) const override { return BehaviorReader->getNeuralNetworkLayerActivationFunction(netIndex, layerIndex); }
		virtual ConstArrayView<float> getNeuralNetworkLayerActivationFunctionParameters(std::uint16_t netIndex, std::uint16_t layerIndex) const override { return BehaviorReader->getNeuralNetworkLayerActivationFunctionParameters(netIndex, layerIndex); }
		virtual ConstArrayView<float> getNeuralNetworkLayerBiases(std::uint16_t netIndex, std::uint16_t layerIndex) const override { return BehaviorReader->getNeuralNetworkLayerBiases(netIndex, layerIndex); }
		virtual ConstArrayView<float> getNeuralNetworkLayerWeights(std::uint16_t netIndex, std::uint16_t layerIndex) const override { return BehaviorReader->getNeuralNetworkLayerWeights(netIndex, layerIndex); }

		// RBFBehaviorReader methods
		virtual std::uint16_t getRBFPoseCount() const override { return BehaviorReader->getRBFPoseCount(); }
		virtual StringView getRBFPoseName(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseName(poseIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFPoseJointOutputIndices(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseJointOutputIndices(poseIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFPoseBlendShapeChannelOutputIndices(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseBlendShapeChannelOutputIndices(poseIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFPoseAnimatedMapOutputIndices(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseAnimatedMapOutputIndices(poseIndex); }
		virtual ConstArrayView<float> getRBFPoseJointOutputValues(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseJointOutputValues(poseIndex); }
		virtual float getRBFPoseScale(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseScale(poseIndex); }
		virtual std::uint16_t getRBFPoseControlCount() const override { return BehaviorReader->getRBFPoseControlCount(); }
		virtual StringView getRBFPoseControlName(std::uint16_t poseControlIndex) const override { return BehaviorReader->getRBFPoseControlName(poseControlIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFPoseInputControlIndices(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseInputControlIndices(poseIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFPoseOutputControlIndices(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseOutputControlIndices(poseIndex); }
		virtual ConstArrayView<float> getRBFPoseOutputControlWeights(std::uint16_t poseIndex) const override { return BehaviorReader->getRBFPoseOutputControlWeights(poseIndex); }
		virtual std::uint16_t getRBFSolverCount() const override { return BehaviorReader->getRBFSolverCount(); }
		virtual std::uint16_t getRBFSolverIndexListCount() const override { return BehaviorReader->getRBFSolverIndexListCount(); }
		virtual ConstArrayView<std::uint16_t> getRBFSolverIndicesForLOD(std::uint16_t lod) const override { return BehaviorReader->getRBFSolverIndicesForLOD(lod); }
		virtual StringView getRBFSolverName(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverName(solverIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFSolverRawControlIndices(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverRawControlIndices(solverIndex); }
		virtual ConstArrayView<std::uint16_t> getRBFSolverPoseIndices(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverPoseIndices(solverIndex); }
		virtual ConstArrayView<float> getRBFSolverRawControlValues(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverRawControlValues(solverIndex); }
		virtual RBFSolverType getRBFSolverType(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverType(solverIndex); }
		virtual float getRBFSolverRadius(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverRadius(solverIndex); }
		virtual AutomaticRadius getRBFSolverAutomaticRadius(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverAutomaticRadius(solverIndex); }
		virtual float getRBFSolverWeightThreshold(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverWeightThreshold(solverIndex); }
		virtual RBFDistanceMethod getRBFSolverDistanceMethod(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverDistanceMethod(solverIndex); }
		virtual RBFNormalizeMethod getRBFSolverNormalizeMethod(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverNormalizeMethod(solverIndex); }
		virtual RBFFunctionType getRBFSolverFunctionType(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverFunctionType(solverIndex); }
		virtual TwistAxis getRBFSolverTwistAxis(std::uint16_t solverIndex) const override { return BehaviorReader->getRBFSolverTwistAxis(solverIndex); }

		// JointBehaviorMetadataReader methods
		virtual TranslationRepresentation getJointTranslationRepresentation(std::uint16_t jointIndex) const override { return BehaviorReader->getJointTranslationRepresentation(jointIndex); }
		virtual RotationRepresentation getJointRotationRepresentation(std::uint16_t jointIndex) const override { return BehaviorReader->getJointRotationRepresentation(jointIndex); }
		virtual ScaleRepresentation getJointScaleRepresentation(std::uint16_t jointIndex) const override { return BehaviorReader->getJointScaleRepresentation(jointIndex); }

		// TwistSwingBehaviorReader methods
		virtual std::uint16_t getTwistCount() const override { return BehaviorReader->getTwistCount(); }
		virtual TwistAxis getTwistSetupTwistAxis(std::uint16_t twistIndex) const override { return BehaviorReader->getTwistSetupTwistAxis(twistIndex); }
		virtual ConstArrayView<std::uint16_t> getTwistInputControlIndices(std::uint16_t twistIndex) const override { return BehaviorReader->getTwistInputControlIndices(twistIndex); }
		virtual ConstArrayView<std::uint16_t> getTwistOutputJointIndices(std::uint16_t twistIndex) const override { return BehaviorReader->getTwistOutputJointIndices(twistIndex); }
		virtual ConstArrayView<float> getTwistBlendWeights(std::uint16_t twistIndex) const override { return BehaviorReader->getTwistBlendWeights(twistIndex); }
		virtual std::uint16_t getSwingCount() const override { return BehaviorReader->getSwingCount(); }
		virtual TwistAxis getSwingSetupTwistAxis(std::uint16_t swingIndex) const override { return BehaviorReader->getSwingSetupTwistAxis(swingIndex); }
		virtual ConstArrayView<std::uint16_t> getSwingInputControlIndices(std::uint16_t swingIndex) const override { return BehaviorReader->getSwingInputControlIndices(swingIndex); }
		virtual ConstArrayView<std::uint16_t> getSwingOutputJointIndices(std::uint16_t swingIndex) const override { return BehaviorReader->getSwingOutputJointIndices(swingIndex); }
		virtual ConstArrayView<float> getSwingBlendWeights(std::uint16_t swingIndex) const override { return BehaviorReader->getSwingBlendWeights(swingIndex); }

		// Reader
		virtual void unload(dna::DataLayer Layer) override { ensureMsgf(false, TEXT("Assest are not unloadable")); }

		// StreamReader
		virtual void read() override { }
	};
}
