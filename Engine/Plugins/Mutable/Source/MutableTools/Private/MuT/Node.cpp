// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"
#include "Misc/AssertionMacros.h"

#include "MuT/NodeObject.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeComponentSwitch.h"
#include "MuT/NodeComponentVariation.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/NodeImageConditional.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageLuminance.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageMultiLayer.h"
#include "MuT/NodeImageNormalComposite.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSaturate.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/NodeImageVariation.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourVariation.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourArithmeticOperation.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourTable.h"
#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarVariation.h"
#include "MuT/NodeScalarArithmeticOperation.h"
#include "MuT/NodeScalarParameter.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeScalarTable.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshClipMorphPlane.h"
#include "MuT/NodeMeshClipDeform.h"
#include "MuT/NodeMeshClipWithMesh.h"
#include "MuT/NodeMeshParameter.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshApplyPose.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshVariation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeModifier.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeModifierMeshTransformInMesh.h"
#include "MuT/NodeModifierSurfaceEdit.h"
#include "MuT/NodeMatrix.h"
#include "MuT/NodeMatrixConstant.h"
#include "MuT/NodeMatrixParameter.h"
#include "MuT/NodeString.h"
#include "MuT/NodeStringConstant.h"
#include "MuT/NodeStringParameter.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeRangeFromScalar.h"

namespace mu
{

	// Static initialisation
	FNodeType Node::StaticType = FNodeType(Node::EType::Node, nullptr);

	FNodeType NodeObject::StaticType = FNodeType(Node::EType::Object, Node::GetStaticType());
	FNodeType NodeObjectNew::StaticType = FNodeType(Node::EType::ObjectNew, NodeObject::GetStaticType());
	FNodeType NodeObjectGroup::StaticType = FNodeType(Node::EType::ObjectGroup, NodeObject::GetStaticType());

	FNodeType NodeComponent::StaticType = FNodeType(Node::EType::Component, Node::GetStaticType());
	FNodeType NodeComponentNew::StaticType = FNodeType(Node::EType::ComponentNew, NodeComponent::GetStaticType());
	FNodeType NodeComponentSwitch::StaticType = FNodeType(Node::EType::ComponentSwitch, NodeComponent::GetStaticType());
	FNodeType NodeComponentVariation::StaticType = FNodeType(Node::EType::ComponentVariation, NodeComponent::GetStaticType());
	FNodeType NodeComponentEdit::StaticType = FNodeType(Node::EType::ComponentEdit, NodeComponent::GetStaticType());

	FNodeType NodeBool::StaticType = FNodeType(Node::EType::Bool, Node::GetStaticType());
	FNodeType NodeBoolConstant::StaticType = FNodeType(Node::EType::BoolConstant, NodeBool::GetStaticType());
	FNodeType NodeBoolParameter::StaticType = FNodeType(Node::EType::BoolParameter, NodeBool::GetStaticType());
	FNodeType NodeBoolNot::StaticType = FNodeType(Node::EType::BoolNot, NodeBool::GetStaticType());
	FNodeType NodeBoolAnd::StaticType = FNodeType(Node::EType::BoolAnd, NodeBool::GetStaticType());

	FNodeType NodeScalar::StaticType = FNodeType(Node::EType::Scalar, Node::GetStaticType());
	FNodeType NodeScalarSwitch::StaticType = FNodeType(Node::EType::ScalarSwitch, NodeScalar::GetStaticType());
	FNodeType NodeScalarConstant::StaticType = FNodeType(Node::EType::ScalarConstant, NodeScalar::GetStaticType());
	FNodeType NodeScalarParameter::StaticType = FNodeType(Node::EType::ScalarParameter, NodeScalar::GetStaticType());
	FNodeType NodeScalarVariation::StaticType = FNodeType(Node::EType::ScalarVariation, NodeScalar::GetStaticType());
	FNodeType NodeScalarArithmeticOperation::StaticType = FNodeType(Node::EType::ScalarArithmeticOperation, NodeScalar::GetStaticType());
	FNodeType NodeScalarEnumParameter::StaticType = FNodeType(Node::EType::ScalarEnumParameter, NodeScalar::GetStaticType());
	FNodeType NodeScalarTable::StaticType = FNodeType(Node::EType::ScalarTable, NodeScalar::GetStaticType());
	FNodeType NodeScalarCurve::StaticType = FNodeType(Node::EType::ScalarCurve, NodeScalar::GetStaticType());

	FNodeType NodeSurface::StaticType = FNodeType(Node::EType::Surface, Node::GetStaticType());
	FNodeType NodeSurfaceNew::StaticType = FNodeType(Node::EType::SurfaceNew, NodeSurface::GetStaticType());
	FNodeType NodeSurfaceSwitch::StaticType = FNodeType(Node::EType::SurfaceSwitch, NodeSurface::GetStaticType());
	FNodeType NodeSurfaceVariation::StaticType = FNodeType(Node::EType::SurfaceVariation, NodeSurface::GetStaticType());

