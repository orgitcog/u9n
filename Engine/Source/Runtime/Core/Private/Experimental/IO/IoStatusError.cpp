// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/IO/IoStatusError.h"
#include "IO/IoStatus.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"


namespace UE::UnifiedError::IoStore
{
	class FIoStoreErrorDetails : public UE::UnifiedError::FRefCountedErrorDetails
	{
	private:
		FString ErrorMessage;
		mutable FUtf8String CachedErrorName;
		static const FText GenericErrorFormatString;

		const FUtf8String& GetErrorName(const FError& Error) const
		{
			if (CachedErrorName.IsEmpty() && (ErrorMessage[0] != TEXT('\0')))
			{
				CachedErrorName = TCHAR_TO_UTF8(GetIoErrorText(EIoErrorCode(Error.GetErrorCode())));
			}
			return CachedErrorName;
		}

	public:
		FIoStoreErrorDetails(const FStringView& InErrorMessage)
		{
			ErrorMessage = InErrorMessage;
		}
		
		virtual const FText GetErrorFormatString(const FError& Error) const final override
		{
			return GenericErrorFormatString;
		}

		virtual void GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const override
		{
			OutProperties.AddProperty(UTF8TEXTVIEW("ErrorCodeString"), GetErrorName(Error));
			OutProperties.AddProperty(UTF8TEXTVIEW("ModuleIdString"), UE::UnifiedError::IoStore::StaticModuleName);
			OutProperties.AddProperty(UTF8TEXTVIEW("ErrorCode"), Error.GetErrorCode());
			OutProperties.AddProperty(UTF8TEXTVIEW("ModuleId"), Error.GetModuleId());
			OutProperties.AddProperty(UTF8TEXTVIEW("IoStoreErrorMessage"), ErrorMessage);
		}

		virtual const FAnsiStringView GetErrorDetailsTypeName() const
		{
			return ANSITEXTVIEW("FIoStoreErrorDetails");
		}
	};

	const FText FIoStoreErrorDetails::GenericErrorFormatString = NSLOCTEXT("IoStore", "GenericErrorMessage", "{IoStoreErrorMessage}");


	CORE_API UE::UnifiedError::FError ConvertError(const FIoStatus& Status)
	{
		TRefCountPtr<FIoStoreErrorDetails> ErrorDetails = new FIoStoreErrorDetails(Status.GetErrorMessage());
		return FError(UE::UnifiedError::IoStore::StaticModuleId, (int32)(Status.GetErrorCode()), ErrorDetails);
	}
}