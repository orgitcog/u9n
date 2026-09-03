// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundFrontendDocumentCacheInterface.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundFrontendDocumentBuilder.generated.h"

#define UE_API METASOUNDFRONTEND_API


// Forward Declarations
class FMetasoundAssetBase;


namespace Metasound::Frontend
{
	// Forward Declarations
	class INodeTemplate;

	using FConstClassAndNodeFunctionRef = TFunctionRef<void(const FMetasoundFrontendClass&, const FMetasoundFrontendNode&)>;
	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	enum class EInvalidEdgeReason : uint8
	{
		None = 0,
		MismatchedAccessType,
		MismatchedDataType,
		MissingInput,
		MissingOutput,
		COUNT
	};

	METASOUNDFRONTEND_API FString LexToString(const EInvalidEdgeReason& InReason);

	struct FNamedEdge
	{
		const FGuid OutputNodeID;
		const FName OutputName;
		const FGuid InputNodeID;
		const FName InputName;

		friend bool operator==(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return InLHS.OutputNodeID == InRHS.OutputNodeID
				&& InLHS.OutputName == InRHS.OutputName
				&& InLHS.InputNodeID == InRHS.InputNodeID
				&& InLHS.InputName == InRHS.InputName;
		}

		friend bool operator!=(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return !(InLHS == InRHS);
		}

		friend FORCEINLINE uint32 GetTypeHash(const FNamedEdge& InBinding)
		{
			const int32 NameHash = HashCombineFast(GetTypeHash(InBinding.OutputName), GetTypeHash(InBinding.InputName));
			const int32 GuidHash = HashCombineFast(GetTypeHash(InBinding.OutputNodeID), GetTypeHash(InBinding.InputNodeID));
			return HashCombineFast(NameHash, GuidHash);
		}
	};

	struct FModifyInterfaceOptions
	{
		UE_API FModifyInterfaceOptions(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd);
		UE_API FModifyInterfaceOptions(TArray<FMetasoundFrontendInterface>&& InInterfacesToRemove, TArray<FMetasoundFrontendInterface>&& InInterfacesToAdd);
		UE_API FModifyInterfaceOptions(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd);

		TArray<FMetasoundFrontendInterface> InterfacesToRemove;
		TArray<FMetasoundFrontendInterface> InterfacesToAdd;

		// Function used to determine if an old of a removed interface
		// and new member of an added interface are considered equal and
		// to be swapped, retaining preexisting connections (and locations
		// if in editor and 'SetDefaultNodeLocations' option is set)
		TFunction<bool(FName, FName)> NamePairingFunction;

#if WITH_EDITORONLY_DATA
		bool bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA
	};
} // namespace Metasound::Frontend


// Builder Document UObject, which is only used for registration purposes when attempting
// async registration whereby the original document is serialized and must not be mutated.
UCLASS(MinimalAPI)
class UMetaSoundBuilderDocument : public UObject, public IMetaSoundDocumentInterface
{
	GENERATED_BODY()

public:
	UE_API UMetaSoundBuilderDocument(const FObjectInitializer& ObjectInitializer);

	UE_DEPRECATED(5.5, "Use overload supplying MetaSound to copy (builder documents no longer supported for cases outside of cloned document registration.")
	static UE_API UMetaSoundBuilderDocument& Create(const UClass& InBuilderClass);

	// Create and return a valid builder document which copies the provided interface's document & class
	static UE_API UMetaSoundBuilderDocument& Create(const IMetaSoundDocumentInterface& InDocToCopy);

	UE_API virtual bool ConformObjectToDocument() override;

	// Returns the document
	UE_API virtual const FMetasoundFrontendDocument& GetConstDocument() const override;

	// Returns temp path of builder document
	UE_API virtual FTopLevelAssetPath GetAssetPathChecked() const override;

	// Returns the base class registered with the MetaSound UObject registry.
	UE_API virtual const UClass& GetBaseMetaSoundUClass() const final override;

	// Returns the builder class used to modify the given document.
	UE_API virtual const UClass& GetBuilderUClass() const final override;

	// Returns if the document is being actively built (always true as builder documents are always being actively built)
	UE_API virtual bool IsActivelyBuilding() const final override;

private:
	UE_API virtual FMetasoundFrontendDocument& GetDocument() override;

	UE_API virtual void OnBeginActiveBuilder() override;
	UE_API virtual void OnFinishActiveBuilder() override;

	UPROPERTY(Transient)
	FMetasoundFrontendDocument Document;

	UPROPERTY(Transient)
	TObjectPtr<const UClass> MetaSoundUClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UClass> BuilderUClass = nullptr;
};

