// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryRewindDebuggerExtension.h"

#include "ToolMenus.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "SExportTrajectoriesWindow.h"
#include "Engine/Engine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Input/SButton.h"
#include "Logging/StructuredLog.h"

#define LOCTEXT_NAMESPACE "RewindDebuggerTrajectory"

FRewindDebuggerTrajectory::FRewindDebuggerTrajectory()
{
}

void FRewindDebuggerTrajectory::Initialize()
{
	// Generate widgets.
	GenerateMenu();

	// Colors for trajectories. (Up to 4 rn, should cycle if trying to index out of bounds).
	DebugDrawColors.Push(FColor::Cyan);
	DebugDrawColors.Push(FColor::Emerald);
	DebugDrawColors.Push(FColor::Magenta);
	DebugDrawColors.Push(FColor::Yellow);
}

void FRewindDebuggerTrajectory::Shutdown()
{
	// Delete preview window.
	if (BakeOutWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(BakeOutWindow.ToSharedRef());
		BakeOutWindow = nullptr;
	}
}

void FRewindDebuggerTrajectory::Reset()
{
	// Delete preview window. @todo: This is temporary.
	if (BakeOutWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(BakeOutWindow.ToSharedRef());
		BakeOutWindow = nullptr;
	}
	
	// No valid world to visualize.
	WorldToVisualize = nullptr;

	// Stop hooking into debug draw.
	if (DebugDrawDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugDrawDelegateHandle);
		DebugDrawDelegateHandle.Reset();
	}

	// Clear state.
	State.Reset();
}

void FRewindDebuggerTrajectory::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	check(RewindDebugger);
	
	// Early out.
	if (RewindDebugger->IsPIESimulating() || RewindDebugger->GetRecordingDuration() == 0)
	{
		Reset();

		return;
	}

	// @todo: Should we trace actor transforms?
	// @todo: Use orientation from trajectory vs captured pose.

	// Ensure we use RewindDebugger's world for debug drawing.
	WorldToVisualize = RewindDebugger->GetWorldToVisualize();
	
	// Hook into engine flag for debug drawing.
	EnsureDebugDrawDelegateExists();
	
	check(State.Trajectories.Num() == State.ObjectInfos.Num());
}

void FRewindDebuggerTrajectory::RecordingStarted(IRewindDebugger* RewindDebugger)
{
	// Clear up any cached state / variables.
	Reset();
}

void FRewindDebuggerTrajectory::RecordingStopped(IRewindDebugger* RewindDebugger)
{
	
}

void FRewindDebuggerTrajectory::Clear(IRewindDebugger* RewindDebugger)
{
	// Clear up any cached state / variables.
	Reset();
}

void FRewindDebuggerTrajectory::UpdateState(IRewindDebugger* RewindDebugger)
{
	// Query info from trace file.
	const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession();
	if (Session)
	{
		const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
		const IAnimationProvider* AnimationProvider = Session->ReadProvider<IAnimationProvider>("AnimationProvider");

		if (GameplayProvider && AnimationProvider)
		{
			TraceServices::FAnalysisSessionReadScope AnalysisSessionReadScope(*Session);
			
			// Note that recording index is always zero since RewindDebugger starts a new trace for every recording.
			if (const IGameplayProvider::RecordingInfoTimeline* Recording = GameplayProvider->GetRecordingInfo(0))
			{
				const uint64 EventCount = Recording->GetEventCount();

				if (EventCount > 0)
				{
					const FRecordingInfoMessage& LastEvent = Recording->GetEvent(EventCount - 1);
					
					State.Extract.TraceStarTime = 0;
					State.Extract.TraceEndTime = LastEvent.ProfileTime;
				}
				else
				{
					// Empty trace session. Abort.
					Reset();
					return;
				}
			}
			
			BuildTrajectoryOwnersList(RewindDebugger, GameplayProvider, State.Extract.TraceStarTime, State.Extract.TraceEndTime, State.ObjectInfos);
			BuildTrajectorySkeletalMeshInfoList(GameplayProvider, AnimationProvider, State.Extract.TraceStarTime, State.Extract.TraceEndTime, State.ObjectInfos, State.SkelMeshInfos);
			BuildTrajectories(GameplayProvider, AnimationProvider, State.Extract.TraceStarTime, State.Extract.TraceEndTime, State.ObjectInfos, State.Trajectories);
			UpdateDebugInfos(GameplayProvider, State.ObjectInfos, State.DebugInfos);
		}
	}
	else
	{
		// Invalid trace session. Abort.
		Reset();
	}
}

