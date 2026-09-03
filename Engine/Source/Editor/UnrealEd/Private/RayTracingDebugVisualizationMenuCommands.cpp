// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingDebugVisualizationMenuCommands.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Containers/UnrealString.h"
#include "Framework/Commands/InputChord.h"
#include "Internationalization/Text.h"
#include "Templates/Function.h"
#include "RayTracingVisualizationData.h"
#include "Styling/AppStyle.h"
#include "EditorViewportClient.h"
#include "ToolMenu.h"

int32 GRayTracingVisualizeTiming = 0;
static FAutoConsoleVariableRef CVarRayTracingVisualizeTiming(
	TEXT("r.RayTracing.Visualize.Timing"),
	GRayTracingVisualizeTiming,
	TEXT("Include 'Timing' visualization modes in the in-editor 'Ray Tracing Debug' drop down menu.")
);

int32 GRayTracingVisualizeOther = 0;
static FAutoConsoleVariableRef CVarRayTracingVisualizeOther(
	TEXT("r.RayTracing.Visualize.Other"),
	GRayTracingVisualizeOther,
	TEXT("Include 'Other' visualization modes in the in-editor 'Ray Tracing Debug' drop down menu.")
);

#define LOCTEXT_NAMESPACE "RayTracingDebugVisualizationMenuCommands"