// Builder used to support dynamically generating MetaSound documents at runtime. Builder contains caches that speed up
// common search and modification operations on a given document, which may result in slower performance on construction,
// but faster manipulation of its managed document.  The builder's managed copy of a document is expected to not be modified
// by any external system to avoid cache becoming stale.
USTRUCT()
struct FMetaSoundFrontendDocumentBuilder
{
	GENERATED_BODY()

public:
	// Default ctor should typically never be used directly as builder interface (and optionally delegates) should be specified on construction (Default exists only to make UObject reflection happy).
	UE_API FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface = { }, TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> InDocumentDelegates = { }, bool bPrimeCache = false);
	UE_API virtual ~FMetaSoundFrontendDocumentBuilder();

	// Call when the builder will no longer modify the IMetaSoundDocumentInterface
	UE_API void FinishBuilding();

	// Adds new dependency to MetaSound. Typically not necessary to call directly as dependencies are added automatically via registry when nodes are added, and can be removed when no longer referenced (see 'RemoveUnusedDependencies`).
	UE_API const FMetasoundFrontendClass* AddDependency(FMetasoundFrontendClass NewDependency);

	UE_API void AddEdge(FMetasoundFrontendEdge&& InNewEdge, const FGuid* InPageID = nullptr);
	UE_API bool AddNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& ConnectionsToMake, TArray<const FMetasoundFrontendEdge*>* OutEdgesCreated = nullptr, bool bReplaceExistingConnections = true, const FGuid* InPageID = nullptr);
	UE_API bool AddEdgesByNodeClassInterfaceBindings(const FGuid& InFromNodeID, const FGuid& InToNodeID, bool bReplaceExistingConnections = true, const FGuid* InPageID = nullptr);
	UE_API bool AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true, const FGuid* InPageID = nullptr);
	UE_API bool AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true, const FGuid* InPageID = nullptr);

	// Adds Graph Input to document, which in turn adds a referencing input node to ALL pages.  If valid PageID is provided, returns associated page's node pointer.
	// If none provided, returns node pointer to node for the builder's currently set build page ID (see 'GetBuildPageID').
	UE_API const FMetasoundFrontendNode* AddGraphInput(FMetasoundFrontendClassInput ClassInput, const FGuid* InPageID = nullptr);

	UE_API const FMetasoundFrontendVariable* AddGraphVariable(FName VariableName, FName DataType, const FMetasoundFrontendLiteral* Literal = nullptr, const FText* DisplayName = nullptr, const FText* Description = nullptr, const FGuid* InPageID = nullptr);
	UE_API const FMetasoundFrontendNode* AddGraphVariableNode(FName VariableName, EMetasoundFrontendClassType ClassType, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);
	UE_API const FMetasoundFrontendNode* AddGraphVariableMutatorNode(FName VariableName, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);
	UE_API const FMetasoundFrontendNode* AddGraphVariableAccessorNode(FName VariableName, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);
	UE_API const FMetasoundFrontendNode* AddGraphVariableDeferredAccessorNode(FName VariableName, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);

	// Adds node to document to the page associated with the given PageID.  If no valid PageID is provided, adds and returns node pointer to node for the builder's
	// currently set build page ID (see 'GetBuildPageID').
	UE_API const FMetasoundFrontendNode* AddGraphNode(const FMetasoundFrontendGraphClass& InClass, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);

	// Adds Graph Output to document, which in turn adds a referencing output node to ALL pages.  If valid PageID is provided, returns associated page's node pointer.
	// If none provided, returns node pointer to node for the builder's currently set build page ID (see 'GetBuildPageID').
	UE_API const FMetasoundFrontendNode* AddGraphOutput(FMetasoundFrontendClassOutput ClassOutput, const FGuid* InPageID = nullptr);

	UE_API bool AddInterface(FName InterfaceName);

	UE_API const FMetasoundFrontendNode* AddNodeByClassName(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion = 1, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);
	
	UE_API const FMetasoundFrontendNode* AddNodeByTemplate(const Metasound::Frontend::INodeTemplate& InTemplate, FNodeTemplateGenerateInterfaceParams Params, FGuid InNodeID = FGuid::NewGuid(), const FGuid* InPageID = nullptr);

#if WITH_EDITORONLY_DATA
	// Adds a graph page to the given builder's document
	UE_API const FMetasoundFrontendGraph& AddGraphPage(const FGuid& InPageID, bool bDuplicateLastGraph, bool bSetAsBuildGraph = true);
#endif // WITH_EDITORONLY_DATA

	// Returns whether or not the given edge can be added, which requires that its input
	// is not already connected and the edge is valid (see function 'IsValidEdge').
	UE_API bool CanAddEdge(const FMetasoundFrontendEdge& InEdge, const FGuid* InPageID = nullptr) const;

	// Clears document completely of all graph page data (nodes, edges, & member metadata), dependencies,
	// interfaces, member metadata, preset state, etc. Leaves ClassMetadata intact. Reloads the builder state,
	// so external delegates must be relinked if desired.
	UE_API void ClearDocument(TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> ModifyDelegates);

	UE_DEPRECATED(5.5, "Use ClearDocument instead")
	void ClearGraph() {  }

#if WITH_EDITORONLY_DATA
	UE_API bool ClearMemberMetadata(const FGuid& InMemberID);
