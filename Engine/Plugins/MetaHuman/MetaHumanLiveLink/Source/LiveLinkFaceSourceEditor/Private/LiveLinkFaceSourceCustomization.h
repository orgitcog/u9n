// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "LiveLinkFaceDiscovery.h"
#include "SLiveLinkFaceDiscoveryPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"

class ULiveLinkFaceSourceSettings;

class FLiveLinkFaceSourceCustomization : public IDetailCustomization
{
public:

	FLiveLinkFaceSourceCustomization();
	virtual ~FLiveLinkFaceSourceCustomization() override;

	// Start IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	// End IDetailCustomization Interface

	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	bool bIsConnectEnabled = false;

	FTextBlockStyle ButtonTextStyle;

	TSharedPtr<SLiveLinkFaceDiscoveryPanel> DiscoveryPanel;
	TSharedPtr<SEditableTextBox> AddressTextBox;
	TSharedPtr<SNumericEntryBox<uint16>> PortEntryBox;
	TSharedPtr<SEditableTextBox> SubjectNameTextBox;
	
	bool Validate(const ULiveLinkFaceSourceSettings* InSettings);

	TSharedRef<FLiveLinkFaceDiscovery> LiveLinkFaceDiscovery;
	
	TArray<TSharedPtr<FLiveLinkFaceDiscovery::FServer>> ListServers;

};
