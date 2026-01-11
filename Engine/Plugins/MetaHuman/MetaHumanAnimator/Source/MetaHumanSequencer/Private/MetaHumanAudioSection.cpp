// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioSection.h"

FMetaHumanAudioSection::FMetaHumanAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: FAudioSection{ InSection, InSequencer }
{
}

bool FMetaHumanAudioSection::SectionIsResizable() const
{
	return false;
}

bool FMetaHumanAudioSection::IsReadOnly() const
{
	return true;
}
