// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/USDImportNode_v2.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/RenderMeshImport.h"
#include "ChaosClothAsset/USDImportNode.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UnrealUSDWrapper.h"
#include "USDAssetUserData.h"
#include "USDConversionUtils.h"
#include "USDProjectSettings.h"
#include "USDStageImportContext.h"
#include "USDStageImporter.h"
#include "USDStageImportOptions.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdGeomSubset.h"
#include "UsdWrappers/UsdRelationship.h"
#include "UsdWrappers/VtValue.h"
#include "AssetViewUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(USDImportNode_v2)

#define LOCTEXT_NAMESPACE "ChaosClothAssetUSDImportNode_v2"

namespace UE::Chaos::ClothAsset::Private
{
	// User attribute names
	static const FName OriginalIndicesName(TEXT("OriginalIndices"));

	// Cloth USD API names
	static const FName ClothRootAPI(TEXT("ClothRootAPI"));
	static const FName RenderPatternAPI(TEXT("RenderPatternAPI"));
	static const FName SimMeshDataAPI(TEXT("SimMeshDataAPI"));
	static const FName SimPatternAPI(TEXT("SimPatternAPI"));
	static const FName SewingAPI(TEXT("SewingAPI"));

	// USD import material overrides
	static TArray<FSoftObjectPath> UsdClothOverrideMaterials_v2 =
	{
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportMaterial.USDImportMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTranslucentMaterial.USDImportTranslucentMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTwoSidedMaterial.USDImportTwoSidedMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTranslucentTwoSidedMaterial.USDImportTranslucentTwoSidedMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportDisplayColorMaterial.USDImportDisplayColorMaterial")),
	};
	
	// Return the specified UObject's dependencies for top level UAssets that are not in the engine folders
	static TArray<UObject*> GetAssetDependencies(const UObject* Asset)
	{
		constexpr bool bInRequireDirectOuter = true;
		constexpr bool bShouldIgnoreArchetype = true;
		constexpr bool bInSerializeRecursively = false;  // Ignored if LimitOuter is nullptr
		constexpr bool bShouldIgnoreTransient = true;
		constexpr UObject* LimitOuter = nullptr;
		TArray<UObject*> References;
		FReferenceFinder ReferenceFinder(References, LimitOuter, bInRequireDirectOuter, bShouldIgnoreArchetype, bInSerializeRecursively, bShouldIgnoreTransient);
		ReferenceFinder.FindReferences(const_cast<UObject*>(Asset));

		TArray<UObject*> Dependencies;
		Dependencies.Reserve(References.Num());
		for (UObject* const Reference : References)
		{
			constexpr bool bEnginePluginIsAlsoEngine = true;  // Only includes non Engine or non Engine plugins assets (e.g. no USD materials)
			if (FAssetData::IsUAsset(Reference) &&
				FAssetData::IsTopLevelAsset(Reference) &&
				!AssetViewUtils::IsEngineFolder(Reference->GetPackage()->GetName(), bEnginePluginIsAlsoEngine))
			{
				Dependencies.Emplace(Reference);
			}
		}
		return Dependencies;
	}

