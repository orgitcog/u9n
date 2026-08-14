// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemEditorViewportToolbarSections.h"

#include "EditorViewportCommands.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEditorStyle.h"
#include "SNiagaraSystemViewport.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"
#include "Widgets/Input/SSpinBox.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemEditorViewportToolbarSections"

FToolMenuEntry UE::NiagaraSystemEditor::CreateShowSubmenu()
{
	return UE::UnrealEd::CreateShowSubmenu(FNewToolMenuDelegate::CreateStatic(&UE::NiagaraSystemEditor::FillShowSubmenu, true));
}

void UE::NiagaraSystemEditor::FillShowSubmenu(UToolMenu* InMenu, bool bInShowViewportStatsToggle)
{
	const FNiagaraEditorCommands& Commands = FNiagaraEditorCommands::Get();

	if (bInShowViewportStatsToggle)
	{
		FToolMenuSection& UnnamedSection = InMenu->FindOrAddSection(NAME_None);
		UnnamedSection.AddSubMenu(
			"ViewportStats",
			LOCTEXT("ViewportStatsSubMenu", "Viewport Stats"),
			LOCTEXT("CameraSubmenuTooltip", "Camera options"),
			FNewToolMenuDelegate::CreateLambda(
				[](UToolMenu* Submenu) -> void
				{
					FToolMenuSection& CommonStatsSection =
						Submenu->FindOrAddSection("CommonStats", LOCTEXT("CommonStatsLabel", "Common Stats"));

					CommonStatsSection.AddMenuEntry(
						FEditorViewportCommands::Get().ToggleStats, LOCTEXT("ViewportStatsLabel", "Show Stats")
					);

					CommonStatsSection.AddSeparator(NAME_None);

					CommonStatsSection.AddMenuEntry(
						FEditorViewportCommands::Get().ToggleFPS, LOCTEXT("ViewportFPSLabel", "Show FPS")
					);
				}
			)
		);
	}

	FToolMenuSection& CommonShowFlagsSection =
		InMenu->FindOrAddSection("CommonShowFlags", LOCTEXT("CommonShowFlagsLabel", "Common Show Flags"));

	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleEmitterExecutionOrder);
	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleGpuTickInformation);
	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleInstructionCounts);
	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleMemoryInfo);
	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleParticleCounts);
	CommonShowFlagsSection.AddMenuEntry(Commands.ToggleStatelessInfo);
}

TSharedRef<SWidget> UE::NiagaraSystemEditor::CreateMotionMenuWidget(const TSharedRef<SNiagaraSystemViewport>& InNiagaraSystemEditorViewport
)
{
	// We generate a menu via UToolMenus, so we can use FillShowSubmenu call from both old and new toolbar
	FName OldShowMenuName = "NiagaraSystemEditor.OldViewportToolbar.Motion";

	if (!UToolMenus::Get()->IsMenuRegistered(OldShowMenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(OldShowMenuName, NAME_None, EMultiBoxType::Menu, false);
		Menu->AddDynamicSection(
			"BaseSection",
			FNewToolMenuDelegate::CreateLambda(
				[](UToolMenu* InMenu)
				{
					FToolMenuSection& UnnamedSection = InMenu->FindOrAddSection(NAME_None);
					UE::NiagaraSystemEditor::AddMotionSettingsToSection(UnnamedSection);
				}
			)
		);
	}

	FToolMenuContext MenuContext;
	{
		MenuContext.AppendCommandList(InNiagaraSystemEditorViewport->GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject =
				UE::UnrealEd::CreateViewportToolbarDefaultContext(InNiagaraSystemEditorViewport);

			MenuContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(OldShowMenuName, MenuContext);
}

void UE::NiagaraSystemEditor::FillSettingsSubmenu(UToolMenu* InMenu)
{
	FToolMenuSection& ViewportControlsSection =
		InMenu->FindOrAddSection("ViewportControls", LOCTEXT("ViewportControlsLabel", "Viewport Controls"));

	AddMotionSettingsToSection(ViewportControlsSection);
}

void UE::NiagaraSystemEditor::AddMotionSettingsToSection(FToolMenuSection& InSection)
{
	InSection.AddSubMenu(
		"MotionOptions",
		LOCTEXT("MotionOptionsSubMenu", "Motion Options"),
		LOCTEXT("MotionOptionsSubMenu_ToolTip", "Set Motion Options for the Niagara Component"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu)
			{
				FToolMenuSection& UnnamedSection = InMenu->FindOrAddSection(NAME_None);

				UnnamedSection.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleMotion);

				UUnrealEdViewportToolbarContext* ViewportToolbarContext =
					InMenu->FindContext<UUnrealEdViewportToolbarContext>();
				if (!ViewportToolbarContext)
				{
					return;
				}

				TSharedPtr<SNiagaraSystemViewport> NiagaraSystemViewport =
					StaticCastSharedPtr<SNiagaraSystemViewport>(ViewportToolbarContext->Viewport.Pin());
				if (!NiagaraSystemViewport)
				{
					return;
				}

				FToolMenuEntry MotionRateEntry = FToolMenuEntry::InitWidget(
					"MotionRate",
					SNew(SSpinBox<float>)
						.IsEnabled(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::IsMotionEnabled)
						.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.MinSliderValue(0.0f)
						.MaxSliderValue(360.0f)
						.Value(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::GetMotionRate)
						.OnValueChanged(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::SetMotionRate),
					LOCTEXT("MotionSpeed", "Motion Speed")
				);

				FToolMenuEntry MotionRadiusEntry = FToolMenuEntry::InitWidget(
					"MotionRadius",
					SNew(SSpinBox<float>)
						.IsEnabled(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::IsMotionEnabled)
						.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.MinSliderValue(0.0f)
						.MaxSliderValue(1000.0f)
						.Value(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::GetMotionRadius)
						.OnValueChanged(NiagaraSystemViewport.Get(), &SNiagaraSystemViewport::SetMotionRadius),
					LOCTEXT("MotionRadius", "Motion Radius")
				);

				UnnamedSection.AddEntry(MotionRateEntry);
				UnnamedSection.AddEntry(MotionRadiusEntry);
			}
		),
		false,
		FSlateIcon()
	);
}

void UE::NiagaraSystemEditor::ExtendPreviewSceneSettingsSubmenu(FName InSubmenuName)
{
	UToolMenu* const Submenu = UToolMenus::Get()->ExtendMenu(InSubmenuName);
	if (!Submenu)
	{
		return;
	}

	FToolMenuInsert AudioInsertPosition("AssetViewerProfileSelectionSection", EToolMenuInsertType::Before);
	FToolMenuSection& PreviewControlsSection = Submenu->FindOrAddSection(
		"AssetViewerPreviewControlsSection", LOCTEXT("AssetViewerPreviewControlsSectionLabel", "Preview Controls"), AudioInsertPosition
	);

	UE::NiagaraSystemEditor::AddMotionSettingsToSection(PreviewControlsSection);

	//Adds toggle origin axis to menu
	FToolMenuSection& ProfileOptionsSection = Submenu->FindOrAddSection(
		"PreviewSceneSettings", LOCTEXT("AssetViewerProfileOptionsSectionLabel", "Preview Scene Options")
	);
	ProfileOptionsSection.AddMenuEntry(
		FNiagaraEditorCommands::Get().ToggleOriginAxis, 
		TAttribute<FText>(), 
		TAttribute<FText>(),
		FSlateIcon(FNiagaraEditorStyle::Get().GetStyleSetName(), "NiagaraEditor.ToggleOriginAxis")
	);
}

#undef LOCTEXT_NAMESPACE
