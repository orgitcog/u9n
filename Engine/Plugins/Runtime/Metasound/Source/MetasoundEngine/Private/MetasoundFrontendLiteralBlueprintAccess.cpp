// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendLiteralBlueprintAccess.h"
#include "MetasoundDataReference.h"

namespace MetasoundFrontendLiteralBlueprintAccessPrivate
{
	template <typename TLiteralType>
	FMetasoundFrontendLiteral CreatePODMetaSoundLiteral(const TLiteralType& Value)
	{
		FMetasoundFrontendLiteral Literal;
		Literal.Set(Value);
		return Literal;
	}

	template <typename TLiteralType>
	TLiteralType GetPODValueFromMetaSoundLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
	{
		TLiteralType Value;
		OutResult = Literal.TryGet(Value) ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
		return Value;
	}
}

FString UMetasoundFrontendLiteralBlueprintAccess::Conv_MetaSoundLiteralToString(const FMetasoundFrontendLiteral& Literal)
{
	return Literal.ToString();
}

EMetasoundFrontendLiteralType UMetasoundFrontendLiteralBlueprintAccess::GetType(const FMetasoundFrontendLiteral& Literal)
{
	return Literal.GetType();
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateBoolMetaSoundLiteral(bool Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateBoolArrayMetaSoundLiteral(
	const TArray<bool>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateFloatMetaSoundLiteral(float Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateFloatArrayMetaSoundLiteral(
	const TArray<float>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateIntMetaSoundLiteral(int32 Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateIntArrayMetaSoundLiteral(
	const TArray<int32>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateObjectMetaSoundLiteral(UObject* Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateObjectArrayMetaSoundLiteral(
	const TArray<UObject*>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateStringMetaSoundLiteral(const FString& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateStringArrayMetaSoundLiteral(
	const TArray<FString>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateMetaSoundLiteralFromParam(
	const FAudioParameter& Param)
{
	return FMetasoundFrontendLiteral{ Param };
}

bool UMetasoundFrontendLiteralBlueprintAccess::GetBoolValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<bool>(Literal, OutResult);
}

TArray<bool> UMetasoundFrontendLiteralBlueprintAccess::GetBoolArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<TArray<bool>>(Literal, OutResult);
}

float UMetasoundFrontendLiteralBlueprintAccess::GetFloatValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<float>(Literal, OutResult);
}

TArray<float> UMetasoundFrontendLiteralBlueprintAccess::GetFloatArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<TArray<float>>(Literal, OutResult);
}

int32 UMetasoundFrontendLiteralBlueprintAccess::GetIntValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<int>(Literal, OutResult);
}

TArray<int32> UMetasoundFrontendLiteralBlueprintAccess::GetIntArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<TArray<int32>>(Literal, OutResult);
}

UObject* UMetasoundFrontendLiteralBlueprintAccess::GetObjectValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<UObject*>(Literal, OutResult);
}

TArray<UObject*> UMetasoundFrontendLiteralBlueprintAccess::GetObjectArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<TArray<UObject*>>(Literal, OutResult);
}

FString UMetasoundFrontendLiteralBlueprintAccess::GetStringValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<FString>(Literal, OutResult);
}

TArray<FString> UMetasoundFrontendLiteralBlueprintAccess::GetStringArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::GetPODValueFromMetaSoundLiteral<TArray<FString>>(Literal, OutResult);
}
