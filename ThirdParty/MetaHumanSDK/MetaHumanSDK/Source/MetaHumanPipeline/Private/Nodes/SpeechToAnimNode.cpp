// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/SpeechToAnimNode.h"
#include "Templates/UniquePtr.h"
#include "GuiToRawControlsUtils.h"

#if WITH_EDITOR

namespace UE::MetaHuman::Pipeline::Private
{
static EAudioProcessingMode AudioProcessingModeFromOutputControls(const EAudioDrivenAnimationOutputControls& InOutputControls);
}

namespace UE::MetaHuman::Pipeline
{

bool FSpeechToAnimNode::LoadModels()
{
	Speech2Face = FSpeech2Face::Create();
	return Speech2Face.Get() != nullptr;
}

bool FSpeechToAnimNode::LoadModels(const FAudioDrivenAnimationModels& InModels)
{
	Speech2Face = FSpeech2Face::Create(InModels);
	return Speech2Face.Get() != nullptr;
}

void FSpeechToAnimNode::SetMood(const EAudioDrivenAnimationMood& InMood)
{
	// Must be called after LoadModels
	check(Speech2Face);
	if (Speech2Face)
	{
		Speech2Face->SetMood(InMood);
	}
}

void FSpeechToAnimNode::SetMoodIntensity(const float InMoodIntensity)
{
	// Must be called after LoadModels
	check(Speech2Face);
	if (Speech2Face)
	{
		Speech2Face->SetMoodIntensity(InMoodIntensity);
	}
}

void FSpeechToAnimNode::SetOutputControls(const EAudioDrivenAnimationOutputControls& InOutputControls)
{
	OutputControls = InOutputControls;
}

FSpeechToAnimNode::FSpeechToAnimNode(const FString& InName) : FSpeechToAnimNode("SpeechToAnim", InName)
{
}

FSpeechToAnimNode::FSpeechToAnimNode(const FString& InTypeName, const FString& InName) : FNode(InTypeName, InName)
{
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

FSpeechToAnimNode::~FSpeechToAnimNode() = default;

void FSpeechToAnimNode::CancelModelSolve()
{
	bCancelStart = true;
}

bool FSpeechToAnimNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Animation.Reset();
	HeadAnimation.Reset();
	bCancelStart = false;

	PrepareFromOutputControls();

	if (!Speech2Face.Get())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Speech to anim models are not loaded");
		return false;
	}

	if (Audio.IsValid())
	{
		// If the user is not downmixing the channels, make sure the specificed channel index is in range (otherwise it's unused)
		if (!bDownmixChannels && AudioChannelIndex >= static_cast<uint32>(Audio->NumChannels))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::InvalidChannelIndex);
			InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Channel index %d invalid for selected audio (%d channels)"), AudioChannelIndex, Audio->NumChannels));
			return false;
		}

		FSpeech2Face::FAudioParams AudioParams(Audio, OffsetSec, bDownmixChannels, AudioChannelIndex);

		// Solve for face animation
		if (!Speech2Face->GenerateFaceAnimation(AudioParams, FrameRate, bGenerateBlinks, [this] { return bCancelStart; }, Animation, HeadAnimation))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToSolveSpeechToAnimation);
			InPipelineData->SetErrorNodeMessage("Failed to solve speech to animation");
			return false;
		}

		const int32 AudioLength = Audio->GetDuration() * FrameRate;

		while (Animation.Num() < AudioLength)
		{
			Animation.Add(TMap<FString, float>());
		}

		while (HeadAnimation.Num() < AudioLength)
		{
			HeadAnimation.Add(TMap<FString, float>());
		}

		for (int32 Frame = 0; Frame < Animation.Num(); ++Frame)
		{
			TMap<FString, float>& AnimationFrame = Animation[Frame];
			if (AnimationFrame.IsEmpty()) // Skip padded empty frames
			{
				continue;
			}

			FString ErrorMsg;
			if (!PreConversionModifyUiControls(AnimationFrame, ErrorMsg))
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToModifyUiControls);
				InPipelineData->SetErrorNodeMessage(ErrorMsg);
				return false;
			}

			// Convert solve controls to rig controls
			AnimationFrame = GuiToRawControlsUtils::ConvertGuiToRawControls(AnimationFrame);

			if (!PostConversionModifyRawControls(AnimationFrame, ErrorMsg))
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToModifyRawControls);
				InPipelineData->SetErrorNodeMessage(ErrorMsg);
				return false;
			}
		}

		// We can't currently rename the head controls with the Pre/PostConversionModify* functions (as above), as those
		// functions make some assumptions about the nature of the controls to be filtered and the error handling.
		for (FSpeech2Face::FAnimationFrame& HeadAnimationFrame : HeadAnimation)
		{
			if (!HeadAnimationFrame.IsEmpty())
			{
				ReplaceHeadGuiControlsWithRaw(HeadAnimationFrame);
			}
		}

		return true;
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::InvalidAudio);
		InPipelineData->SetErrorNodeMessage("Invalid audio");
		return false;
	}
}