	static void OverrideUsdImportMaterials_v2(const TArray<FSoftObjectPath>& Materials, TArray<FSoftObjectPath>* SavedValues = nullptr)
	{
		if (UUsdProjectSettings* UsdProjectSettings = GetMutableDefault<UUsdProjectSettings>())
		{
			// Check to see if we should save the existing values
			if (SavedValues)
			{
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTranslucentMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTwoSidedMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial);
				SavedValues->Push(UsdProjectSettings->ReferenceDisplayColorMaterial);
			}
			UsdProjectSettings->ReferencePreviewSurfaceMaterial = Materials[0];
			UsdProjectSettings->ReferencePreviewSurfaceTranslucentMaterial = Materials[1];
			UsdProjectSettings->ReferencePreviewSurfaceTwoSidedMaterial = Materials[2];
			UsdProjectSettings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial = Materials[3];
			UsdProjectSettings->ReferenceDisplayColorMaterial = Materials[4];
		}
	}

	static TArray<TObjectPtr<UObject>> ImportStaticMeshesFromUsdStage(const FUsdStage& UsdStage, const FString& UsdFilePath, const FString& PackagePath)
	{
		// Import recognised assets
		FUsdStageImportContext ImportContext;

		const TObjectPtr<UUsdStageImportOptions>& ImportOptions = ImportContext.ImportOptions;
		{
			check(ImportOptions);
			// Data to import
			ImportOptions->bImportActors = false;
			ImportOptions->bImportGeometry = true;
			ImportOptions->bImportSkeletalAnimations = false;
			ImportOptions->bImportLevelSequences = false;
			ImportOptions->bImportMaterials = true;
			ImportOptions->bImportGroomAssets = false;
			ImportOptions->bImportOnlyUsedMaterials = true;
			// Prims to import
			ImportOptions->PrimsToImport = TArray<FString>{ TEXT("/") };
			// USD options
			ImportOptions->PurposesToImport = (int32)(EUsdPurpose::Render | EUsdPurpose::Guide);
			ImportOptions->NaniteTriangleThreshold = TNumericLimits<int32>::Max();  // Don't enable Nanite
			ImportOptions->RenderContextToImport = NAME_None;
			ImportOptions->MaterialPurpose = NAME_None;  // *UnrealIdentifiers::MaterialPreviewPurpose ???
			ImportOptions->RootMotionHandling = EUsdRootMotionHandling::NoAdditionalRootMotion;
			ImportOptions->SubdivisionLevel = 0;
			ImportOptions->bOverrideStageOptions = false;
			ImportOptions->bImportAtSpecificTimeCode = false;
			ImportOptions->ImportTimeCode = 0.f;
			// Groom
			ImportOptions->GroomInterpolationSettings = TArray<FHairGroupsInterpolation>();
			// Collision
			ImportOptions->ExistingActorPolicy = EReplaceActorPolicy::Replace;
			ImportOptions->ExistingAssetPolicy = EReplaceAssetPolicy::Replace;
			// Processing
			ImportOptions->bPrimPathFolderStructure = false;
			ImportOptions->KindsToCollapse = (int32)EUsdDefaultKind::Component;
			ImportOptions->bMergeIdenticalMaterialSlots = true;
			ImportOptions->bInterpretLODs = false;
		}

		constexpr bool bIsAutomated = true;
		constexpr bool bIsReimport = false;
		constexpr bool bAllowActorImport = false;

		ImportContext.Stage = UsdStage;  // Set the stage first to prevent re-opening it in the Init function
		ImportContext.Init(TEXT(""), UsdFilePath, PackagePath, RF_NoFlags, bIsAutomated, bIsReimport, bAllowActorImport);

		TArray<FSoftObjectPath> OriginalUsdMaterials;
		// Override the project settings to point the USD importer to cloth specific parent materials.
		// This is because we want the materials to import into UEFN and the default USD ones
		// use operations that are not allowed.
		OverrideUsdImportMaterials_v2(UsdClothOverrideMaterials_v2, &OriginalUsdMaterials);

		UUsdStageImporter UsdStageImporter;
		UsdStageImporter.ImportFromFile(ImportContext);

		// Restore Original USD Materials
		OverrideUsdImportMaterials_v2(OriginalUsdMaterials);

		return ImportContext.ImportedAssets;
	}

	static FUsdPrim FindClothPrim(const FUsdPrim& RootPrim)
	{
		for (FUsdPrim& ChildPrim : RootPrim.GetChildren())
		{
			if (ChildPrim.HasAPI(ClothRootAPI))
			{
				return ChildPrim;
			}
		}
		return FUsdPrim();
	}

	static bool RemoveMaterialOpacity(const FUsdPrim& Prim)
	{
		bool bHasOpacity = false;
		for (const FUsdPrim& ChildPrim : Prim.GetChildren())
		{
			if (ChildPrim.IsA(TEXT("Material")))
			{
				for (const FUsdPrim& GrandChildPrim : ChildPrim.GetChildren())
				{
					if (GrandChildPrim.IsA(TEXT("Shader")))
					{
						if (const FUsdAttribute OpacityAttr = GrandChildPrim.GetAttribute(TEXT("inputs:opacity")))
						{
							OpacityAttr.ClearConnections();
							OpacityAttr.Clear();
							bHasOpacity = true;
						}
					}
				}
			}
			else
			{
				bHasOpacity = RemoveMaterialOpacity(ChildPrim) || bHasOpacity;
			}
		}
		return bHasOpacity;
	}

	static FUsdPrim FindSimMeshPrim(const FUsdPrim& ClothPrim)
	{
		for (FUsdPrim& ClothChildPrim : ClothPrim.GetChildren())
		{
			if (ClothChildPrim.IsA(TEXT("Mesh")))
			{
				if (ClothChildPrim.HasAPI(SimMeshDataAPI))
				{
					// Check that the sim mesh has at least one valid geomsubset patern
					for (FUsdPrim& SimMeshChildPrim : ClothChildPrim.GetChildren())
					{
						if (SimMeshChildPrim.IsA(TEXT("GeomSubset")) && SimMeshChildPrim.HasAPI(SimPatternAPI))
						{
							return ClothChildPrim;
						}
					}
				}
			}
		}
		return FUsdPrim();
	}

	static FUsdPrim FindRenderMeshPrim(const FUsdPrim& ClothPrim)
	{
		for (FUsdPrim& ClothChildPrim : ClothPrim.GetChildren())
		{
			if (ClothChildPrim.IsA(TEXT("Mesh")))
			{
				// Look for all GeomSubsets to see if this is a suitable render mesh prim
				bool bIsRenderMesh = false;
				for (FUsdPrim& RenderMeshChildPrim : ClothChildPrim.GetChildren())
				{
					if (RenderMeshChildPrim.IsA(TEXT("GeomSubset")) && RenderMeshChildPrim.HasAPI(RenderPatternAPI))
					{
						return ClothChildPrim;
					}
				}
			}
		}
		return FUsdPrim();
	}

	static FVector2f GetSimMeshUVScale(const FUsdPrim& SimMeshPrim)
	{
		FVector2f UVScale(1.f);
		const FUsdAttribute RestPositionScaleAttr = SimMeshPrim.GetAttribute(TEXT("restPositionScale"));
		if (RestPositionScaleAttr.HasValue() && RestPositionScaleAttr.GetTypeName() == TEXT("float2"))
		{
			FVtValue Value;
			RestPositionScaleAttr.Get(Value);
			UsdUtils::FConvertedVtValue ConvertedVtValue;
			if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) &&
				!ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty &&
				ConvertedVtValue.Entries.Num() == 1 && ConvertedVtValue.Entries[0].Num() == 2 && ConvertedVtValue.Entries[0][0].IsType<float>())
			{
				UVScale = FVector2f(
					ConvertedVtValue.Entries[0][0].Get<float>(),
					ConvertedVtValue.Entries[0][1].Get<float>());
			}
		}
		return UVScale;
	}

	static FString GetStringValue(const FUsdAttribute& UsdAttribute)
	{
		if (UsdAttribute.HasValue())
		{
			FVtValue Value;
			UsdAttribute.Get(Value);
			return UsdUtils::Stringify(Value);
		}
		return FString();
	}

	static TArray<int32> GetIntArrayValues(const FUsdAttribute& UsdAttribute)
	{
		using namespace UsdUtils;
		TArray<int32> IntArray;
		if (UsdAttribute.HasValue())
		{
			FVtValue Value;
			UsdAttribute.Get(Value);
			FConvertedVtValue ConvertedVtValue;
			if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
			{
				IntArray.Reserve(ConvertedVtValue.Entries.Num());
				for (const FConvertedVtValueEntry& ValueEntry : ConvertedVtValue.Entries)
				{
					IntArray.Emplace(ValueEntry[0].Get<int32>());
				}
			}
		}
		return MoveTemp(IntArray);
	}

	static bool CheckSimMeshPrimTriangles(const FUsdPrim& SimMeshPrim, FText& OutErrorText)
	{
		const FUsdAttribute FaceVertexCountsAttr = SimMeshPrim.GetAttribute(TEXT("faceVertexCounts"));
		if (!FaceVertexCountsAttr)
		{
			OutErrorText = LOCTEXT("MissingSimMeshFaceCountAttribute", "Missing simulation mesh faceVertexCounts attribute.");
		}
		else if (FaceVertexCountsAttr.GetTypeName() != TEXT("int[]"))
		{
			OutErrorText = LOCTEXT("WrongSimMeshFaceCountTypeName", "Wrong simulation mesh faceVertexCounts type name. Needs to be 'int[]'.");
		}
		else
		{
			bool bIsTriangleMesh = true;
			const TArray<int32> FaceVertexCounts = GetIntArrayValues(FaceVertexCountsAttr);
			for (int32 FaceVertexCount : FaceVertexCounts)
			{
				if (FaceVertexCount != 3)
				{
					OutErrorText = LOCTEXT("WrongSimMeshFaceCount", "Wrong simulation mesh face vertex count. The simulation mesh only supports '3' for triangles.");
					bIsTriangleMesh = false;
					break;
				}
			}
			return bIsTriangleMesh;
		}
		return false;
	}

	static bool ImportPatternsFromMeshPrim(const FUsdPrim& MeshPrim, const FName PatternAPI, TMap<FName, TSet<int32>>& Patterns, FText& OutErrorText)
	{
		Patterns.Reset();
		for (const FUsdPrim& MeshChildPrim : MeshPrim.GetChildren())
		{
			if (MeshChildPrim.IsA(TEXT("GeomSubset")) && MeshChildPrim.HasAPI(PatternAPI))
			{
				const FUsdGeomSubset GeomSubset(MeshChildPrim);

				// Read FamillyName
				const FUsdAttribute FamilyNameAttr = GeomSubset.GetFamilyNameAttr();
				if (GetStringValue(FamilyNameAttr) != TEXT("pattern"))
				{
					OutErrorText = FText::Format(LOCTEXT("WrongPatternGeomSubsetFamilyName", "Wrong pattern family name for GeomSubset '{0}'. Needs to be 'pattern'."), FText::FromString(MeshPrim.GetPrimPath().GetString()));
					return false;
				}

				// Read ElementTypeAttr
				const FUsdAttribute ElementTypeAttr = GeomSubset.GetElementTypeAttr();
				if (GetStringValue(ElementTypeAttr) != TEXT("face"))
				{
					OutErrorText = FText::Format(LOCTEXT("WrongPatternGeomSubsetType", "Wrong pattern type for GeomSubset '{0}'. Needs to be 'face'."), FText::FromString(MeshPrim.GetPrimPath().GetString()));
					return false;
				}

				// Read indices 
				const FUsdAttribute IndicesAttr = GeomSubset.GetIndicesAttr();
				if (IndicesAttr.GetTypeName() != TEXT("int[]"))
				{
					OutErrorText = FText::Format(LOCTEXT("WrongPatternGeomSubsetIndexType", "Wrong pattern index type for GeomSubset '{0}'. Needs to be 'int[]'."), FText::FromString(MeshPrim.GetPrimPath().GetString()));
					return false;
				}

				if (Patterns.Contains(MeshChildPrim.GetName()))
				{
					OutErrorText = FText::Format(LOCTEXT("DuplicatePatternGeomSubsetName", "Duplicate pattern name for GeomSubset '{0}'. The name needs to be unique."), FText::FromString(MeshPrim.GetPrimPath().GetString()));
					return false;
				}

				Patterns.Emplace(MeshChildPrim.GetName(), GetIntArrayValues(IndicesAttr));
			}
		}
		return true;
	}

	static bool ImportPatternsFromRenderMeshPrim(const FUsdPrim& RenderMeshPrim, const FUsdPrim& SimMeshPrim, TMap<FName, TSet<int32>>& Patterns, TMap<FName, TSet<FName>>& RenderToSimPatterns, FText& OutErrorText)
	{
		RenderToSimPatterns.Reset();

		if (ImportPatternsFromMeshPrim(RenderMeshPrim, RenderPatternAPI, Patterns, OutErrorText))
		{
			const FSdfPath SimMeshPath = SimMeshPrim.GetPrimPath();

			for (const FUsdPrim& MeshChildPrim : RenderMeshPrim.GetChildren())
			{
				if (MeshChildPrim.IsA(TEXT("GeomSubset")) && MeshChildPrim.HasAPI(RenderPatternAPI))
				{
					// Read simPattern relationship
					const FUsdRelationship Relationship = MeshChildPrim.GetRelationship(TEXT("simPattern"));
					TArray<FSdfPath> Targets;
					Relationship.GetTargets(Targets);

					// Add a new set of sim mesh patterns for this render pattern
					TSet<FName>& SimMeshPatterns = RenderToSimPatterns.Add(MeshChildPrim.GetName());

					// Add all sim mesh targets
					for (const FSdfPath& Target : Targets)
					{
						if (Target.GetParentPath() == SimMeshPath)
						{
							SimMeshPatterns.Emplace(Target.GetName());
						}
						else
						{
							OutErrorText = FText::Format(LOCTEXT("UnknownOrMultipleSimMesh", "Unknown or more than one simulation mesh found while getting simPattern relationship of render pattern '{0}'."), FText::FromString(MeshChildPrim.GetPrimPath().GetString()));
							return false;
						}
					}
				}
			}
			return true;
		}
		return false;
	}

	static bool ImportPatternsFromSimMeshPrim(const FUsdPrim& SimMeshPrim, TMap<FName, TSet<int32>>& Patterns, FText& OutErrorText)
	{
		return ImportPatternsFromMeshPrim(SimMeshPrim, SimPatternAPI, Patterns, OutErrorText);
	}

	static bool ImportSewingsFromSimMeshPrim(const FUsdPrim& SimMeshPrim, TMap<FName, TSet<FIntVector2>>& Sewings, FText& OutErrorText)
	{
		Sewings.Reset();
		for (const FUsdPrim& SimMeshChildPrim : SimMeshPrim.GetChildren())
		{
			if (SimMeshChildPrim.IsA(TEXT("GeomSubset")) && SimMeshChildPrim.HasAPI(SewingAPI))
			{
				const FUsdGeomSubset GeomSubset(SimMeshChildPrim);

				// Read FamilyName
				const FUsdAttribute FamilyNameAttr = GeomSubset.GetFamilyNameAttr();
				if (GetStringValue(FamilyNameAttr) != TEXT("sewing"))
				{
					OutErrorText = LOCTEXT("WrongSewingGeomSubsetFamilyName", "Wrong sewing GeomSubset family name. Needs to be 'pattern'.");
					return false;
				}

				// Read ElementTypeAttr
				const FUsdAttribute ElementTypeAttr = GeomSubset.GetElementTypeAttr();
				if (GetStringValue(ElementTypeAttr) != TEXT("edge"))
				{
					OutErrorText = LOCTEXT("WrongSewingGeomSubsetType", "Wrong sewing GeomSubset type. Needs to be edge.");
					return false;
				}

				// Read indices 
				const FUsdAttribute IndicesAttr = GeomSubset.GetIndicesAttr();
				if (IndicesAttr.GetTypeName() != TEXT("int[]"))
				{
					OutErrorText = LOCTEXT("WrongSewingGeomSubsetIndexType", "Wrong sewing GeomSubset index type. Needs to be int[].");
					return false;
				}

				if (Sewings.Contains(SimMeshChildPrim.GetName()))
				{
					OutErrorText = LOCTEXT("DuplicateSewingGeomSubsetName", "Duplicate sewing GeomSubset name. The name needs to be unique.");
					return false;
				}

				const TArray<int32> IntArrayValues = GetIntArrayValues(IndicesAttr);
				const int32 NumStitches = IntArrayValues.Num() / 2;
				if (NumStitches * 2 != IntArrayValues.Num())
				{
					OutErrorText = LOCTEXT("OddSewingGeomSubsetIndices", "Odd number of indices for the sewing edges.");
					return false;
				}

				TSet<FIntVector2>& Stitches = Sewings.Emplace(SimMeshChildPrim.GetName());
				Stitches.Reserve(NumStitches);
				for (int32 Index = 0; Index < NumStitches; ++Index)
				{
					const int32 Index0 = IntArrayValues[Index * 2];
					const int32 Index1 = IntArrayValues[Index * 2 + 1];
					Stitches.Emplace(Index0 <= Index1 ? FIntVector2(Index0, Index1) : FIntVector2(Index1, Index0));
				}
			}
		}
		return true;
	}
}  // End namespace Private

