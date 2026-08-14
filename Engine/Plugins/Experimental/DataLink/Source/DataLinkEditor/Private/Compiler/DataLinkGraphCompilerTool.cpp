// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkGraphCompilerTool.h"
#include "DataLinkEditorStyle.h"
#include "DataLinkGraphAssetEditor.h"
#include "DataLinkGraphAssetToolkit.h"
#include "DataLinkGraphCommands.h"
#include "DataLinkGraphCompiler.h"
#include "DataLinkGraphEditorMenuContext.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "DataLinkGraphCompilerToolkit"

namespace UE::DataLinkEditor::Private
{
	const TMap<EDataLinkGraphCompileStatus, FLazyName> CompileStatusBackgrounds =
	{
		{ EDataLinkGraphCompileStatus::Unknown , TEXT("CompileStatus.Background.Unknown") },
		{ EDataLinkGraphCompileStatus::Warning , TEXT("CompileStatus.Background.Warning") },
		{ EDataLinkGraphCompileStatus::Error   , TEXT("CompileStatus.Background.Error")  },
		{ EDataLinkGraphCompileStatus::Good    , TEXT("CompileStatus.Background.Good")   },
	};

	const TMap<EDataLinkGraphCompileStatus, FLazyName> CompileStatusOverlays =
	{
		{ EDataLinkGraphCompileStatus::Unknown , TEXT("CompileStatus.Overlay.Unknown") },
		{ EDataLinkGraphCompileStatus::Warning , TEXT("CompileStatus.Overlay.Warning") },
		{ EDataLinkGraphCompileStatus::Error   , TEXT("CompileStatus.Overlay.Error")  },
		{ EDataLinkGraphCompileStatus::Good    , TEXT("CompileStatus.Overlay.Good")   },
	};
}

void FDataLinkGraphCompilerTool::ExtendMenu(UToolMenu* InMenu)
{
	FToolMenuSection& CompilerSection = InMenu->FindOrAddSection(TEXT("Compile")
		, TAttribute<FText>()
		, FToolMenuInsert(TEXT("Asset"), EToolMenuInsertType::After));

	CompilerSection.AddDynamicEntry(TEXT("Compiler")
		, FNewToolMenuSectionDelegate::CreateStatic(&FDataLinkGraphCompilerTool::ExtendDynamicCompilerSection));
}

FDataLinkGraphCompilerTool::FDataLinkGraphCompilerTool(UDataLinkGraphAssetEditor* InAssetEditor)
	: AssetEditor(InAssetEditor)
	, LastCompiledStatus(EDataLinkGraphCompileStatus::Unknown)
{
	UDataLinkEdGraph* EdGraph = AssetEditor->GetDataLinkEdGraph();
	if (EdGraph && EdGraph->IsCompiledGraphUpToDate())
	{
		LastCompiledStatus = EDataLinkGraphCompileStatus::Good;
	}
}

void FDataLinkGraphCompilerTool::BindCommands(const TSharedRef<FUICommandList>& InCommandList)
{
	const FDataLinkGraphCommands& GraphCommands = FDataLinkGraphCommands::Get();
	InCommandList->MapAction(GraphCommands.Compile, FExecuteAction::CreateSP(this, &FDataLinkGraphCompilerTool::Compile));
}

void FDataLinkGraphCompilerTool::Compile()
{
	FDataLinkGraphCompiler Compiler(AssetEditor->GetDataLinkGraph());
	LastCompiledStatus = Compiler.Compile();
}

void FDataLinkGraphCompilerTool::ExtendDynamicCompilerSection(FToolMenuSection& InSection)
{
	UDataLinkGraphEditorMenuContext* MenuContext = InSection.FindContext<UDataLinkGraphEditorMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	TSharedPtr<FDataLinkGraphAssetToolkit> Toolkit = MenuContext->ToolkitWeak.Pin();
	if (!Toolkit.IsValid())
	{
		return;
	}

	const FDataLinkGraphCommands& GraphCommands = FDataLinkGraphCommands::Get();

	InSection.AddEntry(FToolMenuEntry::InitToolBarButton(GraphCommands.Compile
		, TAttribute<FText>()
		, TAttribute<FText>()
		, TAttribute<FSlateIcon>::CreateSP(&Toolkit->GetCompilerTool(), &FDataLinkGraphCompilerTool::GetCompileIcon)));
}

FSlateIcon FDataLinkGraphCompilerTool::GetCompileIcon() const
{
	EDataLinkGraphCompileStatus CompileStatus = LastCompiledStatus;

	UDataLinkEdGraph* EdGraph = AssetEditor->GetDataLinkEdGraph();
	if (EdGraph && !EdGraph->IsCompiledGraphUpToDate())
	{
		CompileStatus = EDataLinkGraphCompileStatus::Unknown;
	}

	const FName Background = UE::DataLinkEditor::Private::CompileStatusBackgrounds[CompileStatus]; 
	const FName Overlay = UE::DataLinkEditor::Private::CompileStatusOverlays[CompileStatus]; 

	return FSlateIcon(FDataLinkEditorStyle::Get().GetStyleSetName(), Background, Background, Overlay);
}

#undef LOCTEXT_NAMESPACE
