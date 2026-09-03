// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkFaceDiscoveryPanel.h"

#include "DetailLayoutBuilder.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "LiveLinkFaceDiscoveryPanel"

namespace ServerPollUI
{
	const FName ServerName(TEXT("Name"));
	const FName ServerAddress(TEXT("Address"));
	const FName ServerPort(TEXT("Port"));
};

class SProviderPollRow : public SMultiColumnTableRow<TSharedPtr<FLiveLinkFaceDiscovery::FServer>>
{
public:
	SLATE_BEGIN_ARGS(SProviderPollRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FLiveLinkFaceDiscovery::FServer>, Server)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		Server = Args._Server;

		SMultiColumnTableRow::Construct(
			FSuperRowType::FArguments()
			.Padding(FMargin(4.0f, 4.0f)),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ServerPollUI::ServerName)
		{
			return SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString(Server->Name)); 
		}
		else if (ColumnName == ServerPollUI::ServerAddress)
		{
			return SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString(Server->Address));
		}
		else if (ColumnName == ServerPollUI::ServerPort)
		{
			return SNew(STextBlock) 
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString(FString::FromInt(Server->ControlPort)));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FLiveLinkFaceDiscovery::FServer> Server;
};

SLiveLinkFaceDiscoveryPanel::SLiveLinkFaceDiscoveryPanel()
: DiscoveryListBorderBrush(FSlateColorBrush(FSlateColor::UseForeground()))
{
}

void SLiveLinkFaceDiscoveryPanel::Construct(const FArguments& Args)
{
	OnServerSingleClicked = Args._OnServerSingleClicked;
	OnServerDoubleClicked = Args._OnServerDoubleClicked;

	ChildSlot
	[
		SNew(SBox)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		.MinDesiredHeight(58.0f)
		.MaxDesiredHeight(140.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FLiveLinkFaceDiscovery::FServer>>)
			.ListItemsSource(Args._Servers)
			.SelectionMode(ESelectionMode::SingleToggle)
			.OnGenerateRow_Lambda([](const TSharedPtr<FLiveLinkFaceDiscovery::FServer>& Server, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return SNew(SProviderPollRow, OwnerTable)
					.Server(Server);
			})
			.OnSelectionChanged_Lambda([this](const TSharedPtr<FLiveLinkFaceDiscovery::FServer>& Server, ESelectInfo::Type SelectionType)
			{
				if (Server)
				{
					OnServerSingleClicked.ExecuteIfBound(Server->Address, Server->ControlPort);
				}
			})
			.OnMouseButtonDoubleClick_Lambda([this](const TSharedPtr<FLiveLinkFaceDiscovery::FServer>& Server)
			{
				if (Server)
				{
					OnServerDoubleClicked.ExecuteIfBound(Server->Address, Server->ControlPort);
				}
			})
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ServerPollUI::ServerName)
				.DefaultLabel(LOCTEXT("ServerName", "Name"))
				.HeaderContentPadding(FMargin(6.0f))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ServerName", "Name"))
				]
				+ SHeaderRow::Column(ServerPollUI::ServerAddress)
				.DefaultLabel(LOCTEXT("ServerAddress", "Address"))
				.HeaderContentPadding(FMargin(6.0f))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ServerAddress", "Address"))
				]
				+ SHeaderRow::Column(ServerPollUI::ServerPort)
				.DefaultLabel(LOCTEXT("ServerPort", "Port"))
				.HeaderContentPadding(FMargin(6.0f))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ServerPort", "Port"))
				]
			)
		]
	];
}

void SLiveLinkFaceDiscoveryPanel::Refresh() const
{
	ListView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