FChaosClothAssetUSDImportNode_v2::FChaosClothAssetUSDImportNode_v2(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
	, UsdFile(
		FDataflowFunctionProperty::FDelegate::CreateLambda([this, OwningObject = InParam.OwningObject](UE::Dataflow::FContext& /*Context*/)
			{
				const FString AssetPath = OwningObject ? OwningObject->GetPackage()->GetPathName() : FString();
				FText ErrorText;
				if (!ImportUsdFile(UsdFile.FilePath, AssetPath, ErrorText))
				{
					UE::Chaos::ClothAsset::FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidUSDClothSchemaHeadline", "Invalid USD Cloth Schema."),
						FText::Format(LOCTEXT("InvalidUSDClothSchemaDetails", "Error while importing USD cloth from file '{0}':\n{1}\n\nWill now fallback to the legacy schema-less USD import."), FText::FromString(UsdFile.FilePath), ErrorText));

					if(!ImportUsdFile_Schemaless(UsdFile.FilePath, AssetPath, ErrorText))
					{
						UE::Chaos::ClothAsset::FClothDataflowTools::LogAndToastWarning(*this,
							LOCTEXT("FailedToImportUsdFileHeadline", "Failed to import USD file from file."),
							FText::Format(LOCTEXT("FailedToImportUsdDetails", "Error while importing USD cloth from file '{0}':\n{1}"), FText::FromString(UsdFile.FilePath), ErrorText));
					}
				}
			}))
	, ReimportUsdFile(
		FDataflowFunctionProperty::FDelegate::CreateLambda([this](UE::Dataflow::FContext& Context)
			{
				UsdFile.Execute(Context);
			}))
	, ReloadSimStaticMesh(
		FDataflowFunctionProperty::FDelegate::CreateLambda([this](UE::Dataflow::FContext& /*Context*/)
			{
				const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
				FText ErrorText;
				if (!ImportSimStaticMesh(ClothCollection, ErrorText))
				{
					UE::Chaos::ClothAsset::FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("FailedToImportSimMeshHeadline", "Failed to reload the simulation static mesh."),
						FText::Format(LOCTEXT("FailedToImportSimMeshDetails", "Error while re-importing the simulation mesh from static mesh '{0}':\n{1}"), FText::FromString(ImportedSimStaticMesh->GetName()), ErrorText));
				}
				Collection = MoveTemp(*ClothCollection);
			}))
	, ReloadRenderStaticMesh(
		FDataflowFunctionProperty::FDelegate::CreateLambda([this](UE::Dataflow::FContext& /*Context*/)
			{
				const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
				FText ErrorText;
				if (!ImportRenderStaticMesh(ClothCollection, ErrorText))
				{
					UE::Chaos::ClothAsset::FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("FailedToImportRenderMeshHeadline", "Failed to reload the render static mesh."),
						FText::Format(LOCTEXT("FailedToImportRenderMeshDetails", "Error while re-importing the render mesh from static mesh '{0}':\n{1}"), FText::FromString(ImportedRenderStaticMesh->GetName()), ErrorText));
				}
				Collection = MoveTemp(*ClothCollection);
			}))
{
	using namespace UE::Chaos::ClothAsset;

	// Initialize to a valid collection
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
	FCollectionClothFacade(ClothCollection).DefineSchema();
	Collection = MoveTemp(*ClothCollection);

	// Register connections
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetUSDImportNode_v2::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		SetValue(Context, Collection, &Collection);
	}
}

