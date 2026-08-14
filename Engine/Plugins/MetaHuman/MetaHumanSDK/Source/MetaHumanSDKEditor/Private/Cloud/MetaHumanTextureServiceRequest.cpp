// Copyright Epic Games, Inc. All Rights Reserved.
#include "JsonDomBuilder.h"
#include "Misc/ScopeLock.h"
#include "Cloud/MetaHumanTextureSynthesisServiceRequest.h"
#include "Cloud/MetaHumanCloudServicesSettings.h"
#include "MetaHumanDdcUtils.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "ImageUtils.h"
#include "Logging/StructuredLog.h"
#include "Async/Async.h"
#include "Misc/EngineVersion.h"

#include "DerivedDataCacheInterface.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheModule.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataRequestTypes.h"
#include "DerivedDataValueId.h"
#include "Settings/EditorProjectSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanTextureSynthesisRequest, Log, All)

namespace UE::MetaHuman
{
#define MH_CLOUD_TEXTURE_SERVICE_API_VERSION "v1"

	namespace detail
	{
		class FTextureRequestContextBase : public FRequestContextBase
		{
		public:
			int32 Index = 0; // Index is not always used to build a request. For example, request using EBodyTextureType::Underwear_Basecolor
			int32 RequestedResolution = 1024;
			int32 TotalTextureCount = 0;

			FTextureRequestContextBase() = default;
			FTextureRequestContextBase(const FTextureRequestContextBase& Rhs)
				: Index(Rhs.Index),
				RequestedResolution(Rhs.RequestedResolution),
				TotalTextureCount(Rhs.TotalTextureCount)
			{
			}
		};

		template<typename EnumType>
		class TRequestContext : public FTextureRequestContextBase
		{
		public:
			TSharedPtr<THighFrequencyData<EnumType>> HighFrequencyData;
			EnumType Type;
			
			TRequestContext() = default;
			TRequestContext(const TRequestContext& Rhs)
				: FTextureRequestContextBase(Rhs),
				HighFrequencyData(Rhs.HighFrequencyData),
				Type(Rhs.Type)
			{
			}
		};

		using FFaceRequestContext = TRequestContext<EFaceTextureType>;
		using FBodyRequestContext = TRequestContext<EBodyTextureType>;
		
		enum ERequestCategory
		{
			Face,
			Body
		};
		
		// If any of the texture syntheis data we cache (DDC) changes in such a way as to invalidate old cache data this needs to be updated
#define TEXTURE_SYNTHESIS_DERIVEDDATA_VER TEXT("ef89c34db9044f019587aaaeb9a8eb67")
		FString GetCacheKey(ERequestCategory RequestCategory, int32 HighFrequency, int32 TypeIndex, int32 Resolution)
		{
			// if the data format provided by the TS service changes this version 
			// must also change in order to invalidate older DDC TS content
			return FString::Format(TEXT("UEMHCTS_{0}_{1}{2}{3}{4}"),
				{
					TEXTURE_SYNTHESIS_DERIVEDDATA_VER,
					static_cast<int32>(RequestCategory), HighFrequency, TypeIndex, Resolution >> 10u
				});
		}

		bool TryGetCachedData(TArray<uint8>& OutData, ERequestCategory RequestCategory, int32 Index, int32 TypeIndex, int32 Resolution)
		{
			const FString CacheKeyString = GetCacheKey(RequestCategory, Index, TypeIndex, Resolution);
			FSharedBuffer HighFrequencyDataBuffer = TryCacheFetch(CacheKeyString);
			if (!HighFrequencyDataBuffer.IsNull())
			{
				// in the cache, use it as is
				OutData.SetNumUninitialized(HighFrequencyDataBuffer.GetSize());
				FMemory::Memcpy(OutData.GetData(), HighFrequencyDataBuffer.GetData(), HighFrequencyDataBuffer.GetSize());
			}
			return !HighFrequencyDataBuffer.IsNull();
		}

