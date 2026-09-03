// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundCatMakeNode.h"

#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CatMakeNode"

namespace Metasound
{
	class FChannelAgnosticType;
	struct FBuildOperatorParams;
	class FNodeFacade;

	namespace CatMakeNodePrivate
	{
		METASOUND_PARAM(InputFromCat,			"Input", "CAT to Cast");
		METASOUND_PARAM(OutputToCat,			"Output", "CAT Result");

		const FLazyName InputBaseName{"In"};

		FName MakeInputVertexName(const int32 InIndex)
		{
			FName Name = InputBaseName;
			Name.SetNumber(InIndex);
			return Name;
		}

		FInputDataVertex MakeInputDataVertex(const int32 InIndex, const FName InName, const FString& InFriendlyName)
		{
			const FName InputName = MakeInputVertexName(InIndex);
#if WITH_EDITOR
			const FText InputDisplayName = FText::Format(LOCTEXT("In_DisplayName", "{0}"), FText::FromString(InName.ToString()));
#else
			const FText InputDisplayName = FText::GetEmpty();
#endif
			return TInputDataVertex<FAudioBuffer>{InputName, FDataVertexMetadata{FText::FromString(*InFriendlyName), InputDisplayName}};
		}
		
		
		FVertexInterface MakeClassInterface(const FName Format)
		{
			const Audio::FChannelTypeFamily* Found = Audio::GetChannelRegistry().FindConcreteChannel(Format);
			if (!Found)
			{
				return {};
			}
			const int32 NumChannels = Found->NumChannels();

			FInputVertexInterface InputInterface;
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex )
			{
				if (TOptional<Audio::FChannelTypeFamily::FChannelName> Name = Found->GetChannelName(ChannelIndex); Name.IsSet())
				{
					InputInterface.Add(MakeInputDataVertex(ChannelIndex, Name->Name, Name->FriendlyName));
				}
				else
				{
					InputInterface.Add(MakeInputDataVertex(ChannelIndex, TEXT("Input"), FString::FromInt(ChannelIndex) ));	
				}
			}
			FOutputVertexInterface OutputInterface;
            OutputInterface.Add(TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputToCat)));
			return{
				MoveTemp(InputInterface),
				MoveTemp(OutputInterface)
			};
		}
		
		class FCatMakeOperatorData final : public TOperatorData<FCatMakeOperatorData>
		{
		public:
			// The OperatorDataTypeName is used when downcasting an IOperatorData to ensure
			// that the downcast is valid.
			static const FLazyName OperatorDataTypeName;

			explicit FCatMakeOperatorData(const FName& InToTypeName)
				: ToTypeName(InToTypeName)
			{}
			const FName& GetToType() const
			{
				return ToTypeName;
			}
		private:
			FName ToTypeName;
		};

		// Linkage.
		const FLazyName FCatMakeOperatorData::OperatorDataTypeName = TEXT("FCatMakeOperatorData");
	}
	
	class FCatMakeOperator final : public TExecutableOperator<FCatMakeOperator>
	{
	public:
		FCatMakeOperator(const FBuildOperatorParams& InParams, TArray<TDataReadReference<FAudioBuffer>>&& InInputs, FChannelAgnosticTypeWriteRef&& InOutputCat)
			: InputAudioVertices(MoveTemp(InInputs))
			, OutputCat(MoveTemp(InOutputCat))
			, Settings(InParams.OperatorSettings)
		{
		}
		virtual ~FCatMakeOperator() override = default;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace CatMakeNodePrivate ;
			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// outputs
				FOutputVertexInterface OutputInterface;
				OutputInterface.Add(TOutputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputToCat)));
				
				return FVertexInterface({}, OutputInterface);
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}
			
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace CatMakeNodePrivate;

			const FCatMakeOperatorData* CatMakeData = CastOperatorData<const FCatMakeOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!CatMakeData)
			{
				return TUniquePtr<FNoOpOperator>();
			}
			const FName OutputFormat = CatMakeData->GetToType();
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			const Audio::FChannelTypeFamily* ConcreteToType = Audio::GetChannelRegistry().FindConcreteChannel(OutputFormat);

			// Set the number of inputs pins to match the output format.
			TArray<TDataReadReference<FAudioBuffer>> InputAudioVertices;
			for (int32 i = 0; i < ConcreteToType->NumChannels(); ++i)
			{
				InputAudioVertices.Emplace(InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(MakeInputVertexName(i), InParams.OperatorSettings));
			}
						
			// Make output CAT to match our configs settings. (use concrete form).
			FChannelAgnosticTypeWriteRef OutputPin = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteToType->GetName());
			
			return MakeUnique<FCatMakeOperator>(
				InParams,
				MoveTemp(InputAudioVertices),
				MoveTemp(OutputPin)
			);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatMakeNodePrivate;
			for (int32 i = 0; i < InputAudioVertices.Num(); i++)
			{
				InOutVertexData.BindReadVertex(MakeInputVertexName(i), InputAudioVertices[i]);
			}
		}
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatMakeNodePrivate;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputToCat),OutputCat);
		}

		void Reset(const FResetParams& InParams)
		{
			Execute();
		}

		void Execute()
		{
			// Copy each input channel into the CAT.
			for (int32 Index = 0; Index < InputAudioVertices.Num(); Index++)
			{
				const FAudioBuffer& Src = *InputAudioVertices[Index]; 
				TArrayView<float> Dst = OutputCat->GetChannel(Index);
				checkSlow(Src.Num() == Dst.Num());
				FMemory::Memcpy(Dst.GetData(), Src.GetData(), Dst.Num() * sizeof(float));
			}
		}
		static FNodeClassMetadata GetNodeInfo()
		{
			using namespace CatMakeNodePrivate;
			return FNodeClassMetadata
			{
				FNodeClassName{ "Experimental", "CatMakeOperator", "" },
				1, // Major version
				0, // Minor version
				LOCTEXT("CatMakeNodeName", "CAT Make Node"),	
				LOCTEXT("CatMakeNodeNameDescription", "A Node that builds CATs"),	
				TEXT("UE"), // Author
				LOCTEXT("ExampleConfigurablePromptIfMissing", "Enable the MetaSoundExperimental Plugin"), // Prompt if missing
				GetDefaultInterface(),
				{}
			};
		}
	private:
		TArray<TDataReadReference<FAudioBuffer>> InputAudioVertices;
		FChannelAgnosticTypeWriteRef OutputCat;
		FOperatorSettings Settings;
	}; // class FCatCastingOperator
		
	using FCatMakeNode = TNodeFacade<FCatMakeOperator>;

	// register node config.	
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCatMakeNode, FMetaSoundCatMakeNodeConfiguration);
} // namespace Metasound


TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCatMakeNodeConfiguration::OverrideDefaultInterface(
	const FMetasoundFrontendClass& InNodeClass) const
{
	using namespace Metasound::CatMakeNodePrivate;
	return  TInstancedStruct<FMetasoundFrontendClassInterface>::Make(FMetasoundFrontendClassInterface::GenerateClassInterface(MakeClassInterface(Format)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCatMakeNodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::CatMakeNodePrivate::FCatMakeOperatorData>(Format);
}

#undef LOCTEXT_NAMESPACE // 
