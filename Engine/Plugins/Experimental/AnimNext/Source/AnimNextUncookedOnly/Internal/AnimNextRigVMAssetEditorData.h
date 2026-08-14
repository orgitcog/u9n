// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprint.h"
#include "UncookedOnlyUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Entries/AnimNextRigVMAssetEntry.h"
#include "Variables/AnimNextProgrammaticVariable.h"
#include "AnimNextRigVMAssetEditorData.generated.h"

enum class ERigVMGraphNotifType : uint8;
class UAnimNextRigVMAssetEditorData;
class UAnimNextEdGraph;
class UAnimNextDataInterfaceEntry;
class UAnimNextWorkspaceEditorMode;
struct FAnimNextRigVMAssetCompileContext;
struct FAnimNextGetFunctionHeaderCompileContext;
struct FAnimNextGetVariableCompileContext;
struct FAnimNextGetGraphCompileContext;
struct FAnimNextProcessGraphCompileContext;

namespace UE::AnimNext::UncookedOnly
{
	struct FPublicVariablesImpl;
	struct FUtils;
	struct FUtilsPrivate;
	class FScopedCompilerResults;
}

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class SRigVMAssetView;
	class SParameterPicker;
	class SRigVMAssetViewRow;
	class FVariableCustomization;
	class FFindInAnimNextRigVMAssetResult;
	class SFindInAnimNextRigVMAsset;
	class FAnimNextEditorModule;
	class FWorkspaceEditor;
	class FAnimNextAssetItemDetails;
	class FAnimNextGraphItemDetails;
	class FAnimNextFunctionItemDetails;
	struct FVariablesOutlinerEntryItem;
	class FVariablesOutlinerMode;
	class FVariablesOutlinerHierarchy;
	class SVariablesOutlinerValue;
	class SVariablesOutliner;
	class SAddVariablesDialog;
	class FVariableProxyCustomization;
	class FAnimNextAnimGraphEditorModule;
	class FCallFunctionSharedDataDetails;
	class FAssetCompilationHandler;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Graphs;
	class FEditor_AnimGraph_Graphs;
	class FEditor_Variables;
	class FEditor_AnimGraph_Variables;
	class FVariables;
	class FVariables_UOLBindings;
	class FDataInterfaceCompile;
}

enum class EAnimNextEditorDataNotifType : uint8
{
	PropertyChanged,	// An property was changed (Subject == UObject)
	EntryAdded,		// An entry has been added (Subject == UAnimNextRigVMAssetEntry)
	EntryRemoved,	// An entry has been removed (Subject == UAnimNextRigVMAssetEditorData)
	EntryRenamed,	// An entry has been renamed (Subject == UAnimNextRigVMAssetEntry)
	EntryAccessSpecifierChanged,	// An entry access specifier has been changed (Subject == UAnimNextRigVMAssetEntry)
	VariableTypeChanged,	// A variable entry type changed (Subject == UAnimNextVariableEntry)
	UndoRedo,		// Transaction was performed (Subject == UObject)
	VariableDefaultValueChanged,	// A variable entry default value changed (Subject == UAnimNextVariableEntry)
	VariableBindingChanged,	// A variable entry binding changed (Subject == UAnimNextVariableEntry)
};

namespace UE::AnimNext::UncookedOnly
{
	// A delegate for subscribing / reacting to editor data modifications.
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEditorDataModified, UAnimNextRigVMAssetEditorData* /* InEditorData */, EAnimNextEditorDataNotifType /* InType */, UObject* /* InSubject */);

	// An interaction bracket count reached 0
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractionBracketFinished, UAnimNextRigVMAssetEditorData* /* InEditorData */);
}

