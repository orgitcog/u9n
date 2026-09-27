// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConstructionScene.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/CollectionRenderingPatternUtility.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowRenderingFactory.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Selection.h"
#include "AssetViewerSettings.h"
#include "DataflowEditorOptions.h"
#include "Dataflow/DataflowPrimitiveNode.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "Selection/GeometrySelector.h"

#define LOCTEXT_NAMESPACE "FDataflowConstructionScene"

//
// Construction Scene
//

FDataflowConstructionScene::FDataflowConstructionScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* InEditor)
	: FDataflowPreviewSceneBase(ConstructionValues, InEditor, FName("Construction Components"))
{
	RootSceneActor->GetName();
}

FDataflowConstructionScene::~FDataflowConstructionScene()
{
	ResetWireframeMeshElementsVisualizer();
	ResetSceneComponents();
}

TArray<TObjectPtr<UDynamicMeshComponent>> FDataflowConstructionScene::GetDynamicMeshComponents() const
{
	TArray<TObjectPtr<UDynamicMeshComponent>> OutValues;
	DynamicMeshComponents.GenerateValueArray(OutValues);
	return MoveTemp(OutValues);
}

/** Hide all or a single component */
void FDataflowConstructionScene::SetVisibility(bool bVisibility, UActorComponent* InComponent)
{
	auto SetCollectionVisiblity = [](bool bVisibility,TObjectPtr<UDataflowEditorCollectionComponent> Component) {
		Component->SetVisibility(bVisibility);
		if (Component->WireframeComponent)
		{
			Component->WireframeComponent->SetVisibility(bVisibility);
		}
	};

	for (FRenderElement& RenderElement : DynamicMeshComponents)
	{
		if (TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = Cast<UDataflowEditorCollectionComponent>(RenderElement.Value))
		{
			if (InComponent != nullptr)
			{
				if (InComponent == DynamicMeshComponent.Get())
				{
					SetCollectionVisiblity(bVisibility,DynamicMeshComponent);
				}
			}
			else
			{
				SetCollectionVisiblity(bVisibility,DynamicMeshComponent);
			}
		}
	}
}


void FDataflowConstructionScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowPreviewSceneBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObjects(DynamicMeshComponents);
	Collector.AddReferencedObjects(WireframeElements);
}

void FDataflowConstructionScene::TickDataflowScene(const float DeltaSeconds)
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (const UDataflow* Dataflow = EditorContent->GetDataflowAsset())
		{
			if (Dataflow->GetDataflow())
			{
				UE::Dataflow::FTimestamp SystemTimestamp = UE::Dataflow::FTimestamp::Invalid;
				bool bMustUpdateConstructionScene = false;
				for (TObjectPtr<const UDataflowBaseContent> DataflowBaseContent : GetTerminalContents())
				{
					const FName DataflowTerminalName(DataflowBaseContent->GetDataflowTerminal());
					if (TSharedPtr<const FDataflowNode> DataflowTerminalNode = Dataflow->GetDataflow()->FindBaseNode(DataflowTerminalName))
					{
						SystemTimestamp = DataflowTerminalNode->GetTimestamp();
					}

					if (LastRenderedTimestamp < SystemTimestamp)
					{
						LastRenderedTimestamp = SystemTimestamp;
						bMustUpdateConstructionScene = true;
					}
				}
				if (bMustUpdateConstructionScene || EditorContent->IsConstructionDirty())
				{
					UpdateConstructionScene();
				}
			}
		}
	}
	for (TObjectPtr<UInteractiveToolPropertySet>& Propset : PropertyObjectsToTick)
	{
		if (Propset)
		{
			if (Propset->IsPropertySetEnabled())
			{
				Propset->CheckAndUpdateWatched();
			}
			else
			{
				Propset->SilentUpdateWatched();
			}
		}
	}

	for (FRenderWireElement Elem : WireframeElements)
	{
		Elem.Value->OnTick(DeltaSeconds);
	}
}

void FDataflowConstructionScene::FDebugMesh::Reset()
{
	VertexMap.Reset();
	FaceMap.Reset();
}

void FDataflowConstructionScene::FDebugMesh::Build(const TArray<TObjectPtr<UDynamicMeshComponent>>& InDynamicMeshComponents)
{
	ResultMesh.Clear();
	ResultMesh.EnableAttributes();

	for (const UDynamicMeshComponent* const DynamicMeshComponent : InDynamicMeshComponents)
	{
		if (const UE::Geometry::FDynamicMesh3* const Mesh = DynamicMeshComponent->GetMesh())
		{
			ResultMesh.AppendWithOffsets(*Mesh);
		}
	}

	// Disable it for now
	// TODO: Optimize this
#if 0
	Spatial = UE::Geometry::FDynamicMeshAABBTree3(&ResultMesh, true);
#endif
}

