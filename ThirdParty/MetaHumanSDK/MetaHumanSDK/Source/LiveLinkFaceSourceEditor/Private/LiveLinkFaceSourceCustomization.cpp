// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceSourceCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "LiveLinkFaceSourceSettings.h"
#include "SLiveLinkFaceDiscoveryPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LiveLinkFaceSourceCustomization"

FLiveLinkFaceSourceCustomization::FLiveLinkFaceSourceCustomization()
	: LiveLinkFaceDiscovery(MakeShared<FLiveLinkFaceDiscovery>())
{
	LiveLinkFaceDiscovery->OnServersUpdated.BindLambda([this](const TSet<FLiveLinkFaceDiscovery::FServer>& InServers)
	{
		ListServers.Reset();
		for (const FLiveLinkFaceDiscovery::FServer& Server : InServers)
		{
			ListServers.Add(MakeShared<FLiveLinkFaceDiscovery::FServer>(Server));
		}
		ListServers.Sort([](const TSharedPtr<FLiveLinkFaceDiscovery::FServer>& Server1, const TSharedPtr<FLiveLinkFaceDiscovery::FServer>& Server2)
		{
			if (Server1->Name == Server2->Name)
			{
				return Server1->Id < Server2->Id;
			}
			else
			{
				return Server1->Name < Server2->Name;
			}
		});

		if (DiscoveryPanel.IsValid())
		{
			DiscoveryPanel->Refresh();
		}
	});
	LiveLinkFaceDiscovery->Start();
}

FLiveLinkFaceSourceCustomization::~FLiveLinkFaceSourceCustomization()
{
	LiveLinkFaceDiscovery->OnServersUpdated.Unbind();
	LiveLinkFaceDiscovery->Stop();
}

void FLiveLinkFaceSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);
	check(Objects.Num() == 1);
	ULiveLinkFaceSourceSettings* Settings = Cast<ULiveLinkFaceSourceSettings>(Objects[0]);
	
	IDetailCategoryBuilder& CategoryBuilder = InDetailBuilder.EditCategory("Server", LOCTEXT("Server", "Server"), ECategoryPriority::Important);

	DiscoveryPanel = SNew(SLiveLinkFaceDiscoveryPanel)
		.Servers(&ListServers)
		.OnServerSingleClicked_Lambda([this, Settings](const FString& InServerHost, const uint16 InServerPort)
		{
			Settings->SetAddress(InServerHost);
			Settings->SetPort(InServerPort);
			Validate(Settings);
		})
		.OnServerDoubleClicked_Lambda([this, Settings](const FString& InServerHost, const uint16 InServerPort)
		{
			Settings->SetAddress(InServerHost);
			Settings->SetPort(InServerPort);
			if (Validate(Settings))
			{
				Settings->RequestConnect();
			}
		});
	
	CategoryBuilder.AddCustomRow(LOCTEXT("Discovery", "Discovery"))
	               .WholeRowContent()
	[
		DiscoveryPanel.ToSharedRef()
	];

	AddressTextBox = SNew(SEditableTextBox)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		
		.Text_Lambda([Settings]
		{
			return FText::FromString(Settings->GetAddress());
		})
		.OnTextChanged_Lambda([this, Settings](const FText& InAddress)
		{
			Settings->SetAddress(InAddress.ToString());
			Validate(Settings);
		});

	const FText AddressLabel = LOCTEXT("Address", "Address");
	CategoryBuilder.AddCustomRow(AddressLabel)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(LOCTEXT("AddressTooltip", "The network address of the server."))
		.Text(AddressLabel)
	]
	.ValueContent()
	[
		AddressTextBox.ToSharedRef()
	];

	PortEntryBox = SNew(SNumericEntryBox<uint16>)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Value_Lambda([Settings]
		{
			return Settings->GetPort();
		})
		.OnValueChanged_Lambda([this, Settings](const uint16 InPort)
		{
			Settings->SetPort(InPort);
			Validate(Settings);
		});

	const FText PortLabel = LOCTEXT("Port", "Port");
	CategoryBuilder.AddCustomRow(PortLabel)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(LOCTEXT("PortTooltip", "The control port of the server."))
		.Text(PortLabel)
	]
	.ValueContent()
	[
		PortEntryBox.ToSharedRef()
	];

	SubjectNameTextBox = SNew(SEditableTextBox)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text_Lambda([Settings]
		{
			return FText::FromString(Settings->GetSubjectName());
		})
		.OnTextChanged_Lambda([this, Settings](const FText& InSubjectName)
		{
			Settings->SetSubjectName(InSubjectName.ToString());
			Validate(Settings);
		});

	const FText SubjectNameLabel = LOCTEXT("SubjectName", "Subject Name");
	CategoryBuilder.AddCustomRow(SubjectNameLabel)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText(LOCTEXT("SubjectNameTooltip", "The subject name to assign to this animation stream."))
		.Text(SubjectNameLabel)
	]
	.ValueContent()
	[
		SubjectNameTextBox.ToSharedRef()
	];
	
	ButtonTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText");
	ButtonTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFont());

	CategoryBuilder.AddCustomRow(LOCTEXT("Connect", "Connect"))
	.ValueContent()
	[
		SNew(SButton)
		.Text(LOCTEXT("Connect", "Connect"))
		.TextStyle(&ButtonTextStyle)
		.HAlign(HAlign_Center)
		.IsEnabled_Lambda([this]
		{
			return bIsConnectEnabled;
		})
		.OnClicked_Lambda([Settings]()
		{
			Settings->RequestConnect();
			return FReply::Handled();
		})
	];
}

TSharedRef<IDetailCustomization> FLiveLinkFaceSourceCustomization::MakeInstance()
{
	return MakeShared<FLiveLinkFaceSourceCustomization>();
}

bool FLiveLinkFaceSourceCustomization::Validate(const ULiveLinkFaceSourceSettings* InSettings)
{
	if (ensure(IsValid(InSettings)))
	{
		const bool bIsAddressValid = InSettings->IsAddressValid();
		if (!bIsAddressValid)
		{
			AddressTextBox->SetError(LOCTEXT("InvalidAddress", "Invalid address"));
		}
		else
		{
			AddressTextBox->SetError(TEXT(""));
		}
		bIsConnectEnabled = bIsAddressValid;
		return bIsAddressValid;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
