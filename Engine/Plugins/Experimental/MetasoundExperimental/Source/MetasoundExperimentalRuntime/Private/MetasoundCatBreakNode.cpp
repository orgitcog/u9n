// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundCatBreakNode.h"

#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundVertex.h"
#include "DSP/MultiMono.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CatBreakNode"

namespace Metasound
{
	class FChannelAgnosticType;
	struct FBuildOperatorParams;
	class FNodeFacade;

	namespace CatBreakNodePrivate
	{
		METASOUND_PARAM(InputFromCat,			"Input", "CAT to Cast");
		METASOUND_PARAM(OutputToCat,			"Output", "CAT Result");

		const FLazyName InputBaseName{"In"};

		FName MakeOutputVertexName(const int32 InIndex)
		{
			FName Name = InputBaseName;
			Name.SetNumber(InIndex);
			return Name;
		}

		TOutputDataVertex<FAudioBuffer> MakeOutputDataVertex(const int32 InIndex, const FName InName, const FString& InFriendlyName)
		{
			const FName Name = MakeOutputVertexName(InIndex);
#if WITH_EDITOR
			const FText DisplayName = FText::Format(LOCTEXT("In_DisplayName", "{0}"), FText::FromString(InName.ToString()));
#else
			const FText DisplayName = FText::GetEmpty();
#endif
			return TOutputDataVertex<FAudioBuffer>{Name, FDataVertexMetadata{FText::FromString(*InFriendlyName), DisplayName}};
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
            InputInterface.Add(TInputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputToCat)));
			
