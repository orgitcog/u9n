// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDMaterialXShaderGraph.h"

#ifdef ENABLE_USD_MATERIALX

#include "Misc/Paths.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDMemory.h"
#include "USDTypesConversion.h"
#include "UsdWrappers/SdfLayer.h"

namespace mx = MaterialX;

FUsdMaterialXShaderGraph::FUsdMaterialXShaderGraph(const pxr::UsdPrim& UsdShadeMaterialPrim, const TCHAR* RenderContext)
	: MaterialXTypes {
		"boolean",
		"integer",
		"float",
		"color3",
		"color4",
		"vector2",
		"vector3",
		"vector4",
		"matrix33",
		"matrix44",
		"string",
		"filename",
		"geomname",
		"surfaceshader",
		"displacementshader",
		"volumeshader",
		"lightshader",
		"material",
		"none",
		"integerarray",
		"floatarray",
		"color3array",
		"color4array",
		"vector2array",
		"vector3array",
		"vector4array",
		"stringarray",
		"geomnamearray",
	}
	, UsdToMaterialXTypes {
		{"color3f",  "color3"},
		{"color4f",  "color4"},
		{"float2",   "vector2"},
		{"vector3f", "vector3"},
		{"vector4f", "vector4"},
		{"int",      "integer"},
		{"bool",     "boolean"},
	}
	, TangentSpaceInputs{
	    //UsdPreviewSurface
		"normal",
		//Standard Surface
		"coat_normal",
		"tangent",
		//Openpbr Surface
	    "geometry_normal",
		"geometry_coat_normal",
		"geometry_tangent",
		"geometry_coat_tangent"
	}
{
	pxr::UsdShadeMaterial UsdShadeMaterial{UsdShadeMaterialPrim};

	if (!UsdShadeMaterial)
	{
		USD_LOG_ERROR(TEXT("Couldn't create a UsdShadeMaterial for the prim."));
		return;
	}

	Document = mx::createDocument();
	mx::DocumentPtr MaterialXLibrary = mx::createDocument();
	mx::FileSearchPath MaterialXFolder{TCHAR_TO_UTF8(*FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries"), TEXT("ThirdParty"), TEXT("MaterialX")))};
	mx::StringSet LoadedLibs = mx::loadLibraries({"libraries"}, MaterialXFolder, MaterialXLibrary);
	Document->importLibrary(MaterialXLibrary);

	CreateSurfaceMaterial(UsdShadeMaterial, RenderContext);
}

mx::DocumentPtr FUsdMaterialXShaderGraph::GetDocument() const
{
	return Document;
}

const TArray<FUsdMaterialXShaderGraph::FGeomProp>& FUsdMaterialXShaderGraph::GetGeomPropValueNames() const
{
	return GeomPropValueNames;
}

void FUsdMaterialXShaderGraph::CreateSurfaceMaterial(const pxr::UsdShadeMaterial& UsdShadeMaterial, const TCHAR* RenderContext)
{
	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if (RenderContext)
	{
		pxr::TfToken ProvidedRenderContextToken = UnrealToUsd::ConvertToken(RenderContext).Get();
		RenderContextToken = ProvidedRenderContextToken;
	}

	pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource(RenderContextToken);
	pxr::UsdShadeShader DisplacementShader = UsdShadeMaterial.ComputeDisplacementSource(RenderContextToken);

	if (!SurfaceShader)
	{
		USD_LOG_ERROR(
			TEXT("Couldn't find a SurfaceShader for UsdShadeMaterial '%s'."),
			*UsdToUnreal::ConvertToken(UsdShadeMaterial.GetPrim().GetName())
		);
		return;
	}

	auto CreateShaderGraph = [this](const pxr::UsdShadeShader& SurfaceShader, mx::DocumentPtr InDocument) -> mx::NodePtr
	{
		pxr::TfToken NodeDefTokenId;
		SurfaceShader.GetShaderId(&NodeDefTokenId);
		mx::NodeDefPtr NodeDef = InDocument->getNodeDef(NodeDefTokenId.GetString());

		// no node definition, just return
		if (!NodeDef)
		{
			return nullptr;
		}

		mx::NodePtr MtlxSurfaceShader = InDocument->addNodeInstance(NodeDef, SurfaceShader.GetPrim().GetName().GetString());

		// if the shadergraph is not valid (probably caused by a missing nodedef or wrong type), just invalidate it all
		if (!ComputeShaderGraph(MtlxSurfaceShader, SurfaceShader))
		{
			MtlxSurfaceShader = nullptr;
		}

		return MtlxSurfaceShader;
	};

	// Create and store the input interface names
	TUsdStore<std::vector<pxr::UsdShadeInput>> InterfaceInputs = UsdShadeMaterial.GetInterfaceInputs();

	for (const pxr::UsdShadeInput& InterfaceInput : *InterfaceInputs)
	{
		std::string InterfaceNameInput = InterfaceInput.GetBaseName().GetString();
		std::string InterfaceNameType = InterfaceInput.GetTypeName().GetAsToken().GetString();
		GetMatchingInputType(InterfaceNameInput, InterfaceNameType);

		mx::InputPtr InputInterfaceName = Document->addInput(InterfaceNameInput, InterfaceNameType);
		SetInputValue(InputInterfaceName, InterfaceInput);
		InterfaceNames.insert(InputInterfaceName->getName());
	}

	// Create the shader graph for the surface shader and displacement shader
	if (mx::NodePtr MaterialXSurfaceShader = CreateShaderGraph(SurfaceShader, Document))
	{
		mx::NodeDefPtr SurfaceMaterialNodeDef = Document->getNodeDef("ND_surfacematerial");
		mx::NodePtr SurfaceMaterial = Document->addNodeInstance(SurfaceMaterialNodeDef, UsdShadeMaterial.GetPrim().GetName().GetString());

		SurfaceMaterial->setConnectedNode("surfaceshader", MaterialXSurfaceShader);

		// the displacement shader is not necessarily required
		if (DisplacementShader)
		{
			if (mx::NodePtr MaterialXDisplacementShader = CreateShaderGraph(DisplacementShader, Document))
			{
				SurfaceMaterial->setConnectedNode("displacementshader", MaterialXDisplacementShader);
			}
		}

		std::string ErrorMessage;
		SurfaceMaterial->validate(&ErrorMessage);
		if (!ErrorMessage.empty())
		{
			USD_LOG_ERROR(TEXT("USD MaterialX: %s."), UTF8_TO_TCHAR(ErrorMessage.c_str()));
			SurfaceMaterial = nullptr;
		}
	}
}

bool FUsdMaterialXShaderGraph::ComputeShaderGraph(mx::NodePtr Node, const pxr::UsdShadeShader& Shader)
{
	if (!Node)
	{
		return false;
	}

	TUsdStore<std::vector<pxr::UsdShadeInput>> UsdInputs = Shader.GetInputs();

	for (const pxr::UsdShadeInput& UsdInput : *UsdInputs)
	{
		// Basically the idea here is to traverse the entire graph coming from a tangent space input of a surface shader (e.g: 'normal' of
		// <standard_surface>) if we come from a TS input, all geompropvalue along the path needs to be set in TS since we can only set at init-time
		// the value of a TGuardValue and we're in a recursive state, if bTangentSpaceInput is false we set its value to IsTangentSpace otherwise we
		// keep it as is (meaning true)
		TGuardValue<bool> GuardInput(bTangentSpaceInput, bTangentSpaceInput || IsTangentSpaceInput(UsdInput));

		mx::InputPtr Input = GetInput(Node, UsdInput);

		bool bHasValue = true;
		TUsdStore<pxr::UsdShadeInput::SourceInfoVector> ConnectedSources = UsdInput.GetConnectedSources();

		// At this point an input should end up in either of these 3 cases, with the precedence in that order:
		// 1. it has an interfaceName, if that's the case we continue to the next input (it has no value, no nodename)
		// 2. it is connected to a nodename with a valid nodedef (it has no value)
		// 3. it has a value, if not defined by the input, the default one from the nodedef is taken

		// Let's loop over the connected sources, even though we should expect only one
		for (const pxr::UsdShadeConnectionSourceInfo& Source : *ConnectedSources)
		{
			if (SetInterfaceName(Input, Source))
			{
				bHasValue = false;
				break;
			}

			bool bIsNodeAlreadyExist;
			mx::NodePtr ConnectedNode = GetNode(Source, bIsNodeAlreadyExist);
			// Let's set if the input has an interfaceName
			if (!ConnectedNode)
			{
				break;
			}

			Input->setConnectedNode(ConnectedNode);
			
			// no need to add the default output
			if (const std::string& OutputName = Source.sourceName.GetString(); OutputName != "out")
			{
				Input->setOutputString(OutputName);
			}

			// recurse if it's a newly created node, otherwise the work has already been done
			if (!bIsNodeAlreadyExist)
			{
				bHasValue = ComputeShaderGraph(ConnectedNode, Source.source);
			}

			bHasValue = false;
		}

		if (bHasValue)
		{
			SetInputValue(Input, UsdInput);
		}
	}

	return true;
}

bool FUsdMaterialXShaderGraph::IsGeomColor(const pxr::UsdShadeShader& GeomPropValueShader)
{
	if (pxr::UsdShadeInput GeomPropInput = GeomPropValueShader.GetInput(pxr::TfToken{"geomprop"}))
	{
		if (TUsdStore<std::string> Value; GeomPropInput.Get<std::string>(&Value.Get()))
		{
			if (*Value == "displayColor" || *Value == "displayOpacity")
			{
				return true;
			}
		}
	}
	return false;
}

void FUsdMaterialXShaderGraph::SetInputValue(mx::InputPtr Input, const pxr::UsdShadeInput& UsdInput)
{
	auto SetInputValue = [&UsdInput, Input](auto Type)
	{
		using MtlxType = decltype(Type);

		using FPairingUsdMtlxType = FPairingUsdMtlxType_t<MtlxType>;

		if (FPairingUsdMtlxType Value; UsdInput.Get<FPairingUsdMtlxType>(&Value))
		{
			if constexpr (std::is_same_v<FPairingUsdMtlxType, pxr::GfVec4f> || std::is_same_v<FPairingUsdMtlxType, pxr::GfVec3f>
						  || std::is_same_v<FPairingUsdMtlxType, pxr::GfVec2f>)
			{
				Input->setValue(MtlxType{Value.data(), Value.data() + FPairingUsdMtlxType::dimension});
			}
			else
			{
				Input->setValue(Value);
			}
		}
		else if (mx::NodePtr Node = Input->getParent()->asA<mx::Node>())
		{
			mx::NodeDefPtr NodeDef = Node->getNodeDef();
			mx::InputPtr ActiveInput = NodeDef->getActiveInput(Input->getName());
			if (ActiveInput->hasDefaultGeomPropString())
			{
				Node->removeInput(Input->getName());
			}
			else
			{
				MtlxType DefaultValue = ActiveInput->getDefaultValue()->asA<MtlxType>();
				Input->setValue(DefaultValue);
			}
		}
	};

	const std::string& InputType = Input->getType();
	if (InputType == "float")
	{
		SetInputValue(float{});
	}
	else if (InputType == "integer")
	{
		SetInputValue(int32{});
	}
	else if (InputType == "boolean")
	{
		SetInputValue(bool{});
	}
	else if (InputType == "vector2")
	{
		SetInputValue(mx::Vector2{});
	}
	else if (InputType == "vector3")
	{
		SetInputValue(mx::Vector3{});
	}
	else if (InputType == "vector4")
	{
		SetInputValue(mx::Vector4{});
	}
	else if (InputType == "color3")
	{
		SetInputValue(mx::Color3{});
	}
	else if (InputType == "color4")
	{
		SetInputValue(mx::Color4{});
	}
	else if (InputType == "string")
	{
		if (TUsdStore<std::string> Value; UsdInput.Get<std::string>(&Value.Get()))
		{
			Input->setValue(Value.Get());
		}
		else if (mx::NodePtr Node = Input->getParent()->asA<mx::Node>())
		{
			mx::NodeDefPtr NodeDef = Node->getNodeDef();
			mx::InputPtr ActiveInput = NodeDef->getActiveInput(Input->getName());
			std::string DefaultValue = ActiveInput->getDefaultValue()->asA<std::string>();
			Input->setValue(DefaultValue);
		}
	}
	else if (InputType == "filename")
	{
		FString PathToResolve;
		if (pxr::SdfAssetPath AssetPath; UsdInput.Get<pxr::SdfAssetPath>(&AssetPath))
		{
			TUsdStore<UE::FSdfLayer> Layer = UsdUtils::FindLayerForAttribute(UsdInput.GetAttr(), pxr::UsdTimeCode::Default().GetValue());

			PathToResolve = Layer.Get()
								? UsdToUnreal::ConvertString(
									  TUsdStore<std::string>(pxr::SdfComputeAssetPathRelativeToLayer(Layer.Get(), AssetPath.GetAssetPath())).Get()
								  )
								: UsdToUnreal::ConvertString(AssetPath.GetAssetPath());
		}
		Input->setValueString(std::string{TCHAR_TO_UTF8(*PathToResolve)});
	}
	else
	{
		USD_LOG_WARNING(TEXT("Couldn't find a value type for (%s)."), UTF8_TO_TCHAR(InputType.c_str()));
	}
}

bool FUsdMaterialXShaderGraph::SetInterfaceName(MaterialX::InputPtr Input, const pxr::UsdShadeConnectionSourceInfo& Source)
{
	bool bIsSet = false;
	const std::string& SourceName = Source.sourceName.GetString();
	if (InterfaceNames.find(SourceName) != InterfaceNames.end())
	{
		if (mx::InputPtr InputInterfaceName = Document->getInput(SourceName))
		{
			Input->setInterfaceName(InputInterfaceName->getName());
			bIsSet = true;
		}
	}
	return bIsSet;
}

mx::NodePtr FUsdMaterialXShaderGraph::GetNode(const pxr::UsdShadeConnectionSourceInfo& Source, bool& bIsNodeAlreadyExist)
{
	bool bIsGeomPropValue;
	bIsNodeAlreadyExist = true;
	mx::NodeDefPtr NodeDef = GetNodeDef(Source, bIsGeomPropValue);

	if (!NodeDef)
	{
		return nullptr;
	}

	std::string ConnectedShaderName = Source.source.GetPrim().GetName().GetString();

	mx::NodePtr ConnectedNode = Document->getNode(ConnectedShaderName);

	// if the node is not present in the graph, we create it and notify that it's a new node
	if (!ConnectedNode)
	{
		ConnectedNode = Document->addNodeInstance(NodeDef, ConnectedShaderName);
		if (bIsGeomPropValue)
		{
			ConnectedNode->setTypedAttribute("UE:GeomPropImage", bIsGeomPropValue);
		}
		bIsNodeAlreadyExist = false;
	}

	return ConnectedNode;
}

mx::InputPtr FUsdMaterialXShaderGraph::GetInput(mx::NodePtr Node, const pxr::UsdShadeInput& UsdInput)
{
	std::string InputName = UsdInput.GetBaseName().GetString();
	std::string InputType = UsdInput.GetTypeName().GetAsToken().GetString();

	bool bMatch = GetMatchingInputType(InputName, InputType);

	if (!bMatch && InputName == "geomprop")
	{
		if (Node->getCategory() == "geomcolor")
		{
			InputName = "index";
			InputType = "integer";
		}
		else
		{
			InputName = "file";
			InputType = "filename";
			TUsdStore<std::string> Value;
			UsdInput.Get<std::string>(&Value.Get());
			if (!GeomPropValueNames.FindByPredicate(
					[&Value](const FGeomProp& Other)
					{
						return Other.Name == Value->c_str();
					}
				))
			{
				// we only set the the input as being in Tangent Space if also its type is a vec3
				GeomPropValueNames.Emplace(Value->c_str(), bTangentSpaceInput && Node->getType() == "vector3" ? bTangentSpaceInput : false);
			}
		}
	}
	else // take the input given by the nodedef
	{
		mx::NodeDefPtr NodeDef = Node->getNodeDef();
		mx::InputPtr Input = NodeDef->getActiveInput(InputName);
		InputType = Input->getType();
	}

	return Node->addInput(InputName, InputType);
}

mx::NodeDefPtr FUsdMaterialXShaderGraph::GetNodeDef(const pxr::UsdShadeConnectionSourceInfo& Source, bool& bIsGeomPropValue)
{
	pxr::UsdShadeShader UsdConnectedShader{Source.source.GetPrim()};

	if (!UsdConnectedShader)
	{
		USD_LOG_WARNING(TEXT("The '%s' connected source is not a valid USD shader."), *UsdToUnreal::ConvertString(Source.source.GetPrim().GetName()));
	}

	pxr::TfToken NodeDefTokenId;
	UsdConnectedShader.GetShaderId(&NodeDefTokenId);

	std::string NodeDefString = NodeDefTokenId.GetString();
	bIsGeomPropValue = false;
	if (std::size_t Pos = NodeDefString.find("geompropvalue"); Pos != std::string::npos)
	{
		// we convert a geomprop either to an image or a geomcolor in case if the name is displayColor
		std::string NodeName = "image";
		if (IsGeomColor(UsdConnectedShader))
		{
			NodeName = "geomcolor";
		}
		else
		{
			bIsGeomPropValue = true;
			// Here we just replace the replace nodedef of a <geompropvalue> by the <image> one
			// e.g: ND_geompropvalue_vector3
			//		ND_image_vector3
			// for integers/bool, <image> has no such node definitions, we just consider it as floats
			// e.g: ND_geompropvalue_integer
			//		ND_image_float
			if (std::size_t pos = NodeDefString.rfind("integer"); pos != std::string::npos)
			{
				NodeDefString.replace(pos, sizeof("integer") - 1, "float");
			}
			else if (pos = NodeDefString.rfind("boolean"); pos != std::string::npos)
			{
				NodeDefString.replace(pos, sizeof("boolean") - 1, "float");
			}
			else if (pos = NodeDefString.rfind("string"); pos != std::string::npos)
			{
				USD_LOG_WARNING(
					TEXT("'$s': '%s' are not supported"),
					*UsdToUnreal::ConvertString(Source.source.GetPrim().GetName()),
					*UsdToUnreal::ConvertString(NodeDefString)
				);
				return nullptr;
			}
		}
		NodeDefString.replace(Pos, sizeof("geompropvalue") - 1, NodeName);
	}

	mx::NodeDefPtr NodeDef = Document->getNodeDef(NodeDefString);

	if (!NodeDef)
	{
		USD_LOG_WARNING(TEXT("Couldn't find a nodedef for (%s)."), UTF8_TO_TCHAR(NodeDefTokenId.GetString().c_str()));
	}

	return NodeDef;
}

bool FUsdMaterialXShaderGraph::GetMatchingInputType(const std::string& InputName, std::string& InOutInputType)
{
	bool bMatch = false;

	if (MaterialXTypes.find(InOutInputType) == MaterialXTypes.end())
	{
		if (auto It = UsdToMaterialXTypes.find(InOutInputType); It != UsdToMaterialXTypes.end())
		{
			InOutInputType = It->second;
			bMatch = true;
		}
		else if (InputName == "file")	 // we're dealing with a texture, USD calls the type "asset" for the input
		{
			InOutInputType = "filename";
			bMatch = true;
		}
	}

	return bMatch;
}

bool FUsdMaterialXShaderGraph::IsTangentSpaceInput(const pxr::UsdShadeInput& UsdInput) const
{
	return TangentSpaceInputs.find(UsdInput.GetBaseName().GetString()) != TangentSpaceInputs.cend();
}
#endif	  // ENABLE_USD_MATERIALX