void FChaosClothAssetUSDImportNode_v2::Serialize(FArchive& Ar)
{
	using namespace UE::Chaos::ClothAsset;

	if (Ar.IsLoading() && !Ar.IsTransacting())
	{
		// Make sure to always have a valid cloth collection on reload, some new attributes could be missing from the cached collection
		// Must be executed before ImportRenderStaticMesh below, and after serializing the collection above, and even if the serialized version hasn't changed
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (!ClothFacade.IsValid())
		{
			ClothFacade.DefineSchema();
		}
		Collection = MoveTemp(*ClothCollection);

		// Regenerate correct dependencies if needed
		if (!ImportedAssets_DEPRECATED.IsEmpty())
		{
			ImportedAssets_DEPRECATED.Empty();
			ImportedSimAssets = GetImportedAssetDependencies(ImportedSimStaticMesh);
			ImportedRenderAssets = GetImportedAssetDependencies(ImportedRenderStaticMesh);
		}
	}
}

void FChaosClothAssetUSDImportNode_v2::ResetImport()
{
	Collection.Reset();
	PackagePath = FString();
	ImportedRenderStaticMesh = nullptr;
	ImportedSimStaticMesh = nullptr;
	ImportedUVScale = { 1.f, 1.f };
	ImportedRenderAssets.Reset();
	ImportedSimAssets.Reset();
}

// V1 of the USD importer (schemaless)
bool FChaosClothAssetUSDImportNode_v2::ImportUsdFile_Schemaless(const FString& UsdFilePath, const FString& AssetPath, FText& OutErrorText)
{
	using namespace UE::Chaos::ClothAsset;

	ResetImport();

	// Temporary borrow the collection to make the shared ref
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
	ON_SCOPE_EXIT{ Collection = MoveTemp(*ClothCollection); };

	const float NumSteps = bImportRenderMesh ? 2.f : 1.f;  // Sim mesh is always imported
	FScopedSlowTask SlowTask(NumSteps, LOCTEXT("ImportingUSDFile", "Importing USD file..."));

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("CreatingAssets", "Creating assets and importing simulation mesh..."));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FChaosClothAssetUSDImportNode::ImportFromFile(UsdFilePath, AssetPath, bImportSimMesh, ClothCollection, PackagePath, OutErrorText);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static const FString SchemalessSimStaticMeshName = TEXT("");
	static const FString SchemalessRenderStaticMeshName = TEXT("SM_Mesh");
	UpdateImportedAssets(SchemalessSimStaticMeshName, SchemalessRenderStaticMeshName);

	// Add the render mesh to the collection, since it wasn't originally cached in the collection in the first importer
	if (bImportRenderMesh)
	{
		SlowTask.EnterProgressFrame(1.f, LOCTEXT("ImportingRenderMesh", "Importing render mesh..."));
		if (!ImportRenderStaticMesh(ClothCollection, OutErrorText))
		{
			return false;
		}
	}

	return true;
}

