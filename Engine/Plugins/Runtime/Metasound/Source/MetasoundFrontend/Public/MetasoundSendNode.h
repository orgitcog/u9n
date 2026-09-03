// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundBuilderInterface.h"
#include "MetasoundFacade.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundDataReference.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundParamHelper.h"
#include "MetasoundRouter.h"
#include "MetasoundVertex.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"


namespace Metasound
{
	namespace SendVertexNames
	{
		METASOUND_PARAM(AddressInput, "Address", "Address")
	}

	namespace SendNodePrivate
	{
		template<typename TDataType>
		class TSendOperator : public TExecutableOperator<TSendOperator<TDataType>>
		{
		public:

			static const FVertexName& GetSendInputName()
			{
				static const FVertexName& SendInput = GetMetasoundDataTypeName<TDataType>();
				return SendInput;
			}

			TSendOperator(TDataReadReference<TDataType> InInputData, TDataReadReference<FSendAddress> InSendAddress, const FOperatorSettings& InOperatorSettings)
				: InputData(InInputData)
				, SendAddress(InSendAddress)
				, CachedSendAddress(*InSendAddress)
				, CachedSenderParams({InOperatorSettings, 0.0f})
				, Sender(nullptr)
			{
				Sender = CreateNewSender();
			}

			virtual ~TSendOperator() 
			{
				ResetSenderAndCleanupChannel();
			}

			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
			{
				using namespace SendVertexNames; 
				InOutVertexData.BindReadVertex<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput), SendAddress);
				InOutVertexData.BindReadVertex<TDataType>(GetSendInputName(), InputData);
			}

			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
			{
			}

			void Execute()
			{
				if (*SendAddress != CachedSendAddress)
				{
					ResetSenderAndCleanupChannel();
					CachedSendAddress = *SendAddress;
					Sender = CreateNewSender();
					check(Sender.IsValid());
				}

				Sender->Push(*InputData);
			}

			void Reset(const IOperator::FResetParams& InParams)
			{
				ResetSenderAndCleanupChannel();
				CachedSendAddress = *SendAddress;
				Sender = CreateNewSender();
				check(Sender.IsValid());
			}

			static FVertexInterface DeclareVertexInterface()
			{
				using namespace SendVertexNames; 
				static const FDataVertexMetadata AddressInputMetadata
				{
					  FText::GetEmpty() // description
					, METASOUND_GET_PARAM_DISPLAYNAME(AddressInput) // display name
				};

				return FVertexInterface(
					FInputVertexInterface(
						TInputDataVertex<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput), AddressInputMetadata),
						TInputDataVertex<TDataType>(GetSendInputName(), FDataVertexMetadata{ FText::GetEmpty() })
					),
					FOutputVertexInterface(
					)
				);
			}

			static const FNodeClassMetadata& GetNodeInfo()
			{
				auto InitNodeInfo = []() -> FNodeClassMetadata
				{
					const FVertexName& InputName = GetSendInputName();
					FNodeClassMetadata Info;

					Info.ClassName = { "Send", GetMetasoundDataTypeName<TDataType>(), FName() };
					Info.MajorVersion = 1;
					Info.MinorVersion = 0;
					Info.DisplayName = METASOUND_LOCTEXT_FORMAT("Metasound_SendNodeDisplayNameFormat", "Send {0}", GetMetasoundDataTypeDisplayText<TDataType>());
					Info.Description = METASOUND_LOCTEXT("Metasound_SendNodeDescription", "Sends data from a send node with the same name.");
					Info.Author = PluginAuthor;
					Info.PromptIfMissing = PluginNodeMissingPrompt;
					Info.DefaultInterface = DeclareVertexInterface();
					Info.CategoryHierarchy = { METASOUND_LOCTEXT("Metasound_TransmissionNodeCategory", "Transmission") };
					Info.Keywords = { };

					// Then send & receive nodes do not work as expected, particularly 
					// around multiple-consumer scenarios. Deprecate them to avoid
					// metasound assets from relying on send & receive nodes. 
					Info.bDeprecated = true;

					return Info;
				};

				static const FNodeClassMetadata Info = InitNodeInfo();

				return Info;
			}

			static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
			{
				using namespace SendVertexNames;

				if (InParams.InputData.IsVertexBound(GetSendInputName()))
				{
					return MakeUnique<SendNodePrivate::TSendOperator<TDataType>>(
						InParams.InputData.GetOrCreateDefaultDataReadReference<TDataType>(GetSendInputName(), InParams.OperatorSettings),
						InParams.InputData.GetOrCreateDefaultDataReadReference<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput), InParams.OperatorSettings),
						InParams.OperatorSettings
					);
				}
				else
				{
					// No input hook up to send, so this node can no-op
					return MakeUnique<FNoOpOperator>();
				}
			}


		private:
			FSendAddress GetSendAddressWithDataType(const FSendAddress& InAddress) const 
			{
				// The data type of a send address is inferred by the underlying
				// data type of this node. A full send address, including the data type,
				// cannot be constructed from a literal. 
				return FSendAddress{ InAddress.GetChannelName(), GetMetasoundDataTypeName<TDataType>(), InAddress.GetInstanceID() };
			}

			TSenderPtr<TDataType> CreateNewSender() const
			{
				if (ensure(SendAddress->GetDataType().IsNone() || (GetMetasoundDataTypeName<TDataType>() == SendAddress->GetDataType())))
				{
					return FDataTransmissionCenter::Get().RegisterNewSender<TDataType>(GetSendAddressWithDataType(*SendAddress), CachedSenderParams);
				}
				return TSenderPtr<TDataType>(nullptr);
			}

			void ResetSenderAndCleanupChannel()
			{
				Sender.Reset();
				FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(GetSendAddressWithDataType(CachedSendAddress));
			}

			TDataReadReference<TDataType> InputData;
			TDataReadReference<FSendAddress> SendAddress;
			FSendAddress CachedSendAddress;
			FSenderInitParams CachedSenderParams;

			TSenderPtr<TDataType> Sender;
		};
	}

	template<typename TDataType>
	class TSendNode : public TNodeFacade<SendNodePrivate::TSendOperator<TDataType>>
	{
	public:
		using FOperator = SendNodePrivate::TSendOperator<TDataType>;
		using FSuper = TNodeFacade<SendNodePrivate::TSendOperator<TDataType>>;
		using FSuper::FSuper;

		static const FVertexName& GetSendInputName()
		{
			return FOperator::GetSendInputName();
		}

		static FVertexInterface DeclareVertexInterface()
		{
			return FOperator::DeclareVertexInterface();
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			return FOperator::GetNodeInfo();
		}
	};
}
#undef LOCTEXT_NAMESPACE
