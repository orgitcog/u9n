// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceSourceSettings.h"

#include "LiveLinkFaceSource.h"

#include "Internationalization/Regex.h"

const FString ULiveLinkFaceSourceSettings::IPAddressRegex = TEXT("^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$");

void ULiveLinkFaceSourceSettings::Init(FLiveLinkFaceSource* InSource, const FString& InConnectionString)
{
	Source = InSource;
	
	if (InConnectionString.IsEmpty())
	{
		return;
	}
	
	FString ParsedAddress, PortString;
	const bool bSplitSuccess = InConnectionString.Split(TEXT(":"), &ParsedAddress, &PortString);

	if (!bSplitSuccess)
	{
		UE_LOG(LogLiveLinkFaceSource, Error, TEXT("Failed to extract address and port from connection string: '%s'"), *InConnectionString)
		return;
	}

	const bool bIsIntegerPortString = PortString.IsNumeric() && !PortString.Contains(".");
	if (!bIsIntegerPortString)
	{
		UE_LOG(LogLiveLinkFaceSource, Error, TEXT("Port string '%s' contains non-integer characters."), *PortString)
		return;
	}

	Address = ParsedAddress;
	Port = FCString::Atoi(*PortString);
}

void ULiveLinkFaceSourceSettings::SetAddress(const FString& InAddress)
{
	Address = InAddress;
}

void ULiveLinkFaceSourceSettings::SetPort(const uint16 InPort)
{
	Port = InPort;
}

void ULiveLinkFaceSourceSettings::SetSubjectName(const FString& InSubjectName)
{
	SubjectName = InSubjectName;
}

bool ULiveLinkFaceSourceSettings::RequestConnect()
{
	check(Source);

	if (!IsAddressValid())
	{
		return false;
	}

	Source->Connect(this);

	UpdateConnectionString();

	return true;
}

bool ULiveLinkFaceSourceSettings::IsAddressValid() const
{
	const FRegexPattern RegexPattern = FRegexPattern(IPAddressRegex);
	FRegexMatcher RegexMatcher(RegexPattern, Address);
	return RegexMatcher.FindNext();
}

const FString& ULiveLinkFaceSourceSettings::GetAddress() const
{
	return Address;
}

const uint16 ULiveLinkFaceSourceSettings::GetPort() const
{
	return Port;
}

const FString& ULiveLinkFaceSourceSettings::GetSubjectName() const
{
	return SubjectName;
}

void ULiveLinkFaceSourceSettings::UpdateConnectionString()
{
	ConnectionString = FString::Format(TEXT("{0}:{1}"),{ Address, Port }); 
}