void FRewindDebuggerTrajectory::GenerateMenu()
{
	if (UToolMenu* Menu = UToolMenus::Get()->FindMenu("RewindDebugger.ToolBar"))
	{
		FToolMenuSection& Section = Menu->AddSection("TrajectoryWorkflows", LOCTEXT("Trajectory Tools","Trajectory Tools"));
		
		Section.AddSeparator("TrajectoryWorkflows");
		{
			static const FName Name = "Trajectories";
			static const FText Label = LOCTEXT("TrajectoriesMenuLabel", "Trajectories");
			static const FText ToolTip = LOCTEXT("TrajectoriesMenuTooltip", "Toggle trajectories to display in viewport");
			
			Section.AddSubMenu(Name, Label, ToolTip, FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
			{
				FToolMenuSection& Section = InMenu->AddSection(NAME_None);
				Section.AddDynamicEntry("Trajectories", FNewToolMenuSectionDelegate::CreateRaw(this, &FRewindDebuggerTrajectory::MakeTrajectoriesMenu));
			}));
		}
	}
}

void FRewindDebuggerTrajectory::MakeTrajectoriesMenu(FToolMenuSection& InSection)
{
	// Toggle trajectory drawing
	{
		static const FName Name = "ToggleDebugDrawComboButton";
		static const FText Label = LOCTEXT("ToggleDebugDrawTrajectoriesLabel", "Toggle debug draw");
		static const FText ToolTip = LOCTEXT("ToggleDebugDrawTrajectoriesTooltip", "Toggle trajectories to display in viewport");

		InSection.AddSubMenu(Name, Label, ToolTip, FNewToolMenuDelegate::CreateRaw(this, &FRewindDebuggerTrajectory::MakeToggleDebugDrawMenu), false, FSlateIcon(), false);
	}

	// Bake out trajectories
	{
		FUIAction UIAction;
		UIAction.ExecuteAction = FExecuteAction::CreateLambda([this](){ ShowBakeOutTrajectoryWindow(); });
		UIAction.CanExecuteAction = FCanExecuteAction::CreateLambda([this]()
		{
			IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
			return RewindDebugger ? !RewindDebugger->IsPIESimulating() : false;
		});
		
		static const FName Name = "BakeOutSubMenu"; 
		static const FText Label = LOCTEXT("BakeOutTrajectoriesLabel", "Bake out...");
		static const FText Tooltip = LOCTEXT("BakeOutTrajectoriesTooltip", "Bake a trajectory into a standalone asset");
		
		InSection.AddMenuEntry(Name, Label, Tooltip, FSlateIcon(), UIAction);
	}
}