#endif // WITH_EDITORONLY_DATA

	UE_API bool ContainsDependencyOfType(EMetasoundFrontendClassType ClassType) const;
	UE_API bool ContainsEdge(const FMetasoundFrontendEdge& InEdge, const FGuid* InPageID = nullptr) const;
	UE_API bool ContainsNode(const FGuid& InNodeID, const FGuid* InPageID = nullptr) const;

	UE_API bool ConvertFromPreset();
	UE_API bool ConvertToPreset(const FMetasoundFrontendDocument& InReferencedDocument, TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> ModifyDelegates = { });

	UE_API const FMetasoundFrontendClassInput* DuplicateGraphInput(FName ExistingName, FName NewName);
	UE_API const FMetasoundFrontendClassOutput* DuplicateGraphOutput(FName ExistingName, FName NewName);
	UE_API const FMetasoundFrontendVariable* DuplicateGraphVariable(FName ExistingName, FName NewName, const FGuid* InPageID = nullptr);

	UE_DEPRECATED(5.6, "Use the duplicate overload that supplies existing name and new name and returns input")
	UE_API const FMetasoundFrontendNode* DuplicateGraphInput(const FMetasoundFrontendClassInput& InClassInput, const FName InName, const FGuid* InPageID = nullptr);

	UE_DEPRECATED(5.6, "Use the duplicate overload that supplies existing name and new name and returns output")
	UE_API const FMetasoundFrontendNode* DuplicateGraphOutput(const FMetasoundFrontendClassOutput& InClassOutput, const FName InName, const FGuid* InPageID = nullptr);


#if WITH_EDITORONLY_DATA
	UE_API const FMetasoundFrontendEdgeStyle* FindConstEdgeStyle(const FGuid& InNodeID, FName OutputName, const FGuid* InPageID = nullptr) const;
	UE_API FMetasoundFrontendEdgeStyle* FindEdgeStyle(const FGuid& InNodeID, FName OutputName, const FGuid* InPageID = nullptr);
	UE_API FMetasoundFrontendEdgeStyle& FindOrAddEdgeStyle(const FGuid& InNodeID, FName OutputName, const FGuid* InPageID = nullptr);
	UE_API const FMetaSoundFrontendGraphComment* FindGraphComment(const FGuid& InCommentID, const FGuid* InPageID = nullptr) const;
	UE_API FMetaSoundFrontendGraphComment* FindGraphComment(const FGuid& InCommentID, const FGuid* InPageID = nullptr);
	UE_API FMetaSoundFrontendGraphComment& FindOrAddGraphComment(const FGuid& InCommentID, const FGuid* InPageID = nullptr);
	UE_API UMetaSoundFrontendMemberMetadata* FindMemberMetadata(const FGuid& InMemberID);
