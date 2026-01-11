// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextModuleTrack.h"

#if ANIMNEXT_TRACE_ENABLED
#include "AnimNextProvider.h"
#include "Editor.h"
#include "IGameplayProvider.h"
#include "IRewindDebugger.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "StructUtils/PropertyBag.h"
#include "Serialization/MemoryReader.h"

#define LOCTEXT_NAMESPACE "AnimNextModuleTrack"

namespace UE::AnimNextEditor
{

static const FName AnimNextModulesName("AnimNextModules");

 FName FAnimNextModuleTrackCreator::GetTargetTypeNameInternal() const
 {
 	static const FName ObjectName("AnimNextComponent");
	return ObjectName;
}

	
FText FAnimNextModuleTrack::GetDisplayNameInternal() const
{
 	if (DisplayNameCache.IsEmpty())
 	{
 		TraceServices::FAnalysisSessionReadScope SessionReadScope(*IRewindDebugger::Instance()->GetAnalysisSession());
 		const FAnimNextProvider* AnimNextProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<FAnimNextProvider>("AnimNextProvider");
 		const IGameplayProvider* GameplayProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<IGameplayProvider>("GameplayProvider");

 		if (InstanceId != 0)
 		{
 			if (const FDataInterfaceData* Data = AnimNextProvider->GetDataInterfaceData(InstanceId))
 			{
 				const FObjectInfo& ModuleInfo = GameplayProvider->GetObjectInfo(Data->AssetId);
 				DisplayNameCache = FText::FromString(ModuleInfo.Name);
 				return DisplayNameCache;
 			}
 		}
	
		// Don't cache this since it is a placeholder name, will be replaced once the real one is sent
 		return NSLOCTEXT("RewindDebugger", "AnimNextModuleTrackName", "Module");
 	}

 	return DisplayNameCache;
}

void FAnimNextModuleTrackCreator::GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({AnimNextModulesName, LOCTEXT("AnimNextModule", "AnimNextModules")});
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FAnimNextModuleTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<FAnimNextModuleTrack>(ObjectId);
}

		
FAnimNextModuleTrack::FAnimNextModuleTrack(uint64 InObjectId) :
	ObjectId(InObjectId)
{
	Initialize();
}
	
FAnimNextModuleTrack::FAnimNextModuleTrack(uint64 InObjectId, uint64 InInstanceId) :
   	ObjectId(InObjectId),
	InstanceId(InInstanceId)
{
   	Initialize();
}
	
	
FAnimNextModuleTrack::~FAnimNextModuleTrack()
{
 	if (UPropertyBagDetailsObject* DetailsObject = DetailsObjectWeakPtr.Get())
   	{
   		DetailsObject->ClearFlags(RF_Standalone);
   	}
}
	
void FAnimNextModuleTrack::Initialize()
{
	ExistenceRange = MakeShared<SEventTimelineView::FTimelineEventData>();
	ExistenceRange->Windows.Add({0,0, GetDisplayNameInternal(), GetDisplayNameInternal(), FLinearColor(0.1f,0.15f,0.11f)});
	
   	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
   	FDetailsViewArgs DetailsViewArgs;
   	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
   	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InitializeDetailsObject();
}
	
UPropertyBagDetailsObject* FAnimNextModuleTrack::InitializeDetailsObject()
{
	UPropertyBagDetailsObject* DetailsObject = NewObject<UPropertyBagDetailsObject>();
	DetailsObject->SetFlags(RF_Standalone);
	DetailsObjectWeakPtr = MakeWeakObjectPtr(DetailsObject);
	DetailsView->SetObject(DetailsObject);
 	return DetailsObject;
}


TSharedPtr<SWidget> FAnimNextModuleTrack::GetTimelineViewInternal()
{
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Raw(this, &FAnimNextModuleTrack::GetExistenceRange);}

