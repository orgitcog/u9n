// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFaceDiscovery.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/App.h"
#include "Brushes/SlateColorBrush.h"

#include "Widgets/Input/SEditableTextBox.h"

namespace ESelectInfo { enum Type : int; }
template <typename ItemType> class SListView;

class ITableRow;
class STableViewBase;

DECLARE_DELEGATE_TwoParams(FOnLiveLinkFaceServerSelected, FString Address, uint16 Port);

class SLiveLinkFaceDiscoveryPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkFaceDiscoveryPanel) {}
		SLATE_ARGUMENT(TArray<TSharedPtr<FLiveLinkFaceDiscovery::FServer>>*, Servers)
		SLATE_EVENT(FOnLiveLinkFaceServerSelected, OnServerSingleClicked)
		SLATE_EVENT(FOnLiveLinkFaceServerSelected, OnServerDoubleClicked)
	SLATE_END_ARGS()

	SLiveLinkFaceDiscoveryPanel();

	void Construct(const FArguments& Args);

	void Refresh() const;

private:
	
	FSlateColorBrush DiscoveryListBorderBrush;
	
	/** The widgets displayed in the list. */
	TSharedPtr<SListView<TSharedPtr<FLiveLinkFaceDiscovery::FServer>>> ListView;
	
	/** A server in the list was single-clicked. */
	FOnLiveLinkFaceServerSelected OnServerSingleClicked;

	/** A server in the list was double-clicked. */
	FOnLiveLinkFaceServerSelected OnServerDoubleClicked;
	
};