FRayTracingDebugVisualizationMenuCommands::FRayTracingDebugVisualizationMenuCommands()
	: TCommands<FRayTracingDebugVisualizationMenuCommands>
	(
		TEXT("RayTracingDebugVisualizationMenu"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "RayTracingMenu", "Ray Tracing Debug Visualization"), // Localized context name for displaying
		NAME_None, // Parent context name.  
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
{
}

void FRayTracingDebugVisualizationMenuCommands::BuildCommandMap()
{
	const FRayTracingVisualizationData& VisualizationData = GetRayTracingVisualizationData();
	const FRayTracingVisualizationData::TModeMap& ModeMap = VisualizationData.GetModeMap();

	CommandMap.Empty();
	for (FRayTracingVisualizationData::TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FRayTracingVisualizationData::FModeRecord& Entry = It.Value();

		if (Entry.bHiddenInEditor)
		{
			continue;
		}

		FVisualizationRecord& Record = CommandMap.Add(Entry.ModeName, FVisualizationRecord());
		Record.Name = Entry.ModeName;
		Record.Command = FUICommandInfoDecl(
			this->AsShared(),
			Entry.ModeName,
			Entry.ModeText,
			Entry.ModeDesc)
			.UserInterfaceType(EUserInterfaceActionType::RadioButton)
			.DefaultChord(FInputChord()
			);

		switch (Entry.ModeType)
		{
		case FRayTracingVisualizationData::FModeType::Overview:
			Record.Type = FVisualizationType::Overview;
			break;

		default:
		case FRayTracingVisualizationData::FModeType::Standard:
			Record.Type = FVisualizationType::Standard;
			break;

		case FRayTracingVisualizationData::FModeType::Performance:
			Record.Type = FVisualizationType::Performance;
			break;

		case FRayTracingVisualizationData::FModeType::Timing:
			Record.Type = FVisualizationType::Timing;
			break;

		case FRayTracingVisualizationData::FModeType::Other:
			Record.Type = FVisualizationType::Other;
			break;
		}
	}
}

// Deprecated. When removed, also remove AddCommandTypeToMenu(FMenuBuilder& Menu, const FVisualizationType Type)
void FRayTracingDebugVisualizationMenuCommands::BuildVisualisationSubMenu(FMenuBuilder& Menu)
{
	const bool bShowTiming = GRayTracingVisualizeTiming != 0;
	const bool bShowOther = GRayTracingVisualizeOther != 0;

	const FRayTracingDebugVisualizationMenuCommands& Commands = FRayTracingDebugVisualizationMenuCommands::Get();
	if (Commands.IsPopulated())
	{

		{
			Commands.AddCommandTypeToMenu(Menu, FVisualizationType::Overview);
		}

		{
			Menu.BeginSection("LevelViewportRayTracingVisualizationGeneral", LOCTEXT("RayTracingVisualizationGeneral", "General"));
			Commands.AddCommandTypeToMenu(Menu, FVisualizationType::Standard);
			Menu.EndSection();
		}

		{
			Menu.BeginSection("LevelViewportRayTracingVisualizationPerformance", LOCTEXT("RayTracingVisualizationPerformance", "Performance"));
			Commands.AddCommandTypeToMenu(Menu, FVisualizationType::Performance);
			Menu.EndSection();
		}

		if (bShowTiming)
		{
			Menu.BeginSection("LevelViewportRayTracingVisualizationTiming", LOCTEXT("RayTracingVisualizationTiming", "Timing"));
			Commands.AddCommandTypeToMenu(Menu, FVisualizationType::Timing);
			Menu.EndSection();
		}

		if (bShowOther)
		{
			Menu.BeginSection("LevelViewportRayTracingVisualizationOther", LOCTEXT("RayTracingVisualizationOther", "Other"));
			Commands.AddCommandTypeToMenu(Menu, FVisualizationType::Other);
			Menu.EndSection();
		}

		Menu.EndSection();
	}
}

void FRayTracingDebugVisualizationMenuCommands::BuildVisualisationSubMenu(UToolMenu* InMenu)
{
	const FRayTracingDebugVisualizationMenuCommands& Commands = FRayTracingDebugVisualizationMenuCommands::Get();
	if (!Commands.IsPopulated())
	{
		return;
	}

	{
		FToolMenuSection& UnnamedSection = InMenu->AddSection(NAME_None);
		Commands.AddCommandTypeToSection(UnnamedSection, FVisualizationType::Overview);
	}

	{
		FToolMenuSection& GeneralSection = InMenu->AddSection(
			"LevelViewportRayTracingVisualizationGeneral", LOCTEXT("RayTracingVisualizationGeneral", "General")
		);
		Commands.AddCommandTypeToSection(GeneralSection, FVisualizationType::Standard);
	}

	{
		FToolMenuSection& PerformanceSection = InMenu->AddSection(
			"LevelViewportRayTracingVisualizationPerformance",
			LOCTEXT("RayTracingVisualizationPerformance", "Performance")
		);
		Commands.AddCommandTypeToSection(PerformanceSection, FVisualizationType::Performance);
	}

	const bool bShowTiming = GRayTracingVisualizeTiming != 0;
	if (bShowTiming)
	{
		FToolMenuSection& TimingSection = InMenu->AddSection("LevelViewportRayTracingVisualizationTiming", LOCTEXT("RayTracingVisualizationTiming", "Timing"));
		Commands.AddCommandTypeToSection(TimingSection, FVisualizationType::Timing);
	}

	const bool bShowOther = GRayTracingVisualizeOther != 0;
	if (bShowOther)
	{
		FToolMenuSection& OtherSection = InMenu->AddSection("LevelViewportRayTracingVisualizationOther", LOCTEXT("RayTracingVisualizationOther", "Other"));
		Commands.AddCommandTypeToSection(OtherSection, FVisualizationType::Other);
	}
}

bool FRayTracingDebugVisualizationMenuCommands::AddCommandTypeToMenu(FMenuBuilder& Menu, const FVisualizationType Type) const
{
	bool bAddedCommands = false;

	const TVisualizationModeCommandMap& Commands = CommandMap;
	for (TCommandConstIterator It = CreateCommandConstIterator(); It; ++It)
	{
		const FVisualizationRecord& Record = It.Value();
		if (Record.Type == Type)
		{
			Menu.AddMenuEntry(Record.Command, NAME_None, Record.Command->GetLabel());
			bAddedCommands = true;
		}
	}

	return bAddedCommands;
}

bool FRayTracingDebugVisualizationMenuCommands::AddCommandTypeToSection(FToolMenuSection& InSection, const FVisualizationType Type) const
{
	bool bAddedCommands = false;

	for (TCommandConstIterator It = CreateCommandConstIterator(); It; ++It)
	{
		const FVisualizationRecord& Record = It.Value();
		if (Record.Type == Type)
		{
			InSection.AddMenuEntry(Record.Command, Record.Command->GetLabel());
			bAddedCommands = true;
		}
	}

	return bAddedCommands;
}

FRayTracingDebugVisualizationMenuCommands::TCommandConstIterator FRayTracingDebugVisualizationMenuCommands::CreateCommandConstIterator() const
{
	return CommandMap.CreateConstIterator();
}

void FRayTracingDebugVisualizationMenuCommands::RegisterCommands()
{
	BuildCommandMap();
}

void FRayTracingDebugVisualizationMenuCommands::BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const
{
	// Map Buffer visualization mode actions
	for (FRayTracingDebugVisualizationMenuCommands::TCommandConstIterator It = FRayTracingDebugVisualizationMenuCommands::Get().CreateCommandConstIterator(); It; ++It)
	{
		const FRayTracingDebugVisualizationMenuCommands::FVisualizationRecord& Record = It.Value();
		CommandList.MapAction(
			Record.Command,
			FExecuteAction::CreateStatic(&FRayTracingDebugVisualizationMenuCommands::ChangeRayTracingDebugVisualizationMode, Client.ToWeakPtr(), Record.Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FRayTracingDebugVisualizationMenuCommands::IsRayTracingDebugVisualizationModeSelected, Client.ToWeakPtr(), Record.Name)
		);
	}
}

void FRayTracingDebugVisualizationMenuCommands::ChangeRayTracingDebugVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		Client->ChangeRayTracingDebugVisualizationMode(InName);
	}
}

bool FRayTracingDebugVisualizationMenuCommands::IsRayTracingDebugVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		return Client->IsRayTracingDebugVisualizationModeSelected(InName);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
