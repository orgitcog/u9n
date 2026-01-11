// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendNodeClassRegistry.h"
#include "MetasoundFrontendNodeClassRegistryPrivate.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "StructUtils/InstancedStruct.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundGraphNode.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"

#ifndef UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS
#define UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS (0)
#endif

namespace Metasound::Frontend
{
	namespace ConsoleVariables
	{
		static bool bDisableAsyncGraphRegistration = false;
		FAutoConsoleVariableRef CVarMetaSoundDisableAsyncGraphRegistration(
			TEXT("au.MetaSound.DisableAsyncGraphRegistration"),
			Metasound::Frontend::ConsoleVariables::bDisableAsyncGraphRegistration,
			TEXT("Disables async registration of MetaSound graphs\n")
			TEXT("Default: false"),
			ECVF_Default);
	} // namespace ConsoleVariables

	namespace RegistryPrivate
	{
		TScriptInterface<IMetaSoundDocumentInterface> BuildRegistryDocument(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface, bool bAsync)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Frontend::BuildRegistryDocument);

			UObject* DocObject = DocumentInterface.GetObject();
			check(DocObject);
			const FMetasoundFrontendDocument& Document = DocumentInterface->GetConstDocument();

#if WITH_EDITOR
			DocumentInterface = &UMetaSoundBuilderDocument::Create(*DocumentInterface.GetInterface());
			FMetaSoundFrontendDocumentBuilder Builder(DocumentInterface);
			Builder.TransformTemplateNodes();
			return DocumentInterface;
#else
			const bool bIsBuilding = DocumentInterface->IsActivelyBuilding();
			const bool bForceCopy = bIsBuilding && bAsync;

	#if !NO_LOGGING
		// Force a copy if async registration is enabled and we need to protect against race conditions from external modifications.

		#if WITH_EDITORONLY_DATA
			// Only assets require template node processing and support document attachment
			if (DocObject->IsAsset())
			{
				const FMetaSoundFrontendDocumentBuilder& OriginalDocBuilder = IDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(DocumentInterface);
				const bool bContainsTemplateDependency = OriginalDocBuilder.ContainsDependencyOfType(EMetasoundFrontendClassType::Template);
				if (bContainsTemplateDependency)
				{
					UE_LOG(LogMetaSound, Error,
						TEXT("Template node processing disabled but provided asset class at '%s' to register contains template nodes. Runtime graph will fail to build."),
						*OriginalDocBuilder.GetDebugName());
				}

				// Destroy builder if one didn't exist before running template check to ensure
				// that builder existence doesn't inadvertently cause potential future re-registration
				// calls to perform unnecessary document copy below.
				if (!bIsBuilding)
				{
					const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
					IDocumentBuilderRegistry::GetChecked().FinishBuilding(ClassName, OriginalDocBuilder.GetHintPath());
				}
			}
		#endif // WITH_EDITORONLY_DATA
	#endif // !NO_LOGGING

			if (bForceCopy)
			{
				return &UMetaSoundBuilderDocument::Create(*DocumentInterface.GetInterface());
			}
			else
			{
				return DocumentInterface;
			}
#endif // WITH_EDITOR
		}


		// FDocumentNodeClassRegistryEntry encapsulates a node registry entry for a FGraph
		class FDocumentNodeClassRegistryEntry : public INodeClassRegistryEntry
		{
		public:
			FDocumentNodeClassRegistryEntry(
				const FMetasoundFrontendGraphClass& InGraphClass,
				const TSet<FMetasoundFrontendVersion>& InInterfaces,
				FNodeClassInfo&& InNodeClassInfo,
				TSharedPtr<const IGraph> InGraph,
				FTopLevelAssetPath InAssetPath)
			: FrontendClass(InGraphClass)
			, Interfaces(InInterfaces)
			, ClassInfo(MoveTemp(InNodeClassInfo))
			, Graph(InGraph)
			, AssetPath(InAssetPath)
			{
				FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
			}

			FDocumentNodeClassRegistryEntry(const FDocumentNodeClassRegistryEntry&) = default;

			virtual ~FDocumentNodeClassRegistryEntry() = default;

			virtual const FNodeClassInfo& GetClassInfo() const override
			{
				return ClassInfo;
			}

			virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InNodeInitData) const override
			{
				if (Graph.IsValid())
				{
					return MakeUnique<FGraphNode>(InNodeInitData, Graph.ToSharedRef());
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cannot create MetaSound node from class %s due to prior failure to build graph"), *ClassInfo.ClassName.ToString());
					return TUniquePtr<INode>();
				}
			}

			virtual TUniquePtr<INode> CreateNode(FNodeData InNodeData) const override
			{
				if (Graph.IsValid())
				{
					return MakeUnique<FGraphNode>(MoveTemp(InNodeData), Graph.ToSharedRef());
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cannot create MetaSound node from asset %s due to prior failure to build graph"), *AssetPath.ToString());
					return TUniquePtr<INode>();
				}
			}

			virtual const FMetasoundFrontendClass& GetFrontendClass() const override
			{
				return FrontendClass;
			}

			virtual const TSet<FMetasoundFrontendVersion>* GetImplementedInterfaces() const override
			{
				return &Interfaces;
			}

			virtual FVertexInterface GetDefaultVertexInterface() const override
			{
				if (ensure(Graph.IsValid()))
				{
					return Graph->GetMetadata().DefaultInterface;
				}
				else
				{
					return FVertexInterface();
				}
			}

			virtual TInstancedStruct<FMetaSoundFrontendNodeConfiguration> CreateFrontendNodeConfiguration() const override final
			{
				// Document based nodes do not support node configuration because
				// many MetaSound systems assume that a graph defined in a FMetasoundFrontendDocument
				// only supplies a default interface. The use of class interface
				// overrides in FMetasoundFrontendDocument based nodes is unsupported. 
				return TInstancedStruct<FMetaSoundFrontendNodeConfiguration>();
			}
		private:
			FMetasoundFrontendClass FrontendClass;
			TSet<FMetasoundFrontendVersion> Interfaces;
			FNodeClassInfo ClassInfo;
			TSharedPtr<const IGraph> Graph;
			FTopLevelAssetPath AssetPath;
		};
	} // namespace RegistryPrivate
	  
	TUniquePtr<INode> INodeClassRegistryEntry::CreateNode(FNodeData InNodeData) const
	{
#if !UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS
		static bool bDidLogError = false;

		if (!bDidLogError)
		{
			bDidLogError = true;

			FNodeClassInfo ClassInfo = GetClassInfo();
			UE_LOG(LogMetaSound, Warning, TEXT("Use of deprecated code path for node registration. First occurrence on node %s. Please implement INodeClassRegistryEntry::CreateNode(FNodeData) const"), *ClassInfo.ClassName.ToString());
		}
#endif
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Use old path for node creation. 
		return CreateNode(FNodeInitData{InNodeData.Name, InNodeData.ID});
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

#if !UE_METASOUND_PURE_VIRTUAL_CREATE_FRONTEND_NODE_EXTENSION
	TInstancedStruct<FMetaSoundFrontendNodeConfiguration> INodeClassRegistryEntry::CreateFrontendNodeConfiguration() const
	{
		static bool bDidWarn = false;
		if (!bDidWarn)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Please implement INodeClassRegistryEntry::CreateFrontendNodeConfiguration for the registry entry class representing node %s. This method will become pure virtual in future releases. Define UE_METASOUND_PURE_VIRTUAL_CREATE_FRONTEND_NODE_EXTENSION in order to build with this method as a pure virtual on the interface."), *GetClassInfo().ClassName.ToString());
		}
		return TInstancedStruct<FMetaSoundFrontendNodeConfiguration>();
	}
