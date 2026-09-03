// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelCommon.h"
#include "Logging/StructuredLog.h"
#include "NiagaraGpuComputeDispatchInterface.h"

void UNiagaraDataChannelHandler::BeginDestroy()
{
	Super::BeginDestroy();
	Cleanup();
	
	RTFence.BeginFence();
}

bool UNiagaraDataChannelHandler::IsReadyForFinishDestroy()
{
	return RTFence.IsFenceComplete() && Super::IsReadyForFinishDestroy();
}

void UNiagaraDataChannelHandler::Init(const UNiagaraDataChannel* InChannel)
{
	DataChannel = InChannel;
}

void UNiagaraDataChannelHandler::Cleanup()
{
	if(Reader)
	{
		Reader->Cleanup();
		Reader = nullptr;
	}
	
	if(Writer)
	{
		Writer->Cleanup();
		Writer = nullptr;
	}

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//Mark this handler as garbage so any reading DIs will know to stop using it.
		MarkAsGarbage();
	}
}

void UNiagaraDataChannelHandler::BeginFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	CurrentTG = TG_PrePhysics;

	for(auto It = WeakDataArray.CreateIterator(); It; ++It)
	{
		if(It->IsValid() == false)
		{
			It.RemoveCurrentSwap();
		}
	}
}

void UNiagaraDataChannelHandler::EndFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{

}

void UNiagaraDataChannelHandler::Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
	CurrentTG = TickGroup;
}

UNiagaraDataChannelWriter* UNiagaraDataChannelHandler::GetDataChannelWriter()
{
	if(Writer == nullptr)
	{
		Writer =  NewObject<UNiagaraDataChannelWriter>();
		Writer->Owner = this;
	}
	return Writer;
}

UNiagaraDataChannelReader* UNiagaraDataChannelHandler::GetDataChannelReader()
{
	if (Reader == nullptr)
	{
		Reader = NewObject<UNiagaraDataChannelReader>();
		Reader->Owner = this;
	}
	return Reader;
}

void UNiagaraDataChannelHandler::SubscribeToDataChannelUpdates(FOnNewNiagaraDataChannelPublish UpdateDelegate, FNiagaraDataChannelSearchParameters SearchParams, int32& UnsubscribeToken)
{
	if (!UpdateDelegate.IsBound())
	{
		UnsubscribeToken = -1;
		return;
	}

	SubscriberTokens++;
	UnsubscribeToken = SubscriberTokens;
	
	FChannelSubscription& ChannelSubscription = ChannelSubscriptions.AddDefaulted_GetRef();
	ChannelSubscription.SubscriptionToken = SubscriberTokens;
	ChannelSubscription.OnPublishDelegate = UpdateDelegate;
	ChannelSubscription.SearchParams = SearchParams;
}

void UNiagaraDataChannelHandler::UnsubscribeFromDataChannelUpdates(const int32& UnsubscribeToken)
{
	for (int i = 0; i < ChannelSubscriptions.Num(); i++)
	{
		if (ChannelSubscriptions[i].SubscriptionToken == UnsubscribeToken)
		{
			ChannelSubscriptions.RemoveAtSwap(i);
			break;
		}
	}
}

FNiagaraDataChannelDataPtr UNiagaraDataChannelHandler::CreateData()
{
	FNiagaraDataChannelDataPtr Ret = MakeShared<FNiagaraDataChannelData>();
	WeakDataArray.Add(Ret);
	Ret->Init(this);
	return Ret;
}

void UNiagaraDataChannelHandler::NotifySubscribers(FNiagaraDataChannelData* Source, int32 StartIndex, int32 NumNewElements)
{
	if (NumNewElements == 0 || ChannelSubscriptions.IsEmpty() || StartIndex < 0)
	{
		return;
	}

	FNiagaraDataChannelUpdateContext UpdateContext;
	UpdateContext.Reader = GetDataChannelReader();
	UpdateContext.FirstNewDataIndex = StartIndex;
	UpdateContext.LastNewDataIndex = StartIndex + NumNewElements - 1;
	UpdateContext.NewElementCount = NumNewElements;
	for (int i = ChannelSubscriptions.Num() - 1; i >= 0; i--)
	{
		FChannelSubscription& ChannelSubscription = ChannelSubscriptions[i];
		if (ChannelSubscription.OnPublishDelegate.IsCompactable())
		{
			ChannelSubscriptions.RemoveAt(i);
			continue;
		}

		FNiagaraDataChannelDataPtr ChannelData = FindData(ChannelSubscription.SearchParams, ENiagaraResourceAccess::ReadOnly);
		if (ChannelData.Get() == Source)
		{
			UpdateContext.Reader->Data = ChannelData;
			ChannelSubscription.OnPublishDelegate.Execute(UpdateContext);
		}
	}
}

void UNiagaraDataChannelHandler::OnComputeDispatchInterfaceDestroyed(FNiagaraGpuComputeDispatchInterface* InComputeDispatchInterface)
{
	//Destroy all RT proxies when the dispatcher is destroyed.
	//In cases where this is done on a running world, we'll do a lazy reinit next frame.
	ForEachNDCData([InComputeDispatchInterface](FNiagaraDataChannelDataPtr& NDCData)
	{
		check(NDCData);
		NDCData->DestroyRenderThreadProxy(InComputeDispatchInterface);
	});
}