		DECLARE_DELEGATE_OneParam(FExecuteRequestDelegate, FRequestContextBasePtr);
		DECLARE_DELEGATE(FOnUnauthorizedDelegate);
		template<uint8 CategoryId, 
			typename TTextureType, 
			typename THighFrequencyData, 
			typename TRequestParams>
			[[nodiscard]] TSharedPtr<THighFrequencyData> RequestTexturesAsyncImpl(TConstArrayView<int> TextureRequestIndices,
				TConstArrayView<TRequestParams> TextureTypesToRequest,
				FMetaHumanServiceRequestProgressDelegate& MetaHumanServiceRequestProgressDelegate,
				FExecuteRequestDelegate&& ExecuteRequestAsyncFunc,
				FOnUnauthorizedDelegate&& OnUnauthorizedDelegate)
			{
				check(TextureTypesToRequest.Num());
				check(TextureTypesToRequest.Num() == TextureRequestIndices.Num())
				TArray<int32> UncachedTexturesToRequest;
				UncachedTexturesToRequest.Reserve(TextureTypesToRequest.Num());
				TSharedPtr<THighFrequencyData> HighFrequencyData = MakeShared<THighFrequencyData>();
				// check if the cache contains some or all of the textures we will need and fetch the ones that are missing			
				if (!CacheAvailable())
				{
					for (int32 i = 0; i < TextureTypesToRequest.Num(); ++i)
					{
						UncachedTexturesToRequest.Add(i);
					}
				}
				else
				{
					int32 TextureIndex = 0;
					float Counter = 1.0f;
					for (const TRequestParams& RequestParams : TextureTypesToRequest)
					{
						TArray<uint8>& Data = HighFrequencyData->GetMutable(RequestParams.Type);
						if (TryGetCachedData(Data, static_cast<ERequestCategory>(CategoryId), TextureRequestIndices[TextureIndex], static_cast<int32>(RequestParams.Type), RequestParams.Resolution))
						{
							MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(Counter / TextureTypesToRequest.Num());
							Counter += 1.0f;
						}
						else
						{
							UncachedTexturesToRequest.Add(TextureIndex);
						}
						TextureIndex++;
					}
				}
				if (UncachedTexturesToRequest.Num() == 0)
				{
					// everything we needed was in the cache
					return HighFrequencyData;
				}
				else
				{
					using TRequestContext = TRequestContext<TTextureType>;
					TArray<TSharedPtr<TRequestContext>> TextureRequests;
					for (const int UncachedTextureIndex : UncachedTexturesToRequest)
					{
						const TRequestParams& RequestParams = TextureTypesToRequest[UncachedTextureIndex];
						TSharedPtr<TRequestContext> Request = MakeShared<TRequestContext>();
						Request->HighFrequencyData = HighFrequencyData;
						Request->Type = RequestParams.Type;
						Request->Index = TextureRequestIndices[UncachedTextureIndex];
						Request->RequestedResolution = RequestParams.Resolution;
						Request->TotalTextureCount = UncachedTexturesToRequest.Num();
						TextureRequests.Add(Request);
					}

					ServiceAuthentication::CheckHasLoggedInUserAsync(ServiceAuthentication::FOnCheckHasLoggedInUserCompleteDelegate::CreateLambda(
						[
							TextureRequests = MoveTemp(TextureRequests),
							ExecuteRequestAsyncFunc = MoveTemp(ExecuteRequestAsyncFunc),
							OnUnauthorizedDelegate = MoveTemp(OnUnauthorizedDelegate)
						]
						(bool bIsLoggedIn)
						{
							if (bIsLoggedIn)
							{
								// user is logged in; we're allowed to request textures from the service
								for (TSharedPtr<TRequestContext> Request : TextureRequests)
								{
									ExecuteRequestAsyncFunc.Execute(Request);
								}
							}
							else
							{
								// user is not logged in; we need to inform the user that they need to log in
								OnUnauthorizedDelegate.ExecuteIfBound();
							}
						}));
				}
				return {};
			}
	}
	using namespace detail;

