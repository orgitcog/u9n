// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSceneParticleCustomization.h"

#include "ChaosVDCollisionDataDetailsTab.h"
#include "ChaosVDEngine.h"
#include "ChaosVDIndependentDetailsPanelManager.h"
#include "ChaosVDModule.h"
#include "ChaosVDObjectDetailsTab.h"
#include "ChaosVDSceneParticle.h"
#include "ChaosVDScene.h"
#include "ChaosVDTabsIDs.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SChaosVDCollisionDataInspector.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDSceneParticleCustomization::FChaosVDSceneParticleCustomization(const TWeakPtr<SChaosVDMainTab>& InMainTab)
{
	AllowedCategories.Add(FChaosVDSceneParticleCustomization::ParticleDataCategoryName);
	AllowedCategories.Add(FChaosVDSceneParticleCustomization::GeometryCategoryName);

	MainTabWeakPtr = InMainTab;

	ResetCachedView();
}

FChaosVDSceneParticleCustomization::~FChaosVDSceneParticleCustomization()
{
	RegisterCVDScene(nullptr);
}

TSharedRef<IDetailCustomization> FChaosVDSceneParticleCustomization::MakeInstance(TWeakPtr<SChaosVDMainTab> InMainTab)
{
	return MakeShared<FChaosVDSceneParticleCustomization>(InMainTab);
}


void FChaosVDSceneParticleCustomization::AddParticleDataButtons(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ParticleDataCategoryBuilder = DetailBuilder.EditCategory(ParticleDataCategoryName);

	ParticleDataCategoryBuilder.AddCustomRow(FText::GetEmpty()).
	                            WholeRowContent()
	[
		GenerateOpenInNewDetailsPanelButton().ToSharedRef()
	];

	const FText CollisionDataRowLabel = LOCTEXT("ParticleCollisionData", "Particle Collision Data");
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("CollisionData", CollisionDataRowLabel);

	CategoryBuilder.AddCustomRow(CollisionDataRowLabel).
	                WholeRowContent()
	[
		GenerateShowCollisionDataButton().ToSharedRef()
	];
}


void FChaosVDSceneParticleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FChaosVDDetailsCustomizationUtils::HideAllCategories(DetailBuilder, AllowedCategories);

	TSharedPtr<SChaosVDMainTab> MainTabPtr =  MainTabWeakPtr.Pin(); 
	TSharedPtr<FChaosVDScene> Scene = MainTabPtr ? MainTabPtr->GetChaosVDEngineInstance()->GetCurrentScene() : nullptr;

	RegisterCVDScene(Scene);

	if (!Scene)
	{
		ResetCachedView();
		return;
	}

	// We keep the particle data we need to visualize as a shared ptr because copying it each frame we advance/rewind to to an struct that lives in the particle actor it is not cheap.
	// Having a struct details view to which we set that pointer data each time the data in the particle is updated (meaning we assigned another ptr from the recording)
	// seems to be more expensive because it has to rebuild the entire layout from scratch.
	// So a middle ground I found is to have a Particle Data struct in this customization instance, which we add as external property. Then each time the particle data is updated we copy the data over.
	// This allows us to only perform the copy just for the particle that is being inspected and not every particle updated in that frame.

	TArray<TSharedPtr<FStructOnScope>> SelectedObjects;
	DetailBuilder.GetStructsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() > 0)
	{
		//TODO: Add support for multi-selection.
		if (!ensure(SelectedObjects.Num() == 1))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] [%d] objects were selected but this customization panel only support single object selection."), ANSI_TO_TCHAR(__FUNCTION__), SelectedObjects.Num())
		}
		
		FChaosVDSceneParticle* CurrentParticleInstance = CurrentObservedParticle;
		FChaosVDSceneParticle* SelectedParticleInstance = nullptr;

		TSharedPtr<FStructOnScope>& SelectedStruct = SelectedObjects[0];
		if (SelectedStruct->GetStruct() == FChaosVDSceneParticle::StaticStruct())
		{
			SelectedParticleInstance = reinterpret_cast<FChaosVDSceneParticle*>(SelectedStruct->GetStructMemory());
		}

		if (CurrentParticleInstance && CurrentParticleInstance != SelectedParticleInstance)
		{
			ResetCachedView();
		}
		
		if (SelectedParticleInstance)
		{
			UpdateObserverParticlePtr(SelectedParticleInstance);

			HandleSceneUpdated();

			TSharedPtr<IPropertyHandle> InspectedDataPropertyHandlePtr;

			if (TSharedPtr<FChaosVDInstancedMeshData> SelectedGeometryInstance = SelectedParticleInstance->GetSelectedMeshInstance().Pin())
			{
				InspectedDataPropertyHandlePtr = AddExternalStructure(CachedGeometryDataInstanceCopy, DetailBuilder, GeometryCategoryName, LOCTEXT("GeometryShapeDataStructName", "Geometry Shape Data"));

				IDetailCategoryBuilder& ParticleDataCategoryBuilder = DetailBuilder.EditCategory(GeometryCategoryName);

				ParticleDataCategoryBuilder.AddCustomRow(FText::GetEmpty()).
											WholeRowContent()
				[
					GenerateOpenInNewDetailsPanelButton().ToSharedRef()
				];
			}
			else
			{
				InspectedDataPropertyHandlePtr = AddExternalStructure(CachedParticleData, DetailBuilder, ParticleDataCategoryName, LOCTEXT("ParticleDataStructName", "Particle Data"));
				AddParticleDataButtons(DetailBuilder);
			}

			if (InspectedDataPropertyHandlePtr)
			{
				TSharedRef<IPropertyHandle> InspectedDataPropertyHandleRef = InspectedDataPropertyHandlePtr.ToSharedRef();
				FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties({&InspectedDataPropertyHandleRef, 1}, DetailBuilder);
			}
		}
	}
	else
	{
		ResetCachedView();
	}
}