#endif

	FNodeClassRegistryTransaction::FNodeClassRegistryTransaction(ETransactionType InType, const FNodeClassInfo& InNodeClassInfo, FNodeClassRegistryTransaction::FTimeType InTimestamp)
	: Type(InType)
	, NodeClassInfo(InNodeClassInfo)
	, Timestamp(InTimestamp)
	{
	}

	FNodeClassRegistryTransaction::ETransactionType FNodeClassRegistryTransaction::GetTransactionType() const
	{
		return Type;
	}

	const FNodeClassInfo& FNodeClassRegistryTransaction::GetNodeClassInfo() const
	{
		return NodeClassInfo;
	}

	FNodeClassRegistryKey FNodeClassRegistryTransaction::GetNodeRegistryKey() const
	{
		return FNodeClassRegistryKey(NodeClassInfo);
	}

	FNodeClassRegistryTransaction::FTimeType FNodeClassRegistryTransaction::GetTimestamp() const
	{
		return Timestamp;
	}

	namespace NodeClassRegistryKey
	{
		FNodeClassRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion)
		{
			if (InType == EMetasoundFrontendClassType::Graph)
			{
				// No graphs are registered. Any registered graph should be registered as an external node.
				InType = EMetasoundFrontendClassType::External;
			}

			FMetasoundFrontendClassName ClassName;
			FMetasoundFrontendClassName::Parse(InFullClassName, ClassName);
			return FNodeClassRegistryKey(InType, ClassName, InMajorVersion, InMinorVersion);
		}

		const FNodeClassRegistryKey& GetInvalid()
		{
			return FNodeClassRegistryKey::GetInvalid();
		}

		bool IsValid(const FNodeClassRegistryKey& InKey)
		{
			return InKey.IsValid();
		}

		bool IsEqual(const FNodeClassRegistryKey& InLHS, const FNodeClassRegistryKey& InRHS)
		{
			return InLHS == InRHS;
		}

		bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
		{
			if (InLHS.GetClassName() == InRHS.GetClassName())
			{
				if (InLHS.GetType() == InRHS.GetType())
				{
					if (InLHS.GetVersion() == InRHS.GetVersion())
					{
						return true;
					}
				}
			}
			return false;
		}

		bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
		{
			if (InLHS.ClassName == InRHS.GetClassName())
			{
				if (InLHS.Type == InRHS.GetType())
				{
					if (InLHS.Version == InRHS.GetVersion())
					{
						return true;
					}
				}
			}
			return false;
		}

		FNodeClassRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata)
		{
			return FNodeClassRegistryKey(InNodeMetadata);
		}

		FNodeClassRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
		{
			checkf(InNodeMetadata.GetType() != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use CreateKey overload that is provided FMetasoundFrontendGraphClass"));
			return FNodeClassRegistryKey(InNodeMetadata);
		}

		FNodeClassRegistryKey CreateKey(const FMetasoundFrontendGraphClass& InGraphClass)
		{
			return FNodeClassRegistryKey(InGraphClass);
		}

		FNodeClassRegistryKey CreateKey(const FNodeClassInfo& InClassInfo)
		{
			return FNodeClassRegistryKey(InClassInfo);
		}
	} // namespace NodeClassRegistryKey

	void FNodeClassRegistry::BuildAndRegisterGraphFromDocument(const FMetasoundFrontendDocument& InDocument, const FProxyDataCache& InProxyDataCache, FNodeClassInfo&& InNodeClassInfo, const FTopLevelAssetPath& AssetPath)
	{
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FNodeClassRegistry::BuildAndRegisterGraphFromDocument);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Metasound::FNodeClassRegistry::BuildAndRegisterGraphFromDocument asset %s"), *AssetPath.ToString()));

		FGuid AssetClassID;
		if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
		{
			ensureAlways(AssetManager->TryGetAssetIDFromClassName(InNodeClassInfo.ClassName, AssetClassID));
		}
		else
		{
			UE_LOG(LogMetaSound, Warning, TEXT("No AssetManager registered, registering bespoke AssetClassID '%s' for asset '%s'"), *AssetClassID.ToString(), *AssetPath.ToString());
			AssetClassID = FGuid::NewGuid();
		}

		// Use the asset class id for the graph id because it should be locally unique per asset.
		TUniquePtr<FFrontendGraph> FrontendGraph = Frontend::FGraphBuilder::CreateGraph(InDocument, InProxyDataCache, AssetPath.ToString(), /*GraphId=*/AssetClassID);
		if (!FrontendGraph.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to build MetaSound graph in asset '%s'"), *AssetPath.ToString());
		}

		TSharedPtr<const FGraph> GraphToRegister(FrontendGraph.Release());
		TUniquePtr<INodeClassRegistryEntry> RegistryEntry = MakeUnique<FDocumentNodeClassRegistryEntry>(
			InDocument.RootGraph,
			InDocument.Interfaces,
			MoveTemp(InNodeClassInfo),
			GraphToRegister,
			AssetPath);

		const FNodeClassRegistryKey RegistryKey = RegisterNodeInternal(MoveTemp(RegistryEntry));

		// Key must use the asset path provided to function and *NOT* that of the
		// document's owning DocumentInterface object, as that may be a built/optimized
		// transient object with a different, transient asset path.
		const FGraphRegistryKey GraphKey { RegistryKey, AssetPath };
		RegisterGraphInternal(GraphKey, GraphToRegister);
	}

	FNodeClassRegistry* FNodeClassRegistry::LazySingleton = nullptr;

	FNodeClassRegistry& FNodeClassRegistry::Get()
	{
		if (!LazySingleton)
		{
			LazySingleton = new Metasound::Frontend::FNodeClassRegistry();
		}

		return *LazySingleton;
	}

	void FNodeClassRegistry::Shutdown()
	{
		if (nullptr != LazySingleton)
		{
			delete LazySingleton;
			LazySingleton = nullptr;
		}
	}

	FNodeClassRegistry::FNodeClassRegistry()
	: TransactionBuffer(MakeShared<FNodeClassRegistryTransactionBuffer>())
	, AsyncRegistrationPipe( UE_SOURCE_LOCATION )
	{
	}

	void FNodeClassRegistry::RegisterPendingNodes()
	{
		METASOUND_LLM_SCOPE;
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FNodeClassRegistry::RegisterPendingNodes);
		{
			FScopeLock ScopeLock(&LazyInitCommandCritSection);

			for (TUniqueFunction<void()>& Command : LazyInitCommands)
			{
				Command();
			}

			LazyInitCommands.Empty();
		}

		if (!IsRunningCommandlet())
		{
			// Prime search engine after bulk registration.
			ISearchEngine::Get().Prime();
		}
	}

	bool FNodeClassRegistry::EnqueueInitCommand(TUniqueFunction<void()>&& InFunc)
	{

		FScopeLock ScopeLock(&LazyInitCommandCritSection);
		if (LazyInitCommands.Num() >= MaxNumNodesAndDatatypesToInitialize)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Registering more that %d nodes and datatypes for metasounds! Consider increasing MetasoundFrontendRegistryContainer::MaxNumNodesAndDatatypesToInitialize."), MaxNumNodesAndDatatypesToInitialize);
		}

		LazyInitCommands.Add(MoveTemp(InFunc));
		return true;
	}

	void FNodeClassRegistry::SetObjectReferencer(TUniquePtr<IObjectReferencer> InReferencer)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		checkf(ActiveRegistrationTasks.IsEmpty(), TEXT("Object Referencer cannot be set while registry is actively being manipulated"));
		ObjectReferencer = MoveTemp(InReferencer);
	}

	TUniquePtr<Metasound::INode> FNodeClassRegistry::CreateNode(const FNodeClassRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const
	{
		TUniquePtr<INode> Node;

		auto CreateNodeLambda = [&Node, &InInitData](const INodeClassRegistryEntry& Entry) mutable
		{ 
			const FMetasoundFrontendClass& FrontendClass = Entry.GetFrontendClass();

			FNodeData NodeData
			{
				InInitData.InstanceName,
				InInitData.InstanceID,
				Entry.GetDefaultVertexInterface()
			};
			Node = Entry.CreateNode(MoveTemp(NodeData)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			// Creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. 
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FNodeClassRegistry::CreateNode(const FNodeClassRegistryKey& InKey, Metasound::FNodeData InNodeData) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InNodeData](const INodeClassRegistryEntry& Entry) mutable
		{ 
			Node = Entry.CreateNode(MoveTemp(InNodeData)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			// Creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. 
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TArray<::Metasound::Frontend::FConverterNodeInfo> FNodeClassRegistry::GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType)
	{
		FConverterNodeClassRegistryKey InKey = { FromDataType, ToDataType };
		if (!ConverterNodeClassRegistry.Contains(InKey))
		{
			return TArray<FConverterNodeInfo>();
		}
		else
		{
			return ConverterNodeClassRegistry[InKey].PotentialConverterNodes;
		}
	}

	TUniquePtr<FNodeClassRegistryTransactionStream> FNodeClassRegistry::CreateTransactionStream()
	{
		return MakeUnique<FNodeClassRegistryTransactionStream>(TransactionBuffer);
	}

	FGraphRegistryKey FNodeClassRegistry::RegisterGraph(const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface)
	{
		using namespace UE;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FNodeClassRegistry::RegisterGraph);

		check(InDocumentInterface);
		check(IsInGameThread());

		const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
		const FTopLevelAssetPath AssetPath = InDocumentInterface->GetAssetPathChecked();
		const FGraphRegistryKey RegistryKey { FNodeClassRegistryKey(Document.RootGraph), AssetPath };

		if (!RegistryKey.IsValid())
		{
			// Do not attempt to build and register a MetaSound with an invalid registry key
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attempting to register graph for asset %s"), *AssetPath.ToString());
			return RegistryKey;
		}

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FNodeClassRegistry::RegisterGraph key:%s, asset %s"), *RegistryKey.ToString(), *AssetPath.ToString()));

		// Wait for any async tasks that are in flight which correspond to the same graph prior to building, even if this is a synchronous call.
		WaitForAsyncGraphRegistration(RegistryKey);

		const bool bAsync = !ConsoleVariables::bDisableAsyncGraphRegistration;

		// Use the asset path of the provided document interface object for identification, *NOT* the
		// built version as the build process may in fact create a new object with a transient path.
		const TScriptInterface<IMetaSoundDocumentInterface> RegistryDocInterface = RegistryPrivate::BuildRegistryDocument(InDocumentInterface, bAsync);

		UObject* OwningObject = RegistryDocInterface.GetObject();
		check(OwningObject);

		// Proxies are created synchronously to avoid creating proxies in async tasks. Proxies
		// are created from UObjects which need to be protected from GC and non-GT access.
		FProxyDataCache ProxyDataCache;
		ProxyDataCache.CreateAndCacheProxies(Document);

#if !NO_LOGGING
		if (UE_LOG_ACTIVE(LogMetaSound, Verbose))
		{
			const FGuid PageID = IDocumentBuilderRegistry::GetChecked().ResolveTargetPageID(Document.RootGraph);
			const bool bContainsMultipleGraphs = Document.RootGraph.GetConstGraphPages().Num() > 1;
			if (bContainsMultipleGraphs || PageID != Metasound::Frontend::DefaultPageID)
			{
				UE_LOG(LogMetaSound, Verbose, TEXT("Registered MetaSound '%s' Graph Page with PageID '%s'."),
					*AssetPath.GetAssetName().ToString(),
					*PageID.ToString());
				if (bContainsMultipleGraphs)
				{
					UE_LOG(LogMetaSound, Verbose, TEXT("Graphs found with following PageIDs Implemented:"));
					Document.RootGraph.IterateGraphPages([](const FMetasoundFrontendGraph& Graph)
					{
						UE_LOG(LogMetaSound, Verbose, TEXT("    - %s'"), *Graph.PageID.ToString());
					});
				}
			}
		}
#endif // !NO_LOGGING

		// Store update to newly registered node in history so nodes
		// can be queried by transaction ID
		FNodeClassInfo NodeClassInfo(Document.RootGraph);
		{
			FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeRegistration, NodeClassInfo, Timestamp));
		}

		if (bAsync)
		{
			Tasks::FTask BuildAndRegisterTask = AsyncRegistrationPipe.Launch(
				UE_SOURCE_LOCATION,
				[RegistryKey, ClassInfo = MoveTemp(NodeClassInfo), AssetPath, RegistryDocInterface, ProxyDataCache = MoveTemp(ProxyDataCache)]() mutable
				{
					FNodeClassRegistry& Registry = FNodeClassRegistry::Get();
					// Unregister the graph before re-registering
					Registry.UnregisterGraphInternal(RegistryKey);
					Registry.BuildAndRegisterGraphFromDocument(RegistryDocInterface->GetConstDocument(), ProxyDataCache, MoveTemp(ClassInfo), AssetPath);
					Registry.RemoveRegistrationTask(RegistryKey, FNodeClassRegistryTransaction::ETransactionType::NodeRegistration);
					Registry.RemoveDocumentReference(RegistryDocInterface);
				}
			);

			AddDocumentReference(RegistryDocInterface);
			AddRegistrationTask(RegistryKey, FActiveRegistrationTaskInfo
			{
				FNodeClassRegistryTransaction::ETransactionType::NodeRegistration,
				BuildAndRegisterTask,
				AssetPath
			});
		}
		else
		{
			UnregisterGraphInternal(RegistryKey);

			// Build and register graph synchronously
			BuildAndRegisterGraphFromDocument(RegistryDocInterface->GetConstDocument(), ProxyDataCache, MoveTemp(NodeClassInfo), AssetPath);
		}

		return RegistryKey;
	}

	void FNodeClassRegistry::AddRegistrationTask(const FGraphRegistryKey& InKey, FActiveRegistrationTaskInfo&& TaskInfo)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		ActiveRegistrationTasks.FindOrAdd(InKey.NodeKey).Add(MoveTemp(TaskInfo));
	}

	void FNodeClassRegistry::RemoveRegistrationTask(const FGraphRegistryKey& InKey, FNodeClassRegistryTransaction::ETransactionType TransactionType)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);

		int32 NumRemoved = 0;
		if (InKey.AssetPath.IsNull())  // Null provided path instructs to remove all tasks related to the underlying Node Registry Key
		{
			NumRemoved = ActiveRegistrationTasks.Remove(InKey.NodeKey);
		}
		else if (TArray<FActiveRegistrationTaskInfo>* TaskInfos = ActiveRegistrationTasks.Find(InKey.NodeKey))
		{
			auto MatchesEntryInTask = [&InKey, &TransactionType](const FActiveRegistrationTaskInfo& Info)
			{
				const bool bIsAssetPath = Info.AssetPath == InKey.AssetPath;
				const bool bIsTransactionType = Info.TransactionType == TransactionType;
				return bIsAssetPath && bIsTransactionType;
			};

			NumRemoved = TaskInfos->RemoveAllSwap(MatchesEntryInTask, EAllowShrinking::No);
			if (TaskInfos->IsEmpty())
			{
				ActiveRegistrationTasks.Remove(InKey.NodeKey);
			}
		}


		if (NumRemoved == 0)
		{
			const bool bIsCooking = IsRunningCookCommandlet();
			if (ensureMsgf(!bIsCooking,
				TEXT("Failed to find active %s tasks for the graph '%s': Async registration is not supported while cooking"),
				*FNodeClassRegistryTransaction::LexToString(TransactionType),
				*InKey.ToString()))
			{
				UE_LOG(LogMetaSound, Warning,
					TEXT("Failed to find active %s tasks for the graph '%s'."),
					*FNodeClassRegistryTransaction::LexToString(TransactionType),
					*InKey.ToString());
			}
		}
	}

	void FNodeClassRegistry::AddDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface)
	{
		FScopeLock LockActiveReg(&ObjectReferencerCriticalSection);
		if (UObject* Object = DocumentInterface.GetObject())
		{
			if (ObjectReferencer)
			{
				ObjectReferencer->AddObject(Object);
			}
		}
	}

	void FNodeClassRegistry::RemoveDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface)
	{
		FScopeLock LockActiveReg(&ObjectReferencerCriticalSection);
		if (UObject* Object = DocumentInterface.GetObject())
		{
			if (ObjectReferencer)
			{
				ObjectReferencer->RemoveObject(Object);
			}
		}
	}

	void FNodeClassRegistry::RegisterGraphInternal(const FGraphRegistryKey& InKey, TSharedPtr<const FGraph> InGraph)
	{
		using namespace RegistryPrivate;

		FScopeLock Lock(&RegistryMapsCriticalSection);

#if !NO_LOGGING
		if (RegisteredGraphs.Contains(InKey))
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Graph is already registered with the same registry key '%s'. The existing registered graph will be replaced with the new graph."), *InKey.ToString());
		}