	struct FTextureSynthesisServiceRequestBase::FImpl
	{
		union
		{
			FFaceTextureRequestCreateParams Face;
			FBodyTextureRequestCreateParams Body;
		}
		CreateParams;

		ERequestCategory RequestCategory;
		int32 CompletedRequestCount = 0;
		std::atomic<bool> bHasFailure = false;
	};

	TSharedRef<FFaceTextureSynthesisServiceRequest> FTextureSynthesisServiceRequestBase::CreateRequest(const FFaceTextureRequestCreateParams& Params)
	{
		TSharedRef<FFaceTextureSynthesisServiceRequest> Client = MakeShared<FFaceTextureSynthesisServiceRequest>();
		Client->Impl = MakePimpl<FImpl>();
		Client->Impl->RequestCategory = ERequestCategory::Face;
		Client->Impl->CreateParams.Face = Params;
		return Client;
	}

	TSharedRef<FBodyTextureSynthesisServiceRequest> FTextureSynthesisServiceRequestBase::CreateRequest(const FBodyTextureRequestCreateParams& Params)
	{
		TSharedRef<FBodyTextureSynthesisServiceRequest> Client = MakeShared<FBodyTextureSynthesisServiceRequest>();
		Client->Impl = MakePimpl<FImpl>();
		Client->Impl->RequestCategory = ERequestCategory::Body;
		Client->Impl->CreateParams.Body = Params;
		return Client;
	}

	bool FTextureSynthesisServiceRequestBase::DoBuildRequest(TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context)
	{
		check(Context.IsValid());
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();		
		FString RequestUrl = Settings->TextureSynthesisServiceUrl + "/" + MH_CLOUD_TEXTURE_SERVICE_API_VERSION + "/versions/" + FString::FromInt(FEngineVersion::Current().GetMajor()) + "." + FString::FromInt(FEngineVersion::Current().GetMinor()) + "/areas";
		if (!DoBuildRequestImpl(RequestUrl, HttpRequest, Context))
		{
			return false;
		}

		HttpRequest->SetURL(RequestUrl);
		HttpRequest->SetVerb("GET");
		HttpRequest->SetHeader("Content-Type", TEXT("application/json"));
		HttpRequest->SetHeader("Accept-Encoding", TEXT("gzip"));

		return true;
	}

	void FTextureSynthesisServiceRequestBase::OnRequestFailed(EMetaHumanServiceRequestResult Result, FRequestContextBasePtr Context)
	{
		FTextureRequestContextBase* RequestDetails = reinterpret_cast<FTextureRequestContextBase*>(Context.Get());
		++Impl->CompletedRequestCount;
		bool bHasFailure = false;
		if (Impl->bHasFailure.compare_exchange_weak(bHasFailure, true))
		{
			// only invoke this once - subsequent failures (and successes) are quieted
			FMetaHumanServiceRequestBase::OnRequestFailed(Result, Context);
		}
	}

	// ========================================================================================================= Face

	void FFaceTextureSynthesisServiceRequest::UpdateHighFrequencyFaceTextureCacheAsync(FRequestContextBasePtr Context)
	{
		FFaceRequestContext* RequestDetails = reinterpret_cast<FFaceRequestContext*>(Context.Get());
		const FString CacheKeyString = GetCacheKey(ERequestCategory::Face, Impl->CreateParams.Face.HighFrequency, static_cast<int32>(RequestDetails->Type), RequestDetails->RequestedResolution);
		TConstArrayView<uint8> Data = (*RequestDetails->HighFrequencyData)[RequestDetails->Type];
		check(Data.Num());
		const FSharedBuffer SharedBuffer = FSharedBuffer::Clone(Data.GetData(), Data.Num());
		UpdateCacheAsync(CacheKeyString, FSharedString(TEXT("MetaHumanTextureSynthesis")), SharedBuffer);
	}

