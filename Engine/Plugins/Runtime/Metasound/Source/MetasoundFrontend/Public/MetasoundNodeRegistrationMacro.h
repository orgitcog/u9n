// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundBasicNode.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendNodeClassRegistry.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"

#include "Traits/MetasoundNodeConstructorTraits.h"
#include "Traits/MetasoundNodeStaticMemberTraits.h"

#define UE_API METASOUNDFRONTEND_API

// In UE 5.6, registered node are expected to support the constructor signature Constructor(FNodeData, TSharedRef<const FNodeClassMetadata>)
// Because there are many existing nodes, it may take time to update them. For convenience, the deprecations related to this change are
// configurable via a preprocessor macro so that the deprecation warnings do not drown out other compiler errors and warnings.
#ifndef UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS
#define UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS (0)
#endif

namespace Metasound::Frontend
{
	namespace NodeRegistrationPrivate
	{
		// Utilize base class to reduce template bloat in TNodeRegistryEntry
		class FNodeRegistryEntryBase : public INodeClassRegistryEntry
		{
		public:
			UE_API FNodeRegistryEntryBase(const Metasound::FNodeClassMetadata& InMetadata);

			virtual ~FNodeRegistryEntryBase() = default;

			UE_API virtual const FNodeClassInfo& GetClassInfo() const override;

			UE_API virtual const FMetasoundFrontendClass& GetFrontendClass() const override;

			// Unhide CreateNode overloads which exist on INodeClassRegistryEntry
			using INodeClassRegistryEntry::CreateNode;

			UE_DEPRECATED(5.6, "Node classes should be constructed with FNodeData")
			UE_API virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InInitData) const override;

			UE_API virtual const TSet<FMetasoundFrontendVersion>* GetImplementedInterfaces() const override;

			UE_API virtual FVertexInterface GetDefaultVertexInterface() const override;

			UE_API virtual TInstancedStruct<FMetaSoundFrontendNodeConfiguration> CreateFrontendNodeConfiguration() const override;

		protected:

			UE_API TSharedRef<const FNodeClassMetadata> GetNodeClassMetadata() const;
		private:

			TSharedRef<FNodeClassMetadata> ClassMetadata;
			FNodeClassInfo ClassInfo;
			FMetasoundFrontendClass FrontendClass;
		};

		template<typename TNodeType>
		class TNodeRegistryEntryBase : public FNodeRegistryEntryBase
		{
		public:
			// Expose FNodeRegistryEntryBase constructors. 
			using FNodeRegistryEntryBase::FNodeRegistryEntryBase;

			// Unhide CreateNode overloads which exist on INodeClassRegistryEntry
			using FNodeRegistryEntryBase::CreateNode;

			virtual TUniquePtr<INode> CreateNode(FNodeData InNodeData) const override
			{
				if constexpr(std::is_constructible_v<TNodeType, ::Metasound::FNodeData, TSharedRef<const FNodeClassMetadata>>)
				{
					// Prefer construction of nodes using (FNodeData, TShareRef<const FNodeClassMetadata>)
					return MakeUnique<TNodeType>(MoveTemp(InNodeData), GetNodeClassMetadata());
				}
				else if constexpr(std::is_constructible_v<TNodeType, ::Metasound::FNodeData>)
				{
					// Some node classes have FNodeClassMetadata declared as static members on
					// the node class and do not need a separate TSharedRef<const FNodeClassMetadata>. 
					return MakeUnique<TNodeType>(MoveTemp(InNodeData));
				}
				else
				{
					checkNoEntry();
					return nullptr;
				}
			}
		};

		// A node registry entry which also provides a node extension. 
		template<typename NodeType, typename ConfigurationType> 
		class TNodeRegistryEntry : public TNodeRegistryEntryBase<NodeType>
		{
			static_assert(std::is_base_of_v<FMetaSoundFrontendNodeConfiguration, ConfigurationType>, "Configurations must inherit from FMetaSoundFrontendNodeConfiguration");
		public:
			// Inherit constructor
			using TNodeRegistryEntryBase<NodeType>::TNodeRegistryEntryBase;

			virtual TInstancedStruct<FMetaSoundFrontendNodeConfiguration> CreateFrontendNodeConfiguration() const override
			{
				return TInstancedStruct<FMetaSoundFrontendNodeConfiguration>::Make<ConfigurationType>();
			}

		};

		// A partial template specialization for scenario where no node extension is provided
		template<typename NodeType>
		class TNodeRegistryEntry<NodeType, void> : public TNodeRegistryEntryBase<NodeType>
		{
		public:
			// Inherit constructor
			using TNodeRegistryEntryBase<NodeType>::TNodeRegistryEntryBase;
		};

#if UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS

