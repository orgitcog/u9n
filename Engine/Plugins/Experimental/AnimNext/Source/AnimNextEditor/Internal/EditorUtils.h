// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "EdGraphSchema_K2.h"
#include "StructView.h"

struct FAnimNextVariableBindingData;
class UAnimNextModule;
class UAnimNextRigVMAssetEditorData;
struct FAnimNextParamType;
class URigVMController;
struct FAnimNextWorkspaceAssetRegistryExports;
class SWidget;
class URigVMUnitNode;
struct FRigVMFunction;
struct FGraphContextMenuBuilder;

struct FAnimNextAssetRegistryExports;

namespace UE::AnimNext::Editor
{

struct FUtils
{
	static ANIMNEXTEDITOR_API FName ValidateName(const UObject* InObject, const FString& InName);

	static ANIMNEXTEDITOR_API void GetAllEntryNames(const UAnimNextRigVMAssetEditorData* InEditorData, TSet<FName>& OutNames);

	static ANIMNEXTEDITOR_API FAnimNextParamType GetParameterTypeFromMetaData(const FStringView& InStringView);

	static ANIMNEXTEDITOR_API void GetFilteredVariableTypeTree(TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter);

	static ANIMNEXTEDITOR_API bool IsValidParameterNameString(FStringView InStringView, FText& OutErrorText);

	static ANIMNEXTEDITOR_API bool IsValidParameterName(const FName InName, FText& OutErrorText);

	static ANIMNEXTEDITOR_API bool AddSchemaRigUnitAction(const TSubclassOf<URigVMUnitNode>& UnitNodeClass, UScriptStruct* Struct, const FRigVMFunction& Function, FGraphContextMenuBuilder& InContexMenuBuilder);

	static ANIMNEXTEDITOR_API void GetRigUnitStructMetadata(const UScriptStruct* Struct, FString& OutCategoryMetadata, FString& OutDisplayNameMetadata, FString& OutMenuDescSuffixMetadata);
};

}
