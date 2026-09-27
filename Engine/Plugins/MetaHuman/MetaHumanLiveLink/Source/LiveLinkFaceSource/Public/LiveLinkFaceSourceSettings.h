// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceSettings.h"

#include "LiveLinkFaceSourceSettings.generated.h"

class FLiveLinkFaceSource;
class FRegexPattern;

UCLASS()
class LIVELINKFACESOURCE_API ULiveLinkFaceSourceSettings : public ULiveLinkSourceSettings
{
public:

	GENERATED_BODY()

	/** Initialize the source settings with the owning source and a connection string in the format 'ADDRESS:PORT' **/
	void Init(FLiveLinkFaceSource* InSource, const FString& InConnectionString);

	/** Update the stored address value **/
	void SetAddress(const FString& InAddress);

	/** Update the stored port value **/
	void SetPort(const uint16 InPort);

	/** Update the stored subject name value **/
	void SetSubjectName(const FString& InSubjectName);

	/** Request the owning Live Link source to connect to the server **/
	bool RequestConnect();

	/** Check whether the stored address is valid **/
	bool IsAddressValid() const;

	/** Get the stored address value **/
	const FString& GetAddress() const;

	/** Get the stored port value **/
	const uint16 GetPort() const;

	/** Get the store subject name value **/
	const FString& GetSubjectName() const;

private:

	static const FString IPAddressRegex;

	FLiveLinkFaceSource* Source = nullptr;
	FString Address;
	uint16 Port = 14785;
	FString SubjectName;

	void UpdateConnectionString();
	
};