		template<typename T>
		void TriggerDeprecatedNodeConstructorWarning() {}

		template<typename T>
		void TriggerMissingCreateNodeClassMetadataWarning() {}
#else		
		// These only trigger deprecation if the macro UE_METASOUND_DISABLE_5_6_NODE_REGISTRATION_DEPRECATION_WARNINGS is false
		template<typename T>
		UE_DEPRECATED(5.6, "Update the node's constructor to be of the form FMyNode(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata)")
		void TriggerDeprecatedNodeConstructorWarning() {}

		template<typename T>
		UE_DEPRECATED(5.6, "Update the node class to include 'static FNodeClassMetadata CreateNodeClassMetadata()`)")
		void TriggerMissingCreateNodeClassMetadataWarning() {}
#endif

		// Forward declare
		class FDeprecatedNodeAPIFactory;

		// FDeprecatedNodeAPIAdapterBase is used as a back compatible shim
		// for nodes which have not been updated to support the new APIs for node
		// registration.
		class FDeprecatedNodeAPIAdapterBase : public FBasicNode
		{
		protected:
			UE_API FDeprecatedNodeAPIAdapterBase(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata, TUniquePtr<INode> InNode);

		public:

			virtual ~FDeprecatedNodeAPIAdapterBase() = default;
			UE_API virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override;

		private:

			TSharedRef<FDeprecatedNodeAPIFactory> Factory;
		};

		// TDeprecatedNodeAPIAdapter is used as a back compatible shim
		// for nodes which have not been updated to support the new APIs for node
		// registration.
		template<typename TNodeType>
		class TDeprecatedNodeAPIAdapter : public FDeprecatedNodeAPIAdapterBase
		{
		public:
			TDeprecatedNodeAPIAdapter(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata)
			: FDeprecatedNodeAPIAdapterBase(InNodeData, MoveTemp(InClassMetadata), MakeUnique<TNodeType>(FNodeInitData{InNodeData.Name, InNodeData.ID}))
			{
				if constexpr(TIsOnlyDeprecatedNodeConstructorProvided<TNodeType>::Value)
				{
					TriggerDeprecatedNodeConstructorWarning<TNodeType>();
				}
			}
		};
	} // namespace NodeRegistrationPrivate


	template <typename TNodeType, typename ConfigurationType=void>
	bool RegisterNode(const FNodeClassMetadata& InMetadata)
	{
		using namespace NodeRegistrationPrivate;
		if constexpr (TIsOnlyDeprecatedNodeConstructorProvided<TNodeType>::Value)
		{
			return RegisterNode<TDeprecatedNodeAPIAdapter<TNodeType>, ConfigurationType>(InMetadata);
		}
		else
		{
			Frontend::FNodeRegistryKey Key = INodeClassRegistry::Get()->RegisterNode(MakeUnique<TNodeRegistryEntry<TNodeType, ConfigurationType>>(InMetadata));
			const bool bSuccessfullyRegisteredNode = Key.IsValid();
			ensureAlwaysMsgf(bSuccessfullyRegisteredNode, TEXT("Registering node class failed. Please check the logs."));

			return bSuccessfullyRegisteredNode;
		}
	} 

	template <typename TNodeType, typename ConfigurationType=void>
	bool RegisterNode()
	{
		 static_assert(::Metasound::TIsNodeConstructorSupported<TNodeType>::Value, "In order to be registered as a MetaSound node, the node needs to implement the following public constructor: Constructor(Metasound::FNodeData InNodeData) or Construct(Metasound::FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InMetadata)"); 

		if constexpr(TIsCreateNodeClassMetadataDeclared<TNodeType>::Value)
		{
			return RegisterNode<TNodeType, ConfigurationType>(TNodeType::CreateNodeClassMetadata());
		}
		else
		{
			using namespace NodeRegistrationPrivate;
			TriggerMissingCreateNodeClassMetadataWarning<TNodeType>();
			// Register a node using a prototype node.
			FNodeInitData InitData;
			TUniquePtr<Metasound::INode> Node = MakeUnique<TNodeType>(InitData);
			return RegisterNode<TNodeType, ConfigurationType>(Node->GetMetadata());
		}
	}

	template<typename TNodeType>
	bool UnregisterNode()
	{
		FNodeClassRegistryKey RegistryKey;
		if constexpr(TIsCreateNodeClassMetadataDeclared<TNodeType>::Value)
		{
			RegistryKey = FNodeClassRegistryKey(TNodeType::CreateNodeClassMetadata());
		}
		else
		{
			using namespace NodeRegistrationPrivate;
			TriggerMissingCreateNodeClassMetadataWarning<TNodeType>();
			// Register a node using a prototype node.
			FNodeInitData InitData;
			TUniquePtr<Metasound::INode> Node = MakeUnique<TNodeType>(InitData);
			RegistryKey = FNodeClassRegistryKey(Node->GetMetadata());
		}

		return INodeClassRegistry::Get()->UnregisterNode(RegistryKey);
	}

	namespace NodeRegistrationPrivate
	{
		template<typename NodeType, typename ConfigurationType=void>
		bool RegisterNodeUsingMacro()
		{
			// if we reenter this code (because METASOUND_REGISTER_NODE was called twice with the same node type),
			// we catch it here.
			static bool bAlreadyRegisteredThisDataType = false;
			if (bAlreadyRegisteredThisDataType)
			{
				UE_LOG(LogMetaSound, Display, TEXT("Tried to call METASOUND_REGISTER_NODE twice with the same class. ignoring the second call. Likely because METASOUND_REGISTER_NODE is in a header that's used in multiple modules. Consider moving it to a private header or cpp file."));
				return false;
			}

			bAlreadyRegisteredThisDataType = true;

			return RegisterNode<NodeType, ConfigurationType>();
		}
	} // namespace NodeRegistrationPrivate
}

