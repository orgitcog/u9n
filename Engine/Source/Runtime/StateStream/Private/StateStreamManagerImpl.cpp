// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateStreamManagerImpl.h"
#include "StateStreamCreator.h"
#include "StateStreamDebugRenderer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

void FStateStreamManagerImpl::Game_BeginTick()
{
	check(!bIsInTick);
	check (!bGameExited);
	bIsInTick = true;
	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Game_BeginTick();
	}
}

void FStateStreamManagerImpl::Game_EndTick(double AbsoluteTime)
{
	check(bIsInTick);
	bIsInTick = false;
	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Game_EndTick(AbsoluteTime);
	}
}

void FStateStreamManagerImpl::Game_Exit()
{
	check(!bIsInTick);
	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Game_Exit();
	}
	bGameExited = true;
}

bool FStateStreamManagerImpl::Game_IsInTick()
{
	return bIsInTick;
}

void* FStateStreamManagerImpl::Game_GetStreamPointer(uint32 Id)
{
	check(Id < uint32(StateStreamsLookup.Num()));
	return StateStreamsLookup[Id]->Game_GetVoidPointer();
}

void FStateStreamManagerImpl::Render_Register(IStateStream& Stream, bool TakeOwnership)
{
	StateStreamRec Rec { &Stream, TakeOwnership };
	StateStreams.Add(Rec);
	
	uint32 Id = Stream.GetId();
	if (uint32(StateStreamsLookup.Num()) <= Id)
	{
		StateStreamsLookup.SetNum(Id+1);
	}
	check(StateStreamsLookup[Id] == nullptr);
	StateStreamsLookup[Id] = &Stream;
}

void FStateStreamManagerImpl::Render_RegisterDependency(uint32 FromId, uint32 ToId)
{
	// TODO
}

void FStateStreamManagerImpl::Render_RegisterDependency(IStateStream& From, IStateStream& To)
{
	Render_RegisterDependency(From.GetId(), To.GetId());
}

void FStateStreamManagerImpl::Render_Update(double AbsoluteTime)
{
	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Render_Update(AbsoluteTime);
	}

	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Render_PostUpdate();
	}
}

void FStateStreamManagerImpl::Render_Exit()
{
	GarbageCollectTask.Wait();

	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->Render_Exit();
	}
	bRenderExited = true;
}

void FStateStreamManagerImpl::Render_GarbageCollect(bool AsTask)
{
	GarbageCollectTask.Wait();

	auto Func = [this]()
		{
			for (const StateStreamRec& Rec : StateStreams)
			{
				Rec.Stream->Render_GarbageCollect();
			}
		};
	if (AsTask)
	{
		GarbageCollectTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, MoveTemp(Func));
	}
	else
	{
		Func();
	}
}

IStateStream* FStateStreamManagerImpl::Render_GetStream(uint32 Id)
{
	check(Id < uint32(StateStreamsLookup.Num()));
	return StateStreamsLookup[Id];
}

void FStateStreamManagerImpl::Game_DebugRender(IStateStreamDebugRenderer& Renderer)
{
	for (const StateStreamRec& Rec : StateStreams)
	{
		Rec.Stream->DebugRender(Renderer);
	}
}

FStateStreamManagerImpl::~FStateStreamManagerImpl()
{
	for (const StateStreamRec& Rec : StateStreams)
	{
		if (Rec.Owned)
		{
			delete Rec.Stream;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FStateStreamCreator* FStateStreamCreator::First;

FStateStreamCreator::FStateStreamCreator(uint32 Id_, FRegisterFunction&& InRegisterFunction, FUnregisterFunction&& InUnregisterFunction)
:	RegisterFunction(MoveTemp(InRegisterFunction))
,	UnregisterFunction(MoveTemp(InUnregisterFunction))
,	Id(Id_)
{
	Prev = nullptr;
	for (FStateStreamCreator* It=First; It; It=It->Next)
	{
		if (Id < It->Id)
			break;
		Prev = It;
	}

	if (!Prev)
	{
		Next = First;
		First = this;
	}
	else
	{
		Next = Prev->Next;
		Prev->Next = this;
	}

	if (Next)
	{
		Next->Prev = this;
	}
}

FStateStreamCreator::~FStateStreamCreator()
{
	if (Next)
	{
		Next->Prev = Prev;
	}

	if (Prev)
	{
		Prev->Next = Next;
	}
	else
	{
		First = Next;
	}

	Next = nullptr;
	Prev = nullptr;
}

void FStateStreamCreator::RegisterStateStreams(const FStateStreamRegisterContext& Context)
{
	for (FStateStreamCreator* It=First; It; It=It->Next)
	{
		It->RegisterFunction(Context);
	}
}

void FStateStreamCreator::UnregisterStateStreams(const FStateStreamUnregisterContext& Context)
{
	for (FStateStreamCreator* It=First; It; It=It->Next)
	{
		It->UnregisterFunction(Context);
	}
}

void FStateStreamRegisterContext::Register(IStateStream& StateStream, bool TakeOwnership) const
{
	Manager.Render_Register(StateStream, TakeOwnership);
}

void FStateStreamRegisterContext::RegisterDependency(uint32 FromId, uint32 ToId) const
{
	Manager.Render_RegisterDependency(FromId, ToId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
