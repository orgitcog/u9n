// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimationMixerTrack.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "AnimMixerComponentTypes.h"
#include "Misc/AxisDisplayInfo.h"
#include "Systems/MovieSceneRootMotionSystem.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimationMixerTrack"

bool FMovieSceneByteChannelDefaultOnly::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	static const FName MovieSceneByteChannel("MovieSceneByteChannel");
	if (Tag.GetType().IsStruct(MovieSceneByteChannel))
	{
		StaticStruct()->SerializeItem(Slot, this, nullptr);
		return true;
	}

	return false;
}

UMovieSceneAnimationBaseTransformDecoration::UMovieSceneAnimationBaseTransformDecoration(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Location[0].SetDefault(0.0);
	Location[1].SetDefault(0.0);
	Location[2].SetDefault(0.0);
	Rotation[0].SetDefault(0.0);
	Rotation[1].SetDefault(0.0);
	Rotation[2].SetDefault(0.0);

	RootMotionSpace.SetEnum(StaticEnum<EMovieSceneRootMotionSpace>());
	RootMotionSpace.SetDefault((uint8)EMovieSceneRootMotionSpace::AnimationSpace);

	TransformMode.SetEnum(StaticEnum<EMovieSceneRootMotionTransformMode>());
	TransformMode.SetDefault((uint8)EMovieSceneRootMotionTransformMode::Offset);
}

EMovieSceneRootMotionTransformMode UMovieSceneAnimationBaseTransformDecoration::GetRootTransformMode() const
{
	return (EMovieSceneRootMotionTransformMode)TransformMode.GetDefault().Get((uint8)EMovieSceneRootMotionTransformMode::Offset);
}

EMovieSceneChannelProxyType UMovieSceneAnimationBaseTransformDecoration::PopulateChannelProxy(FMovieSceneChannelProxyData& OutProxyData)
{
	EMovieSceneRootMotionTransformMode CurrentTransformMode = GetRootTransformMode();

#if WITH_EDITOR
	FText Group = LOCTEXT("RootMotion", "Root Motion");
	{
		FMovieSceneChannelMetaData ChannelMetaData;
		ChannelMetaData.SetIdentifiers("Space", LOCTEXT("Space", "Space"), Group);
		ChannelMetaData.WeakOwningObject = this;
		ChannelMetaData.SortOrder = 0;
		OutProxyData.Add(RootMotionSpace, ChannelMetaData, TMovieSceneExternalValue<uint8>());
	}
	{
		FMovieSceneChannelMetaData ChannelMetaData;
		ChannelMetaData.SetIdentifiers("Root Mode", LOCTEXT("Mode", "Mode"), Group);
		ChannelMetaData.WeakOwningObject = this;
		ChannelMetaData.SortOrder = 1;
		OutProxyData.Add(TransformMode, ChannelMetaData, TMovieSceneExternalValue<uint8>());
	}

	if (CurrentTransformMode != EMovieSceneRootMotionTransformMode::Asset)
	{
		FName SliderExponent("SliderExponent");

		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseLocation.X", AxisDisplayInfo::GetAxisDisplayName(EAxisList::X), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::X);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 2;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Location[0], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseLocation.Y", AxisDisplayInfo::GetAxisDisplayName(EAxisList::Y), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::Y);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 3;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Location[1], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseLocation.Z", AxisDisplayInfo::GetAxisDisplayName(EAxisList::Z), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::Z);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 4;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Location[2], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseRotation.X", LOCTEXT("RotationX", "Roll"), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::X);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 5;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Rotation[0], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseRotation.Y", LOCTEXT("RotationY", "Pitch"), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::Y);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 6;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Rotation[1], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
		{
			FMovieSceneChannelMetaData ChannelMetaData;
			ChannelMetaData.SetIdentifiers("RootBaseRotation.Z", LOCTEXT("RotationZ", "Yaw"), Group);
			ChannelMetaData.Color = AxisDisplayInfo::GetAxisColor(EAxisList::Z);
			ChannelMetaData.WeakOwningObject = this;
			ChannelMetaData.SortOrder = 7;
			ChannelMetaData.PropertyMetaData.Add(SliderExponent, TEXT("0.2"));
			OutProxyData.Add(Rotation[2], ChannelMetaData, TMovieSceneExternalValue<double>());
		}
	}

#else

	if (CurrentTransformMode != EMovieSceneRootMotionTransformMode::Asset)
	{
		OutProxyData.Add(RootMotionSpace);
		OutProxyData.Add(TransformMode);
	}

	OutProxyData.Add(Location[0]);
	OutProxyData.Add(Location[1]);
	OutProxyData.Add(Location[2]);

	OutProxyData.Add(Rotation[0]);
	OutProxyData.Add(Rotation[1]);
	OutProxyData.Add(Rotation[2]);

#endif

	return EMovieSceneChannelProxyType::Dynamic;
}

