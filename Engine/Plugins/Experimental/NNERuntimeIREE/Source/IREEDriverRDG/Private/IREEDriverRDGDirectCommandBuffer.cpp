// Copyright Epic Games, Inc. All Rights Reserved.

#include "IREEDriverRDGDirectCommandBuffer.h"

#ifdef WITH_IREE_DRIVER_RDG

#include "IREEDriverRDGBuffer.h"
#include "IREEDriverRDGBuiltinExecutables.h"
#include "IREEDriverRDGExecutable.h"
#include "IREEDriverRDGLog.h"
#include "NNERuntimeIREEShader.h"
#include "NNERuntimeIREEShaderFillBufferCS.h"
#include "NNERuntimeIREEShaderShared.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#include "ShaderParameterMetadataBuilder.h"

DECLARE_GPU_STAT_NAMED(FDirectCommandBufferDispatch, TEXT("DirectCommandBuffer.Dispatch"));

namespace UE::IREE::HAL::RDG
{

namespace Private
{

struct FBinding
{
	FRDGBufferRef Buffer = nullptr;
};

class FDirectCommandBuffer
{
public:
	static iree_status_t Create(iree_allocator_t HostAllocator, iree_hal_allocator_t* DeviceAllocator, iree_hal_command_buffer_mode_t Mode, iree_hal_command_category_t CommandCategories, iree_hal_queue_affinity_t QueueAffinity, iree_host_size_t BindingCapacity, iree_hal_command_buffer_t** OutCommandBuffer)
	{
		SCOPED_NAMED_EVENT_TEXT("FDirectCommandBuffer::Create", FColor::Magenta);

		check(OutCommandBuffer);

		FDirectCommandBuffer* CommandBuffer = nullptr;
		iree_host_size_t TotalSize = sizeof(*CommandBuffer) + iree_hal_command_buffer_validation_state_size(Mode, BindingCapacity);

		IREE_RETURN_IF_ERROR(iree_allocator_malloc(HostAllocator, TotalSize, (void**)&CommandBuffer));
		uint8_t* ValidationStatePtr = (uint8_t*)CommandBuffer + sizeof(*CommandBuffer);

		iree_hal_command_buffer_initialize(DeviceAllocator, Mode, CommandCategories, QueueAffinity, BindingCapacity, ValidationStatePtr, &FDirectCommandBuffer::VTable, &CommandBuffer->Base);
		CommandBuffer->HostAllocator = HostAllocator;
		CommandBuffer->DeviceAllocator = DeviceAllocator;

		*OutCommandBuffer = (iree_hal_command_buffer_t*)CommandBuffer;
		return iree_ok_status();
	}

private:
	static FDirectCommandBuffer* Cast(iree_hal_command_buffer_t* CommandBuffer)
	{
		checkf(iree_hal_resource_is(CommandBuffer, &FDirectCommandBuffer::VTable), TEXT("FDirectCommandBuffer: type does not match"));
		return (FDirectCommandBuffer*)CommandBuffer;
	}

	static void Destroy(iree_hal_command_buffer_t* BaseCommandBuffer)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		FDirectCommandBuffer* CommandBuffer = Cast(BaseCommandBuffer);
		iree_allocator_free(CommandBuffer->HostAllocator, CommandBuffer);
	}

	static iree_status_t Begin(iree_hal_command_buffer_t* BaseCommandBuffer)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_ok_status();
	}

