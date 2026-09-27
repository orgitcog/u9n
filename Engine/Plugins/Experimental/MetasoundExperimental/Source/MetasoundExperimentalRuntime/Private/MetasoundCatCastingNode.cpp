// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundCatCastingNode.h"

#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CatCastingNode"

namespace Metasound
{
	class FChannelAgnosticType;
	struct FBuildOperatorParams;
	class FNodeFacade;

	namespace CatCastingPrivate
	{
		METASOUND_PARAM(InputFromCat,			"Input", "CAT to Cast");
		METASOUND_PARAM(OutputToCat,			"Output", "CAT Result");
		
		class FCatCastingOperatorData final : public TOperatorData<FCatCastingOperatorData>
		{
		public:
			// The OperatorDataTypeName is used when downcasting an IOperatorData to ensure
			// that the downcast is valid.
			static const FLazyName OperatorDataTypeName;

			explicit FCatCastingOperatorData(const FName& InToTypeName, Audio::EChannelTranscodeMethod InTranscodeMethod, Audio::EChannelMapMonoUpmixMethod InMixMethod)
				: ToTypeName(InToTypeName)
				, TranscodeMethod(InTranscodeMethod)
				, MixMethod(InMixMethod)
			{}
			const FName& GetToType() const
			{
				return ToTypeName;
			}
			Audio::EChannelMapMonoUpmixMethod GetMixMethod() const { return MixMethod; }
			Audio::EChannelTranscodeMethod GetTranscodeMethod() const { return TranscodeMethod; }
		private:
			FName ToTypeName;
			Audio::EChannelTranscodeMethod TranscodeMethod;
			Audio::EChannelMapMonoUpmixMethod MixMethod;
		};

		// Linkage.
		const FLazyName FCatCastingOperatorData::OperatorDataTypeName = TEXT("FCatCastingOperatorData");
	}
	
	class FCatCastingOperator final : public TExecutableOperator<FCatCastingOperator>
	{
	public:
		FCatCastingOperator(const FBuildOperatorParams& InParams, FChannelAgnosticTypeReadRef&& InInputCat, const CatCastingPrivate::FCatCastingOperatorData& InData, const FName InConcreteName)
			: InputFrom(MoveTemp(InInputCat))
			, ToFormatName(InConcreteName)
			, OutputCastResult(FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, InConcreteName))
			, Settings(InParams.OperatorSettings)
			, TranscodeMethod(InData.GetTranscodeMethod())
			, MixMethod(InData.GetMixMethod())
		{}
		virtual ~FCatCastingOperator() override = default;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace CatCastingPrivate;
			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// inputs
				FInputVertexInterface InputInterface;
				InputInterface.Add(TInputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFromCat)));
				
				// outputs
				FOutputVertexInterface OutputInterface;
				OutputInterface.Add(TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputToCat)));
				
				return FVertexInterface(InputInterface, OutputInterface);
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}
			
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace CatCastingPrivate;

			const FCatCastingOperatorData* CatTestingConfigData = CastOperatorData<const FCatCastingOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!CatTestingConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}
			const FName CastToName = CatTestingConfigData->GetToType();
			
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			const Audio::FChannelTypeFamily* ConcreteToType = Audio::GetChannelRegistry().FindConcreteChannel(CastToName);

			TDataReadReference<FChannelAgnosticType> InputCat = InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(
				METASOUND_GET_PARAM_NAME(InputFromCat), InParams.OperatorSettings);

			// Make sure the cast is to something sane, otherwise use the inputs type... 
			const FName CastToNameSane = ConcreteToType ? ConcreteToType->GetName() : InputCat->GetTypeName();
			
			return MakeUnique<FCatCastingOperator>(
				InParams,
				MoveTemp(InputCat),
				*CatTestingConfigData,
				CastToNameSane
			);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatCastingPrivate;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFromCat) , InputFrom);
		}
		
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatCastingPrivate;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputToCat),OutputCastResult);

			// Create transcoder 
			Transcoder = InputFrom->GetType().GetTranscoder(
				{
					.ToType = OutputCastResult->GetType(),
					.TranscodeMethod = TranscodeMethod,
					.MixMethod = MixMethod,
				}
			);
		}

		void Reset(const FResetParams& InParams)
		{
			Execute();
		}

		void Execute()
		{
			if (Transcoder)
			{
				using namespace CatCastingPrivate;
				using namespace Audio;
				TStackArrayOfPointers<const float> Src = MakeMultiMonoPointersFromView(InputFrom->GetRawMultiMono(), Settings.GetNumFramesPerBlock(), InputFrom->NumChannels());
				TStackArrayOfPointers<float> Dst = MakeMultiMonoPointersFromView(OutputCastResult->GetRawMultiMono(), Settings.GetNumFramesPerBlock(), OutputCastResult->NumChannels());
				Transcoder(Src, Dst,  Settings.GetNumFramesPerBlock());
			}
		}
		static FNodeClassMetadata GetNodeInfo()
		{
			using namespace CatCastingPrivate;
			return FNodeClassMetadata
			{
				FNodeClassName{ "Experimental", "CatCastingOperator", "" },
				1, // Major version
				0, // Minor version
				LOCTEXT("CatCastingNodeName", "CAT Casting Node"),	
				LOCTEXT("CatCastingNodeNameDescription", "A Node that allows Casting to CATs"),	
				TEXT("UE"), // Author
				LOCTEXT("ExampleConfigurablePromptIfMissing", "Enable the MetaSoundExperimental Plugin"), // Prompt if missing
				GetDefaultInterface(),
				{}
			};
		}
	private:
		using FTranscoder = Audio::FChannelTypeFamily::FTranscoder;
		FChannelAgnosticTypeReadRef InputFrom;
		FName ToFormatName;
		FChannelAgnosticTypeWriteRef OutputCastResult;
		FOperatorSettings Settings;
		FTranscoder Transcoder;
		Audio::EChannelTranscodeMethod TranscodeMethod;
		Audio::EChannelMapMonoUpmixMethod MixMethod;
	}; // class FCatCastingOperator
		
	using FCatCastingNode = TNodeFacade<FCatCastingOperator>;

	// register node config.	
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCatCastingNode, FMetaSoundCatCastingNodeConfiguration);
} // namespace Metasound


TArray<FPropertyTextFName> UMetasoundCatCastingOptionsHelper::GetCastingOptions()
{
	const TArray<const Audio::FChannelTypeFamily*> AllFormats = Audio::GetChannelRegistry().GetAllChannelFormats();
	TArray<FPropertyTextFName> FormatsOptions;
	Algo::Transform(AllFormats, FormatsOptions, [](const Audio::FChannelTypeFamily* i) -> FPropertyTextFName
		{ return {.ValueString = i->GetName(), .DisplayName = FText::FromString(i->GetFriendlyName()) }; });
	return FormatsOptions;
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCatCastingNodeConfiguration::OverrideDefaultInterface(
	const FMetasoundFrontendClass& InNodeClass) const
{
	return  TInstancedStruct<FMetasoundFrontendClassInterface>::Make(FMetasoundFrontendClassInterface::GenerateClassInterface(Metasound::FCatCastingOperator::GetDefaultInterface()));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCatCastingNodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::CatCastingPrivate::FCatCastingOperatorData>(
		ToType,
		static_cast<Audio::EChannelTranscodeMethod>(TranscodeMethod),
		static_cast<Audio::EChannelMapMonoUpmixMethod>(MixMethod)
		);
}

#undef LOCTEXT_NAMESPACE // 