void FRewindDebuggerTrajectory::MakeToggleDebugDrawMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->FindOrAddSection("ToggleDebugDrawOptions");

	for (FExtensionState::FDebugInfo& DebugInfoEntry : State.DebugInfos)
	{
		FUIAction UIAction;
		{
			// @todo: For safety, this should probably just copy the component id and try to find it, then toggle it.
			UIAction.ExecuteAction = FExecuteAction::CreateLambda([&DebugInfoEntry](){ DebugInfoEntry.bShouldDraw = !DebugInfoEntry.bShouldDraw; });
			UIAction.GetActionCheckState = FGetActionCheckState::CreateLambda([&DebugInfoEntry] { return DebugInfoEntry.bShouldDraw ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
		}

		// @todo: Add an actor icon for the trajectory picker. Similar to RewindDebugger's.
		Section.AddMenuEntry(DebugInfoEntry.Name, FText::FromName(DebugInfoEntry.Name), FText::FromName(DebugInfoEntry.Name), FSlateIcon(), UIAction, EUserInterfaceActionType::Check);
	}
}

void FRewindDebuggerTrajectory::EnsureDebugDrawDelegateExists()
{
	if (!DebugDrawDelegateHandle.IsValid())
	{
		DebugDrawDelegateHandle = UDebugDrawService::Register(TEXT("GameplayDebug"), FDebugDrawDelegate::CreateRaw(this, &FRewindDebuggerTrajectory::DebugDraw));
	}
}

void FRewindDebuggerTrajectory::DebugDraw(UCanvas* InCanvas, APlayerController* InController)
{
	// @todo: Only draw object ids which are part of the target object.
	if (IsValid(WorldToVisualize) && !State.Trajectories.IsEmpty())
	{
		for (int TrajectoryIndex = 0; TrajectoryIndex < State.Trajectories.Num(); ++TrajectoryIndex)
		{
			if (State.DebugInfos[TrajectoryIndex].bShouldDraw)
			{
				const FGameplayTrajectory& DebugTrajectoryPair = State.Trajectories[TrajectoryIndex];
				const FColor& DebugDrawColorForTrajectory = DebugDrawColors[TrajectoryIndex % DebugDrawColors.Num()];
			
				for (int SampleIndex = 0; SampleIndex < DebugTrajectoryPair.Samples.Num(); ++SampleIndex)
				{
					const FGameplayTrajectory::FSample& Sample = DebugTrajectoryPair.Samples[SampleIndex];

					if (SampleIndex + 1 < DebugTrajectoryPair.Samples.Num())
					{
						const FGameplayTrajectory::FSample& FutureSample = DebugTrajectoryPair.Samples[SampleIndex + 1];

						DrawDebugLine(WorldToVisualize, Sample.Position, FutureSample.Position, DebugDrawColorForTrajectory);
					}

					// TODO: Determine when to draw text. AKA every second, every X frames, etc.
					if (SampleIndex % 10 == 0)
					{
						FVector2D SamplePositionOnScreen;
					
						if (InCanvas->SceneView->WorldToPixel(Sample.Position, SamplePositionOnScreen))
						{
							FFontRenderInfo Info;
							Info.bEnableShadow = true;
					
							SamplePositionOnScreen.X = FMath::RoundToFloat(SamplePositionOnScreen.X);
							SamplePositionOnScreen.Y = FMath::RoundToFloat(SamplePositionOnScreen.Y);
						
							InCanvas->DrawText(GEngine->GetTinyFont(), FString::SanitizeFloat(Sample.Time), SamplePositionOnScreen.X, SamplePositionOnScreen.Y, 1, 1, Info);
						}
					}
				}
			}
		}
	}
}

void FRewindDebuggerTrajectory::ShowBakeOutTrajectoryWindow()
{
	if (BakeOutWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(BakeOutWindow.ToSharedRef());
	}

	// Prepare data before opening window.
	if (IRewindDebugger* RewindDebugger = IRewindDebugger::Instance())
	{
		UpdateState(RewindDebugger);
	}
	
	// Custom window to setup bake out settings and execute the action
	SAssignNew(BakeOutWindow, SWindow)
	.Title(LOCTEXT("BakeOutTrajectoryWindowTitle", "Bake Out Trajectories"))
	.SizingRule(ESizingRule::UserSized)
	.SupportsMaximize(true)
	.SupportsMinimize(true)
	.MinWidth(850)
	.MinHeight(500)
	.CreateTitleBar(true)
	.HasCloseButton(true)
	.ClientSize(FVector2f{1280, 720});

	// Gather all the associated owner names
	TArray<FName> OwnerNames;
	for (const FExtensionState::FDebugInfo& DebugInfo : State.DebugInfos)
	{
		OwnerNames.Add(DebugInfo.Name);
	}

	// Init custom trajectory export window
	TSharedPtr<SExportTrajectoriesWindow> ExportWindow;
	BakeOutWindow->SetContent
	(
		SAssignNew(ExportWindow, SExportTrajectoriesWindow)
		.Trajectories(State.Trajectories)
		.WidgetWindow(BakeOutWindow)
		.OwnerNames(OwnerNames)
		.DebugInfos(State.DebugInfos)
		.SkelMeshInfos(State.SkelMeshInfos)
	);
	BakeOutWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&){ BakeOutWindow = nullptr; }));

	// Hook into main window as a modal window.
	/*TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}
	ExportWindow->SetCanTick(true);*/
	
	// We use a modal window to ensure that we can store id/values from rewind debugger messages and ensure that its data provider are valid,
	// otherwise we would have to copy all the information when opening the window.
	// FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	FSlateApplication::Get().AddWindow(BakeOutWindow.ToSharedRef());
}