#endif // WITH_EDITORONLY_DATA

	static UE_API bool FindDeclaredInterfaces(const FMetasoundFrontendDocument& InDocument, TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces);
	UE_API bool FindDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const;

	UE_API const FMetasoundFrontendClass* FindDependency(const FGuid& InClassID) const;
	UE_API const FMetasoundFrontendClass* FindDependency(const FMetasoundFrontendClassMetadata& InMetadata) const;
	UE_API TArray<const FMetasoundFrontendEdge*> FindEdges(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	UE_API const FMetasoundFrontendClassInput* FindGraphInput(FName InputName) const;
	UE_API const FMetasoundFrontendNode* FindGraphInputNode(FName InputName, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendClassOutput* FindGraphOutput(FName OutputName) const;
	UE_API const FMetasoundFrontendNode* FindGraphOutputNode(FName OutputName, const FGuid* InPageID = nullptr) const;

	UE_API const FMetasoundFrontendVariable* FindGraphVariable(const FGuid& InVariableID, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendVariable* FindGraphVariable(FName InVariableName, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendVariable* FindGraphVariableByNodeID(const FGuid& InNodeID, const FGuid* InPageID = nullptr) const;

	UE_API bool FindInterfaceInputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutInputs, const FGuid* InPageID = nullptr) const;
	UE_API bool FindInterfaceOutputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutOutputs, const FGuid* InPageID = nullptr) const;

	// Accessor for the currently set build graph.
	UE_API const FMetasoundFrontendGraph& FindConstBuildGraphChecked() const;

	UE_API const FMetasoundFrontendNode* FindNode(const FGuid& InNodeID, const FGuid* InPageID = nullptr) const;

	UE_API const TConstStructView<FMetaSoundFrontendNodeConfiguration> FindNodeConfiguration(const FGuid& InNodeID, const FGuid* InPageID = nullptr) const;

	UE_EXPERIMENTAL(5.6, "Non const builder access to node configuration is experimental.")
	UE_API TInstancedStruct<FMetaSoundFrontendNodeConfiguration> FindNodeConfiguration(const FGuid& InNodeID, const FGuid* InPageID = nullptr);

	// Return the node's index in the document's specified paged graph's node array 
	UE_API const int32* FindNodeIndex(const FGuid& InNodeID, const FGuid* InPageID = nullptr) const;

	UE_API const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, FName InVertexName, const FGuid* InPageID = nullptr) const;

	// Returns class defaults associated with the given node input (as defined in the associated node's dependency)
	UE_API const TArray<FMetasoundFrontendClassInputDefault>* FindNodeClassInputDefaults(const FGuid& InNodeID, FName InVertexName, const FGuid* InPageID = nullptr) const;

	// Returns node input's vertex default if valid and assigned.
	UE_API const FMetasoundFrontendVertexLiteral* FindNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	// Returns node input's vertex default if valid and assigned.
	UE_API const FMetasoundFrontendVertexLiteral* FindNodeInputDefault(const FGuid& InNodeID, FName InVertexName, const FGuid* InPageID = nullptr) const;

	UE_API TArray<const FMetasoundFrontendVertex*> FindNodeInputs(const FGuid& InNodeID, FName TypeName = FName(), const FGuid* InPageID = nullptr) const;
	UE_API TArray<const FMetasoundFrontendVertex*> FindNodeInputsConnectedToNodeOutput(const FGuid& InOutputNodeID, const FGuid& InOutputVertexID, TArray<const FMetasoundFrontendNode*>* ConnectedInputNodes = nullptr, const FGuid* InPageID = nullptr) const;

	UE_API const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, FName InVertexName, const FGuid* InPageID = nullptr) const;
	UE_API TArray<const FMetasoundFrontendVertex*> FindNodeOutputs(const FGuid& InNodeID, FName TypeName = FName(), const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendVertex* FindNodeOutputConnectedToNodeInput(const FGuid& InInputNodeID, const FGuid& InInputVertexID, const FMetasoundFrontendNode** ConnectedOutputNode = nullptr, const FGuid* InPageID = nullptr) const;

	// Return the index of the given page in the document's PagedGraphs array 
	UE_API int32 FindPageIndex(const FGuid& InPageID) const;

	UE_API const FMetasoundFrontendDocument& GetConstDocumentChecked() const;
	UE_API const IMetaSoundDocumentInterface& GetConstDocumentInterfaceChecked() const;
	UE_API const FString GetDebugName() const;

	UE_DEPRECATED(5.5, "Use GetConstDocumentChecked() instead")
	UE_API const FMetasoundFrontendDocument& GetDocument() const;

	// The graph ID used when requests are made to mutate specific paged graph topology (ex. adding or removing nodes or edges)
	UE_API const FGuid& GetBuildPageID() const;

#if WITH_EDITOR
	// Gets the editor-only style of a node with the given ID.
	UE_API const FMetasoundFrontendNodeStyle* GetNodeStyle(const FGuid& InNodeID, const FGuid* InPageID = nullptr);
#endif // WITH_EDITOR

	template<typename TObjectType>
	TObjectType& CastDocumentObjectChecked() const
	{
		UObject* Owner = DocumentInterface.GetObject();
		return *CastChecked<TObjectType>(Owner);
	}

	// Generates and returns new class name for the given builder's document. Should be used with extreme caution
	// (i.e. on new assets, when migrating assets, or upon generation of transient MetaSounds), as using a persistent
	// builder registered with the DocumentBuilderRegistry may result in stale asset records keyed off of an undefined class
	// name.  In addition, this can potentially leave existing node references in an abandoned state to this class causing
	// asset validation errors.
	UE_API FMetasoundFrontendClassName GenerateNewClassName();

	UE_API Metasound::Frontend::FDocumentModifyDelegates& GetDocumentDelegates();

	UE_DEPRECATED(5.5, "Use GetConstDocumentInterfaceChecked instead")
	UE_API const IMetaSoundDocumentInterface& GetDocumentInterface() const;

	// Path for document object provided at construction time. Cached on builder as a useful means of debugging
	// and enables weak reference removal from the builder registry should the object be mid-destruction.
	UE_API const FTopLevelAssetPath& GetHintPath() const;

	UE_API FMetasoundAssetBase& GetMetasoundAsset() const;
	
	// Get the asset referenced by this builder's preset asset, nullptr if builder is not a preset.
	UE_API FMetasoundAssetBase* GetReferencedPresetAsset() const;

	UE_API int32 GetTransactionCount() const;

	UE_API TArray<const FMetasoundFrontendNode*> GetGraphInputTemplateNodes(FName InInputName, const FGuid* InPageID = nullptr);

	// If graph is set to be a preset, returns set of graph input names inheriting default data from the referenced graph. If not a preset, returns null.
	UE_API const TSet<FName>* GetGraphInputsInheritingDefault() const;

	UE_API EMetasoundFrontendVertexAccessType GetNodeInputAccessType(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	UE_DEPRECATED(5.5, "Use FindNodeInputClass overloads instead and use GetDefaults() on result (now supports page values)")
	UE_API const FMetasoundFrontendLiteral* GetNodeInputClassDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	UE_DEPRECATED(5.5, "Use FindNodeInputDefault and returned struct Value member instead")
	UE_API const FMetasoundFrontendLiteral* GetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	UE_API EMetasoundFrontendVertexAccessType GetNodeOutputAccessType(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

#if WITH_EDITORONLY_DATA
	UE_API const bool GetIsAdvancedDisplay(const FName MemberName, const EMetasoundFrontendClassType Type) const;
#endif // WITH_EDITORONLY_DATA

	// Returns the default value set for the input with the given name on the given page.
	UE_API const FMetasoundFrontendLiteral* GetGraphInputDefault(FName InputName, const FGuid* InPageID = nullptr) const;

	// Returns the default value set for the variable with the given name on the given page.
	UE_API const FMetasoundFrontendLiteral* GetGraphVariableDefault(FName InputName, const FGuid* InPageID = nullptr) const;

	// Initializes the builder's document, using the (optional) provided document template, (optional) class name, and (optionally) whether or not to reset the existing class version.
	UE_API void InitDocument(const FMetasoundFrontendDocument* InDocumentTemplate = nullptr, const FMetasoundFrontendClassName* InNewClassName = nullptr, bool bResetVersion = true);

	// Initializes GraphClass Metadata, optionally resetting the version back to 1.0 and/or creating a unique class name if a name is not provided.
	static UE_API void InitGraphClassMetadata(FMetasoundFrontendClassMetadata& InOutMetadata, bool bResetVersion = false, const FMetasoundFrontendClassName* NewClassName = nullptr);
	UE_API void InitGraphClassMetadata(bool bResetVersion, const FMetasoundFrontendClassName* NewClassName);

	UE_API void InitNodeLocations();

	UE_DEPRECATED(5.5, "Use invalidate overload that is provided new version of modify delegates")
	void InvalidateCache() { }

	UE_API bool IsDependencyReferenced(const FGuid& InClassID) const;
	UE_API bool IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;
	UE_API bool IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr) const;

	UE_API bool IsInterfaceDeclared(FName InInterfaceName) const;
	UE_API bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InInterfaceVersion) const;
	UE_API bool IsPreset() const;

	// Returns whether or not builder is attached to a DocumentInterface and is valid to build or act on a document.
	UE_API bool IsValid() const;

	// Returns whether or not the given edge is valid (i.e. represents an input and output that equate in data and access types) or malformed.
	// Note that this does not return whether or not the given edge exists, but rather if it could be legally applied to the given edge vertices.
	UE_API Metasound::Frontend::EInvalidEdgeReason IsValidEdge(const FMetasoundFrontendEdge& InEdge, const FGuid* InPageID = nullptr) const;

	// Iterates nodes that are filtered by only subscribing to a class with the given type (asserts if provided invalid class type).
	UE_API void IterateNodesByClassType(Metasound::Frontend::FConstClassAndNodeFunctionRef Func, EMetasoundFrontendClassType ClassType, const FGuid* InPageID = nullptr) const;

	UE_API bool ModifyInterfaces(Metasound::Frontend::FModifyInterfaceOptions&& InOptions);

	UE_DEPRECATED(5.5,
		"Cache invalidation may require new copy of delegates. In addition, re-priming is discouraged. "
		"To enforce this, new recommended pattern is to construct a new builder instead")
	UE_API void ReloadCache();

	// Removes all dependencies with the given ClassID. Removes any nodes (and corresponding edges) remaining in any MetaSound paged graphs.
	UE_API bool RemoveDependency(const FGuid& InClassID);

	// Removes all dependencies with the given Class Type, Name, & Version Number. Removes any nodes (and corresponding edges) remaining in any MetaSound paged graphs.
	UE_API bool RemoveDependency(EMetasoundFrontendClassType ClassType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InClassVersionNumber);

	UE_API bool RemoveEdge(const FMetasoundFrontendEdge& EdgeToRemove, const FGuid* InPageID = nullptr);

	// Removes all edges connected to an input or output vertex associated with the node of the given ID.
	UE_API bool RemoveEdges(const FGuid& InNodeID, const FGuid* InPageID = nullptr);

	UE_API bool RemoveEdgesByNodeClassInterfaceBindings(const FGuid& InOutputNodeID, const FGuid& InInputNodeID, const FGuid* InPageID = nullptr);
	UE_API bool RemoveEdgesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr);
	UE_API bool RemoveEdgeToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr);

