// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorTakeRecorderDropHandler.h"
#include "ITakeRecorderSourcesModule.h"
#include "LevelEditor.h"
#include "Misc/CoreMisc.h"
#include "Templates/SharedPointer.h"

class ISequencer;
template<typename OptionalType>struct TOptional;

class ULevelSequence;
class UTakeRecorderSources;

namespace UE::TakeRecorderSources
{
class FTakeRecorderSourcesModule : public ITakeRecorderSourcesModule, private FSelfRegisteringExec
{
public:

	static FTakeRecorderSourcesModule& Get()
	{
		return FModuleManager::GetModuleChecked<FTakeRecorderSourcesModule>(TEXT("TakeRecorderSources"));
	}

	//~ Begin ITakeRecorderSourcesModule Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void RegisterCanRecordDelegate(FName HandleId, FCanRecordDelegate InDelegate) override;
	virtual void UnregisterCanRecordDelegate(FName HandleId) override;
	//~ End ITakeRecorderSourcesModule Interface

	bool CanRecord(const FCanRecordArgs& InArgs);

private:
	
	FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelEditorMenuExtenderDelegate;

	FActorTakeRecorderDropHandler ActorDropHandler;
	FDelegateHandle SourcesMenuExtension;
	FDelegateHandle LevelEditorExtenderDelegateHandle;
	FDelegateHandle OnSequencerCreatedHandle;

	TSharedPtr<FUICommandList> CommandList;

	TMap<FName, FCanRecordDelegate> CanRecordDelegates;
	
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
	void BindCommands();

	TSharedRef<FExtender> ExtendLevelViewportContextMenu(const TSharedRef<FUICommandList> InCommandList, const TArray<AActor*> SelectedActors);

	static void ExtendSourcesMenu(TSharedRef<FExtender> Extender, UTakeRecorderSources* Sources);
	static void PopulateSourcesMenu(FMenuBuilder& MenuBuilder, UTakeRecorderSources* Sources);
	static void PopulateActorSubMenu(FMenuBuilder& MenuBuilder, UTakeRecorderSources* Sources);

	//~ Begin FSelfRegisteringExec Interface
	virtual bool Exec_Editor(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End FSelfRegisteringExec Interface
	bool HandleRecordTakeCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar);
	bool HandleStopRecordTakeCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar);
	bool HandleCancelRecordTakeCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar);

	void RecordActors(const TArray<AActor*>& ActorsToRecord, TOptional<ULevelSequence*> LevelSequence, TOptional<ULevelSequence*> RootLevelSequence);
	void RecordSelectedActors();

	void OnSequencerCreated(TSharedRef<ISequencer> Sequencer);
};
}
