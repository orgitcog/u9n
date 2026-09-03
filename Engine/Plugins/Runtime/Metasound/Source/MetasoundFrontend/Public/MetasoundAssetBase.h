// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundAccessPtr.h"
#include "MetasoundAssetManager.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentAccessPtr.h"
#include "MetasoundFrontendRegistryKey.h"
#include "MetasoundGraph.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundVertex.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"

#define UE_API METASOUNDFRONTEND_API


// Forward Declarations
class UEdGraph;
struct FMetaSoundFrontendDocumentBuilder;
class IConsoleVariable;
typedef TMulticastDelegate<void(IConsoleVariable*), FDefaultDelegateUserPolicy> FConsoleVariableMulticastDelegate;

namespace Metasound
{
	class IGraph;
	class FGraph;
	namespace Frontend
	{
		// Forward Declarations
		class IInterfaceRegistryEntry;

		METASOUNDFRONTEND_API TRange<float> GetBlockRateClampRange();
		METASOUNDFRONTEND_API float GetBlockRateOverride();
		METASOUNDFRONTEND_API FConsoleVariableMulticastDelegate& GetBlockRateOverrideChangedDelegate();

		METASOUNDFRONTEND_API TRange<int32> GetSampleRateClampRange();
		METASOUNDFRONTEND_API int32 GetSampleRateOverride();
		METASOUNDFRONTEND_API FConsoleVariableMulticastDelegate& GetSampleRateOverrideChangedDelegate();
		class FProxyDataCache;

	} // namespace Frontend
} // namespace Metasound

/** FMetasoundAssetBase is a mix-in subclass for UObjects which utilize MetaSound objects. As MetaSounds
 * can now be generated dynamically via the Builder API, the name does not accurately reflect this classes
 * current implementation.  While it currently has some support for asset, editor graph & document accessors (actively
 * being deprecated), its primary use is to support all access to runtime-specific, MetaSound features and associated
 * data, such as proxy generation and runtime node class registration.  For forward support of the MetaSound document
 * model and supported accessors, see 'IMetaSoundDocumentInterface'.
 */
class FMetasoundAssetBase : public IAudioProxyDataFactory
{
public:
	UE_API virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams) override;

	static UE_API const FString FileExtension;

	FMetasoundAssetBase() = default;
	virtual ~FMetasoundAssetBase() = default;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const = 0;

	// Returns the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @return Editor graph associated with this metasound uobject.
	virtual UEdGraph* GetGraph() const = 0;
	virtual UEdGraph& GetGraphChecked() const = 0;
	virtual void MigrateEditorGraph(FMetaSoundFrontendDocumentBuilder& OutBuilder) = 0;

	// Sets the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @param Editor graph associated with this metasound object.
	virtual void SetGraph(UEdGraph* InGraph) = 0;

	UE_DEPRECATED(5.6, "AssetClass tags now directly serialized using UObject GetAssetRegistryTags call")
	virtual void SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InClassInfo) { }
#endif // WITH_EDITORONLY_DATA

	UE_DEPRECATED(5.5, "Moved to IMetaSoundDocumentInterface::ConformObjectToDocument")
	UE_API virtual bool ConformObjectDataToInterfaces();

	// Registers the root graph of the given asset with the MetaSound Frontend. Unlike 'UpdateAndRegisterForSerialization", this call
	// generates all necessary runtime data to execute the given graph (i.e. INodes).
	UE_API virtual void UpdateAndRegisterForExecution(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions = Metasound::Frontend::FMetaSoundAssetRegistrationOptions());

	UE_DEPRECATED(5.5, "Moved to UpdateAndRegisterForExecution.")
	UE_API virtual void RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions = Metasound::Frontend::FMetaSoundAssetRegistrationOptions());

	// Unregisters the root graph of the given asset with the MetaSound Frontend.
	UE_API void UnregisterGraphWithFrontend();

	UE_DEPRECATED(5.5, "Moved to UpdateAndRegisterForSerialization instead, which is only in builds set to load editor-only data.")
	UE_API void CookMetaSound();

#if WITH_EDITORONLY_DATA
	// Updates and registers this and referenced MetaSound document objects with the NodeClass Registry. AutoUpdates and
	// optimizes aforementioned documents for serialization. Unlike 'UpdateAndRegisterForRuntime', does not generate required
	// runtime data for graph execution. If CookPlatformName is set, used to strip data not required for the provided platform.
	UE_API void UpdateAndRegisterForSerialization(FName CookPlatformName = { });
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Rebuild dependent asset classes
	UE_API void RebuildReferencedAssetClasses();
