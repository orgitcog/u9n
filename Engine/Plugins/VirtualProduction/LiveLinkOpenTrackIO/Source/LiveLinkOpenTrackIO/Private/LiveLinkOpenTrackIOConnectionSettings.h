// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LiveLinkSourceSettings.h"

#include "LiveLinkOpenTrackIOConnectionSettings.generated.h"


// Pick between Multicast or Unicast for this connection
UENUM(BlueprintType)
enum class ELiveLinkOpenTrackIONetworkProtocol : uint8
{
	Multicast UMETA(DisplayName = "Multicast"),
	Unicast   UMETA(DisplayName = "Unicast")
};


USTRUCT()
struct LIVELINKOPENTRACKIO_API FLiveLinkOpenTrackIOConnectionSettings
{
	GENERATED_BODY()

	/** Using this is equivalent to leaving the SubjectName empty. */
	static constexpr TCHAR AutoSubjectName[] = TEXT("Auto");

	/** If empty or "Auto", one will be automatically generated. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FString SubjectName = AutoSubjectName;

	/** Protocol selection: Multicast (default) or Unicast. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	ELiveLinkOpenTrackIONetworkProtocol Protocol = ELiveLinkOpenTrackIONetworkProtocol::Multicast;

	/**
	 * The Source Number will be used as the last octet of the multicast address 235.135.1.[SourceNumber], per the opentrackio.org spec.
	 * Use values in the range [1,200].
	 */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (
		EditCondition = "Protocol == ELiveLinkOpenTrackIONetworkProtocol::Multicast",
		EditConditionHides,
		ClampMin = "1", ClampMax = "200",
		UIMin = "1", UIMax = "200"
		))
	uint8 SourceNumber = 1;

	/** Unicast port number. 0 will self assign, so it is suggested to pick a port that can be used in your system. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (
		EditCondition = "Protocol == ELiveLinkOpenTrackIONetworkProtocol::Unicast",
		EditConditionHides,
		ClampMin = "1", ClampMax = "65535",
		UIMin = "1", UIMax = "65535"
		))
	uint16 UnicastPort = 0;
};
