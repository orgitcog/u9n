// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendLiteral.h"

#include "MetasoundBuilderSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetasoundFrontendLiteralBlueprintAccess.generated.h"

/**
 * Blueprint support for FMetasoundFrontendLiteral
 */
UCLASS()
class UMetasoundFrontendLiteralBlueprintAccess : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (MetaSound Literal)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|String")
	static FString Conv_MetaSoundLiteralToString(const FMetasoundFrontendLiteral& Literal);
	
	UFUNCTION(BlueprintPure, Category = "Audio|MetaSound", meta = (DisplayName = "Get MetaSound Literal Type"))
	static UPARAM(DisplayName = "Type") EMetasoundFrontendLiteralType GetType(const FMetasoundFrontendLiteral& Literal);

	// Literal creation

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Bool Literal"))
	static UPARAM(DisplayName = "Bool Literal") FMetasoundFrontendLiteral CreateBoolMetaSoundLiteral(bool Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Bool Array Literal"))
	static UPARAM(DisplayName = "Bool Array Literal") FMetasoundFrontendLiteral CreateBoolArrayMetaSoundLiteral(const TArray<bool>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Float Literal"))
	static UPARAM(DisplayName = "Float Literal") FMetasoundFrontendLiteral CreateFloatMetaSoundLiteral(float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Float Array Literal"))
	static UPARAM(DisplayName = "Float Array Literal") FMetasoundFrontendLiteral CreateFloatArrayMetaSoundLiteral(const TArray<float>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Int Literal"))
	static UPARAM(DisplayName = "Int32 Literal") FMetasoundFrontendLiteral CreateIntMetaSoundLiteral(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Int Array Literal"))
	static UPARAM(DisplayName = "Int32 Array Literal") FMetasoundFrontendLiteral CreateIntArrayMetaSoundLiteral(const TArray<int32>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Object Literal"))
	static UPARAM(DisplayName = "Object Literal") FMetasoundFrontendLiteral CreateObjectMetaSoundLiteral(UObject* Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Object Array Literal"))
	static UPARAM(DisplayName = "Object Array Literal") FMetasoundFrontendLiteral CreateObjectArrayMetaSoundLiteral(const TArray<UObject*>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound String Literal"))
	static UPARAM(DisplayName = "String Literal") FMetasoundFrontendLiteral CreateStringMetaSoundLiteral(const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound String Array Literal"))
	static UPARAM(DisplayName = "String Array Literal") FMetasoundFrontendLiteral CreateStringArrayMetaSoundLiteral(const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (DisplayName = "Create MetaSound Literal From AudioParameter"))
	static UPARAM(DisplayName = "Param Literal") FMetasoundFrontendLiteral CreateMetaSoundLiteralFromParam(const FAudioParameter& Param);

	// Value accessors 

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static bool GetBoolValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static TArray<bool> GetBoolArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static float GetFloatValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static TArray<float> GetFloatArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static int32 GetIntValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static TArray<int32> GetIntArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static UObject* GetObjectValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static TArray<UObject*> GetObjectArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static FString GetStringValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);
	
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound", meta = (ExpandEnumAsExecs = "OutResult"))
	static TArray<FString> GetStringArrayValueFromLiteral(const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult);
};