	FNodeType NodeLOD::StaticType = FNodeType(Node::EType::LOD, Node::GetStaticType());
	FNodeType NodeExtensionData::StaticType = FNodeType(Node::EType::ExtensionData, Node::GetStaticType());
	FNodeType NodeExtensionDataConstant::StaticType = FNodeType(Node::EType::ExtensionDataConstant, NodeExtensionData::GetStaticType());

	FNodeType NodeImage::StaticType = FNodeType(Node::EType::Image, Node::GetStaticType());
	FNodeType NodeImageConstant::StaticType = FNodeType(Node::EType::ImageConstant, NodeImage::GetStaticType());
	FNodeType NodeImageTable::StaticType = FNodeType(Node::EType::ImageTable, NodeImage::GetStaticType());
	FNodeType NodeImageParameter::StaticType = FNodeType(Node::EType::ImageParameter, NodeImage::GetStaticType());
	FNodeType NodeImageFormat::StaticType = FNodeType(Node::EType::ImageFormat, NodeImage::GetStaticType());
	FNodeType NodeImageBinarise::StaticType = FNodeType(Node::EType::ImageBinarise, NodeImage::GetStaticType());
	FNodeType NodeImageConditional::StaticType = FNodeType(Node::EType::ImageConditional, NodeImage::GetStaticType());
	FNodeType NodeImageInterpolate::StaticType = FNodeType(Node::EType::ImageInterpolate, NodeImage::GetStaticType());
	FNodeType NodeImageInvert::StaticType = FNodeType(Node::EType::ImageInvert, NodeImage::GetStaticType());
	FNodeType NodeImageLayer::StaticType = FNodeType(Node::EType::ImageLayer, NodeImage::GetStaticType());
	FNodeType NodeImageLayerColour::StaticType = FNodeType(Node::EType::ImageLayerColour, NodeImage::GetStaticType());
	FNodeType NodeImageLuminance::StaticType = FNodeType(Node::EType::ImageLuminance, NodeImage::GetStaticType());
	FNodeType NodeImageMipmap::StaticType = FNodeType(Node::EType::ImageMipmap, NodeImage::GetStaticType());
	FNodeType NodeImageMultiLayer::StaticType = FNodeType(Node::EType::ImageMultiLayer, NodeImage::GetStaticType());
	FNodeType NodeImageNormalComposite::StaticType = FNodeType(Node::EType::ImageNormalComposite, NodeImage::GetStaticType());
	FNodeType NodeImagePlainColour::StaticType = FNodeType(Node::EType::ImagePlainColour, NodeImage::GetStaticType());
	FNodeType NodeImageProject::StaticType = FNodeType(Node::EType::ImageProject, NodeImage::GetStaticType());
	FNodeType NodeImageResize::StaticType = FNodeType(Node::EType::ImageResize, NodeImage::GetStaticType());
	FNodeType NodeImageSaturate::StaticType = FNodeType(Node::EType::ImageSaturate, NodeImage::GetStaticType());
	FNodeType NodeImageSwitch::StaticType = FNodeType(Node::EType::ImageSwitch, NodeImage::GetStaticType());
	FNodeType NodeImageSwizzle::StaticType = FNodeType(Node::EType::ImageSwizzle, NodeImage::GetStaticType());
	FNodeType NodeImageTransform::StaticType = FNodeType(Node::EType::ImageTransform, NodeImage::GetStaticType());
	FNodeType NodeImageVariation::StaticType = FNodeType(Node::EType::ImageVariation, NodeImage::GetStaticType());
	FNodeType NodeImageColourMap::StaticType = FNodeType(Node::EType::ImageColorMap, NodeImage::GetStaticType());

	FNodeType NodeColour::StaticType = FNodeType(Node::EType::Color, Node::GetStaticType());
	FNodeType NodeColourConstant::StaticType = FNodeType(Node::EType::ColorConstant, NodeColour::GetStaticType());
	FNodeType NodeColourParameter::StaticType = FNodeType(Node::EType::ColorParameter, NodeColour::GetStaticType());
	FNodeType NodeColourSwitch::StaticType = FNodeType(Node::EType::ColorSwitch, NodeColour::GetStaticType());
	FNodeType NodeColourVariation::StaticType = FNodeType(Node::EType::ColorVariation, NodeColour::GetStaticType());
	FNodeType NodeColourTable::StaticType = FNodeType(Node::EType::ColorTable, NodeColour::GetStaticType());
	FNodeType NodeColourArithmeticOperation::StaticType = FNodeType(Node::EType::ColorArithmeticOperation, NodeColour::GetStaticType());
	FNodeType NodeColourSampleImage::StaticType = FNodeType(Node::EType::ColorSampleImage, NodeColour::GetStaticType());
	FNodeType NodeColourFromScalars::StaticType = FNodeType(Node::EType::ColorFromScalars, NodeColour::GetStaticType());

