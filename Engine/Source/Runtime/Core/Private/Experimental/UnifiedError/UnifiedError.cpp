// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/UnifiedError/UnifiedError.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Logging/StructuredLog.h"
#include "Logging/StructuredLogFormat.h"


namespace UE::UnifiedError
{



// FError functions

void FError::PushErrorDetails(TRefCountPtr<IErrorDetails> InErrorDetails)
{
	/*if (ErrorDetails)
	{
		InErrorDetails->SetInnerErrorDetails(ErrorDetails);
	}
	InErrorDetails->AddRef();
	ErrorDetails->ReleaseRef();*/
	if (!InErrorDetails.IsValid())
	{
		return;
	}
	InErrorDetails->SetInnerErrorDetails(ErrorDetails);
	ErrorDetails = InErrorDetails;
}

TRefCountPtr<const IErrorDetails> FError::GetInnerMostErrorDetails() const
{
	const IErrorDetails* Result = ErrorDetails;
	while (true)
	{
		if (Result->GetInnerErrorDetails())
		{
			Result = Result->GetInnerErrorDetails();
		}
		else
		{
			return Result;
		}
	}
}

FText FError::GetFormatErrorText() const
{
	return ErrorDetails->GetErrorFormatString(*this);
}


#define USE_STRUCTURED_LOG_FOR_FERRORMESSAGE 0

FText FError::GetErrorMessage(bool bIncludeContext) const
{
#if USE_STRUCTURED_LOG_FOR_FERRORMESSAGE
	FString FormatString = ErrorDetails->GetErrorFormatString(*this).ToString();

	TCbWriter<1024> Writer;
	Writer.BeginObject();

	SerializeDetailsForLog(Writer);

	Writer.EndObject();

	FWideStringBuilderBase OutputMessage;
	FInlineLogTemplate Template(*FormatString);
	Template.FormatTo(OutputMessage, Writer.Save());

	return FText::FromString(OutputMessage.ToString());
#else
	FFormatNamedArguments Args;
	FTextFormatArgsPropertyExtractor PropertyExtractor(Args);
	ErrorDetails->GetErrorProperties(*this, PropertyExtractor);
	return FText::Format(ErrorDetails->GetErrorFormatString(*this), Args);
#endif
}

void FError::SerializeDetailsForLog(FCbWriter& Writer) const
{
	const IErrorDetails* DetailsIt = ErrorDetails.GetReference();
	while (DetailsIt != nullptr)
	{
		Writer.SetName(DetailsIt->GetErrorDetailsTypeName());
		DetailsIt->SerializeForLog(Writer);


		DetailsIt = DetailsIt->GetInnerErrorDetails().GetReference();
	}
}

int32 FError::GetErrorCode() const
{
	return ErrorCode;
}

int32 FError::GetModuleId() const
{
	return ModuleId;
}

class FNullErrorPropertyExtractor : public IErrorPropertyExtractor
{
public:
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FStringView& PropertyValue) override { } 
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FUtf8StringView& PropertyValue) override { }
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FText& PropertyValue) override { }
	virtual void AddProperty(const FUtf8StringView& PropertyName, const int64 PropertyValue) override { }
	virtual void AddProperty(const FUtf8StringView& PropertyName, const int32 PropertyValue) override { }
	virtual void AddProperty(const FUtf8StringView& PropertyName, const float PropertyValue) override { }
	virtual void AddProperty(const FUtf8StringView& PropertyName, const double PropertyValue) override { }
};

template<typename SearchType, typename ResultType>
class FPropertySearchVisitor : public FNullErrorPropertyExtractor
{
public:
	FPropertySearchVisitor(const FUtf8StringView& InSearchName, ResultType* InResult) : SearchName(InSearchName), Result(InResult)
	{
	}

	virtual void AddProperty(const FUtf8StringView& PropertyName, SearchType PropertyValue) override
	{
		if ((PropertyName == SearchName) && (Result != nullptr))
		{
			*Result = PropertyValue;
			bFound = true;
		}
	}

	bool WasFound() const { return bFound; }
private:

	const FUtf8StringView& SearchName;
	ResultType* Result;
	mutable bool bFound = false;
};

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, FString& Result) const
{
	FPropertySearchVisitor<const FStringView&, FString> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, FUtf8String& Result) const
{
	FPropertySearchVisitor<const FUtf8StringView&, FUtf8String> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, FText& Result) const
{
	FPropertySearchVisitor<const FText&, FText> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, int64& Result) const
{
	FPropertySearchVisitor<const int64, int64> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, int32& Result) const
{
	FPropertySearchVisitor<const int32, int32> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, double& Result) const
{
	FPropertySearchVisitor<const double, double> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

bool FError::GetDetailByKey(const FUtf8StringView& KeyName, float& Result) const
{
	FPropertySearchVisitor<const float, float> Visitor(KeyName, &Result);
	ErrorDetails->GetErrorProperties(*this, Visitor);
	return Visitor.WasFound();
}

void FError::GetErrorProperties(IErrorPropertyExtractor& Visitor) const
{
	ErrorDetails->GetErrorProperties(*this, Visitor);
}

const FUtf8String FError::GetErrorCodeString() const
{
	FUtf8String Result;
	GetDetailByKey(UTF8TEXTVIEW("ErrorCodeString"), Result);
	return Result;
}

const FUtf8String FError::GetModuleIdString() const
{
	FUtf8String Result;
	GetDetailByKey(UTF8TEXTVIEW("ModuleIdString"), Result);
	return Result;
}

// Structured logging FError integration
class FCbWriterErrorPropertyExtractor : public IErrorPropertyExtractor
{
private:
	FCbWriter& Writer;
public:
	FCbWriterErrorPropertyExtractor(FCbWriter& InWriter) : Writer(InWriter) {}



public:
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FStringView& PropertyValue) override 
	{ 
		Writer.AddString(PropertyName, PropertyValue);
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FUtf8StringView& PropertyValue) override
	{
		Writer.AddString(PropertyName, PropertyValue);
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const FText& PropertyValue) override
	{
		Writer.AddString(PropertyName, PropertyValue.ToString());
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const int64 PropertyValue) override
	{
		Writer.AddInteger(PropertyName, PropertyValue);
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const int32 PropertyValue) override 
	{
		Writer.AddInteger(PropertyName, PropertyValue);
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const float PropertyValue) override
	{
		Writer.AddFloat(PropertyName, PropertyValue);
	}
	virtual void AddProperty(const FUtf8StringView& PropertyName, const double PropertyValue) override 
	{ 
		Writer.AddFloat(PropertyName, PropertyValue);
	}
};

