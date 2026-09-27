// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/Build.h"
#include "Param/ParamType.h"
#include "RigVMCore/RigVMTemplate.h"
#include "StructUtils/InstancedStruct.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "UObject/SoftObjectPath.h"
#include "RigVMModel/RigVMGraph.h"
// --- ---
#include "UncookedOnlyUtils.generated.h"

struct FAnimNextGetVariableCompileContext;
struct FAnimNextVariableBindingData;
struct FEdGraphPinType;
struct FWorkspaceOutlinerItemExports;
struct FWorkspaceOutlinerItemExport;
struct FRigVMGraphFunctionData;
struct FRigVMCompileSettings;
struct FRigVMGraphFunctionHeaderArray;
struct FGraphContextMenuBuilder;
struct FRigVMFunction;
class UAnimNextModule;
class UAnimNextModule_EditorData;
class UAnimNextEdGraph;
class URigVMController;
class URigVMGraph;
class UAnimNextEdGraph;
class UAnimNextRigVMAsset;
class UAnimNextRigVMAssetEditorData;
class UAnimNextRigVMAssetEntry;
class URigVMUnitNode;


namespace UE
{
	namespace AnimNext
	{
		static const FLazyName ExportsAnimNextAssetRegistryTag = TEXT("AnimNextExports");
		static const FLazyName AnimNextPublicGraphFunctionsExportsRegistryTag = TEXT("AnimNextPublicGraphFunctions");
		static const FLazyName ControlRigAssetPublicGraphFunctionsExportsRegistryTag = TEXT("PublicGraphFunctions");
	}
}

UENUM()
enum class EAnimNextExportedVariableFlags : uint32
{
	NoFlags = 0x0,
	Public = 0x1,
	Read = 0x02,
	Write = 0x04,
	Declared = 0x08,
	Max
};

ENUM_CLASS_FLAGS(EAnimNextExportedVariableFlags)

USTRUCT()
struct FAnimNextAssetRegistryExportedVariable
{
	GENERATED_BODY()

	FAnimNextAssetRegistryExportedVariable() = default;

	FAnimNextAssetRegistryExportedVariable(FName InName, const FAnimNextParamType& InType, EAnimNextExportedVariableFlags InFlags = EAnimNextExportedVariableFlags::NoFlags)
		: Name(InName)
		, Type(InType)
		, Flags((uint32)InFlags) 
	{}

	bool operator==(const FAnimNextAssetRegistryExportedVariable& Other) const
	{
		return Name == Other.Name;
	}

	friend uint32 GetTypeHash(const FAnimNextAssetRegistryExportedVariable& Entry)
	{
		return GetTypeHash(Entry.Name);
	}

	EAnimNextExportedVariableFlags GetFlags() const
	{
		return (EAnimNextExportedVariableFlags)Flags;
	}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FAnimNextParamType Type;

	UPROPERTY()
	uint32 Flags = (int32)EAnimNextExportedVariableFlags::NoFlags;
};

USTRUCT()
struct FAnimNextAssetRegistryManifestNode
{
	GENERATED_BODY()

	FAnimNextAssetRegistryManifestNode() = default;

	FAnimNextAssetRegistryManifestNode(const TObjectPtr<URigVMGraph>& InModelGraph, const FString& InNodeName, const FString& InNodeCategory, const FString& InMenuDesc, const FString& InToolTip)
		: ModelGraph(FSoftObjectPath::ConstructFromObject(InModelGraph))
		, NodeName(InNodeName)
		, NodeCategory(InNodeCategory)
		, MenuDesc(InMenuDesc)
		, ToolTip(InToolTip)
	{}

	bool operator==(const FAnimNextAssetRegistryManifestNode& Other) const
	{
		return ModelGraph == Other.ModelGraph && NodeName == Other.NodeName;
	}

	UPROPERTY()
	FSoftObjectPath ModelGraph;

	UPROPERTY()
	FString NodeName;

	UPROPERTY()
	FString NodeCategory;

	UPROPERTY()
	FString MenuDesc;

	UPROPERTY()
	FString ToolTip;
};

USTRUCT()
struct FAnimNextAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextAssetRegistryExportedVariable> Variables;

	UPROPERTY()
	TArray<FAnimNextAssetRegistryManifestNode> ManifestNodes;
};

namespace UE::AnimNext::UncookedOnly
{

extern ANIMNEXTUNCOOKEDONLY_API TAutoConsoleVariable<bool> CVarDumpProgrammaticGraphs;

struct ANIMNEXTUNCOOKEDONLY_API FUtils
{
	static void CompileVariables(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, FAnimNextGetVariableCompileContext& OutCompileContext);

	static void CompileVariableBindings(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, TArray<URigVMGraph*>& OutGraphs);

	static void RecreateVM(UAnimNextRigVMAsset* InAsset);

	// Get the corresponding asset from an asset's editor data (casts the outer appropriately)
	static UAnimNextRigVMAsset* GetAsset(UAnimNextRigVMAssetEditorData* InEditorData);