void FDataflowConstructionScene::UpdateDynamicMeshComponents()
{
	using namespace UE::Geometry;//FDynamicMesh3

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will generate a 
	// list of UPrimitiveComponents for rendering.

	const FDataflowEditorToolkit* EditorToolkit = DataflowEditor ? static_cast<FDataflowEditorToolkit*>(DataflowEditor->GetInstanceInterface()) : nullptr;
	const bool bEvaluateOutputs = EditorToolkit ? (EditorToolkit->GetEvaluationMode() != EDataflowEditorEvaluationMode::Manual) : true;
	
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		const TObjectPtr<UDataflow>& DataflowAsset = EditorContent->GetDataflowAsset();
		const TSharedPtr<UE::Dataflow::FEngineContext>& DataflowContext = EditorContent->GetDataflowContext();
		if(DataflowAsset && DataflowContext)
		{
			for (TObjectPtr<const UDataflowEdNode> Target : DataflowAsset->GetRenderTargets())
			{
				if (Target)
				{
					TSharedPtr<FManagedArrayCollection> RenderCollection(new FManagedArrayCollection);
					GeometryCollection::Facades::FRenderingFacade RenderingFacade(*RenderCollection);
					RenderingFacade.DefineSchema();
					
					const bool bHasPrimitves = UE::Dataflow::RenderNodeOutput(RenderingFacade, *Target, *EditorContent, bEvaluateOutputs);

					if (Target == EditorContent->GetSelectedNode())
					{
						EditorContent->SetRenderCollection(RenderCollection);
					}
					if(!bHasPrimitves)
					{
						const int32 NumGeometry = RenderingFacade.NumGeometry();
	
						DebugMesh.Reset();
						int32 VertexIndex = 0, FaceIndex = 0;

						const TManagedArray<int32>& VertexStart = RenderingFacade.GetVertexStart();
						const TManagedArray<int32>& VertexCount = RenderingFacade.GetVertexCount();
						const TManagedArray<int32>& FaceStart = RenderingFacade.GetIndicesStart();
						const TManagedArray<int32>& FaceCount = RenderingFacade.GetIndicesCount();

						for (int32 MeshIndex = 0; MeshIndex < NumGeometry; ++MeshIndex)
						{
							FDynamicMesh3 DynamicMesh;
							UE::Dataflow::Conversion::RenderingFacadeToDynamicMesh(RenderingFacade, MeshIndex, DynamicMesh);

							if (DynamicMesh.VertexCount())
							{
								const FString MeshName = RenderingFacade.GetGeometryName()[MeshIndex];

								const TManagedArray<FString>& MaterialPaths = RenderingFacade.GetMaterialPaths();
								const int32 MaterialStart = RenderingFacade.GetMaterialStart()[MeshIndex];
								const int32 MaterialCount = RenderingFacade.GetMaterialCount()[MeshIndex];

								TArray<UMaterialInterface*> Materials;
								for (int32 MaterialIndex = MaterialStart; MaterialIndex < MaterialStart + MaterialCount; ++MaterialIndex)
								{
									const FString& Path = MaterialPaths[MaterialIndex];
									UMaterialInterface* const Material = LoadObject<UMaterialInterface>(nullptr, *Path);
									Materials.Add(Material);
								}

								AddDynamicMeshComponent({Target, MeshIndex}, MeshName, MoveTemp(DynamicMesh), Materials);


								for (int32 Idx = 0; Idx < VertexCount[MeshIndex]; ++Idx)
								{
									DebugMesh.VertexMap.Add(VertexIndex, VertexStart[MeshIndex] + Idx);
									VertexIndex++;
								}

								for (int32 Idx = 0; Idx < FaceCount[MeshIndex]; ++Idx)
								{
									DebugMesh.FaceMap.Add(FaceIndex, FaceStart[MeshIndex] + Idx);
									FaceIndex++;
								}
							}
						}
					}
				}
			}

			// If we have a single mesh component in the scene, select it
			if (DynamicMeshComponents.Num() == 1 && DataflowModeManager)
			{
				if (USelection* const SelectedComponents = DataflowModeManager->GetSelectedComponents())
				{
					UDynamicMeshComponent* const DynamicMeshComponent = DynamicMeshComponents.CreateIterator()->Value;
					SelectedComponents->Select(DynamicMeshComponent);
					DynamicMeshComponent->PushSelectionToProxy();
				}
			}

			// Add hidden DynamicMeshComponents for any targets that we want to render in wireframe
			// 
			// Note: UMeshElementsVisualizers need source meshes to pull from. We add invisible dynamic mesh components to the existing DynamicMeshComponents collection
			// for this purpose, but could have instead created a separate collection of meshes for wireframe rendering. We are choosing to keep all the scene DynamicMeshComponents 
			// in one place and using separate structures to dictate how they are used (MeshComponentsForWireframeRendering in this case), in case visualization requirements 
			// change in the future.
			//

			MeshComponentsForWireframeRendering.Reset();
			for (TObjectPtr<const UDataflowEdNode> Target : DataflowAsset->GetWireframeRenderTargets())
			{
				if (Target)
				{
					TSharedPtr<FManagedArrayCollection> RenderCollection(new FManagedArrayCollection);
					GeometryCollection::Facades::FRenderingFacade RenderingFacade(*RenderCollection);
					RenderingFacade.DefineSchema();

					const bool bHasPrimitves = UE::Dataflow::RenderNodeOutput(RenderingFacade, *Target, *EditorContent, bEvaluateOutputs);

					if (Target == EditorContent->GetSelectedNode())

					{
						EditorContent->SetRenderCollection(RenderCollection);
					}

					if(!bHasPrimitves)
					{
						const int32 NumGeometry = RenderingFacade.NumGeometry();
						for (int32 MeshIndex = 0; MeshIndex < NumGeometry; ++MeshIndex)
						{
							FDataflowRenderKey WireframeDynamicMeshKey{ Target, MeshIndex };

							if (DynamicMeshComponents.Contains(WireframeDynamicMeshKey))
							{
								UDynamicMeshComponent* const ExistingMeshComponent = DynamicMeshComponents[WireframeDynamicMeshKey];
								MeshComponentsForWireframeRendering.Add(ExistingMeshComponent);
							}
							else
							{
								FDynamicMesh3 DynamicMesh;
								UE::Dataflow::Conversion::RenderingFacadeToDynamicMesh(RenderingFacade, MeshIndex, DynamicMesh);

								if (DynamicMesh.VertexCount())
								{
									const FString MeshName = RenderingFacade.GetGeometryName()[MeshIndex];
									const FString UniqueObjectName = MakeUniqueObjectName(RootSceneActor, UDataflowEditorCollectionComponent::StaticClass(), FName(MeshName)).ToString();
									UDynamicMeshComponent* const NewDynamicMeshComponent = AddDynamicMeshComponent(WireframeDynamicMeshKey, UniqueObjectName, MoveTemp(DynamicMesh), {});
									NewDynamicMeshComponent->SetVisibility(false);
									MeshComponentsForWireframeRendering.Add(NewDynamicMeshComponent);
								}
							}
						}
					}
				}
			}

			// Hide the floor in orthographic view modes
			if (const UE::Dataflow::IDataflowConstructionViewMode* ConstructionViewMode = EditorContent->GetConstructionViewMode())
			{
				if (!ConstructionViewMode->IsPerspective())
				{
					constexpr bool bDontModifyProfile = true;
					SetFloorVisibility(false, bDontModifyProfile);
				}
				else
				{
					// Restore visibility from profile settings
					const int32 ProfileIndex = GetCurrentProfileIndex();
					if (DefaultSettings->Profiles.IsValidIndex(ProfileIndex))
					{
						const bool bProfileSetting = DefaultSettings->Profiles[CurrentProfileIndex].bShowFloor;
						constexpr bool bDontModifyProfile = true;
						SetFloorVisibility(bProfileSetting, bDontModifyProfile);
					}
				}
			}
		}
	
		// Build a single mesh out of all the components
		DebugMesh.Build(GetDynamicMeshComponents());
	}

	bPreviewSceneDirty = true;
}