namespace Metasound
{
	// Utility class to ensure that a node class can use the constructor the frontend uses.
	template <typename NodeClass>
	struct UE_DEPRECATED(5.6, "Use std::is_constructible<T, FNodeInitData> instead") ConstructorTakesNodeInitData 
	{

		// Use SFINAE trick to see if we have a valid constructor:
		template<typename T>
		static uint16 TestForConstructor(decltype(T(::Metasound::FNodeInitData()))*);

		template<typename T>
		static uint8 TestForConstructor(...);

		static const bool Value = sizeof(TestForConstructor<NodeClass>(nullptr)) == sizeof(uint16);
	};

	template<typename T, typename ConfigurationType=void>
	UE_DEPRECATED(5.6, "Use Frontend::RegisterNode()")
	bool RegisterNodeWithFrontend()
	{
		return Frontend::RegisterNode<T, ConfigurationType>();
	}

	template<typename T, typename ConfigurationType=void>
	UE_DEPRECATED(5.6, "Use Frontend::RegisterNode(const Metasound::FNodeClassMetadata&)")
	bool RegisterNodeWithFrontend(const Metasound::FNodeClassMetadata& InMetadata)
	{
		return Frontend::RegisterNode<T, ConfigurationType>(InMetadata);
	}
}

#define METASOUND_REGISTER_NODE_AND_CONFIGURATION(NodeClass, ConfigurationClass) \
	 static_assert(std::is_base_of<::Metasound::INodeBase, NodeClass>::value, "To be registered as a  Metasound Node," #NodeClass "need to be a derived class from Metasound::INodeBase, Metasound::INode, or Metasound::FNode."); \
	 static_assert(::Metasound::TIsNodeConstructorSupported<NodeClass>::Value, "In order to be registered as a Metasound Node, " #NodeClass " needs to implement the following public constructor: " #NodeClass "(Metasound::FNodeData InNodeData);"); \
	 static bool bSuccessfullyRegistered##NodeClass  = ::Metasound::Frontend::INodeClassRegistry::Get()->EnqueueInitCommand([](){ ::Metasound::Frontend::NodeRegistrationPrivate::RegisterNodeUsingMacro<NodeClass, ConfigurationClass>(); }); // This static bool is useful for debugging, but also is the only way the compiler will let us call this function outside of an expression.

#define METASOUND_REGISTER_NODE(NodeClass) METASOUND_REGISTER_NODE_AND_CONFIGURATION(NodeClass, void)
/*
Macros to help define various FText node fields.
*/
#if WITH_EDITOR
#define METASOUND_LOCTEXT(KEY, NAME_TEXT) LOCTEXT(KEY, NAME_TEXT)
#define METASOUND_LOCTEXT_FORMAT(KEY, NAME_TEXT, ...) FText::Format(LOCTEXT(KEY, NAME_TEXT), __VA_ARGS__)
#else 
#define METASOUND_LOCTEXT(KEY, NAME_TEXT) FText::GetEmpty()
#define METASOUND_LOCTEXT_FORMAT(KEY, NAME_TEXT, ...) FText::GetEmpty()
#endif // WITH_EDITOR

#undef UE_API
