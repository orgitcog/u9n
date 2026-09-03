// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/SectionModelStorageExtension.h"

#include "MVVM/ViewModels/SectionModel.h"
#include "MovieSceneSection.h"
#include "ISequencerCoreModule.h"
#include "Modules/ModuleManager.h"

namespace UE::Sequencer
{

namespace SectionModelStorage
{

	ISequencerCoreModule& GetSequencerCoreModule()
	{
		static ISequencerCoreModule* Module = &FModuleManager::Get().LoadModuleChecked<ISequencerCoreModule>("SequencerCore");
		return *Module;
	}

}// namespace SectionModelStorage

FSectionModelStorageExtension::FSectionModelStorageExtension()
{
}

void FSectionModelStorageExtension::OnReinitialize()
{
	for (auto It = SectionToModel.CreateIterator(); It; ++It)
	{
		if (It.Key().ResolveObjectPtr() == nullptr || It.Value().Pin().Get() == nullptr)
		{
			It.RemoveCurrent();
		}
	}
	SectionToModel.Compact();
}

TSharedPtr<FSectionModel> FSectionModelStorageExtension::CreateModelForSection(UMovieSceneSection* InSection, TSharedRef<ISequencerSection> SectionInterface)
{
	FObjectKey SectionAsKey(InSection);
	if (TSharedPtr<FSectionModel> Existing = SectionToModel.FindRef(SectionAsKey).Pin())
	{
		return Existing;
	}

	TSharedPtr<FSectionModel> SectionModel;

	if (TSharedPtr<FViewModel> ViewModel = SectionModelStorage::GetSequencerCoreModule().FactoryNewModel(InSection))
	{
		SectionModel = ViewModel->CastThisShared<FSectionModel>();
		ensureMsgf(SectionModel, TEXT("Section model type for Section Object was not an FSectionModel! %s (type: %s)"), *InSection->GetPathName(), *InSection->GetClass()->GetName());
	}
	
	if (!SectionModel)
	{
		SectionModel = MakeShared<FSectionModel>();
		SectionModel->InitializeObject(InSection);
	}

	SectionModel->InitializeSection(SectionInterface);

	SectionToModel.Add(SectionAsKey, SectionModel);

	return SectionModel;
}

TSharedPtr<FSectionModel> FSectionModelStorageExtension::FindModelForSection(const UMovieSceneSection* InSection) const
{
	FObjectKey SectionAsKey(InSection);
	return SectionToModel.FindRef(SectionAsKey).Pin();
}

} // namespace UE::Sequencer

