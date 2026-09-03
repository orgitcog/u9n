// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCachedData.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCollectionTransform.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionFirstPersonOutput.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionUserSceneTexture.h"
#include "Materials/MaterialExpressionMeshPaintTextureObject.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExternalCodeRegistry.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "VT/RuntimeVirtualTexture.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "Engine/Font.h"
#include "LandscapeGrassType.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "Engine/TextureCollection.h"
#include "ShaderCompilerCore.h"
#include "MaterialShared.h"
#include "MaterialCache/MaterialCacheMaterial.h"
#include "Materials/MaterialExpressionMaterialCache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialCachedData)

#define LOCTEXT_NAMESPACE "Material"

const FMaterialCachedParameterEntry FMaterialCachedParameterEntry::EmptyData{};
const FMaterialCachedExpressionData FMaterialCachedExpressionData::EmptyData{};
const FMaterialCachedExpressionEditorOnlyData FMaterialCachedExpressionEditorOnlyData::EmptyData{};

static_assert((uint64)(EMaterialProperty::MP_MaterialAttributes)-1 < (8 * sizeof(FMaterialCachedExpressionData::PropertyConnectedMask)), "PropertyConnectedMask cannot contain entire EMaterialProperty enumeration.");

FMaterialCachedExpressionData::FMaterialCachedExpressionData()
	: FunctionInfosStateCRC(0xffffffff)
	, bHasMaterialLayers(false)
	, bHasRuntimeVirtualTextureOutput(false)
	, bHasFirstPersonOutput(false)
	, bHasMaterialCacheOutput(false)
	, bMaterialCacheHasNonUVDerivedExpression(false)
	, bHasSceneColor(false)
	, bHasPerInstanceCustomData(false)
	, bHasPerInstanceRandom(false)
	, bHasVertexInterpolator(false)
	, bHasCustomizedUVs(false)
	, bHasMeshPaintTexture(false)
	, bHasWorldPosition(false)
{
	QualityLevelsUsed.AddDefaulted(EMaterialQualityLevel::Num);
#if WITH_EDITORONLY_DATA
	EditorOnlyData = MakeShared<FMaterialCachedExpressionEditorOnlyData>();
#endif // WITH_EDITORONLY_DATA
}

void FMaterialCachedExpressionData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddStableReferenceArray(&ReferencedTextures);
	Collector.AddStableReferenceArray(&ReferencedTextureCollections);
	Collector.AddStableReferenceArray(&GrassTypes);
	Collector.AddStableReferenceArray(&MaterialLayers.Layers);
	Collector.AddStableReferenceArray(&MaterialLayers.Blends);
	for (FMaterialFunctionInfo& FunctionInfo : FunctionInfos)
	{
		Collector.AddStableReference(&FunctionInfo.Function);
	}
	for (FMaterialParameterCollectionInfo& ParameterCollectionInfo : ParameterCollectionInfos)
	{
		Collector.AddStableReference(&ParameterCollectionInfo.ParameterCollection);
	}
}

void FMaterialCachedExpressionData::AppendReferencedFunctionIdsTo(TArray<FGuid>& Ids) const
{
	Ids.Reserve(Ids.Num() + FunctionInfos.Num());
	for (const FMaterialFunctionInfo& FunctionInfo : FunctionInfos)
	{
		Ids.AddUnique(FunctionInfo.StateId);
	}
}

void FMaterialCachedExpressionData::AppendReferencedParameterCollectionIdsTo(TArray<FGuid>& Ids) const
{
	Ids.Reserve(Ids.Num() + ParameterCollectionInfos.Num());
	for (const FMaterialParameterCollectionInfo& CollectionInfo : ParameterCollectionInfos)
	{
		Ids.AddUnique(CollectionInfo.StateId);
	}
}

void FMaterialCachedExpressionData::GetExternalCodeReferencesHash(FSHAHash& OutHash) const
{
	FSHA1 Hasher;
	for (const TObjectPtr<UClass>& ExternalCodeExpressionClass : ReferencedExternalCodeExpressionClasses)
	{
		if (const UObject* DefaultExternalCodeExpression = ExternalCodeExpressionClass->GetDefaultObject())
		{
			const UMaterialExpressionExternalCodeBase* ExternalCodeExpressionBase = CastChecked<UMaterialExpressionExternalCodeBase>(DefaultExternalCodeExpression);
			for (const FName& ExternalCodeIdentifier : ExternalCodeExpressionBase->ExternalCodeIdentifiers)
			{
				if (const FMaterialExternalCodeDeclaration* ExternalCodeDeclaration = MaterialExternalCodeRegistry::Get().FindExternalCode(ExternalCodeIdentifier))
				{
					ExternalCodeDeclaration->UpdateHash(Hasher);
				}
			}
		}
	}
	OutHash = Hasher.Finalize();
}

