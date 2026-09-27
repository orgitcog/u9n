// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundCatMixerNode.h"

#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CatMixerNode"

namespace Metasound
{
	namespace CatMixerPrivate
	{
		METASOUND_PARAM(InputGain,			"Gain", "Channel Agnostic Output");
		METASOUND_PARAM(InputCat,			"Cat In", "Channel Agnostic Input");
		METASOUND_PARAM(InputNumChannels,	"NumChannels", "Num Output Channels");
		METASOUND_PARAM(OutputCat,			"Cat Out", "Channel Agnostic Output");
		const FString DefaultCatFormat = TEXT("Mono_1_0");
	}
	
	class FCatMixerOperator final : public TExecutableOperator<FCatMixerOperator>
	{
	public:
		FCatMixerOperator(const FBuildOperatorParams& InParams, FChannelAgnosticTypeReadRef&& InInputCat, FFloatReadRef&& InGain, FInt32ReadRef&& InNumOutputChannels)
			: Gain(MoveTemp(InGain))
			, Inputs (MoveTemp(InInputCat))
			, NumOutputChannels(MoveTemp(InNumOutputChannels))
			, Outputs(FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, CatMixerPrivate::DefaultCatFormat)) // initalize to a default format a head of binding. (maybe default init is better?)
			, Settings(InParams.OperatorSettings)
		{
			Reset(InParams);
		}
		virtual ~FCatMixerOperator() override = default;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace CatMixerPrivate;
			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// inputs
				FInputVertexInterface InputInterface;
				InputInterface.Add(TInputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCat),CatMixerPrivate::DefaultCatFormat));
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGain)));
				InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNumChannels)));
				
				// outputs
				FOutputVertexInterface OutputInterface;
				OutputInterface.Add(TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCat)));
				
				return FVertexInterface(InputInterface, OutputInterface);
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata
			(
				FName(TEXT("Cat Mixer Node")),
				METASOUND_LOCTEXT("Metasound_CatMixerNodeDisplayName", "Cat Mixer Node"),
				METASOUND_LOCTEXT("Metasound_CatMixerNodeDescription", "Cat Mixer Node"),
				GetDefaultInterface()	
			);
			return Metadata;
		}
		
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace CatMixerPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;
			return MakeUnique<FCatMixerOperator>(
				InParams,
				InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputCat), InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGain), InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNumChannels), InParams.OperatorSettings)
			);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatMixerPrivate;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputGain) , Gain);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCat) , Inputs);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNumChannels) , NumOutputChannels);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatMixerPrivate;

			if (const int32 NumChannels = *NumOutputChannels; NumChannels > 0) 
			{
				// Create cat type capable of holding N channels.
				// FIXME: Use FName chooser.
				if (const Audio::FChannelTypeFamily* Type = Audio::GetChannelRegistry().FindChannelType(NumChannels))
				{
					Outputs = FChannelAgnosticTypeWriteRef::CreateNew(Settings, Type->GetName().ToString());
				}
			
				// Bind it
				InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputCat), Outputs);
			}
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			PrevGains = *Gain;
			Execute();
		}

		void Execute()
		{
		}
		
	private:

		FFloatReadRef Gain;
		FChannelAgnosticTypeReadRef Inputs;
		FInt32ReadRef NumOutputChannels;
		int32 NumFrames = 0;
		int32 NumInputChannels = 0;
		FChannelAgnosticTypeWriteRef Outputs;
		FOperatorSettings Settings;
		float PrevGains = 0.f;

		static FNodeClassMetadata CreateNodeClassMetadata(const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "CatAudioMixer", InOperatorName, FName() },
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Mix },
				{ METASOUND_LOCTEXT("Metasound_AudioMixerKeyword", "Mixer") },
				FNodeDisplayStyle{}
			};

			return Metadata;
		}
	}; // class FCatMixerOperator

	using FCatMixerNode = TNodeFacade<FCatMixerOperator>;

} // namespace Metasound

#undef LOCTEXT_NAMESPACE // "MetasoundStandardNodes_CatMixerNode"
