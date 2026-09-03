// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorations/MovieSceneDecorationContainer.h"

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"


UObject* FMovieSceneDecorationContainer::FindDecoration(const TSubclassOf<UObject>& InClass) const
{
	if (UClass* Class = InClass.Get())
	{
		// Do an exact match - we intentionally do not support derived decorations
		for (UObject* Decoration : Decorations)
		{
			if (Decoration->IsA(Class))
			{
				return Decoration;
			}
		}
	}
	return nullptr;
}

void FMovieSceneDecorationContainer::AddDecoration(UObject* InDecoration, UObject* Outer, TFunctionRef<void(UObject*)> Event)
{
	check(InDecoration && Outer);

	if (!ensureMsgf(FindDecoration(InDecoration->GetClass()) == nullptr, TEXT("Attempting to add a decoration when one of the same type already exists. This request will be ignored.")))
	{
		return;
	}

	if (!ensureMsgf(InDecoration->IsIn(Outer->GetOutermost()), TEXT("Attempting to add a decoration from a different pacakge - this is not allowed.")))
	{
		return;
	}

	Decorations.Add(InDecoration);
	Event(InDecoration);
}

UObject* FMovieSceneDecorationContainer::GetOrCreateDecoration(const TSubclassOf<UObject>& InClass, UObject* Outer, TFunctionRef<void(UObject*)> Event)
{
	UObject* Found = FindDecoration(InClass);
	if (!Found)
	{
		Found = NewObject<UObject>(Outer, InClass, NAME_None, RF_Transactional);
		Decorations.Add(Found);
		Event(Found);
	}
	return Found;
}

void FMovieSceneDecorationContainer::RemoveDecoration(const TSubclassOf<UObject>& InClass, TFunctionRef<void(UObject*)> Event)
{
	if (UClass* Class = InClass.Get())
	{
		for (int32 Index = Decorations.Num()-1; Index >= 0; --Index)
		{
			if (Decorations[Index]->IsA(Class))
			{
				Event(Decorations[Index]);
				Decorations.RemoveAtSwap(Index);
			}
		}
	}
}

TArrayView<const TObjectPtr<UObject>> FMovieSceneDecorationContainer::GetDecorations() const
{
	return Decorations;
}

void UMovieSceneDecorationContainerObject::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Remove null decorations for safety
	Decorations.RemoveNulls();
}

void UMovieSceneDecorationContainerObject::AddDecoration(UObject* InDecoration)
{
	Decorations.AddDecoration(InDecoration, this, [this](UObject* Decoration){
		this->OnDecorationAdded(Decoration);
	});
}

UObject* UMovieSceneDecorationContainerObject::GetOrCreateDecoration(const TSubclassOf<UObject>& InClass)
{
	return Decorations.GetOrCreateDecoration(InClass, this, [this](UObject* Decoration){
		this->OnDecorationAdded(Decoration);
	});
}

void UMovieSceneDecorationContainerObject::RemoveDecoration(const TSubclassOf<UObject>& InClass)
{
	Decorations.RemoveDecoration(InClass, [this](UObject* Decoration){
		this->OnDecorationRemoved(Decoration);
	});
}