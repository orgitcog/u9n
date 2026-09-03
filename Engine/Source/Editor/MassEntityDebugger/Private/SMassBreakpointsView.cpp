// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMassBreakpointsView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SBoxPanel.h"
#include "MassDebugger.h"
#include "MassDebuggerModel.h"

#define LOCTEXT_NAMESPACE "SMassDebugger"


//----------------------------------------------------------------------//
// SMassBreakpointsView
//----------------------------------------------------------------------//
void SMassBreakpointsView::Construct(const FArguments& InArgs, TSharedRef<FMassDebuggerModel> InDebuggerModel)
{
#if WITH_MASSENTITY_DEBUG
	Initialize(InDebuggerModel);

	FMassDebugger::OnBreakpointsChangedDelegate.AddSP(this, &SMassBreakpointsView::RefreshBreakpoints);

	WriteBreakpointsListView = SNew(SListView<TSharedPtr<FBreakpointDisplay>>)
		.ListItemsSource(&WriteBreakpoints)
		.OnGenerateRow(this, &SMassBreakpointsView::OnGenerateBreakpointRow);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearAllBreakpoints", "Clear All Breakpoints"))
			.OnClicked(this, &SMassBreakpointsView::ClearBreakpointsClicked)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(5)
		[
			WriteBreakpointsListView.ToSharedRef()
		]
	];
#else
	ChildSlot
	[
		SNew(STextBlock)
			.Text(LOCTEXT("MassEntityDebuggingNotEnabled", "Mass Entity Debugging Not Enabled for this configuration"))
	];
#endif
}

void SMassBreakpointsView::OnRefresh()
{
	RefreshBreakpoints();
}

void SMassBreakpointsView::RefreshBreakpoints()
{
#if WITH_MASSENTITY_DEBUG
	WriteBreakpoints.Reset();
	if (DebuggerModel && DebuggerModel->Environment && DebuggerModel->Environment->EntityManager.IsValid())
	{
		FMassDebugger::FEnvironment* Env = FMassDebugger::FindEnvironmentForEntityManager(DebuggerModel->Environment->EntityManager.Pin().ToSharedRef().Get());
		if (Env == nullptr)
		{
			return;
		}

		TMultiMap<const UScriptStruct*, FMassEntityHandle>::TIterator FragmentIterator = Env->FragmentWriteBreakpoints.CreateIterator();
		for (; FragmentIterator; ++FragmentIterator)
		{
			TSharedPtr<FBreakpointDisplay> NewDisplay = MakeShared<FBreakpointDisplay>();
			NewDisplay->WriteFragment = FragmentIterator.Key();
			NewDisplay->Entity = FragmentIterator.Value();
			WriteBreakpoints.Add(NewDisplay);
		}
	}
	if (WriteBreakpointsListView)
	{
		WriteBreakpointsListView->RebuildList();
	}
#endif
}

FReply SMassBreakpointsView::HandleAddWriteBreakpointClicked()
{
#if WITH_MASSENTITY_DEBUG
	FMassDebugger::BreakOnFragmentWriteForSelectedEntity(DebuggerModel->GetSelectedFragment()); 
#endif
	return FReply::Handled();
}

FReply SMassBreakpointsView::ClearBreakpointsClicked()
{
#if WITH_MASSENTITY_DEBUG
	FMassDebugger::ClearAllBreakpoints();
#endif
	return FReply::Handled();
}

TSharedRef<ITableRow> SMassBreakpointsView::OnGenerateBreakpointRow(TSharedPtr<FBreakpointDisplay> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FMassEntityHandle>>, OwnerTable)
		.Content()
		[
#if WITH_MASSENTITY_DEBUG
            SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 2.0f)
            [
                // Add a button to remove the breakpoint
                SNew(SButton)
                .OnClicked_Lambda([this, InItem]()
                {
					if (DebuggerModel.IsValid() && DebuggerModel->Environment.IsValid())
					{
						const FMassEntityManager* EntityManager = DebuggerModel->Environment->EntityManager.Pin().Get();
						if (EntityManager)
						{
							if (InItem->Processor)
							{
								FMassDebugger::ClearProcessorBreakpoint(*EntityManager, InItem->Processor, InItem->Entity);
							}
							else if (InItem->WriteFragment)
							{
								FMassDebugger::ClearFragmentWriteBreak(*EntityManager, InItem->WriteFragment, InItem->Entity);
							}
						}
					}
                    return FReply::Handled();
                })
				[
					SNew(SImage)
						.Image(FCoreStyle::Get().GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseForeground())
				]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 2.0f)
            [
                // Display WriteFragment or Processor name
                SNew(STextBlock)
                .Text(InItem->WriteFragment 
                    ? FText::FromString(InItem->WriteFragment->GetName()) 
                    : (InItem->Processor 
                        ? FText::FromString(InItem->Processor->GetName()) 
                        : LOCTEXT("UnknownBreakpoint", "Unknown")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5.0f, 2.0f)
            [
                // Display Entity handle
                SNew(STextBlock)
				.Text(FText::FromString(InItem->Entity.DebugGetDescription()))
            ]
#else
			SNew(STextBlock)
				.Text(LOCTEXT("MassEntityDebuggingNotEnabled", "Mass Entity Debugging Not Enabled for this configuration"))
#endif
		];
}

#undef LOCTEXT_NAMESPACE

