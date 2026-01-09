// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSceneStateDebugControls.h"
#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SceneStateBlueprint.h"
#include "SceneStateBlueprintDelegates.h"
#include "SceneStateBlueprintEditor.h"
#include "SceneStateBlueprintEditorCommands.h"
#include "SceneStateDebugControlsObjectDetails.h"
#include "SceneStateDebugControlsTool.h"
#include "SceneStateDebugControlsToolbar.h"
#include "SceneStateEventTemplate.h"
#include "SceneStateEventUtils.h"
#include "SceneStateObject.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSceneStateDebugControls"

namespace UE::SceneState::Editor
{

void SDebugControls::Construct(const FArguments& InArgs, const TSharedRef<FSceneStateBlueprintEditor>& InBlueprintEditor)
{
	BlueprintEditorWeak = InBlueprintEditor;

	OnBlueprintDebugObjectChangedHandle = Graph::OnBlueprintDebugObjectChanged.AddSP(this, &SDebugControls::OnBlueprintDebugObjectChanged);

	DebugControlsTool = MakeShared<FDebugControlsTool>(InBlueprintEditor);
	DebugControlsTool->Initialize();

	DebugControlsDetailsView = CreateDebugControlsDetailsView(InBlueprintEditor);
	DebugControlsDetailsView->SetObject(DebugControlsTool->GetDebugControlsObject());

	ContentWidget = CreateContentWidget();
	PlaceholderWidget = CreatePlaceholderWidget();

	ChildSlot
	[
		SAssignNew(WidgetContainer, SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.Padding(0)
		[
			PlaceholderWidget.ToSharedRef()
		]
	];

	Refresh();
}

SDebugControls::~SDebugControls()
{
	Graph::OnBlueprintDebugObjectChanged.Remove(OnBlueprintDebugObjectChangedHandle);
	OnBlueprintDebugObjectChangedHandle.Reset();
}

void SDebugControls::Refresh()
{
	DebugControlsTool->UpdateDebuggedObject();
	DebugControlsDetailsView->ForceRefresh();
}

void SDebugControls::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
	DebugControlsTool->Tick(InDeltaTime);

	if (DebugControlsTool->IsAvailable())
	{
		WidgetContainer->SetContent(ContentWidget.ToSharedRef());
	}
	else
	{
		WidgetContainer->SetContent(PlaceholderWidget.ToSharedRef());
	}
}

TSharedRef<SWidget> SDebugControls::CreateContentWidget()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreateDebugControlsToolbar(DebugControlsTool->GetCommandList())
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			DebugControlsDetailsView.ToSharedRef()	
		];
}

TSharedRef<SWidget> SDebugControls::CreatePlaceholderWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(2, 24, 2, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PlaceholderTitle", "Select an actively playing debug object"))
			.Font(IDetailLayoutBuilder::GetDetailFontItalic())
		];
}

TSharedRef<IDetailsView> SDebugControls::CreateDebugControlsDetailsView(const TSharedRef<FSceneStateBlueprintEditor>& InBlueprintEditor) const
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.HostCommandList = InBlueprintEditor->GetToolkitCommands();
	DetailsViewArgs.HostTabManager = InBlueprintEditor->GetTabManager();
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->RegisterInstancedCustomPropertyLayout(USceneStateDebugControlsObject::StaticClass()
		, FOnGetDetailCustomizationInstance::CreateStatic(&FDebugControlsObjectDetails::MakeInstance));

	return DetailsView;
}

void SDebugControls::OnBlueprintDebugObjectChanged(const Graph::FBlueprintDebugObjectChange& InChange)
{
	TSharedPtr<FSceneStateBlueprintEditor> BlueprintEditor = BlueprintEditorWeak.Pin();
	if (BlueprintEditor.IsValid() && BlueprintEditor->GetBlueprintObj() == InChange.Blueprint)
	{
		Refresh();
	}
}

} // UE::SceneState::Editor

#undef LOCTEXT_NAMESPACE