// V2 of the USD importer (using cloth schema)
bool FChaosClothAssetUSDImportNode_v2::ImportUsdFile(const FString& UsdFilePath, const FString& AssetPath, FText& OutErrorText)
{
	using namespace UE;
	using namespace UE::Chaos::ClothAsset;
	using namespace UE::Chaos::ClothAsset::Private;

	ResetImport();

#if USE_USD_SDK
	// Temporary borrow the collection to make the shared ref
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
	ON_SCOPE_EXIT{ Collection = MoveTemp(*ClothCollection); };

	FCollectionClothFacade ClothFacade(ClothCollection);
	ClothFacade.DefineSchema();

	// Empty file
	if (UsdFilePath.IsEmpty())
	{
		return true;
	}

	// Start slow task
	const float NumSteps = 1.f + (bImportSimMesh ? bImportRenderMesh ? 2.f : 1.f : bImportRenderMesh ? 1.f : 0.f);
	FScopedSlowTask SlowTask(NumSteps, LOCTEXT("ImportingUSDFile", "Importing USD file..."));
	SlowTask.MakeDialogDelayed(1.f);

	// Open stage
	constexpr bool bUseStageCache = false;  // Reload from disk, not from cache
	constexpr EUsdInitialLoadSet UsdInitialLoadSet = EUsdInitialLoadSet::LoadAll;  // TODO: Ideally we should only use LoadNone to start with and load what's needed once the Schema is defined

	FUsdStage UsdStage = UnrealUSDWrapper::OpenStage(*UsdFilePath, UsdInitialLoadSet, bUseStageCache);
	if (!UsdStage)
	{
		OutErrorText = LOCTEXT("CantCreateNewStage", "Failed to open the specified USD file.");
		return false;
	}

	// Find the cloth prim
	const FUsdPrim ClothPrim = FindClothPrim(UsdStage.GetPseudoRoot());
	if (!ClothPrim)
	{
		OutErrorText = LOCTEXT("CantFindClothRootAPI", "Can't find a cloth root inside the specified USD file.");
		return false;
	}

	// Find SimMesh and Render Mesh prims
	const FUsdPrim SimMeshPrim = FindSimMeshPrim(ClothPrim);
	const FUsdPrim RenderMeshPrim = FindRenderMeshPrim(ClothPrim);
	if (!SimMeshPrim && !RenderMeshPrim)
	{
		OutErrorText = LOCTEXT("CantFindMeshPrims", "Can't find a sim mesh or render mesh prim with valid pattern data.");
		return false;
	}

	// Remove Opacity from the stage before import since otherwise it messes up all materials
	if (!bImportWithOpacity)
	{
		RemoveMaterialOpacity(UsdStage.GetPseudoRoot());
	}

	// Read UVScale attribute
	ImportedUVScale = GetSimMeshUVScale(SimMeshPrim);

	// Update import location
	const uint32 UsdPathHash = GetTypeHash(UsdFile.FilePath);  // Path hash to store all import from the same file/same path to the same content folder
	const FString UsdFileName = SlugStringForValidName(FPaths::GetBaseFilename(UsdFile.FilePath));
	const FString PackageName = FString::Printf(TEXT("%s_%08X"), *UsdFileName, UsdPathHash);
	PackagePath = FPaths::Combine(AssetPath + TEXT("_Import"), PackageName);

	// Import the stage
	SlowTask.EnterProgressFrame(1.f);
	TArray<TObjectPtr<UObject>> ImportedAssets = ImportStaticMeshesFromUsdStage(UsdStage, UsdFilePath, PackagePath);

	// Find the imported static meshes in the imported USD assets
	ImportedSimStaticMesh = FindImportedStaticMesh(ImportedAssets, SimMeshPrim.GetPrimPath().GetString());
	ImportedRenderStaticMesh = FindImportedStaticMesh(ImportedAssets, RenderMeshPrim.GetPrimPath().GetString());

	// Import sim mesh from the static mesh
	if (bImportSimMesh)
	{
		SlowTask.EnterProgressFrame(1.f);

		// Check that the entire mesh is made of triangles
		if (!CheckSimMeshPrimTriangles(SimMeshPrim, OutErrorText))
		{
			return false;
		}

		// Import the simulation patterns
		if (!ImportPatternsFromSimMeshPrim(SimMeshPrim, UsdClothData.SimPatterns, OutErrorText))
		{
			return false;
		}

		// Import the sewings
		if (!ImportSewingsFromSimMeshPrim(SimMeshPrim, UsdClothData.Sewings, OutErrorText))
		{
			return false;
		}

		// Lastly import the geometry and finalize the patterns
		if (!ImportSimStaticMesh(ClothCollection, OutErrorText))
		{
			return false;
		}
	}

	// Import render mesh from the static mesh
	if (bImportRenderMesh)
	{
		SlowTask.EnterProgressFrame(1.f);

		// Import the render patterns
		if (!ImportPatternsFromRenderMeshPrim(RenderMeshPrim, SimMeshPrim, UsdClothData.RenderPatterns, UsdClothData.RenderToSimPatterns, OutErrorText))
		{
			return false;
		}

		if (!ImportRenderStaticMesh(ClothCollection, OutErrorText))
		{
			return false;
		}
	}

	return true;

#else  // #if USE_USD_SDK

	OutErrorText = LOCTEXT("NoUsdSdk", "The ChaosClothAssetDataflowNodes module has been compiled without the USD SDK enabled.");
	return false;

#endif  // #else #if USE_USD_SDK
}