// Script-callable editor API hoisted onto UAnimNextRigVMAsset
UCLASS()
class UAnimNextRigVMAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Finds an entry in an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMAssetEntry* FindEntry(UAnimNextRigVMAsset* InAsset, FName InName);

	/** Removes an entry from an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API bool RemoveEntry(UAnimNextRigVMAsset* InAsset, UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Removes multiple entries from an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API bool RemoveEntries(UAnimNextRigVMAsset* InAsset, const TArray<UAnimNextRigVMAssetEntry*>& InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Removes all entries from an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API bool RemoveAllEntries(UAnimNextRigVMAsset* InAsset, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a parameter to an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API UAnimNextVariableEntry* AddVariable(UAnimNextRigVMAsset* InAsset, FName InName, EPropertyBagPropertyType InValueType, EPropertyBagContainerType InContainerType = EPropertyBagContainerType::None, const UObject* InValueTypeObject = nullptr, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds an event graph to an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API UAnimNextEventGraphEntry* AddEventGraph(UAnimNextRigVMAsset* InAsset, FName InName, UScriptStruct* InEventStruct, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a data interface to an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API UAnimNextDataInterfaceEntry* AddDataInterface(UAnimNextRigVMAsset* InAsset, UAnimNextDataInterface* InDataInterface, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a function to an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "UAF|Entries", meta=(ScriptMethod))
	static ANIMNEXTUNCOOKEDONLY_API URigVMLibraryNode* AddFunction(UAnimNextRigVMAsset* InAsset, FName InFunctionName, bool bInMutable, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

/* Base class for all AnimNext editor data objects that use RigVM */
UCLASS(Abstract)
class ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMAssetEditorData : public UObject, public IRigVMClientHost, public IRigVMGraphFunctionHost, public IRigVMClientExternalModelHost
{
	GENERATED_BODY()

public:
	/** Adds a parameter to this asset */
	UAnimNextVariableEntry* AddVariable(FName InName, FAnimNextParamType InType, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds an event graph to this asset */
	UAnimNextEventGraphEntry* AddEventGraph(FName InName, UScriptStruct* InEventStruct, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a data interface to this asset */
	UAnimNextDataInterfaceEntry* AddDataInterface(UAnimNextDataInterface* InDataInterface, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	/** Adds a function to this asset */
	URigVMLibraryNode* AddFunction(FName InFunctionName, bool bInMutable, bool bInSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Find an entry by name
	UAnimNextRigVMAssetEntry* FindEntry(FName InName) const;

	// Get the external packaging status of this asset
	bool IsUsingExternalPackages() const { return bUsesExternalPackages; }

#if WITH_EDITOR
	// Switch the external packaging status of this asset
	static void SetUseExternalPackages(TArrayView<UAnimNextRigVMAsset*> InAssets, bool bInUseExternalPackages);

	// UI helper function
	static FName GetUsesExternalPackagesPropertyName() { return GET_MEMBER_NAME_CHECKED(UAnimNextRigVMAssetEditorData, bUsesExternalPackages); }
#endif

	// Report an error to the user, typically used for scripting APIs
	static void ReportError(const TCHAR* InMessage);

protected:
	friend class UE::AnimNext::Editor::SRigVMAssetView;
	friend class UE::AnimNext::Editor::SRigVMAssetViewRow;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FAnimNextEditorModule;
	friend class UE::AnimNext::Editor::FWorkspaceEditor;
	friend class UAnimNextRigVMAssetEntry;
	friend class UAnimNextRigVMAssetLibrary;
	friend class UAnimNextEdGraph;
	friend class UE::AnimNext::Tests::FEditor_Graphs;
	friend class UE::AnimNext::Tests::FEditor_Variables;
	friend class UE::AnimNext::Tests::FEditor_AnimGraph_Graphs;
	friend class UE::AnimNext::Tests::FEditor_AnimGraph_Variables;
	friend class UE::AnimNext::Editor::FFindInAnimNextRigVMAssetResult;
	friend class UE::AnimNext::Editor::SFindInAnimNextRigVMAsset;
	friend class UE::AnimNext::Tests::FVariables;
	friend class UE::AnimNext::Tests::FVariables_UOLBindings;
	friend class UE::AnimNext::Tests::FDataInterfaceCompile;
	friend class UE::AnimNext::Editor::FVariableCustomization;
	friend class UE::AnimNext::Editor::FAnimNextAssetItemDetails;
	friend class UE::AnimNext::Editor::FAnimNextGraphItemDetails;
	friend class UE::AnimNext::Editor::FAnimNextFunctionItemDetails;
	friend struct UE::AnimNext::Editor::FVariablesOutlinerEntryItem;
	friend class UE::AnimNext::Editor::FVariablesOutlinerMode;
	friend class UE::AnimNext::Editor::FVariablesOutlinerHierarchy;
	friend class UE::AnimNext::Editor::SVariablesOutlinerValue;
	friend class UE::AnimNext::Editor::SVariablesOutliner;
	friend class UAnimNextModuleWorkspaceAssetUserData;
	friend class UE::AnimNext::Editor::SAddVariablesDialog;
	friend class UE::AnimNext::Editor::FVariableProxyCustomization;
	friend class UAnimNextDataInterfaceEntry;
	friend struct UE::AnimNext::UncookedOnly::FPublicVariablesImpl;
	friend UE::AnimNext::Editor::FAnimNextAnimGraphEditorModule;
	friend class UE::AnimNext::Editor::FCallFunctionSharedDataDetails;
	friend UAnimNextWorkspaceEditorMode;
	friend UE::AnimNext::UncookedOnly::FScopedCompilerResults;
	friend UE::AnimNext::Editor::FAssetCompilationHandler;
	friend class UAnimNextStateTreeTreeEditorData;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual bool IsEditorOnly() const override { return true; }
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;	
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;

	void HandlePackageDone(const FEndLoadPackageContext& Context);
	void HandlePackageDone();

	virtual void GetAnimNextAssetRegistryTags(FAssetRegistryTagsContext& Context, FAnimNextAssetRegistryExports& OutExports) const {}

	// IRigVMClientHost interface
	virtual FString GetAssetName() const override { return GetName(); }
	virtual UClass* GetRigVMSchemaClass() const override;
	virtual UScriptStruct* GetRigVMExecuteContextStruct() const override;
	virtual UClass* GetRigVMEdGraphClass() const override;
	virtual UClass* GetRigVMEdGraphNodeClass() const override;
	virtual UClass* GetRigVMEdGraphSchemaClass() const override;
	virtual UClass* GetRigVMEditorSettingsClass() const override;
	virtual FRigVMClient* GetRigVMClient() override;
	virtual const FRigVMClient* GetRigVMClient() const override;
	virtual IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() override;
	virtual const IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() const override;
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override;
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* GetEditorObjectForRigVMGraph(const URigVMGraph* InVMGraph) const override;
	virtual URigVMGraph* GetRigVMGraphForEditorObject(UObject* InObject) const override;
	virtual void RecompileVM() override;
	virtual void RecompileVMIfRequired() override;
	virtual void RequestAutoVMRecompilation() override;
	virtual void SetAutoVMRecompile(bool bAutoRecompile) override;
	virtual bool GetAutoVMRecompile() const override;
	virtual void IncrementVMRecompileBracket() override;
	virtual void DecrementVMRecompileBracket() override;
	virtual void RefreshAllModels(ERigVMLoadType InLoadType) override;
	virtual void OnRigVMRegistryChanged() override;
	virtual void RequestRigVMInit() override;
	virtual URigVMGraph* GetModel(const UEdGraph* InEdGraph = nullptr) const override;
	virtual URigVMGraph* GetModel(const FString& InNodePath) const override;
	virtual URigVMGraph* GetDefaultModel() const override;
	virtual TArray<URigVMGraph*> GetAllModels() const override;
	virtual URigVMFunctionLibrary* GetLocalFunctionLibrary() const override;
	virtual URigVMFunctionLibrary* GetOrCreateLocalFunctionLibrary(bool bSetupUndoRedo =  true) override;
	virtual URigVMGraph* AddModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual bool RemoveModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual FRigVMGetFocusedGraph& OnGetFocusedGraph() override;
	virtual const FRigVMGetFocusedGraph& OnGetFocusedGraph() const override;
	virtual URigVMGraph* GetFocusedModel() const override;
	virtual URigVMController* GetController(const URigVMGraph* InGraph = nullptr) const override;
	virtual URigVMController* GetControllerByName(const FString InGraphName = TEXT("")) const override;
	virtual URigVMController* GetOrCreateController(URigVMGraph* InGraph = nullptr) override;
	virtual URigVMController* GetController(const UEdGraph* InEdGraph) const override;
	virtual URigVMController* GetOrCreateController(const UEdGraph* InGraph) override;
	virtual TArray<FString> GeneratePythonCommands(const FString InNewBlueprintName) override;
	virtual void SetupPinRedirectorsForBackwardsCompatibility() override;
	virtual FRigVMGraphModifiedEvent& OnModified() override;
	virtual bool IsFunctionPublic(const FName& InFunctionName) const override;
	virtual void MarkFunctionPublic(const FName& InFunctionName, bool bIsPublic = true) override;
	virtual void RenameGraph(const FString& InNodePath, const FName& InNewName) override;


	// IRigVMGraphFunctionHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override;

	// IRigVMClientExternalModelHost interface
	virtual const TArray<TObjectPtr<URigVMGraph>>& GetExternalModels() const override { return GraphModels; }
	virtual TObjectPtr<URigVMGraph> CreateContainedGraphModel(URigVMCollapseNode* CollapseNode, const FName& Name) override;

	// Override called during initialization to determine what RigVM controller class is used
	virtual TSubclassOf<URigVMController> GetControllerClass() const { return URigVMController::StaticClass(); }

	// Override called during initialization to determine what RigVM execute struct is used
	virtual UScriptStruct* GetExecuteContextStruct() const PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::GetExecuteContextStruct, return nullptr;)

	// Create and store a UEdGraph that corresponds to a URigVMGraph
	virtual UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce);

	// Create and store a UEdGraph that corresponds to a URigVMCollapseNode
	virtual void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bForce);

	// Destroy a UEdGraph that corresponds to a URigVMCollapseNode
	virtual void RemoveEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bNotify);

