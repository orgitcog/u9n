// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Math/Transform.h"
#include "Misc/EnumClassFlags.h"
#include "MovieSceneSequence.h"

class FControlRigInteractionScope;
class ISequencer;
class UAnimationAuthoringSettings;
class UControlRig;

struct FRigControlElement;
struct FControlRigInteractionTransformContext;

namespace UE::AnimationEditMode
{

// Returns the current sequencer.
TWeakPtr<ISequencer> GetSequencer();
	
/**
 * FCustomMovieSceneRegistry contains custom UMovieSceneSequence that support constraints (among other things)
 * This allows other types than ULevelSequence to manage constraints.
 * Registration can be done at module startup (for example) as follows:
 * FCustomMovieSceneRegistry& Registry = FCustomMovieSceneRegistry::Get();
 * Registry.RegisterSequence<UMyCustomSequence>();
*/

class FCustomMovieSceneRegistry
{
public:
	~FCustomMovieSceneRegistry() = default;

	static CONTROLRIGEDITOR_API FCustomMovieSceneRegistry& Get();

	// Registers a particular UMovieSceneSequence subclass to support constraints.
	template<typename SequenceType>
	void RegisterSequence()
	{
		static_assert(TIsDerivedFrom<SequenceType, UMovieSceneSequence>::Value,
			"The template class SequenceType must be a subclass of UMovieSceneSequence.");
	
		UClass* SequenceClass = SequenceType::StaticClass();
		if (ensureAlways(!SequenceClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			SupportedSequenceTypes.Add(SequenceClass);
		}
	}

	// Whether a particular UMovieSceneSequence subclass is supported.
	bool CONTROLRIGEDITOR_API IsSequenceSupported(const UClass* InSequenceClass) const;

private:
	FCustomMovieSceneRegistry() = default;

	// List of supported UMovieSceneSequence classes.
	TSet<UClass*> SupportedSequenceTypes;
};

/**
 * FControlKeyframeData provides a way of passing the various keyframe parameters a control needs to set / know about.
 * Extend it if necessary to pass in more data to the keyframer.
 */

struct FControlKeyframeData
{
	// Local transform data of the control to be keyed.
	FTransform LocalTransform = FTransform::Identity;

	// Whether this local transform represents a constraint space local transform.
	bool bConstraintSpace = false;
};

/**
 * FControlRigKeyframer enables the storage and application of controls' keyframe data.
 * It stores keyframe data per control (represented as hash values) that can be applied on demand (on mouse release for example)
 * This struct works in conjunction with FControlRigInteractionScope, and captures data from controls currently interacting (whether via the viewport
 * or any other widget that would need to deffer keyframing. 
 */
	
struct FControlRigKeyframer
{
	~FControlRigKeyframer();
	
	// Initializes this keyframer and bounds it to the animation authoring settings.
	void Initialize();

	// Resets the data storage and enable/disable the keyframer.
	void Enable(const bool InEnabled);

	// Returns true if enabled. 
	bool IsEnabled() const;
	
	// Stores the keyframe data for a specific control.
	void Store(const uint32 InControlHash, FControlKeyframeData&& InData);

	// Does the actual work off adding keyframes to the controls currently interacting.
	void Apply(const FControlRigInteractionScope& InInteractionScope, const FControlRigInteractionTransformContext& InTransformContext);

	// Updates whatever needs to once the keyframes have been added (updating constraints is one of them).
	void Finalize(UWorld* InWorld);

	// Empties the storage.
	void Reset();

private:
	
	// Storage representing keyframe data per control.
	TMap<uint32, FControlKeyframeData> KeyframeData;

	// Current state of the keyframer
	enum class EEnableState : uint8
	{
		Disabled = 0x000,			// Keyframing disabled
		EnabledDirectly = 0x001,	// Keyframing enabled via code
		EnabledBySettings = 0x002,	// Keyframing enabled by settings
		
		FullyEnabled = EnabledDirectly | EnabledBySettings
	};
	FRIEND_ENUM_CLASS_FLAGS(EEnableState)
	
	// Whether the keyframer is enabled or not.
	EEnableState EnableState = EEnableState::Disabled;

	// Handle to UAnimationAuthoringSettings::OnSettingsChange delegate.
	FDelegateHandle OnAnimSettingsChanged;
	
	// Used to track changes to animation authoring settings.
	void OnSettingsChanged(const UAnimationAuthoringSettings* InSettings);
};
	
}