	template<typename AssetType, typename EditorDataType>
	static AssetType* GetAsset(EditorDataType* InEditorData)
	{
		using NonConstEditorDataType = std::remove_const_t<EditorDataType>;
		return CastChecked<AssetType>(GetAsset(const_cast<NonConstEditorDataType*>(InEditorData)));
	}

	// Get the corresponding editor data from an asset (casts the editor data appropriately)
	static UAnimNextRigVMAssetEditorData* GetEditorData(UAnimNextRigVMAsset* InAsset);

	template<typename EditorDataType, typename AssetType>
	static EditorDataType* GetEditorData(AssetType* InAsset)
	{
		using NonConstAssetType = std::remove_const_t<AssetType>;
		return CastChecked<EditorDataType>(GetEditorData(const_cast<NonConstAssetType*>(InAsset)));
	}

	/**
	 * Get an AnimNext parameter type from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FAnimNextParamType GetParamTypeFromPinType(const FEdGraphPinType& InPinType);

	/**
	 * Get an FEdGraphPinType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FEdGraphPinType GetPinTypeFromParamType(const FAnimNextParamType& InParamType);

	/**
	 * Get an FRigVMTemplateArgumentType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType);

	/** Set up a simple event graph */
	static void SetupEventGraph(URigVMController* InController, UScriptStruct* InEventStruct, FName InEventName = NAME_None, bool bPrintPythonCommand = false);

	// Gets the variables that are exported to the asset registry for an asset
	static bool GetExportedVariablesForAsset(const FAssetData& InAsset, FAnimNextAssetRegistryExports& OutExports);

	// Gets all the variables that are exported to the asset registry
	static bool GetExportedVariablesFromAssetRegistry(TMap<FAssetData, FAnimNextAssetRegistryExports>& OutExports);

	// Gets the functions that are exported to the asset registry for an asset
	static bool GetExportedFunctionsForAsset(const FAssetData& InAsset, FName Tag, FRigVMGraphFunctionHeaderArray& OutExports);

	// Gets all the functions that are exported to the asset registry for the specified Tag
	static bool GetExportedFunctionsFromAssetRegistry(FName Tag, TMap<FAssetData, FRigVMGraphFunctionHeaderArray>& OutExports);

	// Gets the exported variables that are used by a RigVM asset
	static void GetAssetVariables(const UAnimNextRigVMAssetEditorData* EditorData, FAnimNextAssetRegistryExports& OutExports);
	static void GetAssetVariables(const UAnimNextRigVMAssetEditorData* EditorData, TSet<FAnimNextAssetRegistryExportedVariable>& OutExports);

	// Gets the exported public functions that are used by a RigVM asset
	static void GetAssetFunctions(const UAnimNextRigVMAssetEditorData* InEditorData, FRigVMGraphFunctionHeaderArray& OutExports);

	// Gets the non-exported private functions that are used by a RigVM asset
	static void GetAssetPrivateFunctions(const UAnimNextRigVMAssetEditorData* InEditorData, FRigVMGraphFunctionHeaderArray& OutExports);

	// Gets the asset-registry information needed for representing the contained data into the Workspace Outliner
	// Note: We pass parent as index to avoid the ref from being invalidated due to realloc as the export array grows recursively
	static void GetAssetOutlinerItems(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FAssetRegistryTagsContext Context);
	static void CreateSubGraphsOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, URigVMEdGraph* RigVMEdGraph, FAssetRegistryTagsContext Context);
	static void CreateFunctionLibraryOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, const TArray<FRigVMGraphFunctionData>& PublicFunctions, const TArray<FRigVMGraphFunctionData>& PrivateFunctions);
	static void CreateFunctionsOutlinerItemsRecursive(const UAnimNextRigVMAssetEditorData* EditorData, FWorkspaceOutlinerItemExports& OutExports, FWorkspaceOutlinerItemExport& RootExport, int32 ParentExportIndex, const TArray<FRigVMGraphFunctionData>& Functions, bool bPublicFunctions);

	// Attempts to determine the type from a parameter name
	// If the name cannot be found, the returned type will be invalid
	// Note that this is expensive and can query the asset registry
	static FAnimNextParamType GetParameterTypeFromName(FName InName);

	// Returns an user friendly name for the Function Library
	static const FText& GetFunctionLibraryDisplayName();

#if WITH_EDITOR
	static void OpenProgrammaticGraphs(UAnimNextRigVMAssetEditorData* InEditorData, const TArray<URigVMGraph*>& ProgrammaticGraphs);
#endif // WITH_EDITOR

	// Make a variable name that we use as a wrapper for a function param or return
	static FString MakeFunctionWrapperVariableName(FName InFunctionName, FName InVariableName);

	// Make an event name that we use as a wrapper to call RigVM functions 
	static FString MakeFunctionWrapperEventName(FName InFunctionName);

private:
	static void CompileVariableBindingsInternal(const FRigVMCompileSettings& InSettings, UAnimNextRigVMAsset* InAsset, TArray<URigVMGraph*>& OutGraphs, bool bInThreadSafe);
};

}