#if WITH_EDITOR
static bool TryAddParameter(FMaterialCachedExpressionData& CachedData,
	EMaterialParameterType Type,
	const FMaterialParameterInfo& ParameterInfo,
	const FMaterialCachedParameterEditorInfo& InEditorInfo,
	int32& OutIndex, 
	TOptional<FMaterialCachedParameterEditorInfo>& OutPreviousEditorInfo)
{
	check(CachedData.EditorOnlyData);
	FMaterialCachedParameterEntry& Entry = CachedData.GetParameterTypeEntry(Type);
	FMaterialCachedParameterEditorEntry& EditorEntry = CachedData.EditorOnlyData->EditorEntries[(int32)Type];

	FSetElementId ElementId = Entry.ParameterInfoSet.FindId(ParameterInfo);
	OutIndex = INDEX_NONE;
	if (!ElementId.IsValidId())
	{
		ElementId = Entry.ParameterInfoSet.Add(ParameterInfo);
		OutIndex = ElementId.AsInteger();
		EditorEntry.EditorInfo.Insert(InEditorInfo, OutIndex);
		// should be valid as long as we don't ever remove elements from ParameterInfoSet
		check(Entry.ParameterInfoSet.Num() == EditorEntry.EditorInfo.Num());
		return true;
	}

	// Update any editor values that haven't been set yet
	// TODO still need to do this??
	OutIndex = ElementId.AsInteger();

	FMaterialCachedParameterEditorInfo& EditorInfo = EditorEntry.EditorInfo[OutIndex];
	// Copy the previous parameter's original info before eventually replacing it, for error reporting purposes :
	OutPreviousEditorInfo.Emplace(EditorInfo);

	if (!EditorInfo.ExpressionGuid.IsValid())
	{
		EditorInfo.ExpressionGuid = InEditorInfo.ExpressionGuid;
	}
	if (EditorInfo.Description.IsEmpty())
	{
		EditorInfo.Description = InEditorInfo.Description;
	}
	if (EditorInfo.Group.IsNone())
	{
		EditorInfo.Group = InEditorInfo.Group;
		EditorInfo.SortPriority = InEditorInfo.SortPriority;
	}
	
	// Still return false, to signify this parameter was already added (don't want to add it again)
	return false;
}

