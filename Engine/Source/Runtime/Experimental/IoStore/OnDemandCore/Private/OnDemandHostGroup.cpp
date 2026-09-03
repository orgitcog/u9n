// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/OnDemandHostGroup.h"

#include "Containers/AnsiString.h"
#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "HAL/IConsoleManager.h"

namespace UE::IoStore
{

////////////////////////////////////////////////////////////////////////////////
namespace Config
{

bool bForceInsecureHttp = true;
static FAutoConsoleVariableRef CVar_ForceInsecureHttp(
	TEXT("iax.ForceInsecureHttp"),
	bForceInsecureHttp,
	TEXT("Whether to force the use of insecure HTTP"),
	ECVF_ReadOnly
);

} // UE::IoStore::Config

namespace Private
{

static bool ValidateUrl(FAnsiStringView Url, FString& Reason)
{
	//TODO: Add better validation
	return Url.StartsWith("http") || Url.StartsWith("https");
}

} // namespace Private

struct FOnDemandHostGroup::FImpl
{
	TArray<FAnsiString> HostUrls;
	int32				PrimaryIndex = INDEX_NONE;
};

FOnDemandHostGroup::FOnDemandHostGroup()
	: Impl(MakeShared<FImpl>())
{
}

FOnDemandHostGroup::FOnDemandHostGroup(FOnDemandHostGroup::FSharedImpl&& InImpl)
	: Impl(MoveTemp(InImpl))
{
}

FOnDemandHostGroup::~FOnDemandHostGroup()
{
}

TConstArrayView<FAnsiString> FOnDemandHostGroup::Hosts() const
{
	return Impl->HostUrls;
}

FAnsiStringView FOnDemandHostGroup::Host(int32 Index) const
{
	return Impl->HostUrls.IsEmpty() ? FAnsiStringView() : Impl->HostUrls[Index];
}

FAnsiStringView FOnDemandHostGroup::CycleHost(int32& InOutIndex) const
{
	InOutIndex = (InOutIndex + 1) % Impl->HostUrls.Num();
	return Host(InOutIndex);
}

void FOnDemandHostGroup::SetPrimaryHost(int32 Index)
{
	check(Index >= 0 && Index < Impl->HostUrls.Num() || Index == INDEX_NONE);
	Impl->PrimaryIndex = Index;
}

FAnsiStringView FOnDemandHostGroup::PrimaryHost() const
{
	if (Impl->PrimaryIndex != INDEX_NONE)
	{
		check(Impl->PrimaryIndex >= 0 && Impl->PrimaryIndex < Impl->HostUrls.Num());
		return Impl->HostUrls[Impl->PrimaryIndex];
	}

	return FAnsiStringView();
}

int32 FOnDemandHostGroup::PrimaryHostIndex() const
{
	return Impl->PrimaryIndex;
}

bool FOnDemandHostGroup::IsEmpty() const
{
	return Impl->HostUrls.IsEmpty();
}

TIoStatusOr<FOnDemandHostGroup> FOnDemandHostGroup::Create(FAnsiStringView Url)
{
	if (Url.EndsWith('/'))
	{
		Url.RemoveSuffix(1);
	}

	FString Reason;
	if (Private::ValidateUrl(Url, Reason) == false)
	{
		FIoStatus(EIoErrorCode::InvalidParameter, Reason);
	}

	FSharedImpl Impl = MakeShared<FImpl>();
	FAnsiString AnsiUrl(Url);
	if (Config::bForceInsecureHttp)
	{
		AnsiUrl.ReplaceInline("https", "http");
	}
	Impl->HostUrls.Add(MoveTemp(AnsiUrl));
	Impl->PrimaryIndex = 0;

	return FOnDemandHostGroup(MoveTemp(Impl)); 
}

TIoStatusOr<FOnDemandHostGroup> FOnDemandHostGroup::Create(FStringView Url)
{
	return Create(FAnsiString(StringCast<ANSICHAR>(Url.GetData(), Url.Len()))); 
}

TIoStatusOr<FOnDemandHostGroup> FOnDemandHostGroup::Create(TConstArrayView<FAnsiString> Urls)
{
	if (Urls.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("No URLs specified"));
	}

	FSharedImpl Impl = MakeShared<FImpl>();
	FString		Reason;

	for (const FAnsiString& Url : Urls)
	{
		FAnsiStringView UrlView = Url;

		if (UrlView.EndsWith('/'))
		{
			UrlView.RemoveSuffix(1);
		}

		if (Private::ValidateUrl(UrlView, Reason) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, Reason);
		}

		FAnsiString AnsiUrl(Url);
		if (Config::bForceInsecureHttp)
		{
			AnsiUrl.ReplaceInline("https", "http");
		}
		Impl->HostUrls.Add(MoveTemp(AnsiUrl));
	}
	Impl->PrimaryIndex = 0; 

	return FOnDemandHostGroup(MoveTemp(Impl)); 
}

TIoStatusOr<FOnDemandHostGroup> FOnDemandHostGroup::Create(TConstArrayView<FString> Urls)
{
	if (Urls.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("No URLs specified"));
	}

	FSharedImpl Impl = MakeShared<FImpl>();
	FString		Reason;

	for (const FString& Url : Urls)
	{
		FStringView UrlView = Url;

		if (UrlView.EndsWith(TEXT('/')))
		{
			UrlView.RemoveSuffix(1);
		}

		FAnsiString AnsiUrl(StringCast<ANSICHAR>(UrlView.GetData(), UrlView.Len()));

		if (Private::ValidateUrl(AnsiUrl, Reason) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, Reason);
		}

		if (Config::bForceInsecureHttp)
		{
			AnsiUrl.ReplaceInline("https", "http");
		}
		Impl->HostUrls.Emplace(MoveTemp(AnsiUrl));
	}
	Impl->PrimaryIndex = 0;

	return FOnDemandHostGroup(MoveTemp(Impl));
}

FName FOnDemandHostGroup::DefaultName = FName("Default");

} //namespace UE::IoStore
