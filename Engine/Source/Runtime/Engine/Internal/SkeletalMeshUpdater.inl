// Copyright Epic Games, Inc. All Rights Reserved.

///////////////////////////////////////////////////////////////////////////////

template <typename SkeletalMeshMeshDynamicDataType>
bool FSkeletalMeshUpdateHandle::Update(SkeletalMeshMeshDynamicDataType* MeshDynamicData)
{
	if (ensure(Channel))
	{
		checkf(Channel->IsChannelFor<SkeletalMeshMeshDynamicDataType>(), TEXT("Provided MeshDynamicData is not the correct type for this handle."));
		return Channel->Update(*this, MeshDynamicData);
	}
	return false;
}

inline void FSkeletalMeshUpdateHandle::Release()
{
	if (Channel)
	{
		Channel->Release(MoveTemp(*this));
	}
}

///////////////////////////////////////////////////////////////////////////////

inline void FSkeletalMeshUpdateChannel::FDynamicDataList::Add(FSkeletalMeshDynamicData* Command)
{
	if (!Head)
	{
		Head = Tail = Command;
	}
	else
	{
		Tail->Next = Command;
		Tail       = Command;
	}
}

template <typename LambdaType>
void FSkeletalMeshUpdateChannel::FDynamicDataList::Consume(LambdaType&& Lambda)
{
	for (FSkeletalMeshDynamicData* Current = Head; Current; )
	{
		FSkeletalMeshDynamicData* Next = Current->Next;
		Current->Next = nullptr;
		Lambda(Current, Next != nullptr);
		Current = Next;
	}
	Head = Tail = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

template <typename SkeletalMeshUpdatePacketType>
void FSkeletalMeshUpdateChannel::Replay(FRHICommandList& RHICmdList, SkeletalMeshUpdatePacketType& UpdatePacket)
{
	check(IsInParallelRenderingThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkeletalMeshUpdateChannel::Replay);

	using MeshObjectType      = typename SkeletalMeshUpdatePacketType::MeshObjectType;
	using MeshDynamicDataType = typename SkeletalMeshUpdatePacketType::MeshDynamicDataType;

	for (FOp Op : OpStream.Ops)
	{
		switch (Op.Type)
		{
		case FOp::EType::Add:
		{
			const int32 ExpectedSize = Op.HandleIndex + 1;
			if (SlotRegistry.Slots.Num() < ExpectedSize)
			{
				SlotRegistry.Slots.SetNum(ExpectedSize);
				SlotRegistry.SlotBits.SetNum(ExpectedSize, false);
			}
			SlotRegistry.Slots[Op.HandleIndex].MeshObject = Op.Data_Add.MeshObject;
		}
		break;
		case FOp::EType::Remove:
		{
			FSlot& Slot = SlotRegistry.Slots[Op.HandleIndex];
			Slot.UpdateList.Consume([&](FSkeletalMeshDynamicData* MeshDynamicDataBase, bool bHasNext)
			{
				UpdatePacket.Free(static_cast<MeshDynamicDataType*>(MeshDynamicDataBase));
			});
			Slot = {};
			SlotRegistry.SlotBits[Op.HandleIndex] = false;
		}
		break;
		case FOp::EType::Update:
		{
			FSlot& Slot = SlotRegistry.Slots[Op.HandleIndex];
			check(Slot.MeshObject);
			Slot.UpdateList.Add(Op.Data_Update.MeshDynamicData);
			SlotRegistry.SlotBits[Op.HandleIndex] = true;
		}
		break;
		}
	}

	for (TConstSetBitIterator<> It(SlotRegistry.SlotBits); It; ++It)
	{
		FSlot& Slot = SlotRegistry.Slots[It.GetIndex()];
		auto* MeshObject = static_cast<MeshObjectType*>(Slot.MeshObject);
		Slot.UpdateList.Consume([&] (FSkeletalMeshDynamicData* MeshDynamicDataBase, bool bHasNext)
		{
			auto* MeshDynamicData = static_cast<MeshDynamicDataType*>(MeshDynamicDataBase);
			if (bHasNext)
			{
				UpdatePacket.UpdateImmediate(RHICmdList, MeshObject, MeshDynamicData);
			}
			else
			{
				UpdatePacket.Add(MeshObject, MeshDynamicData);
			}
		});
	}

	SlotRegistry.SlotBits.Init(false, SlotRegistry.SlotBits.Num());
	OpStream = {};
}

///////////////////////////////////////////////////////////////////////////////

template <typename SkeletalMeshObjectType>
FSkeletalMeshUpdateHandle FSkeletalMeshUpdater::Create(SkeletalMeshObjectType* MeshObject)
{
	return Channels[FSkeletalMeshUpdateChannel::GetChannelIndex<SkeletalMeshObjectType>()].Create(MeshObject);
}