void FRewindDebuggerTrajectory::BuildTrajectoryOwnersList(const IRewindDebugger* InRewindDebugger, const IGameplayProvider* InGameplayProvider, double InTraceStartTime, double InTraceEndTime, TArray<FObjectInfo> & OutObjectInfos)
{
	// Clear up previous info.
	OutObjectInfos.Reset();
	
	// Used to filter which objects we can use to build our trajectory.
	const FClassInfo* SkelMeshCmpClassInfo = InGameplayProvider->FindClassInfo(*USkeletalMeshComponent::StaticClass()->GetPathName());
	check(SkelMeshCmpClassInfo)
	
	// Query target actor of the trace file / session.
	InGameplayProvider->EnumerateObjects(InTraceStartTime, InTraceEndTime, [&OutObjectInfos,InGameplayProvider, SkelMeshCmpClassInfo, InRewindDebugger](const FObjectInfo& InObjectInfo)
	{
		// For all root objects find their skeletal mesh component's id, if any.
		if (InObjectInfo.OuterId == 0)
		{
			InGameplayProvider->EnumerateSubobjects(InObjectInfo.Id, [&OutObjectInfos, InGameplayProvider, SkelMeshCmpClassInfo](uint64 InSubObjectId)
			{
				const FObjectInfo& SubObjectInfo = InGameplayProvider->GetObjectInfo(InSubObjectId);

				const bool bIsSubObjectASkelMesh = SubObjectInfo.ClassId == SkelMeshCmpClassInfo->Id || InGameplayProvider->IsSubClassOf(SubObjectInfo.ClassId, SkelMeshCmpClassInfo->Id);
				if (bIsSubObjectASkelMesh)
				{
					OutObjectInfos.Push(SubObjectInfo);
				}
			});
		}
		else
		{
			const bool bIsObjectASkelMesh = InObjectInfo.ClassId == SkelMeshCmpClassInfo->Id || InGameplayProvider->IsSubClassOf(InObjectInfo.ClassId, SkelMeshCmpClassInfo->Id);
			if (bIsObjectASkelMesh)
			{
				const bool bHasOwningActor = InRewindDebugger->FindOwningActorInfo(InGameplayProvider, InObjectInfo.Id) != nullptr;
				if (bHasOwningActor)
				{
					OutObjectInfos.Push(InObjectInfo);
				}
			}
		}
	});
}

