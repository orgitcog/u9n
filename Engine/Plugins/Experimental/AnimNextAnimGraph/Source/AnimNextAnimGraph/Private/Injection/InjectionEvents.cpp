// Copyright Epic Games, Inc. All Rights Reserved.

#include "Injection/InjectionEvents.h"

#include "TraitCore/TraitEventList.h"

namespace UE::AnimNext
{
	void FInjection_InjectEvent::OnExpired(FTraitEventList& OutputEventList)
	{
		auto ActionEvent = MakeTraitEvent<FAnimNextModule_ActionEvent>();
		ActionEvent->ActionFunction = [Request = Request]()
			{
				Request->OnStatusUpdate(EInjectionStatus::Expired);
			};

		OutputEventList.Push(MoveTemp(ActionEvent));
	}

	void FInjection_StatusUpdateEvent::Execute() const
	{
		Request->OnStatusUpdate(Status);
	}

	void FInjection_TimelineUpdateEvent::Execute() const
	{
		Request->OnTimelineUpdate(TimelineState);
	}
}