#if WITH_EDITORONLY_DATA
	UE_API bool RemoveEdgeStyle(const FGuid& InNodeID, FName OutputName, const FGuid* InPageID = nullptr);
	UE_API bool RemoveGraphComment(const FGuid& InCommentID, const FGuid* InPageID = nullptr);
#endif // WITH_EDITORONLY_DATA

	UE_API bool RemoveGraphInput(FName InputName, bool bRemoveTemplateInputNodes = true);
	UE_API bool RemoveGraphOutput(FName OutputName);

#if WITH_EDITORONLY_DATA
	UE_API bool RemoveGraphPage(const FGuid& InPageID);
#endif // WITH_EDITORONLY_DATA

	UE_API bool RemoveGraphVariable(FName VariableName, const FGuid* InPageID = nullptr);

	UE_API bool RemoveInterface(FName Name);
	UE_API bool RemoveNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& InNamedEdgesToRemove, TArray<FMetasoundFrontendEdge>* OutRemovedEdges = nullptr, const FGuid* InPageID = nullptr);
	UE_API bool RemoveNode(const FGuid& InNodeID, const FGuid* InPageID = nullptr);

#if WITH_EDITORONLY_DATA
	UE_API int32 RemoveNodeLocation(const FGuid& InNodeID, const FGuid* InLocationGuid = nullptr, const FGuid* InPageID = nullptr);
