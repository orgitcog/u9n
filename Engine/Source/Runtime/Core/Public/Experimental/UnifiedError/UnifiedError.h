// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Utf8String.h"
#include "Containers/StringView.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Logging/StructuredLog.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/RefCounting.h"


#include <type_traits>

template<typename>
struct deferred_false : std::false_type {};


template<typename T>
class TErrorStructFeatures
{
public:
	static const FAnsiStringView GetErrorDetailsTypeNameAsString()
	{
		static_assert(deferred_false<T>::value, "All types are required to define DECLARE_ERRORSTRUCT_FEATURES to be used as a error context type");
		return ANSITEXTVIEW("");
	}

	static const FAnsiStringView GetErrorContextTypeNameAsString()
	{
		static_assert(deferred_false<T>::value, "All types are required to define DECLARE_ERRORSTRUCT_FEATURES to be used as a error context type");
		return ANSITEXTVIEW("");
	}
};



#define DECLARE_ERRORSTRUCT_FEATURES(DetailsTypeName)\
	template<>\
	class TErrorStructFeatures<UE::UnifiedError::DetailsTypeName>\
	{\
	public:\
		static const FAnsiStringView GetErrorDetailsTypeNameAsString() \
		{\
			return ANSITEXTVIEW("TErrorDetails<"#DetailsTypeName">");\
		}\
		static const FAnsiStringView GetErrorContextTypeNameAsString() \
		{\
			return ANSITEXTVIEW(#DetailsTypeName);\
		}\
	};


#define DECLARE_ERROR_DETAILS_INTERNAL(DetailsNamespace, TypeName)\
	static uint32 StaticGetErrorDetailsTypeId() \
	{ \
		return StaticDetailsTypeId;\
	}\
	virtual uint32 GetErrorDetailsTypeId() const \
	{ \
		return StaticGetErrorDetailsTypeId(); \
	}\
	virtual const FAnsiStringView GetErrorDetailsTypeName() const override \
	{\
		return #DetailsNamespace"::"#TypeName;\
	}


#define DECLARE_FERROR_DETAILS_ABSTRACT(DetailsNamespace, TypeName)\
	public:\
	inline static const uint32 StaticDetailsTypeId = UE::UnifiedError::FErrorDetailsRegistry::Get().RegisterDetails(ANSITEXTVIEW(#DetailsNamespace "::" #TypeName), nullptr);\
	DECLARE_ERROR_DETAILS_INTERNAL(DetailsNamespace, TypeName);
	

#define DECLARE_FERROR_DETAILS(DetailsNamespace, TypeName)\
	public:\
	friend IErrorDetails* Create();\
	inline static IErrorDetails* Create()\
	{\
		return new TypeName();\
	}\
	inline static const uint32 StaticDetailsTypeId = UE::UnifiedError::FErrorDetailsRegistry::Get().RegisterDetails(ANSITEXTVIEW(#DetailsNamespace "::" #TypeName), TFunction<IErrorDetails* ()>([]() -> IErrorDetails* { return DetailsNamespace::TypeName::Create(); }));\
	DECLARE_ERROR_DETAILS_INTERNAL(DetailsNamespace, TypeName);



namespace UE::UnifiedError
{
	
	namespace DetailsTypes
	{
		inline constexpr int32 IERROR_DETAILS_TYPE = 1;
		inline constexpr int32 STATIC_ERROR_DETAILS_TYPE = 2;
		inline constexpr int32 DYNAMIC_ERROR_DETAILS_TYPE = 3;
		inline constexpr int32 FIRST_CUSTOM_DETAILS_TYPE = 4;
	}

	/// <summary>
	/// IErrorPropertyExtractor is an interface used to visit properties exposed by IErrorDetails implementations
	/// See also IErrorDetails::GetErrorDetails
	/// </summary>
	class IErrorPropertyExtractor
	{
	public:
		virtual void AddProperty(const FUtf8StringView& PropertyName, const FStringView& PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const FUtf8StringView& PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const FText& PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const int64 PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const int32 PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const float PropertyValue) = 0;
		virtual void AddProperty(const FUtf8StringView& PropertyName, const double PropertyValue) = 0;
	};

	class FError;

	class IErrorDetails : public IRefCountedObject
	{
	public:
		virtual ~IErrorDetails() = default;

		/// <summary>
		/// GetErrorFormatString; specifies the default error format string to be used when generating FError::GetErrorMessage.
		///	 The format string can specify any property exposed by any encapsulated IErrorDetails::GetErrorProperties.
		///  Example: GetErrorProperties adds Name:"ModuleId" Value:10. GetErrorFormatString returns "Module id was {ModuleId}".  Result "Module id was 10".
		/// </summary>
		/// <param name="Error"></param>
		/// <returns></returns>
		virtual const FText GetErrorFormatString(const FError& Error) const = 0;

		/// <summary>
		/// GetErrorProperties; used to expose error properties in name:value format, Error properties can be used for string formating functions, searching, exposing context to higher level stacks
		///	 See also: FError::GetErrorMessage, IErrorDetails::GetErrorFormatString, FError::GetDetailByKey
		/// </summary>
		/// <param name="Error"></param>
		/// <param name="OutProperties"></param>
		virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const = 0;

		/// <summary>
		/// GetInnerErrorDetails; Exposes inner error details to FError, if this ErrorDetails allows inner details
		/// </summary>
		/// <returns></returns>
		virtual TRefCountPtr<const IErrorDetails> GetInnerErrorDetails() const { return nullptr; }

		/// <summary>
		/// SetInnerErrorDetails; Exposes inner error details to FError, if this ErrorDetails allows inner details
		/// </summary>
		/// <returns></returns>
		virtual void SetInnerErrorDetails(TRefCountPtr<const IErrorDetails> ErrorDetails) { checkf(false, TEXT("SetInnerErrorDetails not implemented!")); }

		/// <summary>
		/// GetErrorDetialsTypeId; Simple type information for error details, generated using hash of details name
		///  See also: #define FERROR_DETAILS
		/// </summary>
		/// <returns></returns>
		virtual uint32 GetErrorDetailsTypeId() const = 0;


		virtual const FAnsiStringView GetErrorDetailsTypeName() const = 0;
		virtual void SerializeForLog(FCbWriter& Writer) const {}

	};


	class FErrorDetailsRegistry
	{
	private:
		TMap<uint32, TFunction<IErrorDetails* ()>> CreateFunctions;

		FErrorDetailsRegistry() { }
	public:
		static FErrorDetailsRegistry& Get()
		{
			static FErrorDetailsRegistry Registry;
			return Registry;
		}

		CORE_API uint32 RegisterDetails(const FAnsiStringView& ErrorDetailsName, TFunction<IErrorDetails*()> CreationFunction);
	};


	/// <summary>
	/// FRefCountedErrorDetails; base implementation of refcounting for IErrorDetails, this is used for heap allocated IErrorDetails implementations
	/// </summary>
	class FRefCountedErrorDetails : public IErrorDetails, public FRefCountBase
	{
		DECLARE_FERROR_DETAILS_ABSTRACT(UnifiedError, FRefCountedErrorDetails);


		CORE_API virtual ~FRefCountedErrorDetails();

		virtual FReturnedRefCountValue AddRef() const final { return FRefCountBase::AddRef(); }
		virtual uint32 Release() const final { return FRefCountBase::Release(); }
		virtual uint32 GetRefCount() const final { return FRefCountBase::GetRefCount(); }

		/*using FRefCountBase::AddRef;
		using FRefCountBase::Release;
		using FRefCountBase::GetRefCount;*/
	};


	/// <summary>
	/// FDynamicErrorDetails; base implementation of inner error details, for use by derived classes to reduce unnessisary reimplementation 
	/// </summary>
	class FDynamicErrorDetails : public FRefCountedErrorDetails
	{
		DECLARE_FERROR_DETAILS_ABSTRACT(UnifiedError, FDynamicErrorDetails);
	private:
		TRefCountPtr<const IErrorDetails> InnerErrorDetails;
	public:
		CORE_API FDynamicErrorDetails(TRefCountPtr<const IErrorDetails> InInnerErrorDetails = nullptr);
		CORE_API virtual ~FDynamicErrorDetails();


		// IErrorDetails functions
		CORE_API virtual void SetInnerErrorDetails(TRefCountPtr<const IErrorDetails> InInnerErrorDetails);
		virtual TRefCountPtr<const IErrorDetails> GetInnerErrorDetails() const { return InnerErrorDetails; }

		/// <summary>
		/// GetErrorFormatString; Pass through to the InnerErrorDetails.
		/// </summary>
		/// <param name="Error"></param>
		/// <returns></returns>
		CORE_API virtual const FText GetErrorFormatString(const FError& Error) const override;

		/// <summary>
		/// GetErrorProperties; by default pass through to the InnerErrorDetails.
		///  It's expected FDynamicErrorDetails will be derived from and implement GetErrorProperties. 
		/// </summary>
		/// <param name="Error"></param>
		/// <param name="OutProperties"></param>
		CORE_API virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const override;
	};


	/// <summary>
	/// FStaticErrorDetails; static error details and members are statically allocated
	///  Every error which uses DEFINE_ERROR will have FStaticErrorDetails generated for it
	///  Can not rely on it to be available for every error as some Error conversion functions will not use pregenerated errors or error codesF
	///  Use FError::GetErrorDetails to discover FStaticErrorDetails
	/// </summary>
	class FStaticErrorDetails : public IErrorDetails
	{
		DECLARE_FERROR_DETAILS_ABSTRACT(UnifiedError,FStaticErrorDetails);
	private:
		const FAnsiStringView& ErrorName;
		const FAnsiStringView& ModuleName;
		FText ErrorFormatString;
	public:
		// TODO: convert AnsiStringView to UTF8StringView when c++ 20 is supported
		CORE_API FStaticErrorDetails(const FAnsiStringView& InErrorName, const FAnsiStringView& InModuleName, const FText& InErrorFormatString);

		virtual ~FStaticErrorDetails() = default;

		/// <summary>
		/// GetErrorCodeString; Accessor for ErrorName.  
		///  Can be called directly on FStaticErrorDetails object.  
		///  See also: FError::GetErrorDetails
		/// </summary>
		/// <returns></returns>
		CORE_API const FAnsiStringView& GetErrorCodeString() const;

		/// <summary>
		/// GetModuleIdString; accessor for ModuleName.
		///  Can be called directly on FStaticErrorDetails object
		///  See also: FError::GetErrorDetails
		/// </summary>
		CORE_API const FAnsiStringView& GetModuleIdString() const;

		// IErrorDetails implementation

		/// <summary>
		/// GetErrorProperties;  Adds error properties exposed by this error details object to the OutProperties value
		///  FStaticErrorDetails includes error details all errors have ErrorCode, ModuleId, ErrorCodeString, ModuleIdString
		///  These can be used in any error format string see also GetErrorFormatString
		/// </summary>
		/// <param name="Error"></param>
		/// <param name="OutProperties"></param>
		CORE_API virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const;

		/// <summary>
		/// GetErrorFormatString; return the localized format text generated in DECLARE_ERROR macro.
		/// </summary>
		/// <param name="Error"></param>
		/// <returns></returns>
		CORE_API virtual const FText GetErrorFormatString(const FError& Error) const final;


		// IRefCountedObject implementation
		//  FStaticErrorDetails is statically allocated, make sure it is never released.
		virtual FReturnedRefCountValue AddRef() const final { return FReturnedRefCountValue(10); }
		virtual uint32 Release() const final { return 10; }
		virtual uint32 GetRefCount() const final { return 10; }
	};


	class FError;
} // UE::UnifiedError

template <typename ValueType>
inline void GatherPropertiesForError(const UE::UnifiedError::FError& Writer, const ValueType& Value, UE::UnifiedError::IErrorPropertyExtractor& Extractor)
{
	static_assert(deferred_false<ValueType>::value, "undefined function, define your own function pls");
}

namespace UE::UnifiedError
{

	template<typename T>
	class TErrorDetails : public FDynamicErrorDetails
	{
	public:
		TErrorDetails() {}
		TErrorDetails(T&& InErrorDetail)
		{
			ErrorDetail = MoveTemp(InErrorDetail);
		}
		TErrorDetails(T&& InErrorDetail, TRefCountPtr<const IErrorDetails> InInnerErrorDetails) : FDynamicErrorDetails(InInnerErrorDetails)
		{
			ErrorDetail = MoveTemp(InErrorDetail);
		}

		friend IErrorDetails* Create();
		inline static IErrorDetails* Create()
		{
			return new TErrorDetails<T>();
		}
		inline static const uint32 StaticDetailsTypeId = UE::UnifiedError::FErrorDetailsRegistry::Get().RegisterDetails(TErrorStructFeatures<T>::GetNameAsString(), TFunction<IErrorDetails * ()>([]() -> IErrorDetails* { return Create(); }));
		static uint32 StaticGetErrorDetailsTypeId()
		{
			return StaticDetailsTypeId;
		}
		virtual uint32 GetErrorDetailsTypeId() const
		{
			return StaticGetErrorDetailsTypeId();
		}
	private:
		T ErrorDetail;

		FAnsiStringView GetTypeNameAsString() const
		{
			return TErrorStructFeatures<T>::GetErrorDetailsTypeNameAsString();
		}
	public:
		const T& GetErrorContext() const
		{
			return &ErrorDetail;
		}

		virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const override
		{
			GatherPropertiesForError(Error, ErrorDetail, OutProperties);
			
			FDynamicErrorDetails::GetErrorProperties(Error, OutProperties);
		}
		virtual void SerializeForLog(FCbWriter& Writer) const override
		{
			::SerializeForLog(Writer, ErrorDetail);
		}

		const T& GetValue() const
		{
			return ErrorDetail;
		}

	};


	class FError
	{
	private: 
		int32 ErrorCode;
		int32 ModuleId;
		TRefCountPtr<const IErrorDetails> ErrorDetails;
	public:
		FError(int32 InModuleId, int32 InErrorCode, const IErrorDetails* InErrorDetails)
		{
			ErrorCode = InErrorCode;
			ModuleId = InModuleId;
			ErrorDetails = InErrorDetails;
		}
	public:
		FError(FError&& InError)
		{
			ErrorCode = InError.ErrorCode;
			InError.ErrorCode = 0;
			ModuleId = InError.ModuleId;
			InError.ModuleId = 0;
			if (InError.ErrorDetails)
			{
				ErrorDetails = InError.ErrorDetails;
				InError.ErrorDetails = nullptr;
			}
		}

		FError(const FError& InError)
		{
			ErrorCode = InError.ErrorCode;
			ModuleId = InError.ModuleId;
			ErrorDetails = InError.ErrorDetails;
		}

		~FError()
		{
			Invalidate();
		}

		CORE_API void GetErrorProperties(IErrorPropertyExtractor& Visitor) const;

		CORE_API int32 GetErrorCode() const;
		CORE_API int32 GetModuleId() const;

		CORE_API const FUtf8String GetErrorCodeString() const;
		CORE_API const FUtf8String GetModuleIdString() const;

		CORE_API void PushErrorDetails(TRefCountPtr<IErrorDetails> InErrorDetails);
		template<typename DetailType>
		TRefCountPtr<const DetailType> GetErrorDetails() const
		{
			TRefCountPtr<const IErrorDetails> CurrentIt = ErrorDetails;
			while (CurrentIt != nullptr)
			{
				if (CurrentIt->GetErrorDetailsTypeId() == DetailType::StaticGetErrorDetailsTypeId())
				{
					return TRefCountPtr<const DetailType>((const DetailType*)CurrentIt.GetReference());
				}
				CurrentIt = CurrentIt->GetInnerErrorDetails();
			}
			return nullptr;
		}

		CORE_API FText GetErrorMessage(bool bAppendContext = false) const;
		CORE_API FText GetFormatErrorText() const;
		template<typename T>
		void PushErrorContext(T&& InErrorStruct)
		{
			PushErrorDetails(TRefCountPtr<IErrorDetails>(new TErrorDetails<T>(MoveTemp(InErrorStruct))));
		}

		template<typename T>
		const T* GetErrorContext() const
		{
			TRefCountPtr<const IErrorDetails> CurrentIt = ErrorDetails;
			while (CurrentIt != nullptr)
			{
				if (CurrentIt->GetErrorDetailsTypeName() == TErrorStructFeatures<T>::GetErrorDetailsTypeNameAsString())
				{
					const TErrorDetails<const T>* Details = (const TErrorDetails<const T>*)(CurrentIt.GetReference());
					return &Details->GetErrorContext();
				}
				CurrentIt = CurrentIt->GetInnerErrorDetails();
			}
			return nullptr;
		}

		CORE_API void SerializeDetailsForLog(FCbWriter& Writer) const;

		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, FString& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, FUtf8String& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, FText& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, int64& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, int32& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, double& Result) const;
		CORE_API bool GetDetailByKey(const FUtf8StringView& KeyName, float& Result) const;

		FORCEINLINE bool IsValid() const
		{
			return (ErrorCode != 0) && (ModuleId != 0);
		}

		FORCEINLINE void Invalidate()
		{
			ErrorCode = 0;
			ModuleId = 0;
			ErrorDetails = nullptr;
		}
	private:
		friend bool operator==(const FError& Error, const FError& OtherError)
		{
			if ((Error.ModuleId == OtherError.ModuleId) &&
				(Error.ErrorCode == OtherError.ErrorCode))
			{
				return true;
			}
			return false;
		}

		TRefCountPtr<const IErrorDetails> GetInnerMostErrorDetails() const;
	};


	class FErrorRegistry
	{
	public:
		static FErrorRegistry& Get()
		{
			static FErrorRegistry StaticRegistry;
			return StaticRegistry;
		}


	private:

		FErrorRegistry() {}

	public:
		inline uint32 RegisterModule(const FStringView& ModuleName)
		{
			// todo: need to replace this with a stable hashing function
			uint32 ModuleId = GetTypeHash(ModuleName);
			checkf(ModuleNameMap.Contains(ModuleId) == false, TEXT("Module %s and %s are trying to register under module id %d"), ModuleName.GetData(), *ModuleNameMap.FindRef(ModuleId), ModuleId);
			ModuleNameMap.Add(ModuleId, ModuleName.GetData());
			return ModuleId;
		}

		inline int32 RegisterErrorCode(const FStringView& ErrorName, int32 ModuleId, int32 ErrorCode)
		{
			TPair<int32, int32> CombinedErrorId(ModuleId, ErrorCode);
			checkf(ErrorCodeNameMap.Contains(CombinedErrorId) == false, TEXT("Error %s and %s are trying to register under same error code moduleid:%d errorcode:%d"), ErrorName.GetData(), *ErrorCodeNameMap.FindRef(CombinedErrorId), ModuleId, ErrorCode);
			ErrorCodeNameMap.Add(CombinedErrorId, ErrorName.GetData());
			return ErrorCode;
		}

	private:
		TMap<int32, FString> ModuleNameMap;
		TMap<TPair<int32, int32>, FString> ErrorCodeNameMap;
	};


	class FTextFormatArgsPropertyExtractor : public IErrorPropertyExtractor
	{
	private:
		FFormatNamedArguments& Arguments;
	public:
		inline  FTextFormatArgsPropertyExtractor(FFormatNamedArguments& InArguments) : Arguments(InArguments)
		{
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const FStringView& PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(FText::FromString(PropertyValue.GetData())));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const FUtf8StringView& PropertyValue)  override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(FText::FromStringView(UTF8_TO_TCHAR(PropertyValue.GetData()))));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const int64 PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(PropertyValue));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const int32 PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(PropertyValue));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const float PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(PropertyValue));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const double PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(PropertyValue));
		}

		inline virtual void AddProperty(const FUtf8StringView& PropertyName, const FText& PropertyValue) override
		{
			Arguments.Add(UTF8_TO_TCHAR(PropertyName.GetData()), FFormatArgumentValue(PropertyValue));
		}
	};

	CORE_API void SerializeForLog(FCbWriter& Writer, const FError& OnlineError);

} // namespace UnifiedError



