// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/AudioFFT.h"

#define UE_API AUDIOSYNESTHESIACORE_API

namespace Audio { class IFFTAlgorithm; }

namespace Audio
{
    /** Equal loudness curves from: ANSI Standards S1.4-1983 and S1.42-2001 */
	enum class ELoudnessCurveType : uint8
	{
		A,
		B,
		C,
		D,
		K,
		None
	};

	/** Generate equal loudness weight for magnitude spectrum using curve A */
    AUDIOSYNESTHESIACORE_API float GetEqualLoudnessAWeigthForFrequency(const float Freq);

	/** Generate equal loudness weight for magnitude spectrum using curve B */
    AUDIOSYNESTHESIACORE_API float GetEqualLoudnessBWeigthForFrequency(const float Freq);

	/** Generate equal loudness weight for magnitude spectrum using curve C */
    AUDIOSYNESTHESIACORE_API float GetEqualLoudnessCWeigthForFrequency(const float Freq);

	/** Generate equal loudness weight for magnitude spectrum using curve D */
    AUDIOSYNESTHESIACORE_API float GetEqualLoudnessDWeigthForFrequency(const float Freq);

	/** Generate equal loudness weight for magnitude spectrum using curve None */
    AUDIOSYNESTHESIACORE_API float GetEqualLoudnessNoneWeigthForFrequency(const float Freq);

	/** Settings for FLoudnessAnalyzer */
    struct FLoudnessAnalyzerSettings
    {
		/** Size of FFT describe in number of samples */
        int32 FFTSize;

		/** Type of window to be applied to input audio */
        EWindowType WindowType;

		/** Type of equal loudness curve to weight power spectrum */
        ELoudnessCurveType LoudnessCurveType;

		/** Minimum frequency for analyzing audio */
        float MinAnalysisFrequency;

		/** Maximum frequency for analyzing audio */
        float MaxAnalysisFrequency;

        /** Default settings */
		FLoudnessAnalyzerSettings()
			: FFTSize(8192)
			, WindowType(EWindowType::Blackman)
			, LoudnessCurveType(ELoudnessCurveType::D)
			, MinAnalysisFrequency(20.0f)
			, MaxAnalysisFrequency(10000.0f)
		{}
    };

	/** FLoudnessAnalyzer
	 *
	 * FLoudnessAnalyzer will calculate perceptual energy or loudness one window at a time. 
	 * This analyzer only handles mono audio. See FMultichannelLoudnessAnalyzer for multichannel
	 * support.
	 */
    class FLoudnessAnalyzer
    {
        public:
			/** Construct analyzer */
            UE_API FLoudnessAnalyzer(float InSampleRate, const FLoudnessAnalyzerSettings& InSettings);

			/**
			 * Calculate the perceptual energy for the input samples.  The number of 
			 * input samples must be equal to the FFTSize supplied in FLoudnessAnalyzerSettings.
			 */
            UE_API float CalculatePerceptualEnergy(TArrayView<const float> InSampleView);

			/**
			 * Calculate the loudness for the input samples.  The number of  input samples must 
			 * be equal to the FFTSize supplied in FLoudnessAnalyzerSettings.
			 */
            UE_API float CalculateLoudness(TArrayView<const float> InSampleView);

			/**
			 * Convert from perceptual energy to loudness [Loudness = 10 * log_10(Energy)]
			 */
			static UE_API float ConvertPerceptualEnergyToLoudness(float InPerceptualEnergy);

			/**
			 * Return const reference to settings used inside this analyzer.
			 */
            UE_API const FLoudnessAnalyzerSettings& GetSettings() const;
        private:

            FLoudnessAnalyzerSettings Settings;

			/** Scale factor used to normalize between differing window sizes and fft sizes */
            float EnergyScale = 1.f;

            int32 MinFreqIndex = 0;
            int32 MaxFreqIndex = 0;

			/** Number of FFTFrequency bins worth looking at. */
			int32 FFTFreqSize = 0;
			int32 ActualFFTSize = 0;

            FWindow Window;
			TUniquePtr<IFFTAlgorithm> FFT;

            FAlignedFloatBuffer WindowedSamples;
            FAlignedFloatBuffer ComplexSpectrum;
            FAlignedFloatBuffer RealSpectrum;

            FAlignedFloatBuffer CurveWeights;
    };


	/** FMultichannelLoudnessAnalyzer
	 *
	 * FMultichannelLoudnessAnalyzer will calculate perceptual energy or loudness one window 
	 * at a time. This module can handle one or more channels at a time. It expects 
	 * interleaved audio samples.
	 */
    class FMultichannelLoudnessAnalyzer 
    {
        public:
			/** Construct analyzer. */
            UE_API FMultichannelLoudnessAnalyzer(float InSampleRate, const FLoudnessAnalyzerSettings& InSettings);

			/** 
			 * Calculate perceptual energy. 
			 *
			 * InSampleView		  InSampleView is expected to contain interleaved audio with 
			 *                    (InSettings.FFTSize * InNumChannels) number of samples.
			 *
			 * InNumChannels      Number of input channels.
			 *
			 * OutChannelEnergies Will be populated with energies on a per channel basis.
			 *
			 * The overall perceptual energy is returned.
			 */
            UE_API float CalculatePerceptualEnergy(TArrayView<const float> InSampleView, const int32 InNumChannels, TArray<float>& OutChannelEnergies); 

			/** 
			 * Calculate loudness
			 *
			 * InSampleView		  InSampleView is expected to contain interleaved audio with 
			 *                    (InSettings.FFTSize * InNumChannels) number of samples.
			 *
			 * InNumChannels      Number of input channels.
			 *
			 * OutChannelLoudness Will be populated with energies on a per channel basis.
			 *
			 * The overall loudness is returned.
			 */
            UE_API float CalculateLoudness(TArrayView<const float> InSampleView, const int32 InNumChannels, TArray<float>& OutChannelLoudness);

			/**
			 * Return const reference to settings used inside this analyzer.
			 */
            UE_API const FLoudnessAnalyzerSettings& GetSettings() const;

        private:

            FAlignedFloatBuffer MonoBuffer;
            TArray<float> ChannelWeights;
            FLoudnessAnalyzer Analyzer;
    };
}

#undef UE_API
