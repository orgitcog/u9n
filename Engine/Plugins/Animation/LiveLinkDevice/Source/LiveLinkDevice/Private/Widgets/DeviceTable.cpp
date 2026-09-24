// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeviceTable.h"
#include "Engine/Engine.h"
#include "Framework/Docking/TabManager.h"
#include "LiveLinkDevice.h"
#include "LiveLinkDeviceCapability.h"
#include "LiveLinkDeviceModule.h"
#include "LiveLinkDeviceSubsystem.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "LiveLinkDevice"


// Intrinsic/"hard-coded" columns (i.e. not driven by a capability; should any of them be?)
const FName SLiveLinkDeviceTable::ColumnName_StatusIcon("StatusIcon");
const FName SLiveLinkDeviceTable::ColumnName_DisplayName("DisplayName");
const FName SLiveLinkDeviceTable::ColumnName_Remove("Remove");


FLiveLinkDeviceRowData::FLiveLinkDeviceRowData(ULiveLinkDevice* InDevice)
	: WeakDevice(InDevice)
{
}


void SLiveLinkDeviceTable::Construct(const FArguments& InArgs, TSharedRef<SDockTab> InContainingTab)
{
	WeakContainingTab = InContainingTab;

	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;

	ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();

	ChildSlot
	[
		SAssignNew(DeviceListView, SListView<TSharedPtr<FLiveLinkDeviceRowData>>)
		.SelectionMode(ESelectionMode::Single)
		.ListItemsSource(&DeviceRows)
		.OnSelectionChanged_Lambda(
			[this]
			(TSharedPtr<FLiveLinkDeviceRowData> InSelectedRow, ESelectInfo::Type InSelectType)
			{
				ULiveLinkDevice* Device = nullptr;
				if (InSelectedRow)
				{
					Device = InSelectedRow->WeakDevice.Get();
				}

				OnSelectionChangedDelegate.ExecuteIfBound(Device);
			}
		)
		.OnMouseButtonDoubleClick_Static(
			[]
			(TSharedPtr<FLiveLinkDeviceRowData> InData)
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FLiveLinkDeviceModule::DeviceDetailsTabName);
			}
		)
		.OnGenerateRow_Lambda(
			[]
			(TSharedPtr<FLiveLinkDeviceRowData> InData, const TSharedRef<STableViewBase>& InOwnerTableView)
			{
				return SNew(SLiveLinkDeviceRowWidget, InData, InOwnerTableView);
			}
		)
		.HeaderRow(
			SAssignNew(HeaderRow, SHeaderRow)
		)
	];

	RegenerateList();

	auto OnDevicesChangedHandler = [this](FGuid InDeviceId, ULiveLinkDevice* InDevice)
	{
		RegenerateRows();
	};

	Subsystem->OnDeviceAdded().AddSPLambda(this, OnDevicesChangedHandler);
	Subsystem->OnDeviceRemoved().AddSPLambda(this, OnDevicesChangedHandler);
}


SLiveLinkDeviceTable::~SLiveLinkDeviceTable()
{
	if (GEngine)
	{
		if (ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>())
		{
			Subsystem->OnDeviceAdded().RemoveAll(this);
			Subsystem->OnDeviceRemoved().RemoveAll(this);
		}
	}
}


void SLiveLinkDeviceTable::RegenerateList()
{
	RegenerateColumns();
	RegenerateRows();
}