void FRewindDebuggerTrajectory::BuildTrajectorySkeletalMeshInfoList(const IGameplayProvider* InGameplayProvider, const IAnimationProvider* InAnimationProvider, double InTraceStartTime, double InTraceEndTime, const TArray<FObjectInfo>& InSkeletalMeshComponentObjectInfos, TArray<FSkeletalMeshInfo>& OutSkeletalMeshInfos)
{
	// Clear up previous info.
	OutSkeletalMeshInfos.Reset();
	
	for (int SkelMeshCmpIndex = 0; SkelMeshCmpIndex < InSkeletalMeshComponentObjectInfos.Num(); ++SkelMeshCmpIndex)
	{
		const FObjectInfo& SkeletalMeshComponentInfo = InSkeletalMeshComponentObjectInfos[SkelMeshCmpIndex];
		
		InAnimationProvider->ReadSkeletalMeshPoseTimeline(SkeletalMeshComponentInfo.Id, [InAnimationProvider, InGameplayProvider, InTraceStartTime, InTraceEndTime, SkelMeshCmpIndex, &SkeletalMeshComponentInfo, &OutSkeletalMeshInfos](const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, bool bHasCurves)
		{
			TimelineData.EnumerateEvents(InTraceStartTime, InTraceEndTime, [InAnimationProvider, InGameplayProvider, SkelMeshCmpIndex, &SkeletalMeshComponentInfo, &OutSkeletalMeshInfos](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InPoseMessage) -> TraceServices::EEventEnumerate
			{
				const FSkeletalMeshInfo* SkeletalMeshInfo = InAnimationProvider->FindSkeletalMeshInfo(InPoseMessage.MeshId);
				const FObjectInfo* SkeletalMeshObjectInfo = InGameplayProvider->FindObjectInfo(InPoseMessage.MeshId); // @todo: Check if this search can be remove and just use the already available id.

				// Query skeletal mesh information.
				if (!SkeletalMeshInfo || !SkeletalMeshObjectInfo)
				{
					OutSkeletalMeshInfos.Reset();
					UE_LOGFMT(LogTemp, Warning, "No SkelMeshInfo or SkelMeshObjectInfo provided. For object: {name}", *SkeletalMeshComponentInfo.Name);
					return TraceServices::EEventEnumerate::Stop;
				}
				
				if (!OutSkeletalMeshInfos.IsValidIndex(SkelMeshCmpIndex))
				{
					OutSkeletalMeshInfos.Push(*SkeletalMeshInfo);
					// UE_LOGFMT(LogTemp, Warning, "[{0}] - Skel Mesh Info START - \nStarting:{1}", SkelMeshCmpIndex, SkeletalMeshObjectInfo->PathName);

					const FObjectInfo* SkeletonObjectInfo = InGameplayProvider->FindObjectInfo(SkeletalMeshInfo->SkeletonId);
					// UE_LOGFMT(LogTemp, Warning, "[{0}] - Skeleton Info START - \nStarting:{1}", SkelMeshCmpIndex, SkeletonObjectInfo != nullptr? SkeletonObjectInfo->PathName : static_cast<const TCHAR*>(TEXT("")));
				}
				else
				{
					// Keep track of any data change.
					FSkeletalMeshInfo& PrevMeshInfo = OutSkeletalMeshInfos[SkelMeshCmpIndex];
					
					bool bSameMesh = SkeletalMeshInfo->Id == PrevMeshInfo.Id; 
					bool bSameSkeleton = SkeletalMeshInfo->SkeletonId == PrevMeshInfo.SkeletonId;
					
					if (!bSameMesh)
					{
						UE_LOGFMT(LogTemp, Warning, "[{0}] - Skel Mesh Info Changed - \nPrev:{1}\nNew:{2}\nName:{3}", SkelMeshCmpIndex, PrevMeshInfo.Id, SkeletalMeshInfo->Id, SkeletalMeshObjectInfo->PathName);
						OutSkeletalMeshInfos[SkelMeshCmpIndex] = *SkeletalMeshInfo;
						
						// If mesh changes is fine but not if the skeleton changes!
						// UE_LOGFMT(LogTemp, Warning, "No SkelMeshInfo or SkelMeshObjectInfo provided. For object: {name}", SkeletalMeshObjectInfo->Name);
					}
					
					if (!bSameSkeleton)
					{
						const FObjectInfo* SkeletonObjectInfo = InGameplayProvider->FindObjectInfo(SkeletalMeshInfo->SkeletonId);
						UE_LOGFMT(LogTemp, Warning, "[{0}] - Skeleton Info Changed - \nPrev:{1}\nNew:{2}\nName:{3}", SkelMeshCmpIndex, PrevMeshInfo.SkeletonId, SkeletalMeshInfo->SkeletonId, SkeletonObjectInfo != nullptr ? SkeletonObjectInfo->PathName : static_cast<const TCHAR*>(TEXT("")));
					}
				}

				return TraceServices::EEventEnumerate::Continue;
			});
		});
	}
}