bool FMaterialCachedExpressionData::AddParameter(const FMaterialParameterInfo& ParameterInfo, const FMaterialParameterMetadata& ParameterMeta, UObject*& OutReferencedTexture, UTextureCollection*& OutReferencedTextureCollection, FText& OutErrorMessage)
{
	check(EditorOnlyData);
	int32 AssetIndex = INDEX_NONE;
	if (!ParameterMeta.AssetPath.IsEmpty())
	{
		AssetIndex = EditorOnlyData->AssetPaths.AddUnique(ParameterMeta.AssetPath);
	}

	const FMaterialCachedParameterEditorInfo EditorInfo(ParameterMeta.ExpressionGuid, ParameterMeta.Description, ParameterMeta.Group, ParameterMeta.SortPriority, AssetIndex);
	int32 Index = INDEX_NONE;
	TOptional<FMaterialCachedParameterEditorInfo> PreviousEditorInfo;
	if (TryAddParameter(*this, ParameterMeta.Value.Type, ParameterInfo, EditorInfo, Index, PreviousEditorInfo))
	{
		switch (ParameterMeta.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			ScalarValues.Insert(ParameterMeta.Value.AsScalar(), Index);
			EditorOnlyData->ScalarMinMaxValues.Insert(FVector2D(ParameterMeta.ScalarMin, ParameterMeta.ScalarMax), Index);
			ScalarPrimitiveDataIndexValues.Insert(ParameterMeta.PrimitiveDataIndex, Index);
			if (ParameterMeta.bUsedAsAtlasPosition)
			{
				EditorOnlyData->ScalarCurveValues.Insert(ParameterMeta.ScalarCurve.Get(), Index);
				EditorOnlyData->ScalarCurveAtlasValues.Insert(ParameterMeta.ScalarAtlas.Get(), Index);
				OutReferencedTexture = ParameterMeta.ScalarAtlas.Get();
			}
			else
			{
				EditorOnlyData->ScalarCurveValues.Insert(nullptr, Index);
				EditorOnlyData->ScalarCurveAtlasValues.Insert(nullptr, Index);
			}
			break;

		case EMaterialParameterType::Vector:
			VectorValues.Insert(ParameterMeta.Value.AsLinearColor(), Index);
			EditorOnlyData->VectorChannelNameValues.Insert(ParameterMeta.ChannelNames, Index);
			EditorOnlyData->VectorUsedAsChannelMaskValues.Insert(ParameterMeta.bUsedAsChannelMask, Index);
			VectorPrimitiveDataIndexValues.Insert(ParameterMeta.PrimitiveDataIndex, Index);
			break;

		case EMaterialParameterType::DoubleVector:
			DoubleVectorValues.Insert(ParameterMeta.Value.AsVector4d(), Index);
			break;

		case EMaterialParameterType::Texture:
			TextureValues.Insert(ParameterMeta.Value.Texture, Index);
			EditorOnlyData->TextureChannelNameValues.Insert(ParameterMeta.ChannelNames, Index);
			OutReferencedTexture = ParameterMeta.Value.Texture;
			break;

		case EMaterialParameterType::TextureCollection:
			TextureCollectionValues.Insert(ParameterMeta.Value.TextureCollection, Index);
			OutReferencedTextureCollection = ParameterMeta.Value.TextureCollection;
			break;

		case EMaterialParameterType::Font:
			FontValues.Insert(ParameterMeta.Value.Font.Value, Index);
			FontPageValues.Insert(ParameterMeta.Value.Font.Page, Index);
			if (ParameterMeta.Value.Font.Value && ParameterMeta.Value.Font.Value->Textures.IsValidIndex(ParameterMeta.Value.Font.Page))
			{
				OutReferencedTexture = ParameterMeta.Value.Font.Value->Textures[ParameterMeta.Value.Font.Page];
			}
			break;

		case EMaterialParameterType::RuntimeVirtualTexture:
			RuntimeVirtualTextureValues.Insert(ParameterMeta.Value.RuntimeVirtualTexture, Index);
			OutReferencedTexture = ParameterMeta.Value.RuntimeVirtualTexture;
			break;

		case EMaterialParameterType::SparseVolumeTexture:
			SparseVolumeTextureValues.Insert(ParameterMeta.Value.SparseVolumeTexture, Index);
			OutReferencedTexture = ParameterMeta.Value.SparseVolumeTexture;
			break;

		case EMaterialParameterType::StaticSwitch:
			StaticSwitchValues.Insert(ParameterMeta.Value.AsStaticSwitch(), Index);
			DynamicSwitchValues.Insert(ParameterMeta.bDynamicSwitchParameter, Index);
			break;

		case EMaterialParameterType::StaticComponentMask:
			EditorOnlyData->StaticComponentMaskValues.Insert(ParameterMeta.Value.AsStaticComponentMask(), Index);
			break;

		default:
			checkNoEntry();
			break;
		}
	}
	else
	{
		auto GetEditorInfoAsText = [EditorOnlyData = EditorOnlyData](const FMaterialCachedParameterEditorInfo& InEditorInfo) -> FText
			{
				FString AssetPath = EditorOnlyData->AssetPaths[InEditorInfo.AssetIndex];
				return FText::Format(LOCTEXT("ReportParameterMetaData", "group:'{0}', asset:'{1}'"), FText::FromName(InEditorInfo.Group), FText::FromString(AssetPath));
			};

		auto ReportError = [&GetEditorInfoAsText](const FString& InCachedValue, const FString& InValue, const FMaterialCachedParameterEditorInfo& InCachedEditorInfo, const FMaterialCachedParameterEditorInfo& InEditorInfo) -> FText
			{
				return FText::Format(LOCTEXT("ReportDifferentParameterValueError", "{0} ({1}) vs. cached: {2} ({3})"), 
					FText::FromString(InValue), GetEditorInfoAsText(InEditorInfo), FText::FromString(InCachedValue), GetEditorInfoAsText(InCachedEditorInfo));
			};

		bool bSameValue = false;
		switch (ParameterMeta.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			bSameValue = ScalarValues[Index] == ParameterMeta.Value.AsScalar();
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(FString::Printf(TEXT("%f"), ScalarValues[Index]), FString::Printf(TEXT("%f"), ParameterMeta.Value.AsScalar()), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::Vector:
			bSameValue = VectorValues[Index] == ParameterMeta.Value.AsLinearColor();
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(VectorValues[Index].ToString(), ParameterMeta.Value.AsLinearColor().ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::DoubleVector:
			bSameValue = DoubleVectorValues[Index] == ParameterMeta.Value.AsVector4d();
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(DoubleVectorValues[Index].ToString(), ParameterMeta.Value.AsVector4d().ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::Texture:
			bSameValue = TextureValues[Index] == ParameterMeta.Value.Texture;
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(TextureValues[Index].ToString(), TSoftObjectPtr<UTexture>(ParameterMeta.Value.Texture).ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::TextureCollection:
			bSameValue = TextureCollectionValues[Index] == ParameterMeta.Value.TextureCollection;
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(TextureCollectionValues[Index].ToString(), TSoftObjectPtr<UTextureCollection>(ParameterMeta.Value.TextureCollection).ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::Font:
			bSameValue = FontValues[Index] == ParameterMeta.Value.Font.Value && FontPageValues[Index] == ParameterMeta.Value.Font.Page;
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(
					FString::Printf(TEXT("%s(%i)"), *FontValues[Index].ToString(), FontPageValues[Index]), 
					FString::Printf(TEXT("%s(%i)"), *TSoftObjectPtr<UFont>(ParameterMeta.Value.Font.Value).ToString(), ParameterMeta.Value.Font.Page), 
					*PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::RuntimeVirtualTexture:
			bSameValue = RuntimeVirtualTextureValues[Index] == ParameterMeta.Value.RuntimeVirtualTexture;
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(RuntimeVirtualTextureValues[Index].ToString(), TSoftObjectPtr<URuntimeVirtualTexture>(ParameterMeta.Value.RuntimeVirtualTexture).ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::SparseVolumeTexture:
			bSameValue = SparseVolumeTextureValues[Index] == ParameterMeta.Value.SparseVolumeTexture;
			if (!bSameValue)
			{
				OutErrorMessage = ReportError(SparseVolumeTextureValues[Index].ToString(), TSoftObjectPtr<USparseVolumeTexture>(ParameterMeta.Value.SparseVolumeTexture).ToString(), *PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::StaticSwitch:
			bSameValue = (StaticSwitchValues[Index] == ParameterMeta.Value.AsStaticSwitch()) && (DynamicSwitchValues[Index] == ParameterMeta.bDynamicSwitchParameter);
			if (!bSameValue)
			{
				auto GetStaticSwitchString = [](bool bInValue, bool bIsDynamicSwitch) -> FString
					{
						return FText::Format(LOCTEXT("StaticSwitchValue", "{0}{1}"), bInValue ? LOCTEXT("true", "true") : LOCTEXT("false", "false"), bIsDynamicSwitch ? LOCTEXT("dynamic", "(dynamic)") : FText()).ToString();
					};
				OutErrorMessage = ReportError(
					GetStaticSwitchString(StaticSwitchValues[Index], DynamicSwitchValues[Index]), 
					GetStaticSwitchString(ParameterMeta.Value.AsStaticSwitch(), ParameterMeta.bDynamicSwitchParameter), 
					*PreviousEditorInfo, EditorInfo);
			}
			break;

		case EMaterialParameterType::StaticComponentMask:
			bSameValue = EditorOnlyData->StaticComponentMaskValues[Index] == ParameterMeta.Value.AsStaticComponentMask();
			if (!bSameValue)
			{
				auto GetStaticComponentMaskString = [](const FStaticComponentMaskValue& InValue) -> FString
					{
						return FText::Format(LOCTEXT("StaticComponentMaskValue", "R={0},G={1},B={2},A={3}")
							, InValue.R ? LOCTEXT("true", "true") : LOCTEXT("false", "false")
							, InValue.G ? LOCTEXT("true", "true") : LOCTEXT("false", "false")
							, InValue.B ? LOCTEXT("true", "true") : LOCTEXT("false", "false")
							, InValue.A ? LOCTEXT("true", "true") : LOCTEXT("false", "false")).ToString();
					};
				OutErrorMessage = ReportError(
					GetStaticComponentMaskString(EditorOnlyData->StaticComponentMaskValues[Index]), 
					GetStaticComponentMaskString(ParameterMeta.Value.AsStaticComponentMask()), 
					*PreviousEditorInfo, EditorInfo);
			}
			break;

		default:
			bSameValue = true;
			break;
		}

		return bSameValue;
	}

	return true;
}

void FMaterialCachedExpressionData::UpdateForFunction(const FMaterialCachedExpressionContext& Context, UMaterialFunctionInterface* Function, EMaterialParameterAssociation Association, int32 ParameterIndex)
{
	if (!Function)
	{
		return;
	}

	// Update expressions for all dependent functions first, before processing the remaining expressions in this function
	// This is important so we add parameters in the proper order (parameter values are latched the first time a given parameter name is encountered)
	FMaterialCachedExpressionContext LocalContext(Context);
	LocalContext.CurrentFunction = Function;
	LocalContext.bUpdateFunctionExpressions = false; // we update functions explicitly
	
	FMaterialCachedExpressionData* Self = this;
	auto ProcessFunction = [Self, &LocalContext, Association, ParameterIndex](UMaterialFunctionInterface* InFunction) -> bool
	{
		Self->UpdateForExpressions(LocalContext, InFunction->GetExpressions(), Association, ParameterIndex);

		FMaterialFunctionInfo NewFunctionInfo;
		NewFunctionInfo.Function = InFunction;
		NewFunctionInfo.StateId = InFunction->StateId;
		Self->FunctionInfos.Add(NewFunctionInfo);
		Self->FunctionInfosStateCRC = FCrc::TypeCrc32(InFunction->StateId, Self->FunctionInfosStateCRC);

		return true;
	};
	Function->IterateDependentFunctions(ProcessFunction);

	ProcessFunction(Function);
}

void FMaterialCachedExpressionData::UpdateForLayerFunctions(const FMaterialCachedExpressionContext& Context, const FMaterialLayersFunctions& LayerFunctions)
{
	for (int32 LayerIndex = 0; LayerIndex < LayerFunctions.Layers.Num(); ++LayerIndex)
	{
		UpdateForFunction(Context, LayerFunctions.Layers[LayerIndex], LayerParameter, LayerIndex);
	}

	for (int32 BlendIndex = 0; BlendIndex < LayerFunctions.Blends.Num(); ++BlendIndex)
	{
		UpdateForFunction(Context, LayerFunctions.Blends[BlendIndex], BlendParameter, BlendIndex);
	}
}

void FMaterialCachedExpressionData::UpdateForExpressions(const FMaterialCachedExpressionContext& Context, TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions, EMaterialParameterAssociation Association, int32 ParameterIndex)
{
	check(EditorOnlyData);
	static const FGuid FirstPersonInterpolationAlphaGuid = FMaterialAttributeDefinitionMap::GetCustomAttributeID(TEXT("FirstPersonInterpolationAlpha"));

	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression)
		{
			continue;
		}

		UObject* ReferencedTexture = nullptr;
		UTextureCollection* ReferencedTextureCollection = nullptr;

		// Add any expression specific custom shader tags
		TArray<FName> ShaderTags;
		Expression->GetShaderTags(ShaderTags);
		EditorOnlyData->ShaderTags.Append(ShaderTags);

		FMaterialParameterMetadata ParameterMeta;
		FText ErrorContext;
		if (Expression->GetParameterValue(ParameterMeta))
		{
			const FName ParameterName = Expression->GetParameterName();

			// If we're processing a function, give that a chance to override the parameter value
			if (Context.CurrentFunction)
			{
				FMaterialParameterMetadata OverrideParameterMeta;
				if (Context.CurrentFunction->GetParameterOverrideValue(ParameterMeta.Value.Type, ParameterName, OverrideParameterMeta))
				{
					ParameterMeta.Value = OverrideParameterMeta.Value;
					ParameterMeta.ExpressionGuid = OverrideParameterMeta.ExpressionGuid;
					ParameterMeta.bUsedAsAtlasPosition = OverrideParameterMeta.bUsedAsAtlasPosition;
					ParameterMeta.ScalarAtlas = OverrideParameterMeta.ScalarAtlas;
					ParameterMeta.ScalarCurve = OverrideParameterMeta.ScalarCurve;
				}
			}

			const FMaterialParameterInfo ParameterInfo(ParameterName, Association, ParameterIndex);

			// Try add the parameter. If this fails, the parameter is being added twice with different values. Report it as error.
			if (!AddParameter(ParameterInfo, ParameterMeta, ReferencedTexture, ReferencedTextureCollection, ErrorContext))
			{
				FText ErrorMessage = FText::Format(LOCTEXT("DuplicateParameterError", "Parameter '{0}' is set multiple times to different values : {1}. Make sure each parameter is set once or always to the same value."),
					 FText::FromName(ParameterName), ErrorContext);
				DuplicateParameterErrors.AddUnique({ Expression, ErrorMessage.ToString() });
			}
		}


		if (ReferencedTexture)
		{
			ReferencedTextures.AddUnique(ReferencedTexture);
		}
		else if (ReferencedTextureCollection)
		{
			ReferencedTextureCollections.AddUnique(ReferencedTextureCollection);
		}
		else if (UTextureCollection* TextureCollection = Expression->GetReferencedTextureCollection())
		{
			ReferencedTextureCollections.AddUnique(TextureCollection);
		}
		else if (Expression->CanReferenceTexture())
		{
			// We first try to extract the referenced texture from the parameter value, that way we'll also get the proper texture in case value is overriden by a function instance
			const UMaterialExpression::ReferencedTextureArray ExpressionReferencedTextures = Expression->GetReferencedTextures();
			for (UObject* ExpressionReferencedTexture : ExpressionReferencedTextures)
			{
				ReferencedTextures.AddUnique(ExpressionReferencedTexture);
			}
		}

		Expression->GetLandscapeLayerNames(EditorOnlyData->LandscapeLayerNames);

		Expression->GetIncludeFilePaths(EditorOnlyData->ExpressionIncludeFilePaths);

		if (UMaterialExpressionUserSceneTexture* ExpressionUserSceneTexture = Cast<UMaterialExpressionUserSceneTexture>(Expression))
		{
			if (!ExpressionUserSceneTexture->UserSceneTexture.IsNone())
			{
				EditorOnlyData->UserSceneTextureInputs.Add(ExpressionUserSceneTexture->UserSceneTexture);
			}
		}

		if (UMaterialExpressionExternalCodeBase* ExternalCodeExpression = Cast<UMaterialExpressionExternalCodeBase>(Expression))
		{
			ReferencedExternalCodeExpressionClasses.AddUnique(Expression->GetClass());
		}

		if (UMaterialExpressionCollectionParameter* ExpressionCollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression))
		{
			UMaterialParameterCollection* Collection = ExpressionCollectionParameter->Collection;
			if (Collection)
			{
				FMaterialParameterCollectionInfo NewInfo;
				NewInfo.ParameterCollection = Collection;
				NewInfo.StateId = Collection->StateId;
				ParameterCollectionInfos.AddUnique(NewInfo);
			}
		}
		else if (UMaterialExpressionCollectionTransform* ExpressionCollectionTransform = Cast<UMaterialExpressionCollectionTransform>(Expression))
		{
			UMaterialParameterCollection* Collection = ExpressionCollectionTransform->Collection;
			if (Collection)
			{
				FMaterialParameterCollectionInfo NewInfo;
				NewInfo.ParameterCollection = Collection;
				NewInfo.StateId = Collection->StateId;
				ParameterCollectionInfos.AddUnique(NewInfo);
			}
		}
		else if (UMaterialExpressionDynamicParameter* ExpressionDynamicParameter = Cast< UMaterialExpressionDynamicParameter>(Expression))
		{
			DynamicParameterNames.Empty(ExpressionDynamicParameter->ParamNames.Num());
			for (const FString& Name : ExpressionDynamicParameter->ParamNames)
			{
				DynamicParameterNames.Add(*Name);
			}
		}
		else if (UMaterialExpressionLandscapeGrassOutput* ExpressionGrassOutput = Cast<UMaterialExpressionLandscapeGrassOutput>(Expression))
		{
			for (const auto& Type : ExpressionGrassOutput->GrassTypes)
			{
				GrassTypes.AddUnique(Type.GrassType);
			}
		}
		else if (UMaterialExpressionQualitySwitch* QualitySwitchNode = Cast<UMaterialExpressionQualitySwitch>(Expression))
		{
			const FExpressionInput DefaultInput = QualitySwitchNode->Default.GetTracedInput();

			for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
			{
				if (QualitySwitchNode->Inputs[InputIndex].IsConnected())
				{
					// We can ignore quality levels that are defined the same way as 'Default'
					// This avoids compiling a separate explicit quality level resource, that will end up exactly the same as the default resource
					const FExpressionInput Input = QualitySwitchNode->Inputs[InputIndex].GetTracedInput();
					if (Input.Expression != DefaultInput.Expression ||
						Input.OutputIndex != DefaultInput.OutputIndex)
					{
						QualityLevelsUsed[InputIndex] = true;
					}
				}
			}
		}
		else if (Expression->IsA(UMaterialExpressionRuntimeVirtualTextureOutput::StaticClass()))
		{
			bHasRuntimeVirtualTextureOutput = true;
		}
		else if (Expression->IsA(UMaterialExpressionFirstPersonOutput::StaticClass()))
		{
			bHasFirstPersonOutput = true;
		}
		else if (Expression->IsA(UMaterialExpressionMaterialCache::StaticClass()))
		{
			bHasMaterialCacheOutput = true;
		}
		else if (Expression->IsA(UMaterialExpressionSceneColor::StaticClass()))
		{
			bHasSceneColor = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceRandom::StaticClass()))
		{
			bHasPerInstanceRandom = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData::StaticClass()))
		{
			bHasPerInstanceCustomData = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData3Vector::StaticClass()))
		{
			bHasPerInstanceCustomData = true;
		}
		else if (Expression->IsA(UMaterialExpressionVertexInterpolator::StaticClass()))
		{
			bHasVertexInterpolator = true;
		}
		else if (Expression->IsA(UMaterialExpressionMeshPaintTextureObject::StaticClass()))
		{
			bHasMeshPaintTexture = true;
		}
		else if (Expression->IsA(UMaterialExpressionWorldPosition::StaticClass()))
		{
			bHasWorldPosition = true;
		}
		else if (Expression->IsA(UMaterialExpressionActorPositionWS::StaticClass()))
		{
			bHasWorldPosition = true;
		}
		else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
		{
			checkf(Association == GlobalParameter, TEXT("UMaterialExpressionMaterialAttributeLayers can't be nested"));
			// Only a single layers expression is allowed/expected...creating additional layer expression will cause a compile error
			if (!bHasMaterialLayers)
			{
				const FMaterialLayersFunctions& Layers = Context.LayerOverrides ? *Context.LayerOverrides : LayersExpression->DefaultLayers;
				UpdateForLayerFunctions(Context, Layers);

				// TODO(?) - Layers for MIs are currently duplicated here and in FStaticParameterSet
				bHasMaterialLayers = true;
				MaterialLayers = Layers.GetRuntime();
				EditorOnlyData->MaterialLayers = Layers.EditorOnly;
				FMaterialLayersFunctions::Validate(MaterialLayers, EditorOnlyData->MaterialLayers);
				LayersExpression->RebuildLayerGraph(false);
			}
		}
		else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (Context.bUpdateFunctionExpressions)
			{
				UpdateForFunction(Context, FunctionCall->MaterialFunction, GlobalParameter, -1);

				// Update the function call node, so it can relink inputs and outputs as needed
				// Update even if MaterialFunctionNode->MaterialFunction is NULL, because we need to remove the invalid inputs in that case
				FunctionCall->UpdateFromFunctionResource();
			}
		}
		else if (UMaterialExpressionSetMaterialAttributes* SetMatAttributes = Cast<UMaterialExpressionSetMaterialAttributes>(Expression))
		{
			for (int32 PinIndex = 0; PinIndex < SetMatAttributes->AttributeSetTypes.Num(); ++PinIndex)
			{
				// For this material attribute pin do we have something connected?
				const FGuid& Guid = SetMatAttributes->AttributeSetTypes[PinIndex];
				const FExpressionInput& AttributeInput = SetMatAttributes->Inputs[PinIndex + 1];
				const EMaterialProperty MaterialProperty = FMaterialAttributeDefinitionMap::GetProperty(Guid);
				if (AttributeInput.Expression)
				{
					SetPropertyConnected(MaterialProperty);
					if (Guid == FirstPersonInterpolationAlphaGuid)
					{
						bHasFirstPersonOutput = true;
					}
				}
			}
		}
		else if (UMaterialExpressionMakeMaterialAttributes* MakeMatAttributes = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
		{
			auto SetMatAttributeConditionally = [&](EMaterialProperty InMaterialProperty, bool InIsConnected)
			{
				if (InIsConnected)
				{
					SetPropertyConnected(InMaterialProperty);
				}
			};

			SetMatAttributeConditionally(EMaterialProperty::MP_BaseColor, MakeMatAttributes->BaseColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Metallic, MakeMatAttributes->Metallic.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Specular, MakeMatAttributes->Specular.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Roughness, MakeMatAttributes->Roughness.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Anisotropy, MakeMatAttributes->Anisotropy.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_EmissiveColor, MakeMatAttributes->EmissiveColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Opacity, MakeMatAttributes->Opacity.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_OpacityMask, MakeMatAttributes->OpacityMask.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Normal, MakeMatAttributes->Normal.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Tangent, MakeMatAttributes->Tangent.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_WorldPositionOffset, MakeMatAttributes->WorldPositionOffset.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_SubsurfaceColor, MakeMatAttributes->SubsurfaceColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomData0, MakeMatAttributes->ClearCoat.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomData1, MakeMatAttributes->ClearCoatRoughness.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_AmbientOcclusion, MakeMatAttributes->AmbientOcclusion.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Refraction, MakeMatAttributes->Refraction.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs0, MakeMatAttributes->CustomizedUVs[0].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs1, MakeMatAttributes->CustomizedUVs[1].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs2, MakeMatAttributes->CustomizedUVs[2].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs3, MakeMatAttributes->CustomizedUVs[3].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs4, MakeMatAttributes->CustomizedUVs[4].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs5, MakeMatAttributes->CustomizedUVs[5].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs6, MakeMatAttributes->CustomizedUVs[6].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs7, MakeMatAttributes->CustomizedUVs[7].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_PixelDepthOffset, MakeMatAttributes->PixelDepthOffset.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_ShadingModel, MakeMatAttributes->ShadingModel.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Displacement, MakeMatAttributes->Displacement.IsConnected());
		}
	}

	if (bHasMaterialCacheOutput)
	{
		for (UMaterialExpression* MaterialExpression : Expressions)
		{
			if (MaterialExpression && MaterialCacheIsExpressionNonUVDerived(MaterialExpression))
			{
				bMaterialCacheHasNonUVDerivedExpression = true;
				break;
			}
		}
	}
}

void FMaterialCachedExpressionData::AnalyzeMaterial(UMaterial& Material)
{
	if (!Material.bUseMaterialAttributes)
	{
		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
			const FExpressionInput* Input = Material.GetExpressionInputForProperty(Property);
			if (Input && Input->IsConnected())
			{
				SetPropertyConnected(Property);
			}
		}
	}
		
	FMaterialCachedExpressionContext Context;
	UpdateForExpressions(Context, Material.GetExpressions(), EMaterialParameterAssociation::GlobalParameter, -1);
}

void FMaterialCachedExpressionData::Validate(const UMaterialInterface& Material)
{
	if (EditorOnlyData)
	{
		for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
		{
			const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[TypeIndex];
			const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry((EMaterialParameterType)TypeIndex);
			check(EditorEntry.EditorInfo.Num() == Entry.ParameterInfoSet.Num());
		}
		FMaterialLayersFunctions::Validate(MaterialLayers, EditorOnlyData->MaterialLayers);

		if (!FPlatformProperties::RequiresCookedData() && AllowShaderCompiling())
		{
			// Mute log errors created by GetShaderSourceFilePath during include path validation
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogShaders, ELogVerbosity::Fatal);

			for (auto PathIt = EditorOnlyData->ExpressionIncludeFilePaths.CreateIterator(); PathIt; ++PathIt)
			{
				const FString& IncludeFilePath = *PathIt;
				bool bValidExpressionIncludePath = false;

				if (!IncludeFilePath.IsEmpty())
				{
					FString ValidatedPath = GetShaderSourceFilePath(IncludeFilePath);
					if (!ValidatedPath.IsEmpty())
					{
						ValidatedPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ValidatedPath);
						if (FPaths::FileExists(ValidatedPath))
						{
							bValidExpressionIncludePath = true;
						}
					}
				}

				if (!bValidExpressionIncludePath)
				{
					UE_LOG(LogMaterial, Warning, TEXT("Expression include file path '%s' is invalid, removing from cached data for material '%s'."), *IncludeFilePath, *Material.GetPathName());
					PathIt.RemoveCurrent();
				}
			}
		}

		// Sort to make hashing less dependent on the order of expression visiting
		EditorOnlyData->ExpressionIncludeFilePaths.Sort(TLess<>());
	}
}