	static iree_status_t End(iree_hal_command_buffer_t* BaseCommandBuffer)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_ok_status();
	}

	static void BeginDebugGroup(iree_hal_command_buffer_t* BaseCommandBuffer, iree_string_view_t Label, iree_hal_label_color_t LabelColor, const iree_hal_label_location_t* Location)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
	}

	static void EndDebugGroup(iree_hal_command_buffer_t* BaseCommandBuffer)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
	}

	static iree_status_t ExecutionBarrier(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_execution_stage_t SourceStageMask, iree_hal_execution_stage_t TargetStageMask, iree_hal_execution_barrier_flags_t Flags, iree_host_size_t MemoryBarrierCount, const iree_hal_memory_barrier_t* MemoryBarriers, iree_host_size_t BufferBarrierCount, const iree_hal_buffer_barrier_t* BufferBarriers)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_ok_status();
	}

	static iree_status_t SignalEvent(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_event_t* Event, iree_hal_execution_stage_t SourceStageMask)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t ResetEvent(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_event_t* Event, iree_hal_execution_stage_t SourceStageMask)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t WaitEvents(iree_hal_command_buffer_t* BaseCommandBuffer, iree_host_size_t EventCount, const iree_hal_event_t** Events, iree_hal_execution_stage_t SourceStageMask, iree_hal_execution_stage_t TargetStageMask, iree_host_size_t MemoryBarrierCount, const iree_hal_memory_barrier_t* MemoryBarriers, iree_host_size_t BufferBarrierCount, const iree_hal_buffer_barrier_t* BufferBarriers)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t DiscardBuffer(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_buffer_ref_t BufferRef)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t FillBuffer(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_buffer_ref_t TargetRef, const void* Pattern, iree_device_size_t PatternLength)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		SCOPED_NAMED_EVENT_TEXT("FDirectCommandBuffer::FillBuffer", FColor::Magenta);

		FDirectCommandBuffer* DirectCommandBuffer = Cast(BaseCommandBuffer);

		FRDGBuilder& GraphBuilder = DeviceAllocatorGetGraphBuilder(DirectCommandBuffer->DeviceAllocator);

		FRDGBufferRef RDGBuffer = BufferRDGBuffer(TargetRef.buffer, &GraphBuilder);
		const uint32 BufferSize = RDGBuffer->Desc.GetSize();
		const uint32 FillOffset = (uint32)TargetRef.offset;

		check(FillOffset < BufferSize);
		const uint32 FillLength = TargetRef.length == IREE_WHOLE_BUFFER ? BufferSize - FillOffset : (uint32)TargetRef.length;

		uint32 ShaderPattern = 0;

		switch (PatternLength)
		{
			case 1:
				ShaderPattern = *static_cast<const uint8*>(Pattern);
				break;
			case 2:
				ShaderPattern = *static_cast<const uint16*>(Pattern);
				break;
			case 4:
				ShaderPattern = *static_cast<const uint32*>(Pattern);
				break;
			default:
				return iree_make_status(IREE_STATUS_INVALID_ARGUMENT, "pattern length (%" PRIhsz") is not a power of two or is too large", PatternLength);
		}

		return BuiltinExecutables::AddFillBufferPass(GraphBuilder, RDGBuffer, ShaderPattern, (uint32)PatternLength, FillOffset, FillLength);
	}

	static iree_status_t UpdateBuffer(iree_hal_command_buffer_t* BaseCommandBuffer, const void* SourceBuffer, iree_host_size_t SourceOffset, iree_hal_buffer_ref_t TargetRef)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t CopyBuffer(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_buffer_ref_t SourceRef, iree_hal_buffer_ref_t TargetRef)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s s 0x%x t 0x%x"), StringCast<TCHAR>(__FUNCTION__).Get(), (uint64)SourceRef.buffer, (uint64)TargetRef.buffer);
