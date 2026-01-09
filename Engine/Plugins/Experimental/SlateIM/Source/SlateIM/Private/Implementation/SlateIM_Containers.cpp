// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateIM.h"

#include "Containers/SImCompoundWidget.h"
#include "Containers/SImContextMenuAnchor.h"
#include "Containers/SImPopUp.h"
#include "Containers/SImScrollBox.h"
#include "Containers/SImStackBox.h"
#include "Containers/SImTableView.h"
#include "Containers/SImWrapBox.h"
#include "Misc/SlateIMLogging.h"
#include "Misc/SlateIMManager.h"
#include "Misc/SlateIMWidgetScope.h"

namespace SlateIM
{
	void BeginStack(EOrientation Orientation)
	{
		TSharedPtr<SImStackBox> ContainerWidget;

		// Note widget scope must fall out of scope before we push container or else we are pushing this new container into itself instead of into its parent
		{
			FWidgetScope<SImStackBox> Scope;
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				ContainerWidget = SNew(SImStackBox);
				Scope.UpdateWidget(ContainerWidget);
			}

			ContainerWidget->SetOrientation(Orientation);
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
	}

	void EndStack()
	{
		FSlateIMManager::Get().PopContainer<SImStackBox>();
	}

	void BeginWrap(EOrientation Orientation)
	{
		TSharedPtr<SImWrapBox> ContainerWidget;

		// Note widget scope must fall out of scope before we push container or else we are pushing this new container into itself instead of into its parent
		{
			FWidgetScope<SImWrapBox> Scope;
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				// Note, SNew cannot take a template as the argument because it converts the argument into a string for type identification. So we have to hard code this even though
				// its essentially the same as BeginHorizonal, just with a different type
				ContainerWidget = SNew(SImWrapBox);
				Scope.UpdateWidget(ContainerWidget);
			}

			ContainerWidget->SetUseAllottedSize(true);
			ContainerWidget->SetOrientation(Orientation);
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
	}

	void EndWrap()
	{
		FSlateIMManager::Get().PopContainer<SImWrapBox>();
	}

	void BeginHorizontalStack()
	{
		BeginStack(Orient_Horizontal);
	}

	void EndHorizontalStack()
	{
		EndStack();
	}

	void BeginVerticalStack()
	{
		BeginStack(Orient_Vertical);
	}

	void EndVerticalStack()
	{
		EndStack();
	}

	void BeginHorizontalWrap()
	{
		BeginWrap(Orient_Horizontal);
	}

	void EndHorizontalWrap()
	{
		EndWrap();
	}

	void BeginVerticalWrap()
	{
		BeginWrap(Orient_Vertical);
	}

	void EndVerticalWrap()
	{
		EndWrap();
	}

	void BeginBorder(const FSlateBrush* BackgroundImage, EOrientation Orientation, bool bAbsorbMouse, FMargin ContentPadding)
	{
		TSharedPtr<SImCompoundWidget> ContainerWidget;

		// Note widget scope must fall out of scope before we push container or else we are pushing this new container into itself instead of into its parent
		{
			FWidgetScope<SImCompoundWidget> Scope(ContentPadding, HAlign_Fill, VAlign_Fill, false);
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				ContainerWidget = SNew(SImCompoundWidget);
					
				Scope.UpdateWidget(ContainerWidget);
			}


			ContainerWidget->SetBackgroundImage(BackgroundImage);
			ContainerWidget->SetContentPadding(ContentPadding);
			ContainerWidget->SetAbsorbMouse(bAbsorbMouse);
			ContainerWidget->SetOrientation(Orientation);
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
	}

	void BeginBorder(const FName BorderStyleName, EOrientation Orientation, bool bAbsorbMouse, FMargin ContentPadding)
	{
		BeginBorder(FAppStyle::GetBrush(BorderStyleName), Orientation, bAbsorbMouse, ContentPadding);
	}

	void EndBorder()
	{
		FSlateIMManager::Get().PopContainer<SImCompoundWidget>();
	}

	bool BeginScrollBox(EOrientation Orientation)
	{
		TSharedPtr<SImScrollBox> ContainerWidget;

		bool bUserScrolled = false;
		{
			FWidgetScope<SImScrollBox> Scope(false);
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				ContainerWidget =
					SNew(SImScrollBox)
					.Orientation(Orientation)
					.OnUserScrolled_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()](float Offset)
					{
						FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
					});