void FRewindDebuggerTrajectory::BuildTrajectories(const IGameplayProvider* InGameplayProvider, const IAnimationProvider* InAnimationProvider, double InTraceStartTime, double InTraceEndTime, const TArray<FObjectInfo>& InObjectInfos, TArray<FGameplayTrajectory>& OutTrajectories)
{
	// Clear up previous info.
	OutTrajectories.Reset();

	// Used to filter which objects we can use to build our trajectory.
	const FClassInfo* SkelMeshCmpClassInfo = InGameplayProvider->FindClassInfo(*USkeletalMeshComponent::StaticClass()->GetPathName());
	check(SkelMeshCmpClassInfo)

	// Build trajectory from pose's root transform over time.
	for (const FObjectInfo& ObjectInfo : InObjectInfos)
	{
		OutTrajectories.Push({});

		// Setup initial index range
		{
			TRange<int32> IndexRange(TRangeBound<int32>::Inclusive(0), TRangeBound<int32>::Open());

			OutTrajectories.Last().TraceInfo.Ranges.Push(IndexRange);
		}
		
		// BoneCount split tracker.
		int64 PrevSampleBoneCount = INDEX_NONE;
		
		InAnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectInfo.Id, [InGameplayProvider, InAnimationProvider, InTraceStartTime, InTraceEndTime, &OutTrajectories, &ObjectInfo, &PrevSampleBoneCount](const IAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
		{
			InTimeline.EnumerateEvents(InTraceStartTime, InTraceEndTime, [InGameplayProvider, InAnimationProvider, &OutTrajectories, &ObjectInfo, &PrevSampleBoneCount](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InPoseMessage)
			{
				// Used to ensure trajectories always start at time being zero.
				const bool bIsFirstSample = OutTrajectories.Last().Samples.IsEmpty();
				const double FirstSampleRecordingTime = bIsFirstSample ? InPoseMessage.RecordingTime : OutTrajectories.Last().Samples[0].Time;
				
				const FVector SamplePosition = InPoseMessage.ComponentToWorld.GetTranslation();		// Root transform's position (SkelMeshCmp World Position).
				const FQuat SampleOrientation = InPoseMessage.ComponentToWorld.GetRotation();		// Root transform's position (SkelMeshCmp World Rotation).
				const double SampleTime = InPoseMessage.RecordingTime - FirstSampleRecordingTime;	// Sample timed, based of range beginning not trace beginning time.

				// Query skeletal mesh information.
				const FSkeletalMeshInfo* SkeletalMeshInfo = InAnimationProvider->FindSkeletalMeshInfo(InPoseMessage.MeshId);
				const FObjectInfo* SkeletalMeshObjectInfo = InGameplayProvider->FindObjectInfo(InPoseMessage.MeshId);
				
				if (!SkeletalMeshInfo || !SkeletalMeshObjectInfo)
				{
					OutTrajectories.Last().Reset();
					
					UE_LOGFMT(LogTemp, Warning, "No SkelMeshInfo or SkelMeshObjectInfo provided. For object: {name}", *ObjectInfo.Name);
					return TraceServices::EEventEnumerate::Stop;
				}
				
				if (InPoseMessage.NumTransforms != SkeletalMeshInfo->BoneCount)
				{
					OutTrajectories.Last().Reset();
					
					UE_LOGFMT(LogTemp, Warning, "BoneCount doesn't match. Traced bone transforms do not match the respective traced mesh asset's bones. For object: {name}", *ObjectInfo.Name);
					return TraceServices::EEventEnumerate::Stop;
				}
				
				// Keep track of instances when bone counts don't match.
				if (PrevSampleBoneCount != SkeletalMeshInfo->BoneCount)
				{
					int32 BoneCountSplitSample = OutTrajectories.Last().Samples.Num();

					// Split range by the new bone count, unless we're initializing.
					if (PrevSampleBoneCount != INDEX_NONE)
					{
						TRange<int32> LastRange = OutTrajectories.Last().TraceInfo.Ranges.Last();
						OutTrajectories.Last().TraceInfo.Ranges.Pop();
					
						OutTrajectories.Last().TraceInfo.Ranges.Append(LastRange.Split(BoneCountSplitSample));
					}
					
					// Assign the skeletal mesh info to the new range
					OutTrajectories.Last().TraceInfo.SkeletalMeshInfos.Add(*SkeletalMeshInfo);
				}
				PrevSampleBoneCount = SkeletalMeshInfo->BoneCount;

				// Extract and append animation pose for the current sample.
				{
					FTransform OutComponentToWorld;
					OutTrajectories.Last().Poses.AddDefaulted();
					InAnimationProvider->GetSkeletalMeshComponentSpacePose(InPoseMessage, *SkeletalMeshInfo, OutComponentToWorld, OutTrajectories.Last().Poses.Last());	
				}

				// Add current sample information.
				OutTrajectories.Last().Samples.Push({SampleTime, SamplePosition, SampleOrientation});

				return TraceServices::EEventEnumerate::Continue;
			});
		});

		// Always set the upper bound range.
		if (!OutTrajectories.Last().Samples.IsEmpty())
		{
			OutTrajectories.Last().TraceInfo.Ranges.Last().SetUpperBound(TRangeBound<int32>(OutTrajectories.Last().Samples.Num() - 1));
		}
		else
		{
			OutTrajectories.Last().TraceInfo.Reset();
		}
	}

	// @todo: This should report back an error if it failed to perform an action.
}