bool FAnimNextModuleTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAnimNextModuleTrack::UpdateInternal);
 	
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
 	const TRange<double> ViewRange = RewindDebugger->GetCurrentViewRange();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

 	bool bChanged = false;
	
	if (const FAnimNextProvider* AnimNextProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<FAnimNextProvider>("AnimNextProvider"))
	{
		double CurrentScrubTime = IRewindDebugger::Instance()->CurrentTraceTime();

		UPropertyBagDetailsObject* DetailsObject = DetailsObjectWeakPtr.Get();
		if (DetailsObject == nullptr)
		{
			// this should not happen unless the object was garbage collected (which should not happen since it's marked as Standalone)
			Initialize();
		}

		if (InstanceId == 0)
		{
			AnimNextProvider->GetModuleId(ObjectId, InstanceId);
		}
		
		if(InstanceId != 0)
		{
			if (const FDataInterfaceData* Data = AnimNextProvider->GetDataInterfaceData(InstanceId))
			{
				if (PreviousScrubTime != CurrentScrubTime)
				{
					PreviousScrubTime = CurrentScrubTime;
					
					const TraceServices::IFrameProvider& FramesProvider = TraceServices::ReadFrameProvider(*AnalysisSession);
					TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

					TraceServices::FFrame MarkerFrame;
					if(FramesProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentScrubTime, MarkerFrame))
					{
						Data->VariablesTimeline.EnumerateEvents(MarkerFrame.StartTime, MarkerFrame.EndTime, [AnimNextProvider, DetailsObject, this](double InStartTime, double InEndTime, uint32 InDepth, const FPropertyVariableData& VariableListData)
						{
							// First look up property description
							const FPropertyDescriptionData* DescriptionData = AnimNextProvider->GetPropertyDescriptionData(VariableListData.DescriptionHash);
							if (DescriptionData == nullptr)
							{
								// Can't do anything without a description
								return TraceServices::EEventEnumerate::Continue;
							}
							
							{
								// Load the property descriptions
								FMemoryReader Reader(DescriptionData->Data);
								FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
								ReaderProxy.UsingCustomVersion(AnimNext::FAnimNextTrace::CustomVersionGUID);
								ReaderProxy << PropertyDescriptions;

								DetailsObject->Properties.Reset();
								DetailsObject->Properties.AddProperties(PropertyDescriptions);
							}

							{
								// Load the property values
								FMemoryReader Reader(VariableListData.ValueData);
								FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);

								UPropertyBag* PropertyBag = const_cast<UPropertyBag*>(DetailsObject->Properties.GetPropertyBagStruct());
								if (PropertyBag != nullptr)
								{
									PropertyBag->SerializeItem(ReaderProxy, DetailsObject->Properties.GetMutableValue().GetMemory(), nullptr);
								}
							}

							return TraceServices::EEventEnumerate::Stop;
						});
					}
				}
		
				ExistenceRange->Windows.SetNum(1,EAllowShrinking::No);
				ExistenceRange->Windows[0].TimeStart = Data->StartTime;
				ExistenceRange->Windows[0].TimeEnd = Data->EndTime;
			}

			TArray<uint64, TInlineAllocator<32>> CurrentChildren;
		
			// update/create child tracks
			AnimNextProvider->EnumerateChildInstances(InstanceId, [this, &bChanged, &CurrentChildren, &ViewRange](const FDataInterfaceData& ChildData)
			{
				if (ChildData.StartTime < ViewRange.GetUpperBoundValue() && ChildData.EndTime > ViewRange.GetLowerBoundValue())
				{
					CurrentChildren.Add(ChildData.InstanceId);
					if (!Children.ContainsByPredicate([&ChildData](TSharedPtr<FAnimNextModuleTrack>& Child) { return Child->InstanceId == ChildData.InstanceId; } ))
					{
						Children.Add(MakeShared<FAnimNextModuleTrack>(ObjectId, ChildData.InstanceId));
						bChanged = true;
					}
				}
			});
			
			int32 NumRemoved = Children.RemoveAll([&CurrentChildren](TSharedPtr<FAnimNextModuleTrack>& Child)
			{
				return !CurrentChildren.Contains(Child->InstanceId);
			});
			bChanged |= (NumRemoved > 0);
		}

		for (auto& Child : Children)
		{
			bChanged |= Child->Update();
		}
	}
	
 	return bChanged;
}

void FAnimNextModuleTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FAnimNextModuleTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FAnimNextModuleTrack::HandleDoubleClickInternal()
{
	const IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		const IGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<IGameplayProvider>("GameplayProvider");
		const FAnimNextProvider* AnimNextProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<FAnimNextProvider>("AnimNextProvider");

		if (InstanceId != 0)
		{
			if (const FDataInterfaceData* ModuleData = AnimNextProvider->GetDataInterfaceData(InstanceId))
			{
				const FObjectInfo& AssetInfo = GameplayProvider->GetObjectInfo(ModuleData->AssetId);
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetInfo.PathName);
				return true;
			}
		}
	}
	return false;
}
	
	
bool FAnimNextModuleTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAnimNextModuleTrack::HasDebugInfoInternal);

	const IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
    
		const FAnimNextProvider* AnimNextProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<FAnimNextProvider>("AnimNextProvider");
    
		uint64 ModuleId;
		return AnimNextProvider->GetModuleId(ObjectId, ModuleId);
	}

	return false;
}


}

#undef LOCTEXT_NAMESPACE

#endif // ANIMNEXT_TRACE_ENABLED