void SLiveLinkDeviceTable::RegenerateColumns()
{
	HeaderRow->ClearColumns();

	HeaderRow->AddColumn(SHeaderRow::Column(ColumnName_StatusIcon)
		.DefaultLabel(FText::GetEmpty())
		.FillSized(30.0f)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center));

	HeaderRow->AddColumn(SHeaderRow::Column(ColumnName_DisplayName)
		.DefaultLabel(LOCTEXT("ColumnLabelDisplayName", "Name"))
		.FillWidth(1.0f));

	HeaderRow->AddColumn(SHeaderRow::Column(ColumnName_Remove)
		.DefaultLabel(FText::GetEmpty())
		.FillSized(30.0f)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center));

	// Add columns defined by capabilities.
	// TODO?: Hide columns if devices implementing that capability aren't present?
	ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();
	for (const TSubclassOf<ULiveLinkDeviceCapability>& Capability : Subsystem->GetKnownCapabilities())
	{
		ULiveLinkDeviceCapability* CapabilityCDO = Capability.GetDefaultObject();
		for (const TPair<FName, ULiveLinkDeviceCapability::FDeviceTableColumnDesc>& Column : CapabilityCDO->GetTableColumns())
		{
			const FName ColumnId = Column.Key;

			SHeaderRow::FColumn::FArguments ColumnArgs = SHeaderRow::Column(ColumnId);
			CapabilityCDO->GenerateHeaderForColumn(ColumnId, ColumnArgs);

			// Keep the "Remove Device" column at the end.
			const int32 InsertIdx = FMath::Max(0, HeaderRow->GetColumns().Num() - 1);

			HeaderRow->InsertColumn(ColumnArgs, InsertIdx);
		}
	}
}


void SLiveLinkDeviceTable::RegenerateRows()
{
	ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();

	const TMap<FGuid, TObjectPtr<ULiveLinkDevice>>& DeviceMap = Subsystem->GetDeviceMap();
	const int32 DeviceCount = DeviceMap.Num();
	DeviceRows.Empty(DeviceCount);
	for (const TPair<FGuid, TObjectPtr<ULiveLinkDevice>>& DevicePair : DeviceMap)
	{
		DeviceRows.Emplace(MakeShared<FLiveLinkDeviceRowData>(DevicePair.Value));
	}

	DeviceListView->RequestListRefresh();
}


void SLiveLinkDeviceRowWidget::Construct(
	const FArguments& InArgs,
	TSharedPtr<FLiveLinkDeviceRowData> InRowData,
	const TSharedRef<STableViewBase>& InOwnerTableView
)
{
	RowData = InRowData;

	FSuperRowType::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef<SWidget> SLiveLinkDeviceRowWidget::GenerateWidgetForColumn(const FName& InColumnId)
{
	ULiveLinkDevice* Device = GetDevice();

	if (!ensure(Device))
	{
		return SNullWidget::NullWidget;
	}

	if (InColumnId == SLiveLinkDeviceTable::ColumnName_StatusIcon)
	{
		return SNullWidget::NullWidget;
	}

	if (InColumnId == SLiveLinkDeviceTable::ColumnName_DisplayName)
	{
		return
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(STextBlock)
				.Text_Lambda([Device]() { return Device->GetDisplayName(); })
				.ToolTipText_Lambda([Device]() { return Device->GetDisplayName(); })
			];
	}

	if (InColumnId == SLiveLinkDeviceTable::ColumnName_Remove)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(LOCTEXT("RemoveDevice", "Remove selected Live Link device"))
			.ContentPadding(0.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.OnClicked_Lambda(
				[Device]
				{
					ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();
					Subsystem->RemoveDevice(Device);
					return FReply::Handled();
				})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Delete"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	// Below here is logic for capability-driven columns.
	ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();
	const TMap<FName, TSubclassOf<ULiveLinkDeviceCapability>>& ColumnIdToCapability = Subsystem->GetTableColumnIdToCapability();
	if (!ensure(ColumnIdToCapability.Contains(InColumnId)))
	{
		return SNullWidget::NullWidget;
	}

	const TSubclassOf<ULiveLinkDeviceCapability>& CapabilityClass = ColumnIdToCapability.FindChecked(InColumnId);
	if (!Device->GetClass()->ImplementsInterface(CapabilityClass))
	{
		// Device does not implement the capability responsible for populating this column.
		return SNullWidget::NullWidget;
	}

	FLiveLinkDeviceWidgetArguments Args = {
		.IsRowSelected = TDelegate<bool()>::CreateSPLambda(this, [this]() { return IsSelected(); })
	};

	return Device->GenerateWidgetForColumn(InColumnId, Args);
}


#undef LOCTEXT_NAMESPACE