void FChaosVDSceneParticleCustomization::HandleSceneUpdated()
{
	if (!CurrentObservedParticle)
	{
		ResetCachedView();
		return;
	}

	// If we have selected a mesh instance, the only data being added to the details panel is the Shape Instance data, so can just update that data here
	if (TSharedPtr<FChaosVDInstancedMeshData> SelectedGeometryInstance = CurrentObservedParticle->GetSelectedMeshInstance().Pin())
	{
		CurrentObservedParticle->VisitGeometryInstances([this, SelectedGeometryInstance](const TSharedRef<FChaosVDInstancedMeshData>& MeshDataHandle)
		{
			if (MeshDataHandle == SelectedGeometryInstance)
			{
				CachedGeometryDataInstanceCopy = MeshDataHandle->GetState();
			}
		});
	}
	else
	{
		TSharedPtr<const FChaosVDParticleDataWrapper> ParticleDataPtr = CurrentObservedParticle->GetParticleData();
		CachedParticleData = ParticleDataPtr ? *ParticleDataPtr : FChaosVDParticleDataWrapper();
	}
}

bool FChaosVDSceneParticleCustomization::GetCollisionDataButtonEnabled() const
{
	return CurrentObservedParticle && CurrentObservedParticle->HasCollisionData();
}

FReply FChaosVDSceneParticleCustomization::ShowCollisionDataForSelectedObject()
{
	if (!CurrentObservedParticle)
	{
		return FReply::Handled();
	}
	
	TSharedPtr<SChaosVDMainTab> OwningTabPtr = MainTabWeakPtr.Pin();
	if (!OwningTabPtr.IsValid())
	{
		return FReply::Handled();
	}

	if (const TSharedPtr<FChaosVDCollisionDataDetailsTab> CollisionDataTab = OwningTabPtr->GetTabSpawnerInstance<FChaosVDCollisionDataDetailsTab>(FChaosVDTabID::CollisionDataDetails).Pin())
	{
		if (const TSharedPtr<FTabManager> TabManager = OwningTabPtr->GetTabManager())
		{
			TabManager->TryInvokeTab(FChaosVDTabID::CollisionDataDetails);

			if (const TSharedPtr<SChaosVDCollisionDataInspector> CollisionInspector = CollisionDataTab->GetCollisionInspectorInstance().Pin())
			{
				CollisionInspector->SetCollisionDataListToInspect(CurrentObservedParticle->GetCollisionData());
			}
		}
	}

	return FReply::Handled();
}