void FRewindDebuggerTrajectory::UpdateDebugInfos(const IGameplayProvider* InGameplayProvider, const TArray<FObjectInfo>& InObjectInfos, TArray<FExtensionState::FDebugInfo>& InOutDebugInfos)
{
	// We need a local copy to search for previous occurances.
	TArray<FExtensionState::FDebugInfo> PrevDebugInfos = InOutDebugInfos;
	InOutDebugInfos.Reset();
	
	for (const FObjectInfo& ObjectInfo : InObjectInfos)
	{
		const FExtensionState::FDebugInfo* MatchedDebugInfo = PrevDebugInfos.FindByPredicate([&ObjectInfo](const FExtensionState::FDebugInfo& Item)
		{
			return Item.OwnerId == ObjectInfo.Id;
		});

		// Use cached version to keep track of stateful date (i.e. ShouldDraw checkbox)
		if (MatchedDebugInfo)
		{
			InOutDebugInfos.Push(*MatchedDebugInfo);
		}
		else
		{
			InOutDebugInfos.Push({ObjectInfo.Id, GetFullNameForDebugInfoOwner(InGameplayProvider, ObjectInfo.Id), false});
		}
	}
}

FName FRewindDebuggerTrajectory::GetFullNameForDebugInfoOwner(const IGameplayProvider* InGameplayProvider, uint64 InOwnerObjectId)
{
	const FClassInfo* ActorClassInfo = InGameplayProvider->FindClassInfo(*AActor::StaticClass()->GetPathName());

	uint64 ObjectId = InOwnerObjectId;

	TStringBuilder<128> Result;
	
	while(true)
	{
		const FObjectInfo& ObjectInfo = InGameplayProvider->GetObjectInfo(ObjectId);

		// We reached the owning actor.
		if (InGameplayProvider->IsSubClassOf(ObjectInfo.ClassId, ActorClassInfo->Id))
		{
			Result.Prepend(TEXTVIEW(" - "));
			Result.Prepend(ObjectInfo.Name);

			const FWorldInfo* WorldInfo = InGameplayProvider->FindWorldInfoFromObject(ObjectId);
			if (WorldInfo)
			{
				bool bIsServer = WorldInfo->NetMode == FWorldInfo::ENetMode::DedicatedServer;

				if (bIsServer)
				{
					Result.Append(TEXTVIEW(" (Server)"));
				}
			}
			
			return FName(Result.ToView());
		}
		// We are a component, keep traversing up the tree.
		else
		{
			if (ObjectInfo.OuterId != 0)
			{
				if (ObjectId != InOwnerObjectId)
				{
					Result.Prepend(TEXTVIEW(" - "));
				}
				Result.Prepend(ObjectInfo.Name);
				
				ObjectId = ObjectInfo.OuterId;
			}
			else
			{
				return NAME_None;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
