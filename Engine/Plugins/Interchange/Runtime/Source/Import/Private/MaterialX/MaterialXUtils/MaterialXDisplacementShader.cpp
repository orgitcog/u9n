// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "MaterialXDisplacementShader.h"
#include "InterchangeImportLog.h"

namespace mx = MaterialX;

FMaterialXDisplacementShader::FMaterialXDisplacementShader(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXSurfaceShaderAbstract(BaseNodeContainer)
{
	NodeDefinition = mx::NodeDefinition::DisplacementFloat; // we set by default a displacement float but the SurfaceMaterial should set the correct nodedef
}

TSharedRef<FMaterialXBase> FMaterialXDisplacementShader::MakeInstance(UInterchangeBaseNodeContainer& BaseNodeContainer)
{
	TSharedRef<FMaterialXDisplacementShader> Result = MakeShared<FMaterialXDisplacementShader>(BaseNodeContainer);
	Result->RegisterConnectNodeOutputToInputDelegates();
	return Result;
}

UInterchangeBaseNode* FMaterialXDisplacementShader::Translate(MaterialX::NodePtr DisplacementNode)
{
	this->SurfaceShaderNode = DisplacementNode;

	UInterchangeShaderNode* DisplacementShaderNode = FMaterialXSurfaceShaderAbstract::Translate(EInterchangeMaterialXShaders::Displacement);
	// by default the center is at 0.5, but in order to compute correctly the normals from the displacement we need it at 0 in MX_Displacement material function
	ShaderGraphNode->SetCustomDisplacementCenterMode(0.f);

	using namespace UE::Interchange::Materials;

	// Outputs
	UInterchangeShaderPortsAPI::ConnectOuputToInputByName(ShaderGraphNode, Common::Parameters::Displacement.ToString(), DisplacementShaderNode->GetUniqueID(), Common::Parameters::Displacement.ToString());

	return DisplacementShaderNode;
}
#endif