void FChaosClothAssetUSDImportNode_v2::UpdateImportedAssets(const FString& SimMeshName, const FString& RenderMeshName)
{
	ImportedSimStaticMesh = nullptr;
	ImportedRenderStaticMesh = nullptr;

	if (!PackagePath.IsEmpty() && (!SimMeshName.IsEmpty() || !RenderMeshName.IsEmpty()))
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		constexpr bool bRecursive = true;
		constexpr bool bIncludeOnlyOnDiskAssets = false;
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*PackagePath), AssetData, bRecursive, bIncludeOnlyOnDiskAssets);

		// Find sim mesh and render mesh (static meshes) dependencies
		for (const FAssetData& AssetDatum : AssetData)
		{
			if (AssetDatum.IsUAsset() && AssetDatum.IsTopLevelAsset() && AssetDatum.GetClass() == UStaticMesh::StaticClass())  // IsUAsset returns false for redirects
			{
				if (AssetDatum.AssetName == SimMeshName)
				{
					ImportedSimStaticMesh = CastChecked<UStaticMesh>(AssetDatum.GetAsset());
					UE_LOG(LogChaosClothAssetDataflowNodes, Display, TEXT("Imported USD Sim Mesh %s, path: %s"), *AssetDatum.AssetName.ToString(), *AssetDatum.GetFullName());
				}
				else if (AssetDatum.AssetName == RenderMeshName)
				{
					ImportedRenderStaticMesh = CastChecked<UStaticMesh>(AssetDatum.GetAsset());
					UE_LOG(LogChaosClothAssetDataflowNodes, Display, TEXT("Imported USD Render Mesh %s, path: %s"), *AssetDatum.AssetName.ToString(), *AssetDatum.GetFullName());
				}
				if ((ImportedSimStaticMesh || SimMeshName.IsEmpty()) &&
					(ImportedRenderStaticMesh || RenderMeshName.IsEmpty()))
				{
					break;
				}
			}
		}
	}
}

TObjectPtr<UStaticMesh> FChaosClothAssetUSDImportNode_v2::FindImportedStaticMesh(const TArrayView<TObjectPtr<UObject>> ImportedAssets, const FString& MeshPrimPath)
{
	for (TObjectPtr<UObject>& ImportedAsset : ImportedAssets)
	{
		if (const TObjectPtr<UStaticMesh> ImportedStaticMesh = Cast<UStaticMesh>(ImportedAsset))
		{
			if (const UUsdMeshAssetUserData* const AssetUserData = Cast<UUsdMeshAssetUserData>(
				ImportedStaticMesh->GetAssetUserDataOfClass(UUsdMeshAssetUserData::StaticClass())))
			{
				if (AssetUserData->PrimPaths.Find(MeshPrimPath) != INDEX_NONE)
				{
					return ImportedStaticMesh;
				}
			}
		}
	}
	return nullptr;
}

TArray<TObjectPtr<UObject>> FChaosClothAssetUSDImportNode_v2::GetImportedAssetDependencies(const UObject* StaticMesh)
{
	using namespace UE::Chaos::ClothAsset::Private;

	TSet<TObjectPtr<UObject>> ImportedAssets;
	if (StaticMesh)
	{
		TQueue<const UObject*> AssetsToVisit;
		AssetsToVisit.Enqueue(StaticMesh);

		const UObject* VisitedAsset;
		while (AssetsToVisit.Dequeue(VisitedAsset))
		{
			const FName VisitedAssetPackageName(VisitedAsset->GetPackage()->GetName());
			const TArray<UObject*> AssetDependencies = GetAssetDependencies(VisitedAsset);

			UE_CLOG(AssetDependencies.Num(), LogChaosClothAssetDataflowNodes, Verbose, TEXT("Dependencies for Object %s - %s:"), *VisitedAsset->GetName(), *VisitedAssetPackageName.ToString());
			for (UObject* const AssetDependency : AssetDependencies)
			{
				if (!ImportedAssets.Contains(AssetDependency))
				{
					// Add the dependency
					UE_LOG(LogChaosClothAssetDataflowNodes, Verbose, TEXT("Found %s"), *AssetDependency->GetPackage()->GetName());
					ImportedAssets.Emplace(AssetDependency);

					// Visit this asset too
					AssetsToVisit.Enqueue(AssetDependency);
				}
			}
		}
	}
	return ImportedAssets.Array();
}

