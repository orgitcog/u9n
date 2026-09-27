// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeMaterialInstanceNode.generated.h"

#define UE_API INTERCHANGENODES_API

class UInterchangeBaseNodeContainer;

UCLASS(MinimalAPI, BlueprintType)
class UInterchangeMaterialInstanceNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:

	/**
     * Return the node type name of the class. This is used when reporting errors.
     */
	UE_API virtual FString GetTypeName() const override;


	/**
	 * Build and return a UID name for a shader node.
	 */
	static UE_API FString MakeNodeUid(const FStringView NodeName, const FStringView ParentNodeUid);

	/**
     * Creates a new UInterchangeShaderGraphNode and adds it to NodeContainer as a translated node.
     */
	static UE_API UInterchangeMaterialInstanceNode* Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView NodeName, const FStringView ParentNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool SetCustomParent(const FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool GetCustomParent(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool AddScalarParameterValue(const FString & ParameterName, float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool GetScalarParameterValue(const FString& ParameterName, float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool AddVectorParameterValue(const FString& ParameterName, const FLinearColor& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool GetVectorParameterValue(const FString& ParameterName, FLinearColor& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool AddTextureParameterValue(const FString& ParameterName, const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool GetTextureParameterValue(const FString& ParameterName, FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool AddStaticSwitchParameterValue(const FString& ParameterName, bool AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material Instance")
	UE_API bool GetStaticSwitchParameterValue(const FString& ParameterName, bool& AttributeValue) const;

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(Parent);

};

#undef UE_API