void FDataflowConstructionScene::UpdatePrimitiveComponents()
{
	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if (const TSharedPtr<UE::Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext())
		{
			if (UDataflowEdNode* SelectedNode = EditorContent->GetSelectedNode())
			{
				if (TSharedPtr<FDataflowNode> SelectedDataflowNode = SelectedNode->GetDataflowNode())
				{
					if(SelectedDataflowNode->IsA(FDataflowPrimitiveNode::StaticType()))
					{
						StaticCastSharedPtr<FDataflowPrimitiveNode>(SelectedDataflowNode)->AddPrimitiveComponents( 
							EditorContent->GetRenderCollection(), DataflowContext->Owner, RootSceneActor, PrimitiveComponents);
					}
				}
			}
		}
	}
	for(TObjectPtr<UPrimitiveComponent> PrimitiveComponent : PrimitiveComponents)
	{
		PrimitiveComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
		PrimitiveComponent->UpdateBounds();

		AddComponent(PrimitiveComponent, PrimitiveComponent->GetRelativeTransform());	
		AddSceneObject(PrimitiveComponent, true);
	}
	bPreviewSceneDirty = true;
}

void FDataflowConstructionScene::RemoveSceneComponent(USelection* SelectedComponents, UPrimitiveComponent* PrimitiveComponent)
{
	if(PrimitiveComponent)
	{
		PrimitiveComponent->SelectionOverrideDelegate.Unbind();
		if (SelectedComponents->IsSelected(PrimitiveComponent))
		{
			SelectedComponents->Deselect(PrimitiveComponent);
			PrimitiveComponent->PushSelectionToProxy();
		}
		RemoveSceneObject(PrimitiveComponent);
		RemoveComponent(PrimitiveComponent);
		PrimitiveComponent->DestroyComponent();
	}
}