	void FFaceTextureSynthesisServiceRequest::RequestTexturesAsync(TConstArrayView<FFaceTextureRequestParams> TexturesToRequestParams)
	{
		TArray<int32> HighFrequencyIndices;
		HighFrequencyIndices.Init(Impl->CreateParams.Face.HighFrequency, TexturesToRequestParams.Num());

		TSharedPtr<FFaceHighFrequencyData> HighFrequencyData;
		HighFrequencyData = RequestTexturesAsyncImpl<ERequestCategory::Face, EFaceTextureType, FFaceHighFrequencyData>(HighFrequencyIndices,
			TexturesToRequestParams,
			MetaHumanServiceRequestProgressDelegate,
			FExecuteRequestDelegate::CreateSPLambda(this, [this](FRequestContextBasePtr Context)
				{
					FFaceTextureSynthesisServiceRequest::ExecuteRequestAsync(Context);
				}),
			FOnUnauthorizedDelegate::CreateSPLambda(this, [this]()
				{
					FTextureSynthesisServiceRequestBase::OnRequestFailed(EMetaHumanServiceRequestResult::Unauthorized, nullptr);
				}
			));
		if (HighFrequencyData.IsValid())
		{
			FaceTextureSynthesisRequestCompleteDelegate.ExecuteIfBound(HighFrequencyData);
		}
	}
	
	bool FFaceTextureSynthesisServiceRequest::DoBuildRequestImpl(FString& InOutRequestUrl, TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context)
	{
		check(Context.IsValid());
		FFaceRequestContext* RequestDetails = reinterpret_cast<FFaceRequestContext*>(Context.Get());
		
		InOutRequestUrl += "/face/textureTypes";
		int32 AnimatedMapIndex = 0;
		bool bSupportedTextureType = true;
		switch (RequestDetails->Type)
		{
		case EFaceTextureType::Cavity:
			InOutRequestUrl += "/cavity";
			break;
		case EFaceTextureType::Normal:
		case EFaceTextureType::Normal_Animated_WM1:
		case EFaceTextureType::Normal_Animated_WM2:
		case EFaceTextureType::Normal_Animated_WM3:
			InOutRequestUrl += "/normal";
			AnimatedMapIndex = static_cast<int32>(RequestDetails->Type) - static_cast<int32>(EFaceTextureType::Normal);
			break;
		case EFaceTextureType::Basecolor:
		case EFaceTextureType::Basecolor_Animated_CM1:
		case EFaceTextureType::Basecolor_Animated_CM2:
		case EFaceTextureType::Basecolor_Animated_CM3:
			InOutRequestUrl += "/albedo";
			AnimatedMapIndex = static_cast<int32>(RequestDetails->Type) - static_cast<int32>(EFaceTextureType::Basecolor);
			break;
		default:
			bSupportedTextureType = false;
			break;
		}
		check(bSupportedTextureType);

		int32 HighFrequencyIndex = RequestDetails->Index;
		InOutRequestUrl += "/highFrequencyIds/" + FString::FromInt(HighFrequencyIndex);
		InOutRequestUrl += "/animatedMaps/" + FString::FromInt(AnimatedMapIndex);
		InOutRequestUrl += "/resolutions/" + FString::FromInt(RequestDetails->RequestedResolution >> 10u) + "k";

		return true;
	}

	void FFaceTextureSynthesisServiceRequest::OnRequestCompleted(const TArray<uint8>& Content, FRequestContextBasePtr Context)
	{
		FFaceRequestContext* RequestDetails = reinterpret_cast<FFaceRequestContext*>(Context.Get());
		{
			TArray<uint8>& Data = RequestDetails->HighFrequencyData->GetMutable(RequestDetails->Type);
			Data = Content;
			if ( Data.Num() )
			{
				if (!Impl->bHasFailure)
				{
					const bool bInvokeDelegate = ++Impl->CompletedRequestCount == RequestDetails->TotalTextureCount;
					MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(Impl->CompletedRequestCount / static_cast<float>(RequestDetails->TotalTextureCount));
					if (bInvokeDelegate)
					{
						TSharedRef<FFaceHighFrequencyData> Response(RequestDetails->HighFrequencyData.ToSharedRef());
						FaceTextureSynthesisRequestCompleteDelegate.ExecuteIfBound(Response);
					}
				}
				// cache anything that succeeded (even if other textures have failed)
				UpdateHighFrequencyFaceTextureCacheAsync(Context);
			}
			else
			{
				// we don't really have much context in this case, but something invalid has come back from the server
				OnRequestFailed(EMetaHumanServiceRequestResult::ServerError, Context);
			}
		}
	}