#endif // WITH_EDITOR

	UE_API void Reload(TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> Delegates = {}, bool bPrimeCache = false);

#if WITH_EDITORONLY_DATA
	UE_API bool RemoveGraphInputDefault(FName InputName, const FGuid& InPageID, bool bClearInheritsDefault = true);
#endif // WITH_EDITORONLY_DATA

	UE_API bool RemoveNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FGuid* InPageID = nullptr);
	UE_API bool RemoveUnusedDependencies();

	UE_DEPRECATED(5.5, "Use GenerateNewClassName instead")
	UE_API bool RenameRootGraphClass(const FMetasoundFrontendClassName& InName);

#if WITH_EDITORONLY_DATA
	UE_API bool ResetGraphInputDefault(FName InputName);

	// Removes all graph pages except the default.  If bClearDefaultPage is true, clears the default graph page implementation.
	UE_API void ResetGraphPages(bool bClearDefaultGraph);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	UE_API void SetAuthor(const FString& InAuthor);

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	// Sets the builder's targeted paged graph ID to the given ID if it exists.
	// Returns true if the builder is already targeting the given ID or if it successfully
	// found a page implementation with the given ID and was able to switch to it, false if not.
	// Swapping the targeted build graph ID clears the local cache, so swapping frequently can
	// induce cash thrashing. BroadcastDelegate should always be true unless dealing with the controller
	// API (exposed as a mechanism for mutating via controllers while deprecating.  Option will be removed
	// in a future build).
	UE_API bool SetBuildPageID(const FGuid& InBuildPageID, bool bBroadcastDelegate = true);

	// Sets the given input`s IsAdvancedDisplay state. AdvancedDisplay pins are hidden in the node by default.
	// returns true if state was changed.
	UE_API bool SetGraphInputAdvancedDisplay(const FName InputName, const bool InAdvancedDisplay);
#endif // WITH_EDITORONLY_DATA

	// Sets the given graph input's access type. If connected to other nodes and access type is not compatible,
	// associated edges/connections are removed.  Returns true if either DataType was successfully set to new
	// value or if AccessType is already the given AccessType.
	UE_API bool SetGraphInputAccessType(FName InputName, EMetasoundFrontendVertexAccessType AccessType);

	// Sets the given graph input's data type. If connected to other nodes, associated edges/connections
	// are removed.  Returns true if either DataType was successfully set to new value or if DataType is
	// already the given DataType.
	UE_API bool SetGraphInputDataType(FName InputName, FName DataType);

	UE_API bool SetGraphInputDefault(FName InputName, FMetasoundFrontendLiteral InDefaultLiteral, const FGuid* InPageID = nullptr);
	UE_API bool SetGraphInputDefaults(FName InputName, TArray<FMetasoundFrontendClassInputDefault> Defaults);

#if WITH_EDITORONLY_DATA
	// Sets the graph input's description. Returns true if found and set, false if not.
	UE_API bool SetGraphInputDescription(FName InputName, FText Description);

	// Sets the graph input's display name. Returns true if found and set, false if not.
	UE_API bool SetGraphInputDisplayName(FName InputName, FText DisplayName);
#endif // WITH_EDITORONLY_DATA

	// Sets whether or not graph input inherits default.  By default, updates only if graph is marked as a preset.
	// Optionally, if bForceUpdate is set, updates inheritance even if not a preset (primarily for clearing flag
	// if non-preset has unnecessary data).
	UE_API bool SetGraphInputInheritsDefault(FName InName, bool bInputInheritsDefault, bool bForceUpdate = false);

	// Sets a given graph input's name to a new name. Succeeds if the graph output exists and the new name is set (or is the same as the old name).
	UE_API bool SetGraphInputName(FName InputName, FName InName);

#if WITH_EDITORONLY_DATA
	// Sets the graph input's sort order index. Returns true if found and set, false if not.
	UE_API bool SetGraphInputSortOrderIndex(const FName InputName, const int32 InSortOrderIndex);

	// Sets the graph output's sort order index. Returns true if found and set, false if not.
	UE_API bool SetGraphOutputSortOrderIndex(const FName OutputName, const int32 InSortOrderIndex);

	// Sets the given output`s IsAdvancedDisplay state. AdvancedDisplay pins are hidden in the node by default.
	// returns true if state was changed.
	UE_API bool SetGraphOutputAdvancedDisplay(const FName OutputName, const bool InAdvancedDisplay);
