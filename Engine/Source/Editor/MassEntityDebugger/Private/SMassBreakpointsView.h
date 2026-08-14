// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SMassDebuggerViewBase.h"
#include "Widgets/Views/SListView.h"
#include "MassEntityHandle.h"

struct FMassDebuggerModel;
struct FMassDebuggerArchetypeData;
struct FMassDebuggerProcessorData;
class ITableRow;
class STableViewBase;
class UMassProcessor;

class SMassBreakpointsView : public SMassDebuggerViewBase
{
public:
	SLATE_BEGIN_ARGS(SMassBreakpointsView)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FMassDebuggerModel> InDebuggerModel);

protected:

	/** Unused pure virtual function */
	virtual void OnRefresh() override;

	/** Unused pure virtual function */
	virtual void OnProcessorsSelected(TConstArrayView<TSharedPtr<FMassDebuggerProcessorData>> SelectedProcessors, ESelectInfo::Type SelectInfo) override
	{
	}

	/** Unused pure virtual function */
	virtual void OnArchetypesSelected(TConstArrayView<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes, ESelectInfo::Type SelectInfo) override
	{
	}

private:
	FReply HandleAddWriteBreakpointClicked();
	FReply ClearBreakpointsClicked();

	struct FBreakpointDisplay
	{
		const UMassProcessor* Processor;
		const UScriptStruct* WriteFragment;
		FMassEntityHandle Entity;
	};
	TArray<TSharedPtr<FBreakpointDisplay>> WriteBreakpoints;
	TSharedRef<ITableRow> OnGenerateBreakpointRow(TSharedPtr<FBreakpointDisplay> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SListView<TSharedPtr<FBreakpointDisplay>>> WriteBreakpointsListView;

	void RefreshBreakpoints();
};