void FDataflowConstructionScene::ResetSceneComponents()
{
	USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
	for (FRenderElement RenderElement : DynamicMeshComponents)
	{
		RemoveSceneComponent(SelectedComponents, RenderElement.Value);
	}
	for (TObjectPtr<UPrimitiveComponent> PrimitiveComponent : PrimitiveComponents)
	{
		RemoveSceneComponent(SelectedComponents, PrimitiveComponent);
	}
	DynamicMeshComponents.Reset();
	PrimitiveComponents.Reset();
	bPreviewSceneDirty = true;

	RemoveSceneObject(RootSceneActor);
}

TObjectPtr<UDynamicMeshComponent>& FDataflowConstructionScene::AddDynamicMeshComponent(FDataflowRenderKey InKey, const FString& MeshName, UE::Geometry::FDynamicMesh3&& DynamicMesh, const TArray<UMaterialInterface*>& MaterialSet)
{
	// Don't use the MakeUniqueObjectName for the component, we need to keep the name aligned with the collection so selection will work in 
	// other editors. 
	// const FName UniqueObjectName = MakeUniqueObjectName(RootSceneActor, UDataflowEditorCollectionComponent::StaticClass(), FName(MeshName));
	TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = NewObject<UDataflowEditorCollectionComponent>(RootSceneActor, FName(MeshName));

	DynamicMeshComponent->MeshIndex = InKey.Value;
	DynamicMeshComponent->Node = InKey.Key;
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
	
	if (MaterialSet.Num() == 0)
	{
		const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent();
		if (EditorContent && EditorContent->GetDataflowAsset() && EditorContent->GetDataflowAsset()->Material)
		{
			DynamicMeshComponent->ConfigureMaterialSet({ EditorContent->GetDataflowAsset()->Material });
		}
		else
		{
			ensure(FDataflowEditorStyle::Get().DefaultTwoSidedMaterial);
			DynamicMeshComponent->SetOverrideRenderMaterial(FDataflowEditorStyle::Get().DefaultTwoSidedMaterial);
			DynamicMeshComponent->SetShadowsEnabled(false);
		}
	}
	else
	{
		DynamicMeshComponent->ConfigureMaterialSet(MaterialSet);
	}

	DynamicMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
	DynamicMeshComponent->UpdateBounds();

	// Fix up any triangles without valid material IDs
	int32 DefaultMaterialID = INDEX_NONE;
	for (const int32 TriID : DynamicMeshComponent->GetMesh()->TriangleIndicesItr())
	{
		const int32 MaterialID = DynamicMeshComponent->GetMesh()->Attributes()->GetMaterialID()->GetValue(TriID);
		if (!DynamicMeshComponent->GetMaterial(MaterialID))
		{
			if (DefaultMaterialID == INDEX_NONE)
			{
				DefaultMaterialID = DynamicMeshComponent->GetNumMaterials();
				DynamicMeshComponent->SetMaterial(DefaultMaterialID, FDataflowEditorStyle::Get().VertexMaterial);
			}
			DynamicMeshComponent->GetMesh()->Attributes()->GetMaterialID()->SetValue(TriID, DefaultMaterialID);
		}
	}

	AddComponent(DynamicMeshComponent, DynamicMeshComponent->GetRelativeTransform());	
	DynamicMeshComponents.Emplace(InKey, DynamicMeshComponent);
	AddSceneObject(DynamicMeshComponent, true);
	return DynamicMeshComponents[InKey];
}