void SerializeForLog(FCbWriter& Writer, const FError& Error)
{
	Writer.BeginObject();
	Writer.AddString(ANSITEXTVIEW("$type"), ANSITEXTVIEW("FError"));

#if USE_STRUCTURED_LOG_FOR_FERRORMESSAGE
	

	Error.SerializeDetailsForLog(Writer);

	Writer.AddString(ANSITEXTVIEW("$text"), Error.GetFormatErrorText().ToString());

#else

	FCbWriterErrorPropertyExtractor ExtractToCbWriter(Writer);
	Error.GetErrorProperties(ExtractToCbWriter);


	// build a format string
	const FText StructuredLogFormatString = NSLOCTEXT("UnifiedError", "StructuredLogErrorMessage", "{ModuleIdString}.{ErrorCodeString}({ModuleId}.{ErrorCode}): ");
	FText ErrorFormatText = FText::Join(FText::FromString(TEXT("")), StructuredLogFormatString, Error.GetFormatErrorText());
	Writer.AddString(ANSITEXTVIEW("ErrorFormatString"), ErrorFormatText.ToString());

	// build up a formatted error string including some additional information about the error
	FFormatNamedArguments ErrorMessageArgs;
	FTextFormatArgsPropertyExtractor ExtractToFTextArgs(ErrorMessageArgs);
	Error.GetErrorProperties(ExtractToFTextArgs);
	Writer.AddString("$text", FText::Format(ErrorFormatText, MoveTemp(ErrorMessageArgs)).ToString());

	Writer.EndObject();
#endif
}

// FRefCountedErrorDetails fucntions
FRefCountedErrorDetails::~FRefCountedErrorDetails()
{
}

// FDynamicErrorDetails functions

FDynamicErrorDetails::FDynamicErrorDetails(TRefCountPtr<const IErrorDetails> InInnerErrorDetails)
{
	InnerErrorDetails = InInnerErrorDetails;
}
FDynamicErrorDetails::~FDynamicErrorDetails() 
{
}

const FText FDynamicErrorDetails::GetErrorFormatString(const FError & Error) const
{
	check(InnerErrorDetails);
	return InnerErrorDetails->GetErrorFormatString(Error);
}
/*

const FUtf8String& FDynamicErrorDetails::GetErrorName(const FError& Error) const
{
	check(InnerErrorDetails);
	return InnerErrorDetails->GetErrorName(Error);
}
*/


void FDynamicErrorDetails::GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const
{
	InnerErrorDetails->GetErrorProperties(Error, OutProperties);
}

void FDynamicErrorDetails::SetInnerErrorDetails(TRefCountPtr<const IErrorDetails> InInnerErrorDetails)
{
	InnerErrorDetails = InInnerErrorDetails;
}

// FStaticErrorDetails functions

FStaticErrorDetails::FStaticErrorDetails(const FAnsiStringView& InErrorName, const FAnsiStringView& InModuleName, const FText& InErrorFormatString) : ErrorName(InErrorName), ModuleName(InModuleName), ErrorFormatString(InErrorFormatString)
{
}



void FStaticErrorDetails::GetErrorProperties(const FError& Error, IErrorPropertyExtractor& OutProperties) const
{
	OutProperties.AddProperty(UTF8TEXTVIEW("ErrorCodeString"), ErrorName);
	OutProperties.AddProperty(UTF8TEXTVIEW("ModuleIdString"), ModuleName);
	OutProperties.AddProperty(UTF8TEXTVIEW("ErrorCode"), Error.GetErrorCode());
	OutProperties.AddProperty(UTF8TEXTVIEW("ModuleId"), Error.GetModuleId());
}

const FAnsiStringView& FStaticErrorDetails::GetErrorCodeString() const
{
	return ErrorName;
}

const FAnsiStringView& FStaticErrorDetails::GetModuleIdString() const
{
	return ModuleName;
}

const FText FStaticErrorDetails::GetErrorFormatString(const FError& Error) const
{
	return ErrorFormatString;
}

// FErrorDetailsRegistry
uint32 FErrorDetailsRegistry::RegisterDetails(const FAnsiStringView& ErrorDetailsName, TFunction<IErrorDetails*()> CreationFunction)
{
	uint32 DetailsId = FCrc::StrCrc32<FAnsiStringView::ElementType>(ErrorDetailsName.GetData());
	CreateFunctions.Add(DetailsId, CreationFunction);
	return DetailsId;
}


} // namespace UE::Error


FString LexToString(const UE::UnifiedError::FError& Error)
{
	
	// TODO: daniel, switch to using structured logging output in the future instead of this
	return FString::Printf(TEXT("%hs:%hs - %s"),
		*Error.GetModuleIdString(),
		*Error.GetErrorCodeString(),
		*Error.GetErrorMessage().ToString());
}