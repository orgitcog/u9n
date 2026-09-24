// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/EditModeAnimationUtil.h"

#include "ControlRig.h"
#include "ConstraintsManager.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "Constraints/MovieSceneConstraintChannelHelper.h"
#include "EditMode/ControlRigEditModeUtil.h"
#include "ILevelSequenceEditorToolkit.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Rigs/RigHierarchyDefines.h"
#include "Sequencer/AnimationAuthoringSettings.h"

namespace UE::AnimationEditMode
{

TWeakPtr<ISequencer> GetSequencer()
{
	// if getting sequencer from level sequence need to use the current(leader), not the focused
	if (ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence())
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			static constexpr bool bFocusIfOpen = false;
			IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(LevelSequence, bFocusIfOpen);
			const ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
			return LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;
		}
	}

	// look for custom UMovieSceneSequence as a fallback.
	const FCustomMovieSceneRegistry& Registry = FCustomMovieSceneRegistry::Get();
	const TArray<TWeakPtr<ISequencer>> Sequencers = FLevelEditorSequencerIntegration::Get().GetSequencers();
	const int32 Found = Sequencers.IndexOfByPredicate([&Registry](const TWeakPtr<ISequencer>& WeakSequence)
	{
		if (const TSharedPtr<ISequencer> Sequencer = WeakSequence.IsValid() ? WeakSequence.Pin() : nullptr)
		{
			if (UMovieSceneSequence* MovieSceneSequence = Sequencer->GetRootMovieSceneSequence())
			{
				return Registry.IsSequenceSupported(MovieSceneSequence->GetClass());
			}
		}
		return false;
	});
		
	return Found == INDEX_NONE ? nullptr : Sequencers[Found];
}

FCustomMovieSceneRegistry& FCustomMovieSceneRegistry::Get()
{
	static FCustomMovieSceneRegistry Singleton;
	return Singleton;
}

bool FCustomMovieSceneRegistry::IsSequenceSupported(const UClass* InSequenceClass) const
{
	return InSequenceClass ? SupportedSequenceTypes.Contains(InSequenceClass) : false;
}
	
namespace Private
{

void EvaluateRigIfAdditive(UControlRig* InControlRig)
{
	if (InControlRig->IsAdditive())
	{
		TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
		InControlRig->Evaluate_AnyThread();
	}
}

}

using namespace ControlRigEditMode;
using namespace Private;

FControlRigKeyframer::~FControlRigKeyframer()
{
	if (OnAnimSettingsChanged.IsValid())
	{
		if (const UAnimationAuthoringSettings* Settings = GetDefault<UAnimationAuthoringSettings>())
		{
			Settings->OnSettingsChange.Remove(OnAnimSettingsChanged);
		}
		OnAnimSettingsChanged.Reset();
	}
}
	
void FControlRigKeyframer::Initialize()
{
	EnableState = EEnableState::Disabled;
	
	if (const UAnimationAuthoringSettings* Settings = GetDefault<UAnimationAuthoringSettings>())
	{
		OnSettingsChanged(Settings);

		if (!OnAnimSettingsChanged.IsValid())
		{
			OnAnimSettingsChanged = Settings->OnSettingsChange.AddRaw(this, &FControlRigKeyframer::OnSettingsChanged);
		}
	}
}
	
void FControlRigKeyframer::Enable(const bool InEnabled)
{
	Reset();

	if (InEnabled)
	{
		EnumAddFlags(EnableState, EEnableState::EnabledDirectly);
	}
	else
	{
		EnumRemoveFlags(EnableState, EEnableState::EnabledDirectly);
	}
}
	
void FControlRigKeyframer::Reset()
{
	KeyframeData.Reset();
}

void FControlRigKeyframer::Store(const uint32 InControlHash, FControlKeyframeData&& InData)
{
	if (IsEnabled() && InControlHash != 0)
	{
		FControlKeyframeData& ControlKeyframeData = KeyframeData.FindOrAdd(InControlHash);
		ControlKeyframeData = MoveTemp(InData);
	}
}
	
void FControlRigKeyframer::Apply(const FControlRigInteractionScope& InInteractionScope, const FControlRigInteractionTransformContext& InTransformContext)
{
	if (!IsEnabled())
	{
		return;
	}
	
	const TArray<FRigElementKey>& InteractingControls = InInteractionScope.GetElementsBeingInteracted();
	UControlRig* InteractingRig = InInteractionScope.GetControlRig();
	if (!InteractingRig || InteractingControls.IsEmpty())
	{
		return;
	}

	static const FRigControlModifiedContext NoKeyContext(EControlRigSetKey::Never);
	static constexpr bool bNotify = false, bSetupUndo = false;
	
	const bool bFixEulerFlips = !InteractingRig->IsAdditive() && InTransformContext.bRotation;
	UControlRig::FControlModifiedEvent& AutoKeyEvent = InteractingRig->ControlModified();

	for (const FRigElementKey& ControlKey: InteractingControls)
	{
		if (FRigControlElement* Control = InteractingRig->FindControl(ControlKey.Name))
		{
			const uint32 ControlHash = UTransformableControlHandle::ComputeHash(InteractingRig, ControlKey.Name);
			if (const FControlKeyframeData* Data = KeyframeData.Find(ControlHash))
			{
				if (Data->bConstraintSpace)
				{
					// set the control's local transform withing its constraint space transform as it's the value that sequencer has to store
					InteractingRig->SetControlLocalTransform(ControlKey.Name, Data->LocalTransform, bNotify, NoKeyContext, bSetupUndo, bFixEulerFlips);
					EvaluateRigIfAdditive(InteractingRig);
				}
				
				AutoKeyEvent.Broadcast(InteractingRig, Control, EControlRigSetKey::DoNotCare);				
			}

			// driven controls
			if (Control->CanDriveControls())
			{
				for (const FRigElementKey& DrivenKey : Control->Settings.DrivenControls)
				{
					const bool bHandleDrivenKey = DrivenKey.Type == ERigElementType::Control && !InteractingControls.Contains(DrivenKey);						
					if (FRigControlElement* DrivenControl = bHandleDrivenKey ? InteractingRig->FindControl(DrivenKey.Name) : nullptr)
					{
						AutoKeyEvent.Broadcast(InteractingRig, DrivenControl, EControlRigSetKey::DoNotCare);
					}
				}
			}
		}
	}
}

void FControlRigKeyframer::Finalize(UWorld* InWorld)
{
	auto NeedsConstraintUpdate = [this]()
	{
		if (IsEnabled())
		{
			for (const auto& [ControlHash, Data]: KeyframeData)
			{
				if (Data.bConstraintSpace)
				{
					return true;
				}
			}
		}
		
		return false;
	};

	if (InWorld && NeedsConstraintUpdate())
	{
		TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		Controller.EvaluateAllConstraints();
	}
}

bool FControlRigKeyframer::IsEnabled() const
{
	return OnAnimSettingsChanged.IsValid() ?
		EnumHasAllFlags(EnableState, EEnableState::FullyEnabled) : EnumHasAllFlags(EnableState, EEnableState::EnabledDirectly);
}
	
void FControlRigKeyframer::OnSettingsChanged(const UAnimationAuthoringSettings* InSettings)
{
	if (InSettings)
	{
		if (InSettings->bAutoKeyOnRelease)
		{
			EnumAddFlags(EnableState, EEnableState::EnabledBySettings);
		}
		else
		{
			EnumRemoveFlags(EnableState, EEnableState::EnabledBySettings);
		}
	}
}
	
}