#endif // !NO_LOGGING

		RegisteredGraphs.Add(InKey, InGraph);
	}

	bool FNodeClassRegistry::UnregisterGraphInternal(const FGraphRegistryKey& InKey)
	{
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*InKey.ToString(TEXT("FNodeClassRegistry::UnregisterGraphInternal")));

		FScopeLock Lock(&RegistryMapsCriticalSection);
		{
			if (!RegisteredGraphs.Contains(InKey))
			{
				return false;
			}

			const int32 GraphUnregistered = RegisteredGraphs.Remove(InKey) > 0;
			const bool bNodeUnregistered = UnregisterNodeInternal(InKey.NodeKey);

#if !NO_LOGGING
			if (GraphUnregistered)
			{
				UE_LOG(LogMetaSound, VeryVerbose, TEXT("Unregistered graph with key '%s'"), *InKey.ToString());
			}
			else
			{
				// Avoid warning if in cook as we always expect a graph to not get registered/
				// unregistered while cooking (as its unnecessary for serialization).
				if (bNodeUnregistered && !IsRunningCookCommandlet())
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Graph '%s' was not found, but analogous registered node class was when unregistering."), *InKey.ToString());
				}
			}
#endif // !NO_LOGGING

			return bNodeUnregistered;
		}

		return false;
	}

	bool FNodeClassRegistry::UnregisterGraph(const FGraphRegistryKey& InRegistryKey, const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface)
	{
		using namespace UE;
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FNodeClassRegistry::UnregisterGraph);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*InRegistryKey.ToString(TEXT("FNodeClassRegistry::UnregisterGraph")));

		check(IsInGameThread());
		check(InDocumentInterface);

		// Do not attempt to unregister a MetaSound with an invalid registry key
		if (!InRegistryKey.IsValid())
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attempting to unregister graph (%s)"), *InRegistryKey.ToString());
			return false;
		}

		const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
		FNodeClassInfo NodeClassInfo(Document.RootGraph.Metadata);

		// This is a hack to avoid requiring the asset path to be passed while unregistering.
		// The asset path may be invalid by this point if the object being unregistered is being GC'ed.
		// FNodeClassInfo needs to be deprecated in favor of more precise types as a key, editor data, etc.
		// Its currently kind of a dumping ground as it stands.
		NodeClassInfo.Type = EMetasoundFrontendClassType::External;

		// Store update to unregistered node in history so nodes can be queried by transaction ID
		{
			FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeUnregistration, NodeClassInfo, Timestamp));
		}

		// Async registration is only available if:
		// 1. The IMetaSoundDocumentInterface is not actively modified by a builder
		//    (built graph must be released synchronously to avoid a race condition on
		//    reading/writing the IMetaSoundDocumentInterface on the Game Thread)
		// 2. Async registration is globally disabled via console variable.
		const bool bAsync = !(InDocumentInterface->IsActivelyBuilding() || ConsoleVariables::bDisableAsyncGraphRegistration);
		if (bAsync)
		{
			// Wait for any async tasks that are in flight which correspond to the same graph
			WaitForAsyncGraphRegistration(InRegistryKey);

			Tasks::FTask UnregisterTask = AsyncRegistrationPipe.Launch(UE_SOURCE_LOCATION, [RegistryKey = InRegistryKey]()
			{
				FNodeClassRegistry& Registry = FNodeClassRegistry::Get();
				Registry.UnregisterGraphInternal(RegistryKey);
				Registry.RemoveRegistrationTask(RegistryKey, FNodeClassRegistryTransaction::ETransactionType::NodeUnregistration);
			});

			AddRegistrationTask(InRegistryKey, FActiveRegistrationTaskInfo
			{
				FNodeClassRegistryTransaction::ETransactionType::NodeUnregistration,
				UnregisterTask,
				InRegistryKey.AssetPath
			});
		}
		else
		{
			UnregisterGraphInternal(InRegistryKey);
		}

		return true;
	}

	TSharedPtr<const Metasound::FGraph> FNodeClassRegistry::GetGraph(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncGraphRegistration(InKey);

		TSharedPtr<const FGraph> Graph;
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedPtr<const FGraph>* RegisteredGraph = RegisteredGraphs.Find(InKey))
			{
				Graph = *RegisteredGraph;
			}
		}

		if (!Graph)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find graph with registry graph key '%s'."), *InKey.ToString());
		}

		return Graph;
	}

	FNodeClassRegistryKey FNodeClassRegistry::RegisterNodeInternal(TUniquePtr<INodeClassRegistryEntry>&& InEntry)
	{
		using namespace RegistryPrivate;

		METASOUND_LLM_SCOPE;

		if (!InEntry.IsValid())
		{
			return { };
		}

		const FNodeClassRegistryKey Key = FNodeClassRegistryKey(InEntry->GetClassInfo());
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*Key.ToString(TEXT("FNodeClassRegistry::RegisterNodeInternal")))