	FNodeType NodeMesh::StaticType = FNodeType(Node::EType::Mesh, Node::GetStaticType());
	FNodeType NodeMeshConstant::StaticType = FNodeType(Node::EType::MeshConstant, NodeMesh::GetStaticType());
	FNodeType NodeMeshFragment::StaticType = FNodeType(Node::EType::MeshFragment, NodeMesh::GetStaticType());
	FNodeType NodeMeshClipMorphPlane::StaticType = FNodeType(Node::EType::MeshClipMorphPlane, NodeMesh::GetStaticType());
	FNodeType NodeMeshClipDeform::StaticType = FNodeType(Node::EType::MeshClipDeform, NodeMesh::GetStaticType());
	FNodeType NodeMeshClipWithMesh::StaticType = FNodeType(Node::EType::MeshClipWithMesh, NodeMesh::GetStaticType());
	FNodeType NodeMeshParameter::StaticType = FNodeType(Node::EType::MeshParameter, NodeMesh::GetStaticType());
	FNodeType NodeMeshMakeMorph::StaticType = FNodeType(Node::EType::MeshMakeMorph, NodeMesh::GetStaticType());
	FNodeType NodeMeshApplyPose::StaticType = FNodeType(Node::EType::MeshApplyPose, NodeMesh::GetStaticType());
	FNodeType NodeMeshTransform::StaticType = FNodeType(Node::EType::MeshTransform, NodeMesh::GetStaticType());
	FNodeType NodeMeshSwitch::StaticType = FNodeType(Node::EType::MeshSwitch, NodeMesh::GetStaticType());
	FNodeType NodeMeshReshape::StaticType = FNodeType(Node::EType::MeshReshape, NodeMesh::GetStaticType());
	FNodeType NodeMeshMorph::StaticType = FNodeType(Node::EType::MeshMorph, NodeMesh::GetStaticType());
	FNodeType NodeMeshFormat::StaticType = FNodeType(Node::EType::MeshFormat, NodeMesh::GetStaticType());
	FNodeType NodeMeshVariation::StaticType = FNodeType(Node::EType::MeshVariation, NodeMesh::GetStaticType());
	FNodeType NodeMeshTable::StaticType = FNodeType(Node::EType::MeshTable, NodeMesh::GetStaticType());

	FNodeType NodeModifier::StaticType = FNodeType(Node::EType::Modifier, Node::GetStaticType());
	FNodeType NodeModifierMeshClipDeform::StaticType = FNodeType(Node::EType::ModifierMeshClipDeform, NodeModifier::GetStaticType());
	FNodeType NodeModifierMeshClipMorphPlane::StaticType = FNodeType(Node::EType::ModifierMeshClipMorphPlane, NodeModifier::GetStaticType());
	FNodeType NodeModifierMeshClipWithMesh::StaticType = FNodeType(Node::EType::ModifierMeshClipWithMesh, NodeModifier::GetStaticType());
	FNodeType NodeModifierMeshClipWithUVMask::StaticType = FNodeType(Node::EType::ModifierMeshClipWithUVMask, NodeModifier::GetStaticType());
	FNodeType NodeModifierMeshTransformInMesh::StaticType = FNodeType(Node::EType::ModifierTransformInMesh, NodeModifier::GetStaticType());
	FNodeType NodeModifierSurfaceEdit::StaticType = FNodeType(Node::EType::ModifierSurfaceEdit, NodeSurface::GetStaticType());

	FNodeType NodeMatrix::StaticType = FNodeType(Node::EType::Matrix, Node::GetStaticType());
	FNodeType NodeMatrixConstant::StaticType = FNodeType(Node::EType::MatrixConstant, Node::GetStaticType());
	FNodeType NodeMatrixParameter::StaticType = FNodeType(Node::EType::MatrixParameter, Node::GetStaticType());

	FNodeType NodeString::StaticType = FNodeType(Node::EType::String, Node::GetStaticType());
	FNodeType NodeStringConstant::StaticType = FNodeType(Node::EType::StringConstant, NodeString::GetStaticType());
	FNodeType NodeStringParameter::StaticType = FNodeType(Node::EType::StringParameter, NodeString::GetStaticType());

	FNodeType NodeProjector::StaticType = FNodeType(Node::EType::Projector, Node::GetStaticType());
	FNodeType NodeProjectorConstant::StaticType = FNodeType(Node::EType::ProjectorConstant, NodeProjector::GetStaticType());
	FNodeType NodeProjectorParameter::StaticType = FNodeType(Node::EType::ProjectorParameter, NodeProjector::GetStaticType());

	FNodeType NodeRange::StaticType = FNodeType(Node::EType::Range, Node::GetStaticType());
	FNodeType NodeRangeFromScalar::StaticType = FNodeType(Node::EType::RangeFromScalar, NodeRange::GetStaticType());


	FNodeType::FNodeType()
	{
		Type = Node::EType::None;
		Parent = nullptr;
	}


	FNodeType::FNodeType(Node::EType InType, const FNodeType* pParent )
	{
		Type = InType;
		Parent = pParent;
	}


	void Node::SetMessageContext( const void* context )
	{
		MessageContext = context;
	}

	const void* Node::GetMessageContext() const 
	{ 
		return MessageContext; 
	}

}