	// ========================================================================================================= Body

	void FBodyTextureSynthesisServiceRequest::RequestTexturesAsync(TConstArrayView<FBodyTextureRequestParams> TexturesToRequestParams)
	{
		TArray<int> TextureRequestIndices;
		for (const FBodyTextureRequestParams& RequestParams : TexturesToRequestParams)
		{
			switch (RequestParams.Type)
			{
			case EBodyTextureType::Body_Basecolor:
			case EBodyTextureType::Chest_Basecolor:
				TextureRequestIndices.Add(Impl->CreateParams.Body.Tone);
				break;
			case EBodyTextureType::Body_Normal:
			case EBodyTextureType::Body_Cavity:
			case EBodyTextureType::Chest_Normal:
			case EBodyTextureType::Chest_Cavity:
				TextureRequestIndices.Add(Impl->CreateParams.Body.SurfaceMap);
				break;
			default:
				TextureRequestIndices.Add(0);
				break;
			}
		}

		TSharedPtr<FBodyHighFrequencyData> BaseHighFrequencyData;
		BaseHighFrequencyData = RequestTexturesAsyncImpl<ERequestCategory::Body, EBodyTextureType, FBodyHighFrequencyData>(TextureRequestIndices,
			TexturesToRequestParams,
			MetaHumanServiceRequestProgressDelegate,
			FExecuteRequestDelegate::CreateSPLambda(this, [this](FRequestContextBasePtr Context)
				{
					FBodyTextureSynthesisServiceRequest::ExecuteRequestAsync(Context);
				}
			),
			FOnUnauthorizedDelegate::CreateSPLambda(this, [this]() 
				{
					FTextureSynthesisServiceRequestBase::OnRequestFailed(EMetaHumanServiceRequestResult::Unauthorized, nullptr);
				}
			));
		if (BaseHighFrequencyData.IsValid())
		{
			BodyTextureSynthesisRequestCompleteDelegate.ExecuteIfBound(BaseHighFrequencyData);
		}
	}