				Scope.UpdateWidget(ContainerWidget);
			}
			else
			{
				bUserScrolled = Scope.IsActivatedThisFrame();
			}
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));

		return bUserScrolled;
	}

	void EndScrollBox()
	{
		FSlateIMManager::Get().PopContainer<SImScrollBox>();
	}

	void BeginPopUp(const FName BorderStyleName, EOrientation Orientation, bool bAbsorbMouse, FMargin ContentPadding)
	{
		BeginPopUp(FAppStyle::GetBrush(BorderStyleName), Orientation, bAbsorbMouse, ContentPadding);
	}

	void BeginPopUp(const FSlateBrush* BorderBrush, EOrientation Orientation, bool bAbsorbMouse, FMargin ContentPadding)
	{
		TSharedPtr<SImPopUp> ContainerWidget;
		{
			FWidgetScope<SImPopUp> Scope(FMargin(0));
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				ContainerWidget =
					SNew(SImPopUp)
					.ShowMenuBackground(false); 
				
				Scope.UpdateWidget(ContainerWidget);
			}

			// Setting focus by default will cause the pop-up to auto-close when it loses focus which would mean we couldn't have multiple pop-ups at once
			// nor could we open a pop-up in response to something else getting focus, so we don't focus the pop-up
			constexpr bool bIsOpen = true;
			constexpr bool bSetFocus = false;
			ContainerWidget->SetIsOpen(bIsOpen, bSetFocus);
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
		SlateIM::BeginBorder(BorderBrush, Orientation, bAbsorbMouse, ContentPadding);
	}

	void EndPopUp()
	{
		SlateIM::EndBorder();
		FSlateIMManager::Get().PopContainer<SImPopUp>();
	}

	void BeginTableRow()
	{
		// If we're in a row, we're adding a child row, otherwise it's a top-level row in the table
		TSharedPtr<SImTableView> Table = FSlateIMManager::Get().GetCurrentContainer<SImTableView>();
		TSharedPtr<FSlateIMTableRow> ParentRow = !Table.IsValid() ? FSlateIMManager::Get().GetCurrentContainer<FSlateIMTableRow>() : nullptr;
		if (ensureMsgf(Table || ParentRow, TEXT("Table Rows/Cells can only exist within Tables")))
		{
			TSharedPtr<FSlateIMTableRow> TableRow;
			// Note widget scope must fall out of scope before we push container or else we are pushing this new container into itself instead of into its parent
			{
				FWidgetScope<FSlateIMTableRow> Scope;
				TableRow = Scope.GetWidget();

				if (!TableRow)
				{
					TableRow = MakeShared<FSlateIMTableRow>();
					Scope.UpdateWidget(TableRow);
				}
			}

			FSlateIMManager::Get().PushContainer(FContainerNode(TableRow));
		}
	}

	void EndTableRow()
	{
		FSlateIMManager::Get().PopContainer<FSlateIMTableRow>();
	}
	
	void BeginTable(const FTableViewStyle* InStyle, const FTableRowStyle* InRowStyle)
	{
		TSharedPtr<SImTableView> ContainerWidget;

		// Note widget scope must fall out of scope before we push container or else we are pushing this new container into itself instead of into its parent
		{
			FWidgetScope<SImTableView> Scope;
			ContainerWidget = Scope.GetWidget();
			
			Scope.HashData(InStyle);

			if (!ContainerWidget)
			{
				ContainerWidget = SNew(SImTableView)
					.TreeViewStyle(InStyle)
					.SelectionMode(ESelectionMode::None);
				Scope.UpdateWidget(ContainerWidget);
			}
			else if (Scope.IsDataHashDirty())
			{
				ContainerWidget->SetStyle(InStyle);
			}

			ContainerWidget->SetTableRowStyle(InRowStyle);
			ContainerWidget->BeginTableUpdates();
		}

		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
	}

	void EndTable()
	{
		// If there's an open cell, close it
		if (TSharedPtr<SImStackBox> Cell = FSlateIMManager::Get().GetCurrentContainer<SImStackBox>())
		{
			SlateIM::EndVerticalStack();
		}

		TSharedPtr<FSlateIMTableRow> Row = FSlateIMManager::Get().GetCurrentContainer<FSlateIMTableRow>();
		TSharedPtr<SImTableView> Table = FSlateIMManager::Get().GetCurrentContainer<SImTableView>();
		ensureAlwaysMsgf((Row.IsValid() || Table.IsValid()), TEXT("Current container should be a row or a table - Is there a missing SlateIM::EndX() statement?"));
		if (Row)
		{
			EndTableRow();
		}

		// If we just ended a Row, then we need to fetch the table
		if (!Table.IsValid())
		{
			Table = FSlateIMManager::Get().GetCurrentContainer<SImTableView>();
		}
		
		if (ensureAlwaysMsgf(Table, TEXT("Current container should be a table - Is there a missing SlateIM::EndX() statement?")))
		{
			FSlateIMManager::Get().PopContainer<SImTableView>();
			Table->EndTableUpdates();
		}
	}

	void AddTableColumn(const FStringView& ColumnLabel)
	{
		if (TSharedPtr<SImTableView> Table = FSlateIMManager::Get().GetCurrentContainer<SImTableView>())
		{
			constexpr bool bAutoSize = false;
			const FSlateIMSlotData SlotData = FSlateIMManager::Get().GetCurrentAlignmentData(Defaults::Padding, HAlign_Fill, VAlign_Fill, bAutoSize
				, Defaults::MinWidth, Defaults::MinHeight, Defaults::MaxWidth, Defaults::MaxHeight);
			
			Table->AddColumn(ColumnLabel, FSlateIMManager::Get().GetCurrentRoot().CurrentToolTip, SlotData);
			
			FSlateIMManager::Get().GetMutableCurrentRoot().CurrentToolTip.Empty();
			FSlateIMManager::Get().ResetAlignmentData();
		}
	}

	void FixedTableColumnWidth(float Width)
	{
		SlateIM::AutoSize();
		SlateIM::MinWidth(Width);
		SlateIM::MaxWidth(Width);
	}

	void InitialTableColumnWidth(float Width)
	{
		SlateIM::AutoSize();
		SlateIM::MinWidth(Width);
	}

	bool NextTableCell()
	{
		SCOPED_NAMED_EVENT_TEXT("SlateIM::NextTableCell", FColorList::Goldenrod);
		
		// If there's an open cell, close it
		if (TSharedPtr<SImStackBox> Cell = FSlateIMManager::Get().GetCurrentContainer<SImStackBox>())
		{
			SlateIM::EndVerticalStack();
		}

		// We haven't drawn any table cells yet, begin the first row
		if (TSharedPtr<SImTableView> Table = FSlateIMManager::Get().GetCurrentContainer<SImTableView>())
		{
			Table->BeginTableContent();
			BeginTableRow();
		}

		TSharedPtr<FSlateIMTableRow> Row = FSlateIMManager::Get().GetCurrentContainer<FSlateIMTableRow>();
		if (ensure(Row))
		{
			const FContainerNode* ContainerNode = FSlateIMManager::Get().GetCurrentContainerNode();
			check(ContainerNode);
			
			// Start next row if we've filled all the columns in this row
			if (Row->GetColumnCount() == Row->CountCellWidgetsUpToIndex(ContainerNode->LastUsedChildIndex))
			{
				EndTableRow();
				BeginTableRow();
				Row = FSlateIMManager::Get().GetCurrentContainer<FSlateIMTableRow>();
				ensure(Row);
			}
			

			// Default table cells to fill (NextHAlign might have been set by the user, so we check first) 
			if (!FSlateIMManager::Get().NextHAlign.IsSet())
			{
				FSlateIMManager::Get().NextHAlign = HAlign_Fill;
			}
			// Create a vertical stack to act as our cell widget
			// TODO - Only automatically create a container when one is not created by the user
			SlateIM::BeginVerticalStack();
		}

		return Row && Row->AreTableRowContentsRequired();
	}

	bool BeginTableRowChildren()
	{
		// TODO - Let caller set default expansion state
		// TODO - Save and restore expansion state
		
		// If there's an open cell, close it
		if (TSharedPtr<SImStackBox> Cell = FSlateIMManager::Get().GetCurrentContainer<SImStackBox>())
		{
			SlateIM::EndVerticalStack();
		}

		TSharedPtr<FSlateIMTableRow> ParentRow = FSlateIMManager::Get().GetCurrentContainer<FSlateIMTableRow>();
		if (!ensureMsgf(ParentRow, TEXT("Child Table Rows can only be added to table rows. Did you forget to call NextTableCell()?")))
		{
			return false;
		}

		SlateIM::BeginTableRow();

		return ParentRow->IsExpanded();
	}

	void EndTableRowChildren()
	{
		// If there's an open cell, close it
		if (TSharedPtr<SImStackBox> Cell = FSlateIMManager::Get().GetCurrentContainer<SImStackBox>())
		{
			SlateIM::EndVerticalStack();
		}
		EndTableRow();
	}
}