#endif
		SCOPED_NAMED_EVENT_TEXT("FDirectCommandBuffer::CopyBuffer", FColor::Magenta);

		check(SourceRef.length == TargetRef.length);

		FDirectCommandBuffer* DirectCommandBuffer = Cast(BaseCommandBuffer);

		FRDGBuilder& GraphBuilder = DeviceAllocatorGetGraphBuilder(DirectCommandBuffer->DeviceAllocator);

		if (iree_all_bits_set(TargetRef.buffer->memory_type, IREE_HAL_MEMORY_TYPE_HOST_LOCAL))
		{
			UE_LOG(LogIREEDriverRDG, Warning, TEXT("Skipped buffer readback..."));
			return iree_ok_status();
		}
		
		checkf(iree_hal_buffer_allocated_buffer(SourceRef.buffer) == SourceRef.buffer, TEXT("Buffer spans not supported yet!"));
		checkf(iree_hal_buffer_allocated_buffer(TargetRef.buffer) == TargetRef.buffer, TEXT("Buffer spans not supported yet!"));

		FRDGBufferRef SourceRDGBuffer = BufferRDGBuffer(SourceRef.buffer, &GraphBuilder);
		FRDGBufferRef TargetRDGBuffer = BufferRDGBuffer(TargetRef.buffer, &GraphBuilder);

		if (SourceRDGBuffer == TargetRDGBuffer)
		{
			FRDGBufferRef TmpBuffer = GraphBuilder.CreateBuffer(SourceRDGBuffer->Desc, TEXT("IREE::CopyBufferTmp"));

			AddCopyBufferPass(GraphBuilder, TmpBuffer, 0, SourceRDGBuffer, SourceRef.offset, SourceRef.length);
			AddCopyBufferPass(GraphBuilder, TargetRDGBuffer, TargetRef.offset, TmpBuffer, 0, SourceRef.length);
		}
		else
		{
			AddCopyBufferPass(GraphBuilder, TargetRDGBuffer, TargetRef.offset, SourceRDGBuffer, SourceRef.offset, SourceRef.length);
		}

		return iree_ok_status();
	}

	static iree_status_t Collective(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_channel_t* Channel, iree_hal_collective_op_t Op, uint32 Param, iree_hal_buffer_ref_t SendingRef, iree_hal_buffer_ref_t ReceivingRef, iree_device_size_t ElementCount)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}

	static iree_status_t Dispatch(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_executable_t* Executable, int32 EntryPoint, const uint32 WorkgroupCount[3], iree_const_byte_span_t Constants, iree_hal_buffer_ref_list_t Bindings, iree_hal_dispatch_flags_t Flags)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		SCOPED_NAMED_EVENT_TEXT("FDirectCommandBuffer::Dispatch", FColor::Magenta);

		check(BaseCommandBuffer);
		check(Executable);
		
		FDirectCommandBuffer* DirectCommandBuffer = Cast(BaseCommandBuffer);

		FRDGBuilder& GraphBuilder = DeviceAllocatorGetGraphBuilder(DirectCommandBuffer->DeviceAllocator);

		const FNNERuntimeIREEResource* KernelResource;
		IREE_RETURN_IF_ERROR(ExecutableGetResource(Executable, EntryPoint, &KernelResource));

		const FString& KernelName = KernelResource->GetFriendlyName();
		const FShaderParametersMetadata* ShaderParameterMetadata = KernelResource->GetShaderParamMetadata();
		const TArray<FShaderParametersMetadata::FMember> &Members = ShaderParameterMetadata->GetMembers();

		FNNERuntimeIREEShader::FParameters *ShaderParameters = GraphBuilder.AllocParameters<FNNERuntimeIREEShader::FParameters>(ShaderParameterMetadata);
		uint8* ShaderParameterDataPtr = (uint8*)ShaderParameters;

		TMap<FRDGBufferRef /* Destination */, FRDGBufferRef /* Source */> BufferCopiesToAppend;

		const uint32 StructSize = ShaderParameterMetadata->GetSize();

		uint32 BufferIdx = 0;

		for (int32 i = 0; i < Members.Num(); i++)
		{
			const FShaderParametersMetadata::FMember& Member = Members[i];

			const FString MemberName(Member.GetName());

			EUniformBufferBaseType BaseType = Member.GetBaseType();
			if (Member.IsVariableNativeType())
			{
				SCOPED_NAMED_EVENT_TEXT("Constant", FColor::Magenta);

				check(MemberName == TEXT("Constant"));
				check(Constants.data);
				check(Constants.data_length > 0);
				check(Constants.data_length <= Member.GetMemberSize());
				check(Member.GetOffset() + Constants.data_length <= StructSize);

				uint8* ElementPtr = (uint8*)(ShaderParameterDataPtr + Member.GetOffset());
				FPlatformMemory::Memcpy(ElementPtr, Constants.data, Constants.data_length);
			}
			else
			{
				SCOPED_NAMED_EVENT_TEXT("Buffer", FColor::Magenta);

				check(MemberName.StartsWith(TEXT("Buffer")));
				check(BaseType == EUniformBufferBaseType::UBMT_RDG_BUFFER_UAV);

				const int32 BindingIndex = KernelResource->GetBindingIndex(BufferIdx++);
				check(BindingIndex >= 0);
				check(BindingIndex < Bindings.count);

				const iree_hal_buffer_ref_t& BufferRef = Bindings.values[BindingIndex];
				check(BufferRef.buffer != nullptr);
				check(BufferRef.offset == 0);

				checkf(iree_hal_buffer_allocated_buffer(BufferRef.buffer) == BufferRef.buffer, TEXT("Buffer spans not supported yet!"));

				FRDGBufferRef RDGBuffer = BufferRDGBuffer(Bindings.values[BindingIndex].buffer, &GraphBuilder);

				// Require byte address buffer
				FRDGBufferRef RDGWorkingBuffer = nullptr;
				if (EnumHasAnyFlags(RDGBuffer->Desc.Usage, EBufferUsageFlags::ByteAddressBuffer))
				{
					RDGWorkingBuffer = RDGBuffer;
				}
				else
				{
					auto FindResult = BufferCopiesToAppend.Find(RDGBuffer);
					if (FindResult)
					{
						RDGWorkingBuffer = *FindResult;
					}
					else
					{
						FRDGBufferDesc BufferCopyDesc = FRDGBufferDesc::CreateByteAddressDesc(RDGBuffer->Desc.GetSize());
						FRDGBufferRef BufferCopy = GraphBuilder.CreateBuffer(BufferCopyDesc, TEXT("BufferCopy"));

						AddCopyBufferPass(GraphBuilder, BufferCopy, RDGBuffer);

						RDGWorkingBuffer = BufferCopiesToAppend.Emplace(RDGBuffer, BufferCopy);
					}
				}

				check(Member.GetOffset() + sizeof(FRDGBufferUAVRef) <= StructSize);

				FRDGBufferUAVRef* ElementPtr = (FRDGBufferUAVRef*)(ShaderParameterDataPtr + Member.GetOffset());
				*ElementPtr = GraphBuilder.CreateUAV(RDGWorkingBuffer);
			}
		}

		TShaderRef<FNNERuntimeIREEShader> Shader = KernelResource->GetShader(0);
		if (Shader.IsValid())
		{
			RDG_EVENT_SCOPE_STAT(GraphBuilder, FDirectCommandBufferDispatch, "DirectCommandBuffer.Dispatch %s", *KernelName);
			RDG_GPU_STAT_SCOPE(GraphBuilder, FDirectCommandBufferDispatch);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DirectCommandBuffer.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				Shader,
				ShaderParameterMetadata,
				ShaderParameters,
				FIntVector(WorkgroupCount[0], WorkgroupCount[1], WorkgroupCount[2]));
		}
		else
		{
			UE_LOG(LogIREEDriverRDG, Warning, TEXT("%s: Missing shader for executable %s."), StringCast<TCHAR>(__FUNCTION__).Get(), *KernelName);
		}

		for (auto BufferCopyResources : BufferCopiesToAppend)
		{
			AddCopyBufferPass(GraphBuilder, BufferCopyResources.Key, BufferCopyResources.Value);
		}

		return iree_ok_status();
	}

	static iree_status_t DispatchIndirect(iree_hal_command_buffer_t* BaseCommandBuffer, iree_hal_executable_t* Executable, int32 EntryPoint, iree_hal_buffer_ref_t WorkgroupsRef, iree_const_byte_span_t Constants, iree_hal_buffer_ref_list_t Bindings, iree_hal_dispatch_flags_t Flags)
	{
#if IREE_DRIVER_RDG_VERBOSITY == 1
		UE_LOG(LogIREEDriverRDG, Display, TEXT("%s"), StringCast<TCHAR>(__FUNCTION__).Get());
#endif
		return iree_make_status(IREE_STATUS_UNIMPLEMENTED, __FUNCTION__);
	}
	
	 static const iree_hal_command_buffer_vtable_t VTable;

	iree_hal_command_buffer_t Base;
	iree_allocator_t HostAllocator;
	iree_hal_allocator_t* DeviceAllocator;
};