#endif // WITH_EDITOR

int32 FMaterialCachedExpressionData::FindParameterIndex(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const FSetElementId ElementId = Entry.ParameterInfoSet.FindId(FMaterialParameterInfo(ParameterInfo));
	return ElementId.AsInteger();
}

void FMaterialCachedExpressionData::GetParameterValueByIndex(EMaterialParameterType Type, int32 ParameterIndex, FMaterialParameterMetadata& OutResult) const
{
#if WITH_EDITORONLY_DATA
	bool bIsEditorOnlyDataStripped = true;
	if (EditorOnlyData)
	{
		const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[(int32)Type];
		bIsEditorOnlyDataStripped = EditorEntry.EditorInfo.Num() == 0;
		if (!bIsEditorOnlyDataStripped)
		{
			const FMaterialCachedParameterEditorInfo& EditorInfo = EditorEntry.EditorInfo[ParameterIndex];
			OutResult.ExpressionGuid = EditorInfo.ExpressionGuid;
			OutResult.Description = EditorInfo.Description;
			OutResult.Group = EditorInfo.Group;
			OutResult.SortPriority = EditorInfo.SortPriority;
			if (EditorInfo.AssetIndex != INDEX_NONE)
			{
				OutResult.AssetPath = EditorOnlyData->AssetPaths[EditorInfo.AssetIndex];
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	switch (Type)
	{
	case EMaterialParameterType::Scalar:
		OutResult.Value = ScalarValues[ParameterIndex];
		OutResult.PrimitiveDataIndex = ScalarPrimitiveDataIndexValues[ParameterIndex];
#if WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ScalarMin = EditorOnlyData->ScalarMinMaxValues[ParameterIndex].X;
			OutResult.ScalarMax = EditorOnlyData->ScalarMinMaxValues[ParameterIndex].Y;
			{
				const TSoftObjectPtr<UCurveLinearColor>& Curve = EditorOnlyData->ScalarCurveValues[ParameterIndex];
				const TSoftObjectPtr<UCurveLinearColorAtlas>& Atlas = EditorOnlyData->ScalarCurveAtlasValues[ParameterIndex];
				if (!Curve.IsNull() && !Atlas.IsNull())
				{
					OutResult.ScalarCurve = Curve;
					OutResult.ScalarAtlas = Atlas;
					OutResult.bUsedAsAtlasPosition = true;
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::Vector:
		OutResult.Value = VectorValues[ParameterIndex];
		OutResult.PrimitiveDataIndex = VectorPrimitiveDataIndexValues[ParameterIndex];
#if  WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ChannelNames = EditorOnlyData->VectorChannelNameValues[ParameterIndex];
			OutResult.bUsedAsChannelMask = EditorOnlyData->VectorUsedAsChannelMaskValues[ParameterIndex];
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::DoubleVector:
		OutResult.Value = DoubleVectorValues[ParameterIndex];
		break;
	case EMaterialParameterType::Texture:
		OutResult.Value = TextureValues[ParameterIndex].LoadSynchronous();
#if WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ChannelNames = EditorOnlyData->TextureChannelNameValues[ParameterIndex];
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::TextureCollection:
		OutResult.Value = TextureCollectionValues[ParameterIndex].LoadSynchronous();
		break;
	case EMaterialParameterType::RuntimeVirtualTexture:
		OutResult.Value = RuntimeVirtualTextureValues[ParameterIndex].LoadSynchronous();
		break;
	case EMaterialParameterType::SparseVolumeTexture:
		OutResult.Value = SparseVolumeTextureValues[ParameterIndex].LoadSynchronous();
		break;
	case EMaterialParameterType::Font:
		OutResult.Value = FMaterialParameterValue(FontValues[ParameterIndex].LoadSynchronous(), FontPageValues[ParameterIndex]);
		break;
	case EMaterialParameterType::StaticSwitch:
		OutResult.Value = StaticSwitchValues[ParameterIndex];
		OutResult.bDynamicSwitchParameter = DynamicSwitchValues[ParameterIndex];
		break;
#if WITH_EDITORONLY_DATA
	case EMaterialParameterType::StaticComponentMask:
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.Value = EditorOnlyData->StaticComponentMaskValues[ParameterIndex];
		}
		break;
#endif // WITH_EDITORONLY_DATA
	default:
		checkNoEntry();
		break;
	}
}

bool FMaterialCachedExpressionData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	return false;
}

void FMaterialCachedExpressionData::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::IncreaseMaterialAttributesInputMask)
		{
			PropertyConnectedMask = uint64(PropertyConnectedBitmask_DEPRECATED);
		}
	}

#if WITH_EDITORONLY_DATA
	if(Ar.IsLoading())
	{
		bool bIsEditorOnlyDataStripped = true;
		if (EditorOnlyData)
		{
			const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[(int32)EMaterialParameterType::StaticSwitch];
			bIsEditorOnlyDataStripped = EditorEntry.EditorInfo.Num() == 0;
		}

		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			StaticSwitchValues = EditorOnlyData->StaticSwitchValues_DEPRECATED;
			check(DynamicSwitchValues.Num() == 0);
			DynamicSwitchValues.AddDefaulted(StaticSwitchValues.Num());
		}
	}