	bool FBodyTextureSynthesisServiceRequest::DoBuildRequestImpl(FString& InOutRequestUrl, TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context)
	{
		check(Context.IsValid());
		FBodyRequestContext* RequestDetails = reinterpret_cast<FBodyRequestContext*>(Context.Get());
		
		bool bSupportedTextureType = true;
		switch (RequestDetails->Type)
		{
		case EBodyTextureType::Body_Basecolor:
		case EBodyTextureType::Body_Normal:
		case EBodyTextureType::Body_Cavity:
		case EBodyTextureType::Body_Underwear_Basecolor:
		case EBodyTextureType::Body_Underwear_Normal:
		case EBodyTextureType::Body_Underwear_Mask:
			InOutRequestUrl += "/body/textureTypes";
			break;
		case EBodyTextureType::Chest_Basecolor:
		case EBodyTextureType::Chest_Normal:
		case EBodyTextureType::Chest_Cavity:
		case EBodyTextureType::Chest_Underwear_Basecolor:
		case EBodyTextureType::Chest_Underwear_Normal:
			InOutRequestUrl += "/chest/textureTypes";
			break;
		default:
			bSupportedTextureType = false;
			break;
		}

		switch (RequestDetails->Type)
		{
		case EBodyTextureType::Body_Basecolor:
		case EBodyTextureType::Chest_Basecolor:
			InOutRequestUrl += "/albedo/tones" ;
			break;
		case EBodyTextureType::Body_Normal:
		case EBodyTextureType::Chest_Normal:
			InOutRequestUrl += "/normal/surfaceMaps";
			break;
		case EBodyTextureType::Body_Cavity:
		case EBodyTextureType::Chest_Cavity:
			InOutRequestUrl += "/cavity/surfaceMaps";
			break;
		case EBodyTextureType::Body_Underwear_Basecolor:
		case EBodyTextureType::Body_Underwear_Normal:
		case EBodyTextureType::Body_Underwear_Mask:
		case EBodyTextureType::Chest_Underwear_Basecolor:
		case EBodyTextureType::Chest_Underwear_Normal:
			InOutRequestUrl += "/underwear/subTypes";
			break;
		default:
			bSupportedTextureType = false;
			break;
		}

		check(bSupportedTextureType);

		switch (RequestDetails->Type)
		{
		case EBodyTextureType::Body_Underwear_Basecolor:
		case EBodyTextureType::Chest_Underwear_Basecolor:
			InOutRequestUrl += "/albedo";
			break;
		case EBodyTextureType::Body_Underwear_Normal:
		case EBodyTextureType::Chest_Underwear_Normal:
			InOutRequestUrl += "/normal";
			break;
		case EBodyTextureType::Body_Underwear_Mask:
			InOutRequestUrl += "/mask";
			break;
		default:
			break;
		}

		switch (RequestDetails->Type)
		{
		case EBodyTextureType::Body_Basecolor:
		case EBodyTextureType::Body_Normal:
		case EBodyTextureType::Body_Cavity:
		case EBodyTextureType::Chest_Basecolor:
		case EBodyTextureType::Chest_Normal:
		case EBodyTextureType::Chest_Cavity:
			InOutRequestUrl += "/" + FString::FromInt(RequestDetails->Index);
			break;
		default:
			break;
		}
		
		InOutRequestUrl += "/resolutions/" + FString::FromInt(RequestDetails->RequestedResolution >> 10u) + "k";
		
		return true;
	}

	void FBodyTextureSynthesisServiceRequest::UpdateHighFrequencyBodyTextureCacheAsync(FRequestContextBasePtr Context)
	{
		FBodyRequestContext* RequestDetails = reinterpret_cast<FBodyRequestContext*>(Context.Get());
		const FString CacheKeyString = GetCacheKey(ERequestCategory::Body, RequestDetails->Index, static_cast<int32>(RequestDetails->Type), RequestDetails->RequestedResolution);
		TConstArrayView<uint8> Data = (*RequestDetails->HighFrequencyData)[RequestDetails->Type];
		check(Data.Num());
		const FSharedBuffer SharedBuffer = FSharedBuffer::Clone(Data.GetData(), Data.Num());
		UpdateCacheAsync(CacheKeyString, FSharedString(TEXT("MetaHumanTextureSynthesis")), SharedBuffer);
	}

	void FBodyTextureSynthesisServiceRequest::OnRequestCompleted(const TArray<uint8>& Content, FRequestContextBasePtr Context)
	{
		FBodyRequestContext* RequestDetails = reinterpret_cast<FBodyRequestContext*>(Context.Get());
		{
			TArray<uint8>& Data = RequestDetails->HighFrequencyData->GetMutable(RequestDetails->Type);
			Data = Content;
			if (Data.Num())
			{
				if (!Impl->bHasFailure)
				{
					const bool bInvokeDelegate = ++Impl->CompletedRequestCount == RequestDetails->TotalTextureCount;
					MetaHumanServiceRequestProgressDelegate.ExecuteIfBound(Impl->CompletedRequestCount / static_cast<float>(RequestDetails->TotalTextureCount));
					if (bInvokeDelegate)
					{
						TSharedRef<FBodyHighFrequencyData> Response(RequestDetails->HighFrequencyData.ToSharedRef());
						BodyTextureSynthesisRequestCompleteDelegate.ExecuteIfBound(Response);
					}
				}
				// cache anything that succeeded (even if other textures have failed)
				UpdateHighFrequencyBodyTextureCacheAsync(Context);
			}
			else
			{
				// we don't really have much context in this case, but something invalid has come back from the server
				OnRequestFailed(EMetaHumanServiceRequestResult::ServerError, Context);
			}
		}
	}
}