FReply FChaosVDSceneParticleCustomization::OpenNewDetailsPanel()
{
	if (!CurrentObservedParticle)
	{
		return FReply::Handled();
	}
	
	TSharedPtr<SChaosVDMainTab> OwningTabPtr = MainTabWeakPtr.Pin();
	if (!OwningTabPtr.IsValid())
	{
		return FReply::Handled();
	}

	if (TSharedPtr<FChaosVDIndependentDetailsPanelManager> IndependentDetailsPanelManager = OwningTabPtr->GetIndependentDetailsPanelManager())
	{
		if (TSharedPtr<FChaosVDStandAloneObjectDetailsTab> DetailsTab = IndependentDetailsPanelManager->GetAvailableStandAloneDetailsPanelTab())
		{
			DetailsTab->SetStructToInspect(CurrentObservedParticle);
			return FReply::Handled();
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("OpenDetailsPanelError", "No (selection independent) Details Panel slot available.\n\nPlease close a panel and try again."));
		}
	}

	return FReply::Handled();
}

void FChaosVDSceneParticleCustomization::ResetCachedView()
{
	if (CurrentObservedParticle)
	{
		CurrentObservedParticle->ParticleDestroyedDelegate.Unbind();
	}

	CurrentObservedParticle = nullptr;
	CachedParticleData = FChaosVDParticleDataWrapper();
	CachedGeometryDataInstanceCopy = FChaosVDMeshDataInstanceState();
}

void FChaosVDSceneParticleCustomization::RegisterCVDScene(const TSharedPtr<FChaosVDScene>& InScene)
{
	TSharedPtr<FChaosVDScene> CurrentScene = SceneWeakPtr.Pin();
	if (InScene != CurrentScene)
	{
		if (CurrentScene)
		{
			CurrentScene->OnSceneUpdated().RemoveAll(this);
		}

		if (InScene)
		{
			InScene->OnSceneUpdated().AddSP(this, &FChaosVDSceneParticleCustomization::HandleSceneUpdated);
		}

		SceneWeakPtr = InScene;
	}
}

TSharedPtr<SWidget> FChaosVDSceneParticleCustomization::GenerateShowCollisionDataButton()
{
	TSharedPtr<SWidget> ShowCollisionButton = SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	.Padding(12.0f, 7.0f, 12.0f, 7.0f)
	.FillWidth(1.0f)
	[
		SNew(SButton)
		.ToolTip(SNew(SToolTip).Text(LOCTEXT("OpenCollisionDataDesc", "Click here to open the collision data for this particle on the collision data inspector.")))
		.IsEnabled_Raw(this, &FChaosVDSceneParticleCustomization::GetCollisionDataButtonEnabled)
		.ContentPadding(FMargin(0, 5.f, 0, 4.f))
		.OnClicked_Raw(this, &FChaosVDSceneParticleCustomization::ShowCollisionDataForSelectedObject)
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(FMargin(3, 0, 0, 0))
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallButtonText")
				.Text(LOCTEXT("ShowCollisionDataOnInspector","Show Collision Data in Inspector"))
			]
		]
	];

	return ShowCollisionButton;
}

TSharedPtr<SWidget> FChaosVDSceneParticleCustomization::GenerateOpenInNewDetailsPanelButton()
{
	TSharedPtr<SWidget> OpenButton = SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	.Padding(12.0f, 7.0f, 12.0f, 7.0f)
	.FillWidth(1.0f)
	[
		SNew(SButton)
		.ToolTip(SNew(SToolTip).Text(LOCTEXT("OpenDetailsPanelDesc", "Click here to open a new (selection independent) details panel for this particle.")))
		.ContentPadding(FMargin(0, 5.f, 0, 4.f))
		.OnClicked_Raw(this, &FChaosVDSceneParticleCustomization::OpenNewDetailsPanel)
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(FMargin(3, 0, 0, 0))
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallButtonText")
				.Text(LOCTEXT("OpenDetailsPanelText","Show Data in New Panel"))
			]
		]
	];

	return OpenButton;
}

void FChaosVDSceneParticleCustomization::UpdateObserverParticlePtr(FChaosVDSceneParticle* NewObservedParticle)
{
	if (CurrentObservedParticle)
	{
		CurrentObservedParticle->ParticleDestroyedDelegate.Unbind();
	}

	if (NewObservedParticle)
	{
		NewObservedParticle->ParticleDestroyedDelegate.BindSP(this, &FChaosVDSceneParticleCustomization::HandleObservedParticleInstanceDestroyed);
		CurrentObservedParticle = NewObservedParticle;	
	}
	else
	{
		ResetCachedView();
	}
}


void FChaosVDSceneParticleCustomization::HandleObservedParticleInstanceDestroyed()
{
	ResetCachedView();
}

#undef LOCTEXT_NAMESPACE