bool FChaosClothAssetUSDImportNode_v2::ImportSimStaticMesh(const TSharedRef<FManagedArrayCollection> ClothCollection, FText& OutErrorText)
{
	using namespace UE::Chaos::ClothAsset;

	FCollectionClothFacade ClothFacade(ClothCollection);
	check(ClothFacade.IsValid());  // The Cloth Collection schema must be valid at this point

	// Define the selection schema if needed
	FCollectionClothSelectionFacade ClothSelectionFacade(ClothCollection);
	if (!ClothSelectionFacade.IsValid())
	{
		ClothSelectionFacade.DefineSchema();
	}

	// Empty the current sim mesh and any previously created selection set
	FClothGeometryTools::DeleteSimMesh(ClothCollection);
	FClothGeometryTools::DeleteSelections(ClothCollection, ClothCollectionGroup::SimFaces);

	ON_SCOPE_EXIT
		{
			// Bind to root bone on exit
			constexpr bool bBindSimMesh = true;
			constexpr bool bBindRenderMesh = false;
			FClothGeometryTools::BindMeshToRootBone(ClothCollection, bBindSimMesh, bBindRenderMesh);

			// Make sure to clean the dependencies whatever the import status is
			ImportedSimAssets = GetImportedAssetDependencies(ImportedSimStaticMesh);
		};

	if (!ImportedSimStaticMesh)
	{
		return true;  // Nothing to import
	}

	// Init the static mesh attributes
	constexpr int32 LODIndex = 0;
	const FMeshDescription* const MeshDescription = ImportedSimStaticMesh->GetMeshDescription(LODIndex);
	check(MeshDescription);
	const FStaticMeshConstAttributes StaticMeshAttributes(*MeshDescription);

	if (!StaticMeshAttributes.GetVertexInstanceUVs().GetNumChannels())
	{
		OutErrorText = LOCTEXT("CantFindUVs", "Missing UV layer to initialize sim mesh data.");
		return false;
	}

	TArray<FVector2f> RestPositions2D;
	TArray<FVector3f> DrapedPositions3D;
	TArray<FIntVector3> TriangleToVertexIndex;

	// Retrieve 3D drapped positions
	DrapedPositions3D = StaticMeshAttributes.GetVertexPositions().GetRawArray();

	// Retrieve triangle indices and 2D rest positions
	RestPositions2D.SetNumZeroed(DrapedPositions3D.Num());

	const TConstArrayView<FVertexID> VertexInstanceVertexIndices = StaticMeshAttributes.GetVertexInstanceVertexIndices().GetRawArray();
	const TConstArrayView<FVertexInstanceID> TriangleVertexInstanceIndices = StaticMeshAttributes.GetTriangleVertexInstanceIndices().GetRawArray();
	const TConstArrayView<FVector2f> VertexInstanceUVs = StaticMeshAttributes.GetVertexInstanceUVs().GetRawArray();

	check(TriangleVertexInstanceIndices.Num() % 3 == 0)
		TriangleToVertexIndex.SetNumUninitialized(TriangleVertexInstanceIndices.Num() / 3);

	auto SetRestPositions2D = [&RestPositions2D, &VertexInstanceUVs](FVertexID VertexID, FVertexInstanceID VertexInstanceID) -> bool
		{
			if (RestPositions2D[VertexID] == FVector2f::Zero())
			{
				RestPositions2D[VertexID] = VertexInstanceUVs[VertexInstanceID];
			}
			else if (!RestPositions2D[VertexID].Equals(VertexInstanceUVs[VertexInstanceID]))
			{
				return false;
			}
			return true;
		};

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleToVertexIndex.Num(); ++TriangleIndex)
	{
		const FVertexInstanceID VertexInstanceID0 = TriangleVertexInstanceIndices[TriangleIndex * 3];
		const FVertexInstanceID VertexInstanceID1 = TriangleVertexInstanceIndices[TriangleIndex * 3 + 1];
		const FVertexInstanceID VertexInstanceID2 = TriangleVertexInstanceIndices[TriangleIndex * 3 + 2];

		const FVertexID VertexID0 = VertexInstanceVertexIndices[VertexInstanceID0];
		const FVertexID VertexID1 = VertexInstanceVertexIndices[VertexInstanceID1];
		const FVertexID VertexID2 = VertexInstanceVertexIndices[VertexInstanceID2];

		TriangleToVertexIndex[TriangleIndex] = FIntVector3(VertexID0, VertexID1, VertexID2);

		if (!SetRestPositions2D(VertexID0, VertexInstanceID0) ||
			!SetRestPositions2D(VertexID1, VertexInstanceID1) ||
			!SetRestPositions2D(VertexID2, VertexInstanceID2))
		{
			OutErrorText = LOCTEXT("UsdSimMeshWelded", "The sim mesh has already been welded. This importer needs an unwelded sim mesh.");
			// TODO: unweld vertices, generate seams(?), and reindex all constraints
			return false;
		}
	}

	// Rescale the 2D mesh with the UV scale, and flip the UV's Y coordinates
	for (FVector2f& Pos : RestPositions2D)
	{
		Pos.Y = 1.f - Pos.Y;
		Pos *= ImportedUVScale;
	}

	// Save pattern to the collection cache
	check(RestPositions2D.Num() == DrapedPositions3D.Num());  // Should have already exited with the UsdSimMeshWelded error in this case
	if (TriangleToVertexIndex.Num() && RestPositions2D.Num())
	{
		// Cleanup sim mesh
		FClothDataflowTools::FSimMeshCleanup SimMeshCleanup(TriangleToVertexIndex, RestPositions2D, DrapedPositions3D);

		bool bHasRepairedTriangles = SimMeshCleanup.RemoveDegenerateTriangles();
		bHasRepairedTriangles = SimMeshCleanup.RemoveDuplicateTriangles() || bHasRepairedTriangles;

		const TArray<int32> OriginalToNewTriangles = FClothDataflowTools::GetOriginalToNewIndices<TSet<int32>>(SimMeshCleanup.OriginalTriangles, TriangleToVertexIndex.Num());

		// Add support for original indices
		ClothFacade.AddUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimFaces);
		ClothFacade.AddUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimVertices2D);

		// Add the patterns from the clean mesh
		for (const TPair<FName, TSet<int32>>& PatternNameFaces : UsdClothData.SimPatterns)
		{
			// Filter the pattern selection set using the remaining triangles from the cleaned triangle list
			TSet<int32> PatternSet;
			PatternSet.Reserve(PatternNameFaces.Value.Num());
			for (const int32 Face : PatternNameFaces.Value)
			{
				if (OriginalToNewTriangles.IsValidIndex(Face) && OriginalToNewTriangles[Face] != INDEX_NONE)
				{
					PatternSet.Emplace(OriginalToNewTriangles[Face]);
				}
			}

			// Add the new pattern
			if (PatternSet.Num())
			{
				TArray<FIntVector3> PatternTriangleToVertexIndex;
				TArray<TArray<int32>> PatternOriginalTriangles;
				PatternTriangleToVertexIndex.Reserve(PatternSet.Num());
				PatternOriginalTriangles.Reserve(PatternSet.Num());
				{
					for (const int32 Index : PatternSet)
					{
						PatternTriangleToVertexIndex.Emplace(SimMeshCleanup.TriangleToVertexIndex[Index]);
						PatternOriginalTriangles.Emplace(SimMeshCleanup.OriginalTriangles[Index].Array());
					}
				}

				TArray<FVector2f> PatternRestPositions2D;
				TArray<FVector3f> PatternDrapedPositions3D;
				TArray<TArray<int32>> PatternOriginalVertices;
				TArray<int32> PatternVertexReindex;
				const int32 MaxNumVertices = SimMeshCleanup.RestPositions2D.Num();
				PatternRestPositions2D.Reserve(MaxNumVertices);
				PatternDrapedPositions3D.Reserve(MaxNumVertices);
				PatternOriginalVertices.Reserve(MaxNumVertices);
				PatternVertexReindex.Init(INDEX_NONE, MaxNumVertices);

				int32 NewIndex = -1;
				for (FIntVector3& Triangle : PatternTriangleToVertexIndex)
				{
					for (int32 Vertex = 0; Vertex < 3; ++Vertex)
					{
						// Add the new vertex
						int32& Index = Triangle[Vertex];
						if (PatternVertexReindex[Index] == INDEX_NONE)
						{
							PatternVertexReindex[Index] = ++NewIndex;
							PatternRestPositions2D.Emplace(SimMeshCleanup.RestPositions2D[Index]);
							PatternDrapedPositions3D.Emplace(SimMeshCleanup.DrapedPositions3D[Index]);
							PatternOriginalVertices.Emplace(SimMeshCleanup.OriginalVertices[Index].Array());
						}
						// Reindex the triangle vertex with the new index
						Index = PatternVertexReindex[Index];
					}
				}

				// Add this pattern to the cloth collection
				const int32 SimPatternIndex = ClothFacade.AddSimPattern();
				FCollectionClothSimPatternFacade SimPattern = ClothFacade.GetSimPattern(SimPatternIndex);
				SimPattern.Initialize(PatternRestPositions2D, PatternDrapedPositions3D, PatternTriangleToVertexIndex);

				// Keep track of the original triangle indices
				const TArrayView<TArray<int32>> OriginalTriangles =
					ClothFacade.GetUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimFaces);  // Don't move outside the loop, the array might get re-allocated
				const int32 SimFacesOffset = SimPattern.GetSimFacesOffset();
				for (int32 Index = 0; Index < PatternOriginalTriangles.Num(); ++Index)
				{
					OriginalTriangles[SimFacesOffset + Index] = PatternOriginalTriangles[Index];
				}

				// Keep track of the original vertex indices
				const TArrayView<TArray<int32>> OriginalVertices =
					ClothFacade.GetUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimVertices2D);  // Don't move outside the loop, the array might get re-allocated
				const int32 SimVertices2DOffset = SimPattern.GetSimVertices2DOffset();
				for (int32 Index = 0; Index < PatternOriginalVertices.Num(); ++Index)
				{
					OriginalVertices[SimVertices2DOffset + Index] = PatternOriginalVertices[Index];
				}

				// Add the pattern triangle list as a selection set
				TSet<int32>& SelectionSet = ClothSelectionFacade.FindOrAddSelectionSet(PatternNameFaces.Key, ClothCollectionGroup::SimFaces);
				SelectionSet.Empty(PatternSet.Num());
				for (int32 Index = SimFacesOffset; Index < SimFacesOffset + PatternTriangleToVertexIndex.Num(); ++Index)
				{
					SelectionSet.Emplace(Index);
				}
			}
		}

		// Check the resulting cleaned mesh
		const int32 NumSimVertices2D = ClothFacade.GetNumSimVertices2D();
		const int32 NumSimFaces = ClothFacade.GetNumSimFaces();
		if (!NumSimVertices2D || !NumSimFaces)
		{
			return true;  // Empty mesh
		}

		const TConstArrayView<TArray<int32>> OriginalTriangles =
			ClothFacade.GetUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimFaces);
		const TArray<int32> OriginalToNewFaceIndices = FClothDataflowTools::GetOriginalToNewIndices(OriginalTriangles, TriangleToVertexIndex.Num());

		const TConstArrayView<TArray<int32>> OriginalVertices =
			ClothFacade.GetUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::SimVertices2D);
		const TArray<int32> OriginalToNewVertexIndices = FClothDataflowTools::GetOriginalToNewIndices(OriginalVertices, RestPositions2D.Num());

		// Add the sewings
		for (const TPair<FName, TSet<FIntVector2>>& SewingNameIndices : UsdClothData.Sewings)
		{
			TSet<FIntVector2> Indices;
			for (const FIntVector2& Stitch : SewingNameIndices.Value)
			{
				if (!OriginalToNewVertexIndices.IsValidIndex(Stitch[0]) || !OriginalToNewVertexIndices.IsValidIndex(Stitch[1]))
				{
					OutErrorText = LOCTEXT("BadSewingIndex", "An out of renge sewing index has been found.");
					return false;
				}
				const int32 StitchIndex0 = OriginalToNewVertexIndices[Stitch[0]];
				const int32 StitchIndex1 = OriginalToNewVertexIndices[Stitch[1]];
				if (StitchIndex0 != INDEX_NONE && StitchIndex1 != INDEX_NONE)
				{
					Indices.Emplace(StitchIndex0 < StitchIndex1 ? FIntVector2(StitchIndex0, StitchIndex1) : FIntVector2(StitchIndex1, StitchIndex0));
				}
			}

			FCollectionClothSeamFacade ClothSeamFacade = ClothFacade.AddGetSeam();
			ClothSeamFacade.Initialize(Indices.Array());
		}
	}
	return true;
}