const iree_hal_command_buffer_vtable_t FDirectCommandBuffer::VTable = 
{
	.destroy = FDirectCommandBuffer::Destroy,
	.begin = FDirectCommandBuffer::Begin,
	.end = FDirectCommandBuffer::End,
	.begin_debug_group = FDirectCommandBuffer::BeginDebugGroup,
	.end_debug_group = FDirectCommandBuffer::EndDebugGroup,
	.execution_barrier = FDirectCommandBuffer::ExecutionBarrier,
	.signal_event = FDirectCommandBuffer::SignalEvent,
	.reset_event = FDirectCommandBuffer::ResetEvent,
	.wait_events = FDirectCommandBuffer::WaitEvents,
	.discard_buffer = FDirectCommandBuffer::DiscardBuffer,
	.fill_buffer = FDirectCommandBuffer::FillBuffer,
	.update_buffer = FDirectCommandBuffer::UpdateBuffer,
	.copy_buffer = FDirectCommandBuffer::CopyBuffer,
	.collective = FDirectCommandBuffer::Collective,
	.dispatch = FDirectCommandBuffer::Dispatch,
	.dispatch_indirect = FDirectCommandBuffer::DispatchIndirect
};

} // namespace Private

iree_status_t DirectCommandBufferCreate(iree_allocator_t HostAllocator, iree_hal_allocator_t* DeviceAllocator, iree_hal_command_buffer_mode_t Mode, iree_hal_command_category_t CommandCategories, iree_hal_queue_affinity_t QueueAffinity, iree_host_size_t BindingCapacity, iree_hal_command_buffer_t** OutCommandBuffer)
{
	return Private::FDirectCommandBuffer::Create(HostAllocator, DeviceAllocator, Mode, CommandCategories, QueueAffinity, BindingCapacity, OutCommandBuffer);
}

} // UE::IREE

#endif // WITH_IREE_DRIVER_RDG