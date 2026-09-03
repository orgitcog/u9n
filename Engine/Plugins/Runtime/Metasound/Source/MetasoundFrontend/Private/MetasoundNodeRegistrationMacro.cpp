// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundNodeRegistrationMacro.h"

#include "MetasoundFrontendDocument.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundFrontend.h"

namespace Metasound::Frontend
{
	namespace NodeRegistrationPrivate
	{
		FNodeRegistryEntryBase::FNodeRegistryEntryBase(const FNodeClassMetadata& InMetadata)
		: ClassMetadata(MakeShared<FNodeClassMetadata>(InMetadata))
		, ClassInfo(FMetasoundFrontendClassMetadata::GenerateClassMetadata(InMetadata, EMetasoundFrontendClassType::External))
		, FrontendClass(GenerateClass(InMetadata))
		{
		}

		const FNodeClassInfo& FNodeRegistryEntryBase::GetClassInfo() const
		{
			return ClassInfo;
		}

		const FMetasoundFrontendClass& FNodeRegistryEntryBase::GetFrontendClass() const
		{
			return FrontendClass;
		}

		TUniquePtr<INode> FNodeRegistryEntryBase::CreateNode(const FNodeInitData& InInitData) const 
		{
			return CreateNode(FNodeData{InInitData.InstanceName, InInitData.InstanceID, ClassMetadata->DefaultInterface});
		}

		const TSet<FMetasoundFrontendVersion>* FNodeRegistryEntryBase::GetImplementedInterfaces() const
		{
			return nullptr;
		}

		FVertexInterface FNodeRegistryEntryBase::GetDefaultVertexInterface() const
		{
			return ClassMetadata->DefaultInterface;
		}

		TInstancedStruct<FMetaSoundFrontendNodeConfiguration> FNodeRegistryEntryBase::CreateFrontendNodeConfiguration() const
		{
			return TInstancedStruct<FMetaSoundFrontendNodeConfiguration>();
		}

		TSharedRef<const FNodeClassMetadata> FNodeRegistryEntryBase::GetNodeClassMetadata() const
		{
			return ClassMetadata;
		}

		// This adapter class forwards the correct FBuilderOperatorParams
		// to the node's operator creation method. Many operator creation
		// methods downcast the supplied INode in `FBuilderOperatorParams`
		// and so it is required that it point to the correct runtime instance
		// when calling CreateOperator(...)
		class FDeprecatedNodeAPIFactory : public IOperatorFactory 
		{
		public:
			FDeprecatedNodeAPIFactory(TUniquePtr<INode> InNode)
			: Node(MoveTemp(InNode))
			{
				check(Node);
			}

			virtual ~FDeprecatedNodeAPIFactory() = default;

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
			{
				FBuildOperatorParams ForwardParams 
				{
					*Node,  // Point to correct INode instance
					InParams.OperatorSettings,
					InParams.InputData,
					InParams.Environment,
					InParams.Builder,
					InParams.GraphRenderCost
				};
				return Node->GetDefaultOperatorFactory()->CreateOperator(ForwardParams, OutResults);
			}
		private:
			TUniquePtr<INode> Node;
		};

		FDeprecatedNodeAPIAdapterBase::FDeprecatedNodeAPIAdapterBase(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata, TUniquePtr<INode> InNode)
		: FBasicNode(MoveTemp(InNodeData), MoveTemp(InClassMetadata))
		, Factory(MakeShared<FDeprecatedNodeAPIFactory>(MoveTemp(InNode)))
		{
		}

		FOperatorFactorySharedRef FDeprecatedNodeAPIAdapterBase::GetDefaultOperatorFactory() const 
		{
			return Factory;
		}
	}
}