	// Remove the UEdGraph that corresponds to a URigVMGraph
	virtual bool RemoveEdGraph(URigVMGraph* InModel);

	// Initialize the asset for use
	virtual void Initialize(bool bRecompileVM);

	// Handle RigVM modification events
	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	// Class to use when instantiating AssetUserData for the EditorData instance
	virtual TSubclassOf<UAssetUserData> GetAssetUserDataClass() const;

	// Override point called during initialization (PostLoad/PostDuplicate) used for setting up asset user data.
	// By default this instantiates any asset user data that is missing according to GetAssetUserDataClass().
	virtual void InitializeAssetUserData();

	// Get all the kinds of entry for this asset
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::GetEntryClasses, return {};)

	// Override to allow assets to prevent certain entries being created
	virtual bool CanAddNewEntry(TSubclassOf<UAnimNextRigVMAssetEntry> InClass) const { return true; }

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Begin Compilation overrides, in order of operation

	// Compilation phase 1: Called before RigVM compilation to setup compiler settings and clean our outer asset of compiler-generated data
	virtual void OnPreCompileAsset(FRigVMCompileSettings& InSettings) {}

	// Compilation phase 2: Called before RigVM compilation to allow this asset to specify function headers that require generation, along with function generation metadata.
	// While users may manually generate graphs using function headers, for convience we provide an autogeneration process for function headers requested here.
	virtual void OnPreCompileGetProgrammaticFunctionHeaders(const FRigVMCompileSettings& InSettings, FAnimNextGetFunctionHeaderCompileContext& OutCompileContext) {}

	// Compilation phase 3: Called before RigVM compilation to allow this asset to generate variables to be injected, separate method to allow programmatic graphs to use these vars
	// These variables will be regenerated each compile, and are not saved between compiles
	virtual void OnPreCompileGetProgrammaticVariables(const FRigVMCompileSettings& InSettings, FAnimNextGetVariableCompileContext& OutCompileContext) {}

	// Compilation phase 4: Called before RigVM compilation to allow this asset to generate graphs to be injected
	virtual void OnPreCompileGetProgrammaticGraphs(const FRigVMCompileSettings& InSettings, FAnimNextGetGraphCompileContext& OutCompileContext) {}

	// Compilation phase 5: Called before RigVM compilation to allow this asset to process, transform or replace the graphs that will be compiled
	virtual void OnPreCompileProcessGraphs(const FRigVMCompileSettings& InSettings, FAnimNextProcessGraphCompileContext& OutCompileContext) {}

	// Compilation phase 6: Called after RigVM compilation to clean up/finish the compilation process
	virtual void OnPostCompileCleanup(const FRigVMCompileSettings& InSettings) {}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// End Compilation overrides

	// Customization point for derived types to transform new asset entries
	virtual void CustomizeNewAssetEntry(UAnimNextRigVMAssetEntry* InNewEntry) const {}

	// Helper for creating new sub-entries. Sets package flags and outers appropriately 
	static UObject* CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData, TSubclassOf<UObject> InClass);

	// Helper for creating new sub-entries. Sets package flags and outers appropriately
	template<typename EntryClassType>
	static EntryClassType* CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData)
	{
		return CastChecked<EntryClassType>(CreateNewSubEntry(InEditorData, EntryClassType::StaticClass()));
	}

	// Get all the entries for this asset
	TConstArrayView<TObjectPtr<UAnimNextRigVMAssetEntry>> GetAllEntries() const { return Entries; } 

	// Access all the UEdGraphs in this asset
	TArray<UEdGraph*> GetAllEdGraphs() const;

	// Iterate over all entries of the specified type
	// If predicate returns false, iteration is stopped
	template<typename EntryType, typename PredicateType>
	void ForEachEntryOfType(PredicateType InPredicate) const
	{
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(EntryType* TypedEntry = Cast<EntryType>(Entry))
			{
				if(!InPredicate(TypedEntry))
				{
					return;
				}
			}
		}
	}

	// Find the first entry of the specified type
	template<typename EntryType>
	EntryType* FindFirstEntryOfType() const
	{
		EntryType* FirstEntry = nullptr;
		ForEachEntryOfType<EntryType>([&FirstEntry](EntryType* InEntry)
		{
			FirstEntry = InEntry;
			return false;
		});
		return FirstEntry;
	}

	// Returns all nodes in all graphs of the specified class
	template<class T>
	void GetAllNodesOfClass(TArray<T*>& OutNodes) const
	{
		ForEachEntryOfType<IAnimNextRigVMGraphInterface>([&OutNodes](IAnimNextRigVMGraphInterface* InGraphInterface)
		{
			URigVMEdGraph* RigVMEdGraph = InGraphInterface->GetEdGraph();
			check(RigVMEdGraph)

			TArray<T*> GraphNodes;
			RigVMEdGraph->GetNodesOfClass<T>(GraphNodes);

			TArray<UEdGraph*> SubGraphs;
			RigVMEdGraph->GetAllChildrenGraphs(SubGraphs);
			for (const UEdGraph* SubGraph : SubGraphs)
			{
				if (SubGraph)
				{
					SubGraph->GetNodesOfClass<T>(GraphNodes);
				}
			}

			OutNodes.Append(GraphNodes);

			return true;
		});

		for (URigVMEdGraph* RigVMEdGraph : FunctionEdGraphs)
		{
			if (RigVMEdGraph)
			{
				RigVMEdGraph->GetNodesOfClass<T>(OutNodes);

				TArray<UEdGraph*> SubGraphs;
				RigVMEdGraph->GetAllChildrenGraphs(SubGraphs);
				for (const UEdGraph* SubGraph : SubGraphs)
				{
					if (SubGraph)
					{
						SubGraph->GetNodesOfClass<T>(OutNodes);
					}
				}
			}
		}
	}

	// Remove an entry from the asset
	// @return true if the item was removed
	bool RemoveEntry(UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Remove a number of entries from the asset
	// @return true if any items were removed
	bool RemoveEntries(TConstArrayView<UAnimNextRigVMAssetEntry*> InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Remove all entries from the asset
	// @return true if any items were removed
	bool RemoveAllEntries(bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	void BroadcastModified(EAnimNextEditorDataNotifType InType, UObject* InSubject);

	void ReconstructAllNodes();

	// Called from PostLoad to load external packages
	void PostLoadExternalPackages();

	// Find an entry that corresponds to the specified RigVMGraph. This uses the name of the graph to match the entry 
	UAnimNextRigVMAssetEntry* FindEntryForRigVMGraph(const URigVMGraph* InRigVMGraph) const;

	// Find an entry that corresponds to the specified RigVMGraph. This uses the name of the graph to match the entry 
	UAnimNextRigVMAssetEntry* FindEntryForRigVMEdGraph(const URigVMEdGraph* InRigVMEdGraph) const;

	// Checks all entries to see if any are public variables
	bool HasPublicVariables() const;

	// Gets any public variables that this asset has. Variables have no specified order.
	// May recurse into implemented data interfaces, so public variables returned by this function may not be directly owned by this asset.
	void GetPublicVariables(TArray<UAnimNextVariableEntry*>& OutPublicVariables) const;

	// Gets any variables that this asset has. Variables have no specified order.
	// May recurse into implemented data interfaces (for public variables only), so variables returned by this function may not be directly owned by this asset.
	void GetAllVariables(TArray<UAnimNextVariableEntry*>& OutPublicVariables) const;

	// Refresh the 'external' models for the RigVM client to reference
	void RefreshExternalModels();

	// Clear the error info for all EdGraphNodes
	void ClearErrorInfoForAllEdGraphs();

	// Handle compiler reporting
	void HandleReportFromCompiler(EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage);

	// Support extra references in GC
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Add a new entry to this asset, taking into account external packaging status
	void AddEntryInternal(UAnimNextRigVMAssetEntry* InEntry);

	// Remove an entry to this asset, taking into account external packaging status
	void RemoveEntryInternal(UAnimNextRigVMAssetEntry* InEntry);

	// Remove any programmatic graphs generated during compilation and consign them to the transient package
	void RemoveProgrammaticGraphs(TArrayView<URigVMGraph*> InGraphs);

	// Remove any transient graphs in the passed-in array (e.g. generated during compilation) and consign them to the transient package
	void RemoveTransientGraphs(TArrayView<URigVMGraph*> InGraphs);

	// Handle removing a notify
	static void HandleRemoveNotify(UObject* InAsset, const FString& InFindString, bool bFindWholeWord, ESearchCase::Type InSearchCase);

	// Handle replacing a notify
	static void HandleReplaceNotify(UObject* InAsset, const FString& InFindString, const FString& InReplaceString, bool bFindWholeWord, ESearchCase::Type InSearchCase);

	// Check whether this asset should be recompiled
	bool IsDirtyForRecompilation() const;

	/** All entries in this asset - not saved, discovered at load time and also contains InternalEntries */
	UPROPERTY(transient)
	TArray<TObjectPtr<UAnimNextRigVMAssetEntry>> Entries;

	/** Internal entries in this asset */
	UPROPERTY()
	TArray<TObjectPtr<UAnimNextRigVMAssetEntry>> InternalEntries;

	UPROPERTY()
	FRigVMClient RigVMClient;

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;

	// The native C++ struct that is used to communicate with this asset
	UE_DEPRECATED(5.6, "This property is deprecated. Please use NativeInterfaces instead")
	UPROPERTY(meta = (ShowDisplayNames, MetaStruct = "/Script/AnimNext.AnimNextNativeDataInterface"))
	TObjectPtr<const UScriptStruct> NativeInterface_DEPRECATED;

	// The list of native C++ structs that are used to communicate with this asset
	UPROPERTY(EditAnywhere, Category = "Native", meta = (ShowDisplayNames, MetaStruct = "/Script/AnimNext.AnimNextNativeDataInterface"))
	TArray<TObjectPtr<const UScriptStruct>> NativeInterfaces;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigVMEdGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM", meta = (AllowPrivateAccess = "true"))
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;

	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket = 0;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;

	UPROPERTY(VisibleAnywhere, Category = "External Packages", AssetRegistrySearchable)
	bool bUsesExternalPackages = true;

	FOnRigVMCompiledEvent RigVMCompiledEvent;

	FRigVMGraphModifiedEvent RigVMGraphModifiedEvent;

	// Delegate to subscribe to modifications to this editor data
	UE::AnimNext::UncookedOnly::FOnEditorDataModified ModifiedDelegate;

	// Delegate to get notified when an interaction bracket reaches 0
	UE::AnimNext::UncookedOnly::FOnInteractionBracketFinished InteractionBracketFinished;

	// Cached exports, generated lazily or on compilation
	mutable TOptional<FAnimNextAssetRegistryExports> CachedExports;
	
	// Collection of models gleaned from graphs
	TArray<TObjectPtr<URigVMGraph>> GraphModels;

	// Set of functions implemented for this graph
	UPROPERTY()
	TArray<TObjectPtr<URigVMEdGraph>> FunctionEdGraphs;

	// Default FunctionLibrary EdGraph
	UPROPERTY()
	TObjectPtr<UAnimNextEdGraph> FunctionLibraryEdGraph;

	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bWarningsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendAllNotifications = false;
	bool bCompileInDebugMode = false;
	bool bSuspendPythonMessagesForRigVMClient = true;
	bool bSuspendEditorDataNotifications = false;
	bool bSuspendCompilationNotifications = false;
};
