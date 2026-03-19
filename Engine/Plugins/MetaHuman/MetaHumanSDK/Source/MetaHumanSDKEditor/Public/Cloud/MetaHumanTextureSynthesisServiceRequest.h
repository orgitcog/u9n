// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Cloud/MetaHumanServiceRequest.h"
#include "Templates/PimplPtr.h"
#include "MetaHumanTypes.h"

namespace UE::MetaHuman
{

	class FFaceTextureSynthesisServiceRequest;
	class FBodyTextureSynthesisServiceRequest;

	template<typename EnumType UE_REQUIRES(TIsEnumClass<EnumType>::Value)>
	class THighFrequencyData : public TSharedFromThis<THighFrequencyData<EnumType>, ESPMode::ThreadSafe>
	{
	public:
		THighFrequencyData()
		{
			FMemory::Memset(HighFrequencyDataArray, 0);
		}
		TConstArrayView<uint8> operator[](EnumType Type) const
		{
			const TArray<uint8>& Data = HighFrequencyDataArray[static_cast<size_t>(Type)];
			return MakeConstArrayView<uint8>(Data.GetData(), Data.Num());
		}
		TArray<uint8>& GetMutable(EnumType Type)
		{
			return HighFrequencyDataArray[static_cast<size_t>(Type)];
		}
	private:
		using FHighFrequencyDataArray = TStaticArray<TArray<uint8>, static_cast<uint32>(EnumType::Count)>;
		FHighFrequencyDataArray HighFrequencyDataArray;
	};

	template<typename EnumType UE_REQUIRES(TIsEnumClass<EnumType>::Value)>
	struct TTextureRequestParams
	{
		EnumType Type;
		int32 Resolution;
	};

	using FFaceHighFrequencyData = METAHUMANSDKEDITOR_API THighFrequencyData<EFaceTextureType>;
	using FBodyHighFrequencyData = METAHUMANSDKEDITOR_API THighFrequencyData<EBodyTextureType>;

	// creation parameters for a request for (potentially) multiple face textures for a given high frequency ID
	struct METAHUMANSDKEDITOR_API FFaceTextureRequestCreateParams
	{
		// must be in the range [0, FMetaHumanFaceTextureSynthesizer::GetMaxHighFrequencyIndex() - 1]
		int32 HighFrequency;
	};
	// creation parameters for a request for (potentially) multiple body textures
	struct METAHUMANSDKEDITOR_API FBodyTextureRequestCreateParams
	{
		int32 SurfaceMap;
		int32 Tone;
	};

	using FFaceTextureRequestParams = METAHUMANSDKEDITOR_API TTextureRequestParams<EFaceTextureType>;
	using FBodyTextureRequestParams = METAHUMANSDKEDITOR_API TTextureRequestParams<EBodyTextureType>;

	namespace detail
	{
		// base class for texture face, body, and chest synthesis request types
		class METAHUMANSDKEDITOR_API FTextureSynthesisServiceRequestBase
			: public FMetaHumanServiceRequestBase
		{
		public:
			// create a request instance that can live for long enough
			static TSharedRef<FFaceTextureSynthesisServiceRequest> CreateRequest(const FFaceTextureRequestCreateParams& Params);
			// create a request instance that can live for long enough
			static TSharedRef<FBodyTextureSynthesisServiceRequest> CreateRequest(const FBodyTextureRequestCreateParams& Params);
		protected:
			FTextureSynthesisServiceRequestBase() = default;
			struct FImpl;
			TPimplPtr<FImpl> Impl;

			virtual bool DoBuildRequest(TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context) override;
			virtual bool DoBuildRequestImpl(FString& InOutRequestUrl, TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context) = 0;
			virtual void OnRequestFailed(EMetaHumanServiceRequestResult Result, FRequestContextBasePtr MaybeContext) override;
		};
	}
	
	/*
	* A face texture synthesis service request
	*/
	class METAHUMANSDKEDITOR_API FFaceTextureSynthesisServiceRequest final
		: public detail::FTextureSynthesisServiceRequestBase
	{		
	public:
		FFaceTextureSynthesisServiceRequest() = default;

		// this delegate is invoked for each completed synthesis request
		DECLARE_DELEGATE_OneParam(FFaceTextureSynthesisRequestCompleteDelegate, TSharedPtr<FFaceHighFrequencyData> FaceHighFrequencyData);
		FFaceTextureSynthesisRequestCompleteDelegate FaceTextureSynthesisRequestCompleteDelegate;

		// Issue requests for the given list of texture types 
		// NOTE that only ONE success OR ONE failure callback will be invoked
		void RequestTexturesAsync(TConstArrayView<FFaceTextureRequestParams> TexturesToRequestParams);
		
	protected:
		virtual void OnRequestCompleted(const TArray<uint8>& Response, FRequestContextBasePtr Context) override;
		virtual bool DoBuildRequestImpl(FString& InOutRequestUrl, TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context) override;
	private:
		void UpdateHighFrequencyFaceTextureCacheAsync(FRequestContextBasePtr Context);
	};

	/*
	* A body texture synthesis service request
	*/
	class METAHUMANSDKEDITOR_API FBodyTextureSynthesisServiceRequest final
		: public detail::FTextureSynthesisServiceRequestBase
	{
	public:
		FBodyTextureSynthesisServiceRequest() = default;

		// this delegate is invoked for each completed synthesis request
		DECLARE_DELEGATE_OneParam(FBodyTextureSynthesisRequestCompleteDelegate, TSharedPtr<FBodyHighFrequencyData> BodyHighFrequencyData);
		FBodyTextureSynthesisRequestCompleteDelegate BodyTextureSynthesisRequestCompleteDelegate;

		// Issue requests for the given list of texture types 
		// NOTE that only ONE success OR ONE failure callback will be invoked
		void RequestTexturesAsync(TConstArrayView<FBodyTextureRequestParams> TexturesToRequestParams);

	protected:
		virtual void OnRequestCompleted(const TArray<uint8>& Response, FRequestContextBasePtr Context) override;
		virtual bool DoBuildRequestImpl(FString& InOutRequestUrl, TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context) override;
	private:
		void UpdateHighFrequencyBodyTextureCacheAsync(FRequestContextBasePtr Context);
	};
}