CORE_API FString LexToString(const UE::UnifiedError::FError& Error);
/*

CORE_API constexpr uint32 GetTypeIdHash(const TCHAR* TypeName)
{
	return FCrc::StrCrc32(GetData(S));
}
*/





#define DECLARE_ERROR_MODULE(ModuleName, ModuleId) \
	namespace UE::UnifiedError { namespace ModuleName {\
		inline const int32 StaticModuleId = UE::UnifiedError::FErrorRegistry::Get().RegisterModule(TEXTVIEW(#ModuleName));\
		static constexpr FAnsiStringView StaticModuleName = ANSITEXTVIEW(#ModuleName);\
	}}


#define DECLARE_ERROR_INTERNAL(ErrorName, ErrorCode, ModuleName, FormatString) \
	namespace UE::UnifiedError { namespace ModuleName { namespace ErrorName {\
			static constexpr FAnsiStringView StaticErrorName = ANSITEXTVIEW(#ErrorName);\
			FORCEINLINE TRefCountPtr<const FStaticErrorDetails> GetStaticErrorDetails() \
			{\
				static FStaticErrorDetails StaticErrorDetails(StaticErrorName, UE::UnifiedError::ModuleName::StaticModuleName, FormatString);\
				return TRefCountPtr<const FStaticErrorDetails>(&StaticErrorDetails);\
			}\
			inline const int32 ErrorCodeId = UE::UnifiedError::FErrorRegistry::Get().RegisterErrorCode(TEXTVIEW(#ErrorName), UE::UnifiedError::ModuleName::StaticModuleId, ErrorCode);\
			FORCEINLINE int32 GetErrorCodeId() \
			{\
				return ErrorCodeId;\
			}\
	}}}


#define DECLARE_ERROR(ErrorName, ErrorCode, ModuleName, FormatString) \
	DECLARE_ERROR_INTERNAL(ErrorName, ErrorCode, ModuleName, FormatString) \
	namespace UE::UnifiedError { namespace ModuleName { namespace ErrorName {\
			FORCEINLINE FError MakeError() { return FError(UE::UnifiedError::ModuleName::StaticModuleId, GetErrorCodeId(), TRefCountPtr<const IErrorDetails>(GetStaticErrorDetails().GetReference())); }\
			FORCEINLINE const FError& GetStaticError() \
			{\
				static FError StaticError = MakeError();\
				return StaticError;\
			}\
	}}}




#define DECLARE_ERROR_ONEPARAM(ErrorName, ErrorCode, ModuleName, FormatString, ParamOneType, ParamOneName, ParamOneDefault) \
	DECLARE_ERROR_INTERNAL(ErrorName, ErrorCode, ModuleName, FormatString) \
	namespace UE::UnifiedError { namespace ModuleName { namespace ErrorName { \
			class FDetails : public FDynamicErrorDetails \
			{ \
				DECLARE_FERROR_DETAILS(ModuleName::ErrorName, FDetails)\
			private:\
				ParamOneType ParamOneName; \
				FDetails() : ParamOneName(ParamOneDefault) {}\
			public: \
				FDetails(const ParamOneType& In##ParamOneName, TRefCountPtr<const IErrorDetails> InErrorDetails) : FDynamicErrorDetails(InErrorDetails) \
				{ \
					ParamOneName = In##ParamOneName; \
				} \
				virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const override \
				{ \
					OutProperties.AddProperty(UTF8TEXTVIEW(#ParamOneName), ParamOneName); \
					FDynamicErrorDetails::GetErrorProperties(Error, OutProperties); \
				} \
				const ParamOneType& Get##ParamOneName() const \
				{ \
					return ParamOneName; \
				} \
			}; \
			FORCEINLINE FError MakeError(ParamOneType ParamOneName = ParamOneDefault) \
			{ \
				FDetails* NewDetails = new FDetails(ParamOneName, TRefCountPtr<const IErrorDetails>(GetStaticErrorDetails().GetReference())); \
				return FError(UE::UnifiedError::ModuleName::StaticModuleId, GetErrorCodeId(), NewDetails); \
			} \
			FORCEINLINE const FError& GetStaticError() \
			{\
				static FError StaticError = MakeError();\
				return StaticError;\
			}\
	}}}