bool FSpeechToAnimNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const int32 InternalFrameIndex = InPipelineData->GetFrameNumber() - ProcessingStartFrameOffset;

	FFrameAnimationData AnimationData;
	AnimationData.AudioProcessingMode = ProcessingMode;

	if (Animation.IsValidIndex(InternalFrameIndex))
	{
		AnimationData.AnimationData = MoveTemp(Animation[InternalFrameIndex]);
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::InvalidFrame);
		InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Invalid animation frame number for face animation: %d"), InternalFrameIndex));
		return false;
	}

	// Only add the head pose to the pipeline data if we're processing the full face. It is not used for mouth only or tongue only processing.
	if (ProcessingMode == EAudioProcessingMode::FullFace)
	{
		if (HeadAnimation.IsValidIndex(InternalFrameIndex))
		{
			const FSpeech2Face::FAnimationFrame HeadAnimationFrame = MoveTemp(HeadAnimation[InternalFrameIndex]);
			const bool bIsPaddingFrame = HeadAnimationFrame.IsEmpty();

			if (!bIsPaddingFrame)
			{
				AnimationData.Pose = GetHeadPoseTransformFromRawControls(HeadAnimationFrame);
			}
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::InvalidFrame);
			InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Invalid frame number for head animation: %d"), InternalFrameIndex));
			return false;
		}
	}

	InPipelineData->SetData<FFrameAnimationData>(Pins[0], MoveTemp(AnimationData));

	return true;
}

bool FSpeechToAnimNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Animation.Reset();
	HeadAnimation.Reset();

	return true;
}

bool FSpeechToAnimNode::PreConversionModifyUiControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg)
{
	// clamp tongue ui control
	if (bClampTongueInOut)
	{
		const FString TongueInOutCtrl = FString("CTRL_C_tongue_inOut.ty");
		if (InOutAnimationFrame.Contains(TongueInOutCtrl))
		{
			if (InOutAnimationFrame[TongueInOutCtrl] < 0)
			{
				InOutAnimationFrame[TongueInOutCtrl] = 0;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

bool FSpeechToAnimNode::PostConversionModifyRawControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg)
{
	if (ActiveRawControls.IsEmpty())
	{
		return true;
	}

	// Here we filter out any controls in the animation frame which are not a part of the active control set (e.g. Mouth only controls)

	TMap<FString, float> AnimationFrame = InOutAnimationFrame;
	InOutAnimationFrame.Empty();

	for (const FString& UnmaskedControl : ActiveRawControls)
	{
		const float* UnmaskedControlValue = AnimationFrame.Find(UnmaskedControl);
		if (!UnmaskedControlValue)
		{
			OutErrorMsg = FString::Printf(
				TEXT("Could not find the '%s' control in the animation data. Please upgrade your MetaHuman Identity to the latest MetaHuman rig version."),
				*UnmaskedControl
			);
			return false;
		}
		InOutAnimationFrame.Add(UnmaskedControl, *UnmaskedControlValue);
	}

	return true;
}

void FSpeechToAnimNode::PrepareFromOutputControls()
{
	using namespace UE::MetaHuman::Pipeline::Private;

	ProcessingMode = AudioProcessingModeFromOutputControls(OutputControls);

	// We use the processing mode AND the active raw control set to determine which controls to provide in the output. In other words, just because
	// the active raw controls set is empty, it doesn't mean there will be no controls in the output (it saves us defining all the controls for FullFace).
	ActiveRawControls.Empty();

	if (OutputControls == EAudioDrivenAnimationOutputControls::MouthOnly)
	{
		ActiveRawControls = GetMouthOnlyRawControls();
	}
}
} // namespace UE::MetaHuman::Pipeline

namespace UE::MetaHuman::Pipeline::Private
{
EAudioProcessingMode AudioProcessingModeFromOutputControls(const EAudioDrivenAnimationOutputControls& InOutputControls)
{
	EAudioProcessingMode ProcessingMode;

	// For the time being, TongueOnly is achieved using the tongue tracker node instead. We could remove that node and 
	// bring its filtering logic in here too, but that's a job for another day.
	switch (InOutputControls)
	{
		case EAudioDrivenAnimationOutputControls::MouthOnly:
			ProcessingMode = EAudioProcessingMode::MouthOnly;
			break;
		default:
			ProcessingMode = EAudioProcessingMode::FullFace;
			break;
	}

	return ProcessingMode;
}
} // namespace UE::MetaHuman::Pipeline::Private

#endif // WITH_EDITOR