#endif
}

bool FMaterialCachedExpressionData::GetParameterValue(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo, FMaterialParameterMetadata& OutResult) const
{
	const int32 Index = FindParameterIndex(Type, ParameterInfo);
	if (Index != INDEX_NONE)
	{
		GetParameterValueByIndex(Type, Index, OutResult);
		return true;
	}

	return false;
}

const FGuid& FMaterialCachedExpressionData::GetExpressionGuid(EMaterialParameterType Type, int32 Index) const
{
#if WITH_EDITORONLY_DATA
	if (EditorOnlyData)
	{
		// cooked materials can strip out expression guids
		if (EditorOnlyData->EditorEntries[(int32)Type].EditorInfo.Num() != 0)
		{
			return EditorOnlyData->EditorEntries[(int32)Type].EditorInfo[Index].ExpressionGuid;
		}
	}
#endif // WITH_EDITORONLY_DATA
	static const FGuid EmptyGuid;
	return EmptyGuid;
}

void FMaterialCachedExpressionData::GetAllParametersOfType(EMaterialParameterType Type, TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& OutParameters) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameters.Reserve(OutParameters.Num() + NumParameters);

	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		const FMaterialParameterInfo& ParameterInfo = Entry.ParameterInfoSet[FSetElementId::FromInteger(ParameterIndex)];
		FMaterialParameterMetadata& Result = OutParameters.Emplace(ParameterInfo);
		GetParameterValueByIndex(Type, ParameterIndex, Result);
	}
}