#endif // WITH_EDITORONLY_DATA

	// Sets the given graph output's access type. If connected to other nodes and access type is not compatible,
	// associated edges/connections are removed.  Returns true if either DataType was successfully set to new
	// value or if AccessType is already the given AccessType.
	UE_API bool SetGraphOutputAccessType(FName OutputName, EMetasoundFrontendVertexAccessType AccessType);

	// Sets the given graph output's data type. If connected to other nodes, associated edges/connections
	// are removed.  Returns true if either DataType was successfully set to new value or if DataType is
	// already the given DataType.
	UE_API bool SetGraphOutputDataType(FName OutputName, FName DataType);

#if WITH_EDITORONLY_DATA
	// Sets the graph output's description. Returns true if found and set, false if not.
	UE_API bool SetGraphOutputDescription(FName OutputName, FText Description);

	// Sets the graph input's display name. Returns true if found and set, false if not.
	UE_API bool SetGraphOutputDisplayName(FName OutputName, FText DisplayName);
#endif // WITH_EDITORONLY_DATA

	// Sets a given graph output's name to a new name. Succeeds if the graph output exists and the new name is set (or is the same as the old name).
	UE_API bool SetGraphOutputName(FName InputName, FName InName);

	// Sets the given graph variable's default.
	UE_API bool SetGraphVariableDefault(FName VariableName, FMetasoundFrontendLiteral InDefaultLiteral, const FGuid* InPageID = nullptr);

#if WITH_EDITORONLY_DATA
	// Sets the given graph variable's description.
	UE_API bool SetGraphVariableDescription(FName VariableName, FText Description, const FGuid* InPageID = nullptr);

	// Sets the given graph variable's display name.
	UE_API bool SetGraphVariableDisplayName(FName VariableName, FText DisplayName, const FGuid* InPageID = nullptr);
#endif // WITH_EDITORONLY_DATA

	// Sets the given graph variable's description.
	UE_API bool SetGraphVariableName(FName VariableName, FName NewName, const FGuid* InPageID = nullptr);

#if WITH_EDITOR
	UE_API void SetDisplayName(const FText& InDisplayName);
	UE_API void SetDescription(const FText& InDescription);
	UE_API void SetKeywords(const TArray<FText>& InKeywords);
	UE_API void SetCategoryHierarchy(const TArray<FText>& InCategoryHierarchy);
	UE_API void SetIsDeprecated(const bool bInIsDeprecated);

	UE_API void SetMemberMetadata(UMetaSoundFrontendMemberMetadata& NewMetadata);

	// Sets the editor-only comment to the provided value.
	// Returns true if the node was found and the comment was updated, false if not.
	UE_API bool SetNodeComment(const FGuid& InNodeID, FString&& InNewComment, const FGuid* InPageID = nullptr);

	// Sets the editor-only comment visibility.
	// Returns true if the node was found and the visibility was set, false if not.
	UE_API bool SetNodeCommentVisible(const FGuid& InNodeID, bool bIsVisible, const FGuid* InPageID = nullptr);

	// Sets the editor-only node location of a node with the given ID to the provided location.
	// Returns true if the node was found and the location was updated, false if not.
	UE_API bool SetNodeLocation(const FGuid& InNodeID, const FVector2D& InLocation, const FGuid* InLocationGuid = nullptr, const FGuid* InPageID = nullptr);

	// Sets the editor-only Unconnected Pins Hidden for a node with the given ID.
	UE_API bool SetNodeUnconnectedPinsHidden(const FGuid& InNodeID, const bool bUnconnectedPinsHidden, const FGuid* InPageID = nullptr);
#endif // WITH_EDITOR
	
	// Sets the node configuration for the given node and updates the interface
	// returns true if the node configuration is set
	UE_API bool SetNodeConfiguration(const FGuid& InNodeID, TInstancedStruct<FMetaSoundFrontendNodeConfiguration> InNodeConfiguration, const FGuid* InPageID = nullptr);

	UE_API bool SetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FMetasoundFrontendLiteral& InLiteral, const FGuid* InPageID = nullptr);

	// Sets the document's version number.  Should only be called by document versioning.
	UE_API void SetVersionNumber(const FMetasoundFrontendVersionNumber& InDocumentVersionNumber);

	UE_API bool SwapGraphInput(const FMetasoundFrontendClassVertex& InExistingInputVertex, const FMetasoundFrontendClassVertex& NewInputVertex);
	UE_API bool SwapGraphOutput(const FMetasoundFrontendClassVertex& InExistingOutputVertex, const FMetasoundFrontendClassVertex& NewOutputVertex);

#if WITH_EDITOR
	UE_DEPRECATED(5.5, "Use 'UpdateDependencyRegistryData' instead and supply keys (comprised of name, version & node class type)")
	UE_API bool UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames);

	UE_API bool UpdateDependencyRegistryData(const TMap<Metasound::Frontend::FNodeRegistryKey, Metasound::Frontend::FNodeRegistryKey>& OldToNewClassKeys);
