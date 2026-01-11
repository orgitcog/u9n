// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundDataReference.h"
#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"

#include "HarmonixMetasound/DataTypes/MidiEventInfo.h"

#define UE_API HARMONIXMETASOUND_API

namespace HarmonixMetasound::Analysis
{
	class FMidiStreamVertexAnalyzer final : public Metasound::Frontend::FVertexAnalyzerBase
	{
	public:
		static UE_API const FName& GetAnalyzerName();
		static UE_API const FName& GetDataType();

		struct FOutputs
		{
			static UE_API const Metasound::Frontend::FAnalyzerOutput& GetValue();
		};

		class FFactory final : public Metasound::Frontend::TVertexAnalyzerFactory<FMidiStreamVertexAnalyzer>
		{
		public:
			UE_API virtual const TArray<Metasound::Frontend::FAnalyzerOutput>& GetAnalyzerOutputs() const override;
		};

		UE_API explicit FMidiStreamVertexAnalyzer(const Metasound::Frontend::FCreateAnalyzerParams& InParams);
		virtual ~FMidiStreamVertexAnalyzer() override = default;

		UE_API virtual void Execute() override;

	private:
		FMidiEventInfoWriteRef LastMidiEvent;
	};
}

#undef UE_API