void FDataflowConstructionScene::AddWireframeMeshElementsVisualizer()
{
	ensure(WireframeElements.Num() == 0);
	for (UDynamicMeshComponent* Elem : MeshComponentsForWireframeRendering)
	{
		if (TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = Cast<UDataflowEditorCollectionComponent>(Elem))
		{
			// Set up the wireframe display of the rest space mesh.

			TObjectPtr<UMeshElementsVisualizer> WireframeDraw = NewObject<UMeshElementsVisualizer>(RootSceneActor);
			WireframeElements.Add(DynamicMeshComponent, WireframeDraw);

			WireframeDraw->CreateInWorld(GetWorld(), FTransform::Identity);
			checkf(WireframeDraw->Settings, TEXT("Expected UMeshElementsVisualizer::Settings to exist after CreateInWorld"));

			WireframeDraw->Settings->DepthBias = 2.0;
			WireframeDraw->Settings->bAdjustDepthBiasUsingMeshSize = false;
			WireframeDraw->Settings->bShowWireframe = true;
			WireframeDraw->Settings->bShowBorders = true;
			WireframeDraw->Settings->bShowUVSeams = false;
			WireframeDraw->WireframeComponent->BoundaryEdgeThickness = 2;
			DynamicMeshComponent->WireframeComponent = WireframeDraw->WireframeComponent;

			WireframeDraw->SetMeshAccessFunction([DynamicMeshComponent](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc)
				{
					ProcessFunc(*DynamicMeshComponent->GetMesh());
				});

			for (FRenderElement RenderElement : DynamicMeshComponents)
			{
				RenderElement.Value->OnMeshChanged.Add(FSimpleMulticastDelegate::FDelegate::CreateLambda([WireframeDraw, this]()
					{
						WireframeDraw->NotifyMeshChanged();
					}));
			}

			WireframeDraw->Settings->bVisible = false;
			PropertyObjectsToTick.Add(WireframeDraw->Settings);
		}
	}
}

void FDataflowConstructionScene::ResetWireframeMeshElementsVisualizer()
{
	for (FRenderWireElement Elem : WireframeElements)
	{
		Elem.Value->Disconnect();
	}
	WireframeElements.Empty();
}

void FDataflowConstructionScene::UpdateWireframeMeshElementsVisualizer()
{
	ResetWireframeMeshElementsVisualizer();
	AddWireframeMeshElementsVisualizer();
}

bool FDataflowConstructionScene::HasRenderableGeometry()
{
	for (FRenderElement RenderElement : DynamicMeshComponents)
	{
		if (RenderElement.Value->GetMesh()->TriangleCount() > 0)
		{
			return true;
		}
	}
	return false;
}

void FDataflowConstructionScene::ResetConstructionScene()
{
	// The ModeManagerss::USelection will hold references to Components, but 
	// does not report them to the garbage collector. We need to clear the
	// saved selection when the scene is rebuilt. @todo(Dataflow) If that 
	// selection needs to persist across render resets, we will also need to
	// buffer the names of the selected objects so they can be reselected.
	if (GetDataflowModeManager())
	{
		if (USelection* SelectedComponents = GetDataflowModeManager()->GetSelectedComponents())
		{
			SelectedComponents->DeselectAll();
		}
	}

	// Some objects, like the UMeshElementsVisualizer and Settings Objects
	// are not part of a tool, so they won't get ticked.This member holds
	// ticked objects that get rebuilt on Update
	PropertyObjectsToTick.Empty();

	ResetWireframeMeshElementsVisualizer();

	ResetSceneComponents();
}

void FDataflowConstructionScene::UpdateConstructionScene()
{
	ResetConstructionScene();

	// Add root actor to TEDS
	AddSceneObject(RootSceneActor, true);

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will generate a 
	// list of UPrimitiveComponents for rendering.
	UpdateDynamicMeshComponents();
	
	// Attach a wireframe renderer to the DynamicMeshComponents
	UpdateWireframeMeshElementsVisualizer();

	// Update all the primitive components potentially added by the selected node
	UpdatePrimitiveComponents();

	for (const UDynamicMeshComponent* const DynamicMeshComponent : MeshComponentsForWireframeRendering)
	{
		WireframeElements[DynamicMeshComponent]->Settings->bVisible = true;
	}

	if (const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		EditorContent->SetConstructionDirty(false);
	}

	for(const TObjectPtr<UDataflowBaseContent>& TerminalContent : GetTerminalContents())
	{
		TerminalContent->SetConstructionDirty(false);
	}
}


#undef LOCTEXT_NAMESPACE