#endif // WITH_EDITOR

	// Returns whether an interface with the given version is declared by the given asset's document.
	UE_API bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InVersion) const;

	// Returns all the class keys of this asset's referenced assets
	virtual const TSet<FString>& GetReferencedAssetClassKeys() const = 0;

	// Returns set of class references set call to serialize in the editor
	// Used at runtime load register referenced classes.
	virtual TArray<FMetasoundAssetBase*> GetReferencedAssets() = 0;

	// Return all dependent asset paths to load asynchronously
	virtual const TSet<FSoftObjectPath>& GetAsyncReferencedAssetClassPaths() const = 0;

	// Called when async assets have finished loading.
	virtual void OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences) = 0;

	UE_API bool AddingReferenceCausesLoop(const FMetasoundAssetBase& InMetaSound) const;

	UE_DEPRECATED(5.5, "Use overload that is provided an AssetBase")
	UE_API bool AddingReferenceCausesLoop(const FSoftObjectPath& InReferencePath) const;

	UE_API bool IsReferencedAsset(const FMetasoundAssetBase& InAssetToCheck) const;

	UE_API bool IsRegistered() const;

	// Imports data from a JSON string directly
	UE_API bool ImportFromJSON(const FString& InJSON);

	// Imports the asset from a JSON file at provided path
	UE_API bool ImportFromJSONAsset(const FString& InAbsolutePath);

	// Soft Deprecated in favor of DocumentBuilder API. Returns handle for the root metasound graph of this asset.
	UE_API Metasound::Frontend::FDocumentHandle GetDocumentHandle();
	UE_API Metasound::Frontend::FConstDocumentHandle GetDocumentHandle() const;

	// Soft Deprecated in favor of DocumentBuilder API. Returns handle for the root metasound graph of this asset.
	UE_API Metasound::Frontend::FGraphHandle GetRootGraphHandle();
	UE_API Metasound::Frontend::FConstGraphHandle GetRootGraphHandle() const;

	UE_DEPRECATED(5.5, "Direct mutation of the document is no longer supported via AssetBase.")
	UE_API void SetDocument(FMetasoundFrontendDocument InDocument, bool bMarkDirty = true);

	UE_API virtual const FMetasoundFrontendDocument& GetConstDocumentChecked() const;

	// Soft deprecated.  Document layer should not be directly mutated via asset base in anticipation
	// of moving all mutable document calls to the Frontend/Subsystem Document Builder API.
	UE_API FMetasoundFrontendDocument& GetDocumentChecked();

	UE_DEPRECATED(5.5, "Use GetConstDocument from casting Owning Asset to IMetaSoundDocumentInterface (See 'GetOwningAsset') instead.")
	UE_API const FMetasoundFrontendDocument& GetDocumentChecked() const;

	UE_API const Metasound::Frontend::FGraphRegistryKey& GetGraphRegistryKey() const;

#if WITH_EDITORONLY_DATA
	UE_API bool VersionAsset(FMetaSoundFrontendDocumentBuilder& Builder);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/*
	 * Caches transient metadata (class & vertex) found in the registry
	 * that is not necessary for serialization or core graph generation.
	 *
	 * @return - Whether class was found in the registry & data was cached successfully.
	 */
	UE_API void CacheRegistryMetadata();

	UE_API FMetasoundFrontendDocumentModifyContext& GetModifyContext();

	UE_DEPRECATED(5.5, "Use GetConstModifyContext")
	UE_API const FMetasoundFrontendDocumentModifyContext& GetModifyContext() const;

	UE_API const FMetasoundFrontendDocumentModifyContext& GetConstModifyContext() const;
#endif // WITH_EDITOR

	// Calls the outermost package and marks it dirty.
	UE_API bool MarkMetasoundDocumentDirty() const;

	struct FSendInfoAndVertexName
	{
		Metasound::FMetaSoundParameterTransmitter::FSendInfo SendInfo;
		Metasound::FVertexName VertexName;
	};

	// Returns the owning asset responsible for transactions applied to MetaSound
	virtual UObject* GetOwningAsset() = 0;

	// Returns the owning asset responsible for transactions applied to MetaSound
	virtual const UObject* GetOwningAsset() const = 0;

	UE_API FString GetOwningAssetName() const;