void FMaterialCachedExpressionData::GetAllParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameterInfo.Reserve(OutParameterInfo.Num() + NumParameters);
	OutParameterIds.Reserve(OutParameterIds.Num() + NumParameters);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const int32 ParameterIndex = It.GetId().AsInteger();
		OutParameterInfo.Add(*It);
		OutParameterIds.Add(GetExpressionGuid(Type, ParameterIndex));
	}
}

void FMaterialCachedExpressionData::GetAllGlobalParametersOfType(EMaterialParameterType Type, TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& OutParameters) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameters.Reserve(OutParameters.Num() + NumParameters);

	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		const FMaterialParameterInfo& ParameterInfo = Entry.ParameterInfoSet[FSetElementId::FromInteger(ParameterIndex)];
		if (ParameterInfo.Association == GlobalParameter)
		{
			FMaterialParameterMetadata& Meta = OutParameters.FindOrAdd(ParameterInfo);
			if (Meta.Value.Type == EMaterialParameterType::None)
			{
				GetParameterValueByIndex(Type, ParameterIndex, Meta);
			}
		}
	}
}

void FMaterialCachedExpressionData::GetAllGlobalParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameterInfo.Reserve(OutParameterInfo.Num() + NumParameters);
	OutParameterIds.Reserve(OutParameterIds.Num() + NumParameters);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const FMaterialParameterInfo& ParameterInfo = *It;
		if (ParameterInfo.Association == GlobalParameter)
		{
			const int32 ParameterIndex = It.GetId().AsInteger();
			OutParameterInfo.Add(*It);
			OutParameterIds.Add(GetExpressionGuid(Type, ParameterIndex));
		}
	}
}

#undef LOCTEXT_NAMESPACE
