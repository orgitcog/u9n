// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringView.h"
#include "IO/IoDispatcherBackend.h"
#include "Templates/UniquePtr.h"

struct FAnalyticsEventAttribute;

namespace UE::IoStore
{

class FOnDemandIoStore;
class FOnDemandHttpThread;
class IAnalyticsRecording;
class IIasCache;
struct FOnDemandStreamingCacheUsage;

struct FDistributedEndpointUrl
{
	FString EndpointUrl;
	FString FallbackUrl;

	bool IsValid() const
	{
		return !EndpointUrl.IsEmpty();
	}

	bool HasFallbackUrl() const
	{
		return !FallbackUrl.IsEmpty();
	}

	void Reset()
	{
		EndpointUrl.Empty();
		FallbackUrl.Empty();
	}
};

struct FOnDemandEndpointConfig
{
	FString DistributionUrl;
	FString FallbackUrl;

	TArray<FString> ServiceUrls;
	FString TocPath;
	FString TocFilePath;

	bool IsValid() const
	{
		return (DistributionUrl.Len() > 0 || ServiceUrls.Num() > 0) && TocPath.Len() > 0;
	}

	bool RequiresTls() const
	{
		auto IsHttps = [](FStringView Url) -> bool { return Url.StartsWith(TEXTVIEW("https"), ESearchCase::IgnoreCase); };

		if (IsHttps(DistributionUrl) || IsHttps(FallbackUrl))
		{
			return true;
		}
		for (const FString& Url : ServiceUrls)
		{
			if (IsHttps(Url))
			{
				return true;
			}
		}

		return false;
	}
};

class IOnDemandIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandIoDispatcherBackend() = default;

	virtual void SetBulkOptionalEnabled(bool bInEnabled) = 0;
	virtual void AbandonCache() = 0;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const = 0;
	virtual TUniquePtr<IAnalyticsRecording> StartAnalyticsRecording() const = 0;
	virtual FOnDemandStreamingCacheUsage GetCacheUsage() const = 0;
};

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(
	const FOnDemandEndpointConfig& Config,
	FOnDemandIoStore& IoStore,
	FOnDemandHttpThread& HttpClient,
	TUniquePtr<IIasCache>&& Cache);

} // namespace UE::IoStore
