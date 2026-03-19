// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceSettings.h"

#include "LiveLinkOpenTrackIOConnectionSettings.h"

#include "LiveLinkOpenTrackIOSourceSettings.generated.h"

UENUM()
enum class ELiveLinkOpenTrackIOTransformSubjects
{
	NoTransformSubjects					 UMETA(ToolTip = "Do not generate transform subjects from OpenTrackIO data."),
	EnableTransformSubjects				 UMETA(ToolTip = "Generate transform subjects and calculate final transform on the camera frame data."),
	EnableTransformSubjectsNoCameraXForm UMETA(ToolTip = "Generate transform subjects and do not calculate final transform on the camera frame data."),
};

UCLASS()
class LIVELINKOPENTRACKIO_API ULiveLinkOpenTrackIOSourceSettings : public ULiveLinkSourceSettings
{
	GENERATED_BODY()

public:

	/** Protocol being used for the connection. */
	UPROPERTY()
	ELiveLinkOpenTrackIONetworkProtocol Protocol = ELiveLinkOpenTrackIONetworkProtocol::Multicast;

	/**
	 * The Multicast port to receive packets on.
	 *
	 * The default value 55555 per OpenTrackIO specifications.
	 */
	UPROPERTY(EditAnywhere, Category = Transport, meta = (
		EditCondition = "Protocol == ELiveLinkOpenTrackIONetworkProtocol::Multicast",
		EditConditionHides
	))
	uint16 MulticastPort = 55555;

	/**
	 * The IP endpoint to listen to and send packets from.  
	 *
	 * The format is IP_ADDRESS:PORT_NUMBER.
	 * 0.0.0.0:0 will bind to the default network adapter on Windows,
	 * and all available network adapters on other operating systems.
	 *
	 */
	UPROPERTY(EditAnywhere, Category=Transport)
	FString UnicastEndpoint = TEXT("0.0.0.0:0");

	/**
	 * Emit subjects for each transform included in the OpenTrackIO channel.
	 */
	UPROPERTY(EditAnywhere, Category=Subjects)
	ELiveLinkOpenTrackIOTransformSubjects SubjectsPerTransform = ELiveLinkOpenTrackIOTransformSubjects::NoTransformSubjects;

	bool ShouldExtractTransformSubjects() const
	{
		return SubjectsPerTransform != ELiveLinkOpenTrackIOTransformSubjects::NoTransformSubjects;
	}

	bool ShouldApplyXformToCamera() const
	{
		return SubjectsPerTransform != ELiveLinkOpenTrackIOTransformSubjects::EnableTransformSubjectsNoCameraXForm;
	}
};