bool FChaosClothAssetUSDImportNode_v2::ImportRenderStaticMesh(const TSharedRef<FManagedArrayCollection> ClothCollection, FText& OutErrorText)
{
	using namespace UE::Chaos::ClothAsset;

	FCollectionClothFacade ClothFacade(ClothCollection);
	check(ClothFacade.IsValid());  // The Cloth Collection schema must be valid at this point

	// Define the selection schema if needed
	FCollectionClothSelectionFacade ClothSelectionFacade(ClothCollection);
	if (!ClothSelectionFacade.IsValid())
	{
		ClothSelectionFacade.DefineSchema();
	}

	// Empty the current render mesh and previously create selections
	FClothGeometryTools::DeleteRenderMesh(ClothCollection);
	FClothGeometryTools::DeleteSelections(ClothCollection, ClothCollectionGroup::RenderFaces);

	// Make sure to clean the dependencies whatever the import status is
	ON_SCOPE_EXIT
		{
			// Bind to root bone on exit
			constexpr bool bBindSimMesh = false;
			constexpr bool bBindRenderMesh = true;
			FClothGeometryTools::BindMeshToRootBone(ClothCollection, bBindSimMesh, bBindRenderMesh);

			// Make sure to clean the dependencies whatever the import status is
			ImportedRenderAssets = GetImportedAssetDependencies(ImportedRenderStaticMesh);
		};

	// Import the LOD 0
	if (ImportedRenderStaticMesh && ImportedRenderStaticMesh->GetNumSourceModels())
	{
		constexpr int32 LODIndex = 0;
		if (const FMeshDescription* const MeshDescription = ImportedRenderStaticMesh->GetMeshDescription(LODIndex))
		{
			const FMeshBuildSettings& BuildSettings = ImportedRenderStaticMesh->GetSourceModel(LODIndex).BuildSettings;
			FRenderMeshImport RenderMeshImport(*MeshDescription, BuildSettings);

			const TArray<FStaticMaterial>& StaticMaterials = ImportedRenderStaticMesh->GetStaticMaterials();
			RenderMeshImport.AddRenderSections(ClothCollection, StaticMaterials, Private::OriginalIndicesName, Private::OriginalIndicesName);

			// Create pattern selection sets
			const TConstArrayView<TArray<int32>> OriginalTriangles =
				ClothFacade.GetUserDefinedAttribute<TArray<int32>>(Private::OriginalIndicesName, ClothCollectionGroup::RenderFaces);

			if (OriginalTriangles.Num())
			{
				const TConstArrayView<FIntVector3> TriangleToVertexIndex = ClothFacade.GetRenderIndices();
				const TArray<int32> OriginalToNewTriangles = FClothDataflowTools::GetOriginalToNewIndices<TArray<int32>>(OriginalTriangles, TriangleToVertexIndex.Num());

				for (const TPair<FName, TSet<int32>>& PatternNameFaces : UsdClothData.RenderPatterns)
				{
					// Add the pattern triangle list as a selection set
					TSet<int32>& SelectionSet = ClothSelectionFacade.FindOrAddSelectionSet(PatternNameFaces.Key, ClothCollectionGroup::RenderFaces);
					SelectionSet.Empty(PatternNameFaces.Value.Num());
					for (const int32 Index : PatternNameFaces.Value)
					{
						SelectionSet.Emplace(OriginalToNewTriangles[Index]);
					}
				}
			}
			// TODO: Proxy deformer
		}
		else
		{
			OutErrorText = LOCTEXT("MissingMeshDescription", "An imported render static mesh has no mesh description!");
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