#if WITH_EDITORONLY_DATA
	UE_API void ClearVersionedOnLoad();
	UE_API bool GetVersionedOnLoad() const;
	UE_API void SetVersionedOnLoad();
#endif // WITH_EDITORONLY_DATA

	UE_DEPRECATED(5.5, "Use IMetaSoundDocumentInterface 'IsActivelyBuilding' instead")
	virtual bool IsBuilderActive() const { checkNoEntry(); return false; }

protected:
	UE_API void OnNotifyBeginDestroy();

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use SetReferencedAssets instead")
	virtual void SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses) { }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual void SetReferencedAssets(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetRef>&& InAssetRefs) = 0;
#endif

	// Get information for communicating asynchronously with MetaSound running instance.
	UE_DEPRECATED(5.3, "MetaSounds no longer communicate using FSendInfo.")
	UE_API TArray<FSendInfoAndVertexName> GetSendInfos(uint64 InInstanceID) const;

#if WITH_EDITORONLY_DATA
	UE_API FText GetDisplayName(FString&& InTypeName) const;
#endif // WITH_EDITORONLY_DATA

	UE_DEPRECATED(5.6, "AccessPtrs are actively being deprecated. Writable access outside of the builder API "
		" is particularly problematic as in so accessing, the builder's caches are reset which can cause major "
		"editor performance regressions.")
	virtual Metasound::Frontend::FDocumentAccessPtr GetDocumentAccessPtr() = 0;

	// Returns an access pointer to the document.
	virtual Metasound::Frontend::FConstDocumentAccessPtr GetDocumentConstAccessPtr() const = 0;

	UE_DEPRECATED(5.5, "AutoUpdate implementation now private and implemented within 'Version Dependencies'")
	UE_API bool AutoUpdate(bool bInLogWarningsOnDroppedConnection);

	UE_DEPRECATED(5.5, "Moved to private, non-cook specific implementation")
	UE_API void CookReferencedMetaSounds();

	// Ensures all referenced graph classes are registered (or re-registers depending on options).
	UE_API void RegisterAssetDependencies(const Metasound::Frontend::FMetaSoundAssetRegistrationOptions & InRegistrationOptions);

private:
#if WITH_EDITORONLY_DATA
	UE_API void UpdateAndRegisterReferencesForSerialization(FName CookPlatformName);
#endif // WITH_EDITORONLY_DATA

	// Checks if version is up-to-date. If so, returns true. If false, updates the interfaces within the given asset's document to the most recent version.
	UE_API bool TryUpdateInterfaceFromVersion(const FMetasoundFrontendVersion& Version);

	// Versions dependencies to most recent version where applicable. If asset is a preset, MetaSound is rebuilt to accommodate any referenced node class interface changes.
	// Otherwise, automatically updates any nodes and respective dependent classes to accommodate changes to interfaces therein preserving edges/connections where possible.
	UE_API bool VersionDependencies(FMetaSoundFrontendDocumentBuilder& Builder, bool bInLogWarningsOnDroppedConnection);

	// Returns new interface to be versioned to from the given version. If no interface versioning is
	// required, returns invalid interface (interface with no name and invalid version number).
	UE_API FMetasoundFrontendInterface GetInterfaceToVersion(const FMetasoundFrontendVersion& InterfaceVersion) const;

#if WITH_EDITORONLY_DATA
	bool bVersionedOnLoad = false;
#endif // WITH_EDITORONLY_DATA

	Metasound::Frontend::FGraphRegistryKey GraphRegistryKey;
};

class FMetasoundAssetProxy final : public Audio::TProxyData<FMetasoundAssetProxy>
{
public:
	IMPL_AUDIOPROXY_CLASS(FMetasoundAssetProxy);

	struct FParameters
	{
		TSet<FMetasoundFrontendVersion> Interfaces;
		TSharedPtr<const Metasound::IGraph> Graph;

	};

	UE_API explicit FMetasoundAssetProxy(const FParameters& InParams);
	
	UE_API FMetasoundAssetProxy(const FMetasoundAssetProxy& Other);

	const Metasound::IGraph* GetGraph() const
	{
		return Graph.Get();
	}

	const TSet<FMetasoundFrontendVersion>& GetInterfaces() const
	{
		return Interfaces;
	}

private:
	
	TSet<FMetasoundFrontendVersion> Interfaces;
	TSharedPtr<const Metasound::IGraph> Graph;
};
using FMetasoundAssetProxyPtr = TSharedPtr<FMetasoundAssetProxy, ESPMode::ThreadSafe>;

#undef UE_API
