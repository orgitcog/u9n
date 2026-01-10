// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomDataflowVisualization.h"
#include "GroomComponent.h"
#include "GroomVisualizationData.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowSimulationViewportClient.h"
#include "Dataflow/DataflowSimulationScene.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetDataflowSimulationVisualization"

namespace UE::Groom
{
	const FName FGroomDataflowSimulationVisualization::Name = FName("GroomDataflowSimulationVisualization");

	FName FGroomDataflowSimulationVisualization::GetName() const
	{
		return Name;
	}

	FGroomDataflowSimulationVisualization::FGroomDataflowSimulationVisualization()
	{
		VisualizationFlags.Init(false, static_cast<uint8>(EGroomViewMode::Count));
	}

	void FGroomDataflowSimulationVisualization::ExtendSimulationVisualizationMenu(const TSharedPtr<FDataflowSimulationViewportClient>& ViewportClient, FMenuBuilder& MenuBuilder)
	{
		if (ViewportClient)
		{
			if (TSharedPtr<FDataflowEditorToolkit> Toolkit = ViewportClient->GetDataflowEditorToolkit().Pin())
			{
				if (const TSharedPtr<FDataflowSimulationScene>& SimulationScene = Toolkit->GetSimulationScene())
				{
					if (GetGroomComponent(SimulationScene.Get()))
					{
						static IConsoleVariable* CVarGroomViewMode = nullptr; 
						if (CVarGroomViewMode == nullptr)
						{
							if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(
								FGroomVisualizationData::GetVisualizeConsoleCommandName()))
							{
								CVarGroomViewMode = ConsoleVariable->AsVariable();
							}
						}
						static IConsoleVariable* CVarGroomStrandWidth = nullptr; 
						if (CVarGroomStrandWidth == nullptr)
						{
							if (IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(
								TEXT("r.HairStrands.StrandWidth")))
							{
								CVarGroomStrandWidth = ConsoleVariable->AsVariable();
							}
						}
						constexpr uint8 NumFlags = static_cast<uint8>(EGroomViewMode::Count);
						auto AddSimulationVisualisation = [this, &MenuBuilder, NumFlags](const TAttribute<FText>& FlagLabel, const uint8 FlagIndex, const int32 ViewMode, const float StrandWidth)
						{
							// Handler for visualization entry being clicked
							const FExecuteAction ExecuteAction = FExecuteAction::CreateLambda([this, FlagIndex, ViewMode, StrandWidth]()
							{
								VisualizationFlags[FlagIndex] = !VisualizationFlags[FlagIndex];
								if(VisualizationFlags[FlagIndex])
								{
									CVarGroomViewMode->SetWithCurrentPriority(ViewMode);
									CVarGroomStrandWidth->SetWithCurrentPriority(StrandWidth);
									for(int32 OtherIndex = 0; OtherIndex < NumFlags; ++OtherIndex)
									{
										if(OtherIndex != FlagIndex)
										{
											VisualizationFlags[OtherIndex] = false;
										}
									}
								}
								else
								{
									CVarGroomViewMode->SetWithCurrentPriority(0);
									CVarGroomStrandWidth->SetWithCurrentPriority(0.0f);
								}
							});

							// Checkstate function for visualization entries
							const FIsActionChecked IsActionChecked = FIsActionChecked::CreateLambda([this, FlagIndex]()
							{
								return VisualizationFlags[FlagIndex];
							});

							const FUIAction Action(ExecuteAction, FCanExecuteAction(), IsActionChecked);

							// Add menu entry
							MenuBuilder.AddMenuEntry(FlagLabel, FText(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);
						};
						MenuBuilder.BeginSection(TEXT("GroomSimulation_Visualizations"), LOCTEXT("GroomSimulationVisualization", "Groom Simulation Visualization"));

						AddSimulationVisualisation(LOCTEXT("SimulatedGuides", "Simulated guides"), static_cast<uint8>(EGroomViewMode::SimHairStrands), 21, 0.5f);
						AddSimulationVisualisation(LOCTEXT("RenderedStrands", "Rendered strands"), static_cast<uint8>(EGroomViewMode::RenderHairStrands), 22, 0.0f);
						AddSimulationVisualisation(LOCTEXT("StrandsGroups", "Strands groups"), static_cast<uint8>(EGroomViewMode::Group), 35, 0.0f);
						AddSimulationVisualisation(LOCTEXT("StrandsClumps", "Strands clumps"), static_cast<uint8>(EGroomViewMode::ClumpID), 39, 0.0f);
						AddSimulationVisualisation(LOCTEXT("StrandsClusters", "Strands clusters"), static_cast<uint8>(EGroomViewMode::Cluster), 31, 0.0f);
						AddSimulationVisualisation(LOCTEXT("MeshProjection", "Mesh projection"), static_cast<uint8>(EGroomViewMode::MeshProjection), 12, 0.0f);
						AddSimulationVisualisation(LOCTEXT("CardsGuides", "Cards guides"), static_cast<uint8>(EGroomViewMode::CardGuides), 37, 0.0f);
						AddSimulationVisualisation(LOCTEXT("LODColoration", "LOD Coloration"), static_cast<uint8>(EGroomViewMode::LODColoration), 36, 0.0f);
						
						MenuBuilder.EndSection();
					}
				}
			}
		}
	}

	const UGroomComponent* FGroomDataflowSimulationVisualization::GetGroomComponent(const FDataflowSimulationScene* SimulationScene) const
	{
		if (SimulationScene)
		{
			if (const TObjectPtr<AActor> PreviewActor = SimulationScene->GetPreviewActor())
			{
				return PreviewActor->GetComponentByClass<UGroomComponent>();
			}
		}
		return nullptr;
	}

	void FGroomDataflowSimulationVisualization::Draw(const FDataflowSimulationScene* SimulationScene, FPrimitiveDrawInterface* PDI)
	{
		
	}

	void FGroomDataflowSimulationVisualization::DrawCanvas(const FDataflowSimulationScene* SimulationScene, FCanvas* Canvas, const FSceneView* SceneView)
	{
		
	}

	FText FGroomDataflowSimulationVisualization::GetDisplayString(const FDataflowSimulationScene* SimulationScene) const
	{
		return FText();
	}

}  // End namespace UE::Groom

#undef LOCTEXT_NAMESPACE 
 