#endif // WITH_EDITOR

	// If a node contains a node configuration, update the node class interface and interface.
	// Returns true if node is found.
	UE_API bool UpdateNodeInterfaceFromConfiguration(const FGuid& InNodeID, const FGuid* InPageID = nullptr);

#if WITH_EDITORONLY_DATA
	// Transforms template nodes within the given builder's document, which can include swapping associated edges and/or
	// replacing nodes with other, registry-defined concrete node class instances. Returns true if any template nodes were processed.
	UE_API bool TransformTemplateNodes();

	// Versions legacy document members that contained interface information
	UE_DEPRECATED(5.5, "Moved to internally implemented versioning logic")
	UE_API bool VersionInterfaces();

	// Struct enabling property migration of data that must be applied prior to versioning logic
	struct IPropertyVersionTransform
	{
	public:
		virtual ~IPropertyVersionTransform() = default;

	protected:
		virtual bool Transform(FMetaSoundFrontendDocumentBuilder& Builder) const = 0;

		// Allows for unsafe access to a document for property migration.
		static FMetasoundFrontendDocument& GetDocumentUnsafe(const FMetaSoundFrontendDocumentBuilder& Builder);
	};
#endif // WITH_EDITORONLY_DATA

private:
	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	UE_API FMetasoundFrontendNode* AddNodeInternal(const FMetasoundFrontendClassMetadata& InClassMetadata, Metasound::Frontend::FFinalizeNodeFunctionRef FinalizeNode, const FGuid& InPageID, FGuid InNodeID = FGuid::NewGuid(), int32* NewNodeIndex = nullptr);
	UE_API FMetasoundFrontendNode* AddNodeInternal(const Metasound::Frontend::FNodeRegistryKey& InClassKey, FFinalizeNodeFunctionRef FinalizeNode, const FGuid& InPageID, FGuid InNodeID = FGuid::NewGuid(), int32* NewNodeIndex = nullptr);
	UE_API void BeginBuilding(TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> Delegates = {}, bool bPrimeCache = false);

	// Conforms GraphOutput node's ClassID, Access & Data Type with the GraphOutput.
	// creating and removing dependencies as necessary within the document dependency array. Does *NOT*
	// modify edge data (i.e. if the DataType is changed on the given node and it has corresponding
	// edges, edges may then be invalid due to access type/DataType incompatibility).
	UE_API bool ConformGraphInputNodeToClass(const FMetasoundFrontendClassInput& GraphInput);

	// Conforms GraphOutput node's ClassID, Access & Data Type with the GraphOutput.
	// Creates and removes dependencies as necessary within the document dependency array. Does *NOT*
	// modify edge data (i.e. if the DataType is changed on the given node and it has corresponding
	// edges, edges may then be invalid due to access type/DataType incompatibility).
	UE_API bool ConformGraphOutputNodeToClass(const FMetasoundFrontendClassOutput& GraphOutput);

	UE_API FMetasoundFrontendGraph& FindBuildGraphChecked() const;

	UE_API FMetasoundFrontendVariable* FindGraphVariableInternal(FName InVariableName, const FGuid* InPageID = nullptr);

	UE_API bool FindNodeClassInterfaces(const FGuid& InNodeID, TSet<FMetasoundFrontendVersion>& OutInterfaces, const FGuid& InPageID) const;
	UE_API FMetasoundFrontendNode* FindNodeInternal(const FGuid& InNodeID, const FGuid* InPageID = nullptr);

	UE_API const FMetasoundFrontendNode* FindHeadNodeInVariableStack(FName VariableName, const FGuid* InPageID = nullptr) const;
	UE_API const FMetasoundFrontendNode* FindTailNodeInVariableStack(FName VariableName, const FGuid* InPageID = nullptr) const;

	UE_API void IterateNodesConnectedWithVertex(const FMetasoundFrontendVertexHandle& Vertex, TFunctionRef<void(const FMetasoundFrontendEdge&, FMetasoundFrontendNode&)> NodeIndexIterFunc, const FGuid& InPageID);

	UE_API const FTopLevelAssetPath GetBuilderClassPath() const;
	UE_API FMetasoundFrontendDocument& GetDocumentChecked() const;
	UE_API IMetaSoundDocumentInterface& GetDocumentInterfaceChecked() const;

	UE_API void RemoveSwapDependencyInternal(int32 Index);

	UE_API bool SpliceVariableNodeFromStack(const FGuid& InNodeID, const FGuid& InPageID);
	UE_API bool UnlinkVariableNode(const FGuid& InNodeID, const FGuid& InPageID);

	UPROPERTY(Transient)
	TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface;

	// Page ID to apply build transaction to if no optional PageID is provided in explicit function call.
	// (Also used to support back compat for Controller API until mutable controllers are adequately deprecated).
	UPROPERTY(Transient)
	FGuid BuildPageID;

	TSharedPtr<Metasound::Frontend::IDocumentCache> DocumentCache;
	TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> DocumentDelegates;

	FTopLevelAssetPath HintPath;
};

#undef UE_API