			FOutputVertexInterface OutputInterface;
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex )
			{
				if (TOptional<Audio::FChannelTypeFamily::FChannelName> Name = Found->GetChannelName(ChannelIndex); Name.IsSet())
				{
					OutputInterface.Add(MakeOutputDataVertex(ChannelIndex, Name->Name, Name->FriendlyName));
				}
				else
				{
					OutputInterface.Add(MakeOutputDataVertex(ChannelIndex, TEXT("Output"), FString::FromInt(ChannelIndex) ));	
				}
			}
			return{
				MoveTemp(InputInterface),
				MoveTemp(OutputInterface)
			};
		}
		
		class FCatBreakOperatorData final : public TOperatorData<FCatBreakOperatorData>
		{
		public:
			// The OperatorDataTypeName is used when downcasting an IOperatorData to ensure
			// that the downcast is valid.
			static const FLazyName OperatorDataTypeName;

			explicit FCatBreakOperatorData(const FName& InToTypeName, const Audio::EChannelTranscodeMethod InMethod, const Audio::EChannelMapMonoUpmixMethod InMixMethod)
				: ToTypeName(InToTypeName)
				, TranscodeMethod(InMethod)
			{}
			const FName& GetToType() const
			{
				return ToTypeName;
			}
			Audio::EChannelTranscodeMethod GetTranscodeMethod() const
			{
				return TranscodeMethod;
			}
			Audio::EChannelMapMonoUpmixMethod GetMixMethod() const
			{
				return MixMethod;
			}
		private:
			FName ToTypeName;
			Audio::EChannelTranscodeMethod TranscodeMethod = Audio::EChannelTranscodeMethod::ChannelDrop;
			Audio::EChannelMapMonoUpmixMethod MixMethod = Audio::EChannelMapMonoUpmixMethod::EqualPower; 
		};

		// Linkage.
		const FLazyName FCatBreakOperatorData::OperatorDataTypeName = TEXT("FCatBreakOperatorData");

		// Helper to create array of multi-mono channel pointers from a CAT.
		Audio::TStackArrayOfPointers<float> MakeMultiMonoPointersFromBufferArray(const TArray<TDataWriteReference<FAudioBuffer>>& InArrayOfCat)
		{
			Audio::TStackArrayOfPointers<float> Result;
			Result.SetNum(InArrayOfCat.Num());
			for (int32 i = 0; i < InArrayOfCat.Num(); ++i)
			{
				Result[i] = InArrayOfCat[i]->GetData();
			}
			return Result;
		}
	}
	
	class FCatBreakOperator final : public TExecutableOperator<FCatBreakOperator>
	{
	public:
		using FTranscoder = Audio::FChannelTypeFamily::FTranscoder;
		FCatBreakOperator(const FBuildOperatorParams& InParams, FChannelAgnosticTypeReadRef&& InInputCat, TArray<TDataWriteReference<FAudioBuffer>>&& InOutputVerticies,
			FTranscoder&& InTranscoder, const FName InFormat)
			: InputCat(MoveTemp(InInputCat))
			, OutputAudioVertices(MoveTemp(InOutputVerticies))
			, Settings(InParams.OperatorSettings)
			, Transcoder(InTranscoder)
			, Format(InFormat)
		{}
		virtual ~FCatBreakOperator() override = default;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace CatBreakNodePrivate ;
			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// inputs
				FInputVertexInterface InputInterface;
				InputInterface.Add(TInputDataVertex<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFromCat)));
				
				// outputs
				FOutputVertexInterface OutputInterface;
				
				return FVertexInterface(MoveTemp(InputInterface), MoveTemp(OutputInterface));
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}
			
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace CatBreakNodePrivate;

			const FCatBreakOperatorData* CatBreakData = CastOperatorData<const FCatBreakOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!CatBreakData)
			{
				return MakeUnique<FNoOpOperator>();
			}
			
			const FName OutputFormat = CatBreakData->GetToType();
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			const Audio::FChannelTypeFamily* ConcreteToType = Audio::GetChannelRegistry().FindConcreteChannel(OutputFormat);
			if (!ConcreteToType)
			{
				// Throw editor error, we don't know the format.
				return MakeUnique<FNoOpOperator>();
			}

			// Create the input pin.
			TDataReadReference<FChannelAgnosticType> InputPin = InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(
				METASOUND_GET_PARAM_NAME(OutputToCat), InParams.OperatorSettings);

			// Create output based on format.
			TArray<TDataWriteReference<FAudioBuffer>> OutputAudioVertices;
			for (int32 i = 0; i < ConcreteToType->NumChannels(); ++i)
			{
				OutputAudioVertices.Emplace(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
			}

			// Always ask for the transcoder 
			// In the trivial case where we are the same format, this will be just a memcpy.
			FTranscoder Transcoder = InputPin->GetType().GetTranscoder(
				{
					.ToType = *ConcreteToType,
					.TranscodeMethod = CatBreakData->GetTranscodeMethod(),
					.MixMethod = CatBreakData->GetMixMethod(),
			});
			
			return MakeUnique<FCatBreakOperator>(
				InParams,
				MoveTemp(InputPin),
				MoveTemp(OutputAudioVertices),
				MoveTemp(Transcoder),
				ConcreteToType->GetName()
			);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatBreakNodePrivate;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputToCat),InputCat);
		}
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CatBreakNodePrivate;
			for (int32 i = 0; i < OutputAudioVertices.Num(); i++)
			{
				InOutVertexData.BindWriteVertex(MakeOutputVertexName(i), OutputAudioVertices[i]);
			}
		}

		void Reset(const FResetParams& InParams)
		{
			Execute();
		}

		void Execute()
		{
			using namespace Audio;
			using namespace CatBreakNodePrivate;
			if (Transcoder)
			{
				TStackArrayOfPointers<const float> Src = MakeMultiMonoPointersFromView(InputCat->GetRawMultiMono(), Settings.GetNumFramesPerBlock(), InputCat->NumChannels());
				TStackArrayOfPointers<float> Dst = MakeMultiMonoPointersFromBufferArray(OutputAudioVertices);
				Transcoder(Src, Dst, Settings.GetNumFramesPerBlock() );
			}
		}
		
		static FNodeClassMetadata GetNodeInfo()
		{
			using namespace CatBreakNodePrivate;
			return FNodeClassMetadata
			{
				FNodeClassName{ "Experimental", "CatBreakOperator", "" },
				1, // Major version
				0, // Minor version
				LOCTEXT("CatBreakNodeName", "CAT Break Node"),	
				LOCTEXT("CatBreakNodeNameDescription", "A Node that Breaks CATs"),	
				TEXT("UE"), // Author
				LOCTEXT("ExampleConfigurablePromptIfMissing", "Enable the MetaSoundExperimental Plugin"), // Prompt if missing
				GetDefaultInterface(),
				{}
			};
		}
	private:
		FChannelAgnosticTypeReadRef InputCat;
		TArray<TDataWriteReference<FAudioBuffer>> OutputAudioVertices;
		FOperatorSettings Settings;
		FTranscoder Transcoder;
		FName Format;
	}; // class FCatCastingOperator
		
	using FCatBreakNode = TNodeFacade<FCatBreakOperator>;

	// register node config.	
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCatBreakNode, FMetaSoundCatBreakNodeConfiguration);
} // namespace Metasound


TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCatBreakNodeConfiguration::OverrideDefaultInterface(
	const FMetasoundFrontendClass& InNodeClass) const
{
	using namespace Metasound::CatBreakNodePrivate;
	return  TInstancedStruct<FMetasoundFrontendClassInterface>::Make(FMetasoundFrontendClassInterface::GenerateClassInterface(MakeClassInterface(Format)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCatBreakNodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::CatBreakNodePrivate::FCatBreakOperatorData>(
		Format,
		static_cast<Audio::EChannelTranscodeMethod>(TranscodeMethod),
		static_cast<Audio::EChannelMapMonoUpmixMethod>(MixMethod)
	);
}

#undef LOCTEXT_NAMESPACE // 