#if !NO_LOGGING
		TArray<TSharedRef<INodeClassRegistryEntry>> Entries;
#endif // !NO_LOGGING

		{
			TSharedRef<INodeClassRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());
			FScopeLock Lock(&RegistryMapsCriticalSection);
			// check to see if an identical node was already registered, and log if necessary
			// Store registry elements in map so nodes can be queried using registry key.
			RegisteredNodes.Add(Key, Entry);

#if !NO_LOGGING
			RegisteredNodes.MultiFind(Key, Entries);
#endif // !NO_LOGGING
		}

#if !NO_LOGGING
		if (Entries.Num() > 1)
		{
			if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
			{
				const TArray<FTopLevelAssetPath> AssetPaths = AssetManager->FindAssetPaths(FMetaSoundAssetKey(Key));
				TArray<FString> ExistingPaths;
				Algo::Transform(AssetPaths, ExistingPaths, [](const FTopLevelAssetPath& AssetPath) { return AssetPath.ToString(); });
				const FString ExistingAssetPaths = FString::Join(ExistingPaths, TEXT("\n"));
				UE_LOG(LogMetaSound, Error,
					TEXT("Multiple node classes with key '%s' registered. Assets currently registered with class name:\n%s"),
						* Key.ToString(),
						*ExistingAssetPaths
				);
			}
		}