void UMovieSceneAnimationBaseTransformDecoration::ExtendEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	FMovieSceneRootMotionSettings RootMotionSettings;
	{
		RootMotionSettings.RootMotionSpace = (EMovieSceneRootMotionSpace)RootMotionSpace.GetDefault().Get((uint8)EMovieSceneRootMotionSpace::AnimationSpace);
		RootMotionSettings.TransformMode   = GetRootTransformMode();

		UMovieSceneSkeletalAnimationSection* AnimSection = GetTypedOuter<UMovieSceneSkeletalAnimationSection>();
		if (AnimSection)
		{
			RootMotionSettings.LegacySwapRootBone = AnimSection->Params.SwapRootBone;
		}
	}

	RootMotionSettings.RootOriginLocation = RootOriginLocation;

	const bool bAddTransform = RootMotionSettings.TransformMode != EMovieSceneRootMotionTransformMode::Asset;
	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.AddConditional(BuiltInComponents->DoubleChannel[0], &Location[0], bAddTransform)
		.AddConditional(BuiltInComponents->DoubleChannel[1], &Location[1], bAddTransform)
		.AddConditional(BuiltInComponents->DoubleChannel[2], &Location[2], bAddTransform)
		.AddConditional(BuiltInComponents->DoubleChannel[3], &Rotation[0], bAddTransform)
		.AddConditional(BuiltInComponents->DoubleChannel[4], &Rotation[1], bAddTransform)
		.AddConditional(BuiltInComponents->DoubleChannel[5], &Rotation[2], bAddTransform)
		.Add(AnimMixerComponents->RootMotionSettings, RootMotionSettings)
	);
}


UMovieSceneAnimationMixerTrack::UMovieSceneAnimationMixerTrack(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(66, 56, 88, 255);
	bSupportsDefaultSections = false;
#endif
}


bool UMovieSceneAnimationMixerTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	SectionClass.Get()->ImplementsInterface(IMovieSceneAnimationSectionInterface::UClassType::StaticClass());
	return true;
}

bool UMovieSceneAnimationMixerTrack::FixRowIndices()
{
	// Group sections by their type on different rows
	TSortedMap<UClass*, TArray<UMovieSceneSection*>> SectionsByType;
	for (UMovieSceneSection* Section : AnimationSections)
	{
		SectionsByType.FindOrAdd(Section->GetClass()).Add(Section);
	}

	bool bMadeChanges = false;
	int32 CurrentRowIndex = 0;

	TArray<UClass*> SortedSectionTypes;
	SectionsByType.GenerateKeyArray(SortedSectionTypes);

	Algo::SortBy(SortedSectionTypes, [](UClass* Class){
		IMovieSceneAnimationSectionInterface* Interface = Cast<IMovieSceneAnimationSectionInterface>(Class->GetDefaultObject());
		return Interface ? Interface->GetRowSortOrder() : 0;
	});

	for (UClass* Key : SortedSectionTypes)
	{
		TArray<UMovieSceneSection*>& SectionsOfType = SectionsByType.FindChecked(Key);

		// Sort the sections
		Algo::SortBy(SectionsOfType, &UMovieSceneSection::GetRowIndex);

		int32 PreviousIndex = SectionsOfType[0]->GetRowIndex();
		for (UMovieSceneSection* Section : SectionsOfType)
		{
			const int32 ThisIndex = Section->GetRowIndex();
			if (PreviousIndex != ThisIndex)
			{
				++CurrentRowIndex;
				PreviousIndex = ThisIndex;
			}

			if (ThisIndex != CurrentRowIndex)
			{
				bMadeChanges = true;
				Section->SetRowIndex(CurrentRowIndex);
			}
		}

		++CurrentRowIndex;
	}

	return bMadeChanges;
}

void UMovieSceneAnimationMixerTrack::OnSectionAddedImpl(UMovieSceneSection* Section)
{
	if (UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section))
	{
		UMovieSceneAnimationSectionDecoration* NewDecoration = NewObject<UMovieSceneAnimationSectionDecoration>(AnimSection, UMovieSceneAnimationSectionDecoration::StaticClass(), NAME_None, RF_Transactional);
		AnimSection->AddDecoration(NewDecoration);

		UMovieSceneAnimationBaseTransformDecoration* BaseTransform = NewObject<UMovieSceneAnimationBaseTransformDecoration>(AnimSection, UMovieSceneAnimationBaseTransformDecoration::StaticClass(), NAME_None, RF_Transactional);
		AnimSection->AddDecoration(BaseTransform);
	}
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneAnimationMixerTrack::GetTrackRowDisplayName(int32 RowIndex) const
{
	// Row display name is defined by the first section in that row
	for (UMovieSceneSection* Section : AnimationSections)
	{
		if (Section->GetRowIndex() == RowIndex)
		{
			return Section->GetClass()->GetDisplayNameText();
		}
	}

	return Super::GetTrackRowDisplayName(RowIndex);
}

FText UMovieSceneAnimationMixerTrack::GetDefaultDisplayName() const
{
	return NSLOCTEXT("AnimMixer", "DefaultTrackName", "Animation Mixer");
}

bool UMovieSceneAnimationMixerTrack::CanRename() const
{
	return true;
}

#endif // WITH_EDITORONLY_DATA


#undef LOCTEXT_NAMESPACE