#endif // !NO_LOGGING

		return Key;
	}

	FNodeClassRegistryKey FNodeClassRegistry::RegisterNode(TUniquePtr<INodeClassRegistryEntry>&& InEntry)
	{
		const FNodeClassInfo ClassInfo = InEntry->GetClassInfo();
		const FNodeClassRegistryKey Key = RegisterNodeInternal(MoveTemp(InEntry));

		if (Key.IsValid())
		{
			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID
			const FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeRegistration, ClassInfo, Timestamp));
		}

		return Key;
	}

	FNodeClassRegistryKey FNodeClassRegistry::RegisterNodeTemplate(TUniquePtr<INodeTemplateRegistryEntry>&& InEntry)
	{
		METASOUND_LLM_SCOPE;

		FNodeClassRegistryKey Key;

		if (InEntry.IsValid())
		{
			TSharedRef<INodeTemplateRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

			FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

			Key = FNodeClassRegistryKey(Entry->GetClassInfo());

			{
				FScopeLock Lock(&RegistryMapsCriticalSection);
				// check to see if an identical node was already registered, and log
				ensureAlwaysMsgf(
					!RegisteredNodeTemplates.Contains(Key),
					TEXT("Node template with registry key '%s' already registered. The previously registered node will be overwritten."),
					*Key.ToString());

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodeTemplates.Add(Key, Entry);
			}

			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID

			TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));
		}

		return Key;
	}

	bool FNodeClassRegistry::UnregisterNodeInternal(const FNodeClassRegistryKey& InKey, FNodeClassInfo* OutClassInfo)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FNodeClassRegistry::UnregisterNodeInternal key %s"), *InKey.ToString()))

			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeClassRegistryEntry, ESPMode::ThreadSafe>* EntryPtr = RegisteredNodes.Find(InKey))
			{
				const TSharedRef<INodeClassRegistryEntry>& Entry = *EntryPtr;
				if (OutClassInfo)
				{
					*OutClassInfo = Entry->GetClassInfo();
				}
				const uint32 NumRemoved = RegisteredNodes.RemoveSingle(InKey, Entry);
				if (ensure(NumRemoved == 1))
				{
					return true;
				}
			}
		}

		if (OutClassInfo)
		{
			*OutClassInfo = { };
		}
		return false;
	}

	bool FNodeClassRegistry::UnregisterNode(const FNodeClassRegistryKey& InKey)
	{
		FNodeClassInfo ClassInfo;
		if (UnregisterNodeInternal(InKey, &ClassInfo))
		{
			const FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeUnregistration, ClassInfo, Timestamp));

			return true;
		}

		return false;
	}

	bool FNodeClassRegistry::UnregisterNodeTemplate(const FNodeClassRegistryKey& InKey)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
		{
			if (const INodeTemplateRegistryEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				FNodeClassRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				TransactionBuffer->AddTransaction(FNodeClassRegistryTransaction(FNodeClassRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

				{
					FScopeLock Lock(&RegistryMapsCriticalSection);
					RegisteredNodeTemplates.Remove(InKey);
				}
				return true;
			}
		}

		return false;
	}

	bool FNodeClassRegistry::RegisterConversionNode(const FConverterNodeClassRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo)
	{
		if (!ConverterNodeClassRegistry.Contains(InNodeKey))
		{
			ConverterNodeClassRegistry.Add(InNodeKey);
		}

		FConverterNodeClassRegistryValue& ConverterNodeList = ConverterNodeClassRegistry[InNodeKey];

		if (ensureAlways(!ConverterNodeList.PotentialConverterNodes.Contains(InNodeInfo)))
		{
			ConverterNodeList.PotentialConverterNodes.Add(InNodeInfo);
			return true;
		}
		else
		{
			// If we hit this, someone attempted to add the same converter node to our list multiple times.
			return false;
		}
	}

	bool FNodeClassRegistry::IsNodeRegistered(const FNodeClassRegistryKey& InKey) const
	{
		auto IsNodeRegisteredInternal = [this, &InKey]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			return RegisteredNodes.Contains(InKey) || RegisteredNodeTemplates.Contains(InKey);
		};

		if (IsNodeRegisteredInternal())
		{
			return true;
		}
		else
		{
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return IsNodeRegisteredInternal();
		}
	}

	bool FNodeClassRegistry::IsGraphRegistered(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncGraphRegistration(InKey);

		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			return RegisteredGraphs.Contains(InKey);
		}
	}

	bool FNodeClassRegistry::FindDefaultVertexInterface(const FNodeClassRegistryKey& InKey, FVertexInterface& OutVertexInterface) const 
	{
		auto GetDefaultVertexInterface = [&OutVertexInterface](const INodeClassRegistryEntry& Entry)
		{
			OutVertexInterface = Entry.GetDefaultVertexInterface();
		};

		if (AccessNodeEntryThreadSafe(InKey, GetDefaultVertexInterface))
		{
			return true;
		}

		return false;
	}

	bool FNodeClassRegistry::FindFrontendClassFromRegistered(const FNodeClassRegistryKey& InKey, FMetasoundFrontendClass& OutClass) const
	{
		auto SetFrontendClass = [&OutClass](const INodeClassRegistryEntry& Entry)
		{
			OutClass = Entry.GetFrontendClass();
		};

		if (AccessNodeEntryThreadSafe(InKey, SetFrontendClass))
		{
			return true;
		}

		if (const INodeTemplateRegistryEntry* Entry = FindNodeTemplateEntry(InKey))
		{
			OutClass = Entry->GetFrontendClass();
			return true;
		}

		return false;
	}

	TInstancedStruct<FMetaSoundFrontendNodeConfiguration> FNodeClassRegistry::CreateFrontendNodeConfiguration(const FNodeClassRegistryKey& InKey) const
	{
		TInstancedStruct<FMetaSoundFrontendNodeConfiguration> NodeConfiguration;
		auto SetNodeConfiguration = [&NodeConfiguration](const INodeClassRegistryEntry& Entry)
		{
			NodeConfiguration = Entry.CreateFrontendNodeConfiguration();
		};

		AccessNodeEntryThreadSafe(InKey, SetNodeConfiguration);

		// Currently node configuration on template nodes is not supported. To enable that, the node template registry will need 
		// to provide a creation mechanisms for making related FMetaSoundFrontendNodeConfigurations

		return NodeConfiguration;
	}

	bool FNodeClassRegistry::FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeClassRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfaceVersions) const 
	{
		bool bDidCopy = false;

		auto CopyImplementedInterfaces = [&OutInterfaceVersions, &bDidCopy](const INodeClassRegistryEntry& Entry)
		{
			if (const TSet<FMetasoundFrontendVersion>* Interfaces = Entry.GetImplementedInterfaces())
			{
				OutInterfaceVersions = *Interfaces;
				bDidCopy = true;
			}
		};

		AccessNodeEntryThreadSafe(InKey, CopyImplementedInterfaces);

		return bDidCopy;
	}

	bool FNodeClassRegistry::FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeClassRegistryKey& OutKey)
	{
		FMetasoundFrontendClass Class;
		switch (InAccessType)
		{
			case EMetasoundFrontendVertexAccessType::Reference:
			{
				if (IDataTypeRegistry::Get().GetFrontendInputClass(InDataTypeName, Class))
				{
					OutKey = FNodeClassRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InDataTypeName, Class))
				{
					OutKey = FNodeClassRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			default:
			case EMetasoundFrontendVertexAccessType::Unset:
			{
				return false;
			}
		}

		return false;
	}

	bool FNodeClassRegistry::FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeClassRegistryKey& OutKey)
	{
		FMetasoundFrontendClass Class;
		if (IDataTypeRegistry::Get().GetFrontendLiteralClass(InDataTypeName, Class))
		{
			OutKey = FNodeClassRegistryKey(Class.Metadata);
			return true;
		}
		return false;
	}

	bool FNodeClassRegistry::FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeClassRegistryKey& OutKey)
	{
		FMetasoundFrontendClass Class;
		switch (InAccessType)
		{
			case EMetasoundFrontendVertexAccessType::Reference:
			{
				if (IDataTypeRegistry::Get().GetFrontendOutputClass(InDataTypeName, Class))
				{
					OutKey = FNodeClassRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InDataTypeName, Class))
				{
					OutKey = FNodeClassRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;
		}

		return false;
	}

	void FNodeClassRegistry::IterateRegistry(FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType) const
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Calling FMetasoundRegistryContainer::IterateRegistry(...) is not threadsafe. Please use Metasound::Frontend::ISearchEngine instead"));
		auto WrappedFunc = [&](const TPair<FNodeClassRegistryKey, TSharedPtr<INodeClassRegistryEntry, ESPMode::ThreadSafe>>& Pair)
		{
			InIterFunc(Pair.Value->GetFrontendClass());
		};

		if (EMetasoundFrontendClassType::Invalid == InClassType)
		{
			// Iterate through all classes. 
			Algo::ForEach(RegisteredNodes, WrappedFunc);
		}
		else
		{
			// Only call function on classes of certain type.
			auto IsMatchingClassType = [&](const TPair<FNodeClassRegistryKey, TSharedPtr<INodeClassRegistryEntry, ESPMode::ThreadSafe>>& Pair)
			{
				return Pair.Value->GetClassInfo().Type == InClassType;
			};
			Algo::ForEachIf(RegisteredNodes, IsMatchingClassType, WrappedFunc);
		}
	}

	bool FNodeClassRegistry::AccessNodeEntryThreadSafe(const FNodeClassRegistryKey& InKey, TFunctionRef<void(const INodeClassRegistryEntry&)> InFunc) const
	{
		auto TryAccessNodeEntry = [this, &InKey, &InFunc]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeClassRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodes.Find(InKey))
			{
				InFunc(*(*Entry));
				return true;
			}
			return false;
		};

		if (TryAccessNodeEntry())
		{
			return true;
		}
		else
		{
			// Wait for any async registration tasks related to the registry key. 
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return TryAccessNodeEntry();
		}
	}

	const INodeTemplateRegistryEntry* FNodeClassRegistry::FindNodeTemplateEntry(const FNodeClassRegistryKey& InKey) const
	{
		FScopeLock Lock(&RegistryMapsCriticalSection);
		if (const TSharedRef<INodeTemplateRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodeTemplates.Find(InKey))
		{
			return &Entry->Get();
		}

		return nullptr;
	}

	void FNodeClassRegistry::WaitForAsyncGraphRegistration(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncRegistrationInternal(InKey.NodeKey, &InKey.AssetPath);
	}

	void FNodeClassRegistry::WaitForAsyncRegistrationInternal(const FNodeClassRegistryKey& InRegistryKey, const FTopLevelAssetPath* InAssetPath) const
	{
		using namespace UE::Tasks;

		if (AsyncRegistrationPipe.IsInContext())
		{
			// It is not safe to wait for an async registration task from within the async registration pipe because it will result in a deadlock. 
			UE_LOG(LogMetaSound, Verbose, TEXT("Async registration pipe is already in context for registering key %s. Task will not be waited for."), *InRegistryKey.ToString());
			return;
		}

		TArray<FTask> TasksToWaitFor;
		{
			FScopeLock Lock(&ActiveRegistrationTasksCriticalSection);
			if (const TArray<FActiveRegistrationTaskInfo>* FoundTasks = ActiveRegistrationTasks.Find(InRegistryKey))
			{
				// Filter by asset path or ignore if not provided
				Algo::TransformIf(*FoundTasks, TasksToWaitFor,
					[&InAssetPath](const FActiveRegistrationTaskInfo& TaskInfo) { return !InAssetPath || InAssetPath->IsNull() || TaskInfo.AssetPath == *InAssetPath; },
					[](const FActiveRegistrationTaskInfo& TaskInfo) { return TaskInfo.Task; });
			}
		}

		for (const FTask& Task : TasksToWaitFor)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FNodeClassRegistry::WaitForRegistrationTaskToComplete);
			if (Task.IsValid())
			{
				Task.Wait();
			}
		}
	}

	INodeClassRegistry* INodeClassRegistry::Get()
	{
		return &Metasound::Frontend::FNodeClassRegistry::Get();
	}

	void INodeClassRegistry::ShutdownMetasoundFrontend()
	{
		Metasound::Frontend::FNodeClassRegistry::Shutdown();
	}

	bool INodeClassRegistry::GetFrontendClassFromRegistered(const FNodeClassRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
	{
		INodeClassRegistry* Registry = INodeClassRegistry::Get();

		if (ensure(nullptr != Registry))
		{
			return Registry->FindFrontendClassFromRegistered(InKey, OutClass);
		}

		return false;
	}


	bool INodeClassRegistry::GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeClassRegistryKey& OutKey)
	{
		if (INodeClassRegistry* Registry = INodeClassRegistry::Get())
		{
			return Registry->FindInputNodeRegistryKeyForDataType(InDataTypeName, InAccessType, OutKey);
		}
		return false;
	}

	bool INodeClassRegistry::GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeClassRegistryKey& OutKey)
	{
		if (INodeClassRegistry* Registry = INodeClassRegistry::Get())
		{
			return Registry->FindVariableNodeRegistryKeyForDataType(InDataTypeName, OutKey);
		}
		return false;
	}

	bool INodeClassRegistry::GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InVertexAccessType, FNodeClassRegistryKey& OutKey)
	{
		if (INodeClassRegistry* Registry = INodeClassRegistry::Get())
		{
			return Registry->FindOutputNodeRegistryKeyForDataType(InDataTypeName, InVertexAccessType, OutKey);
		}
		return false;
	}
} // namespace Metasound::Frontend
