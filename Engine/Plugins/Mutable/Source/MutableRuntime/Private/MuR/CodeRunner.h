// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/Layout.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Settings.h"
#include "MuR/System.h"
#include "MuR/SystemPrivate.h"
#include "MuR/Types.h"
#include "Tasks/Task.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"

namespace mu::MemoryCounters
{
	struct FStreamingMemoryCounter
	{
		MUTABLERUNTIME_API static std::atomic<SSIZE_T>& Get()
		{
			static std::atomic<SSIZE_T> Counter{0};
			return Counter;
		}
	};
}

namespace  mu
{
	class FModel;
	class FParameters;
	class FRangeIndex;

    /** Code execution of the mutable virtual machine. */
    class CodeRunner : public TSharedFromThis<CodeRunner>
    {
		// The private token allows only members or friends to call MakeShared.
		struct FPrivateToken { explicit FPrivateToken() = default; };

	public:
		static TSharedRef<CodeRunner> Create(
				const FSettings&, 
				class FSystem::Private*, 
				EExecutionStrategy,
				const TSharedPtr<const FModel>&, 
				const FParameters* Params,
				OP::ADDRESS At, uint32 LodMask, uint8 ExecutionOptions, int32 InImageLOD, FScheduledOp::EType);
		// Private constructor to prevent stack allocation. In general we can not call AsShared() if the lifetime is
		// bounded.
		explicit CodeRunner(FPrivateToken, 
			const FSettings&, 
			class FSystem::Private*, 
			EExecutionStrategy,
			const TSharedPtr<const FModel>&, 
			const FParameters* Params,
			OP::ADDRESS At, uint32 LodMask, uint8 ExecutionOptions, int32 InImageLOD, FScheduledOp::EType);

    protected:
		struct FProfileContext
		{
			uint32 NumRunOps = 0;
			uint32 RunOpsPerType[int32(EOpType::COUNT)] = {};
		};

        /** Type of data sometimes stored in the code runner heap to pass info between operation stages. */
        struct FScheduledOpData
        {
			union
			{	
				struct
				{
					float Bifactor;
					int32 Min, Max;
				} Interpolate;

				struct
				{
					int32 Iterations;
					EImageFormat OriginalBaseFormat;
					bool bBlendOnlyOneMip;
				} MultiLayer;

				struct
				{
					uint8 Mip;
					float MipValue;
				} RasterMesh;

				struct
				{
					uint16 SizeX;
					uint16 SizeY;
					uint16 ScaleXEncodedHalf;
					uint16 ScaleYEncodedHalf;
					float MipValue;
				} ImageTransform;
			};

			TSharedPtr<const FResource> Resource;
        };

		// Assertion to know when FScheduledOpData size changes. It is ok to modifiy if needed. 
		static_assert(sizeof(FScheduledOpData) == 4*4+sizeof(TSharedPtr<FResource>), "FScheduledOpData size changed.");


        TSharedPtr<FRangeIndex> BuildCurrentOpRangeIndex( const FScheduledOp&, const FParameters*, const FModel*, int32 ParameterIndex );

        void RunCode( const FScheduledOp&, const FParameters*, const TSharedPtr<const FModel>&, uint32 LodMask);

        void RunCode_Conditional(const FScheduledOp&, const FModel* );
        void RunCode_Switch(const FScheduledOp&, const FModel* );
        void RunCode_Instance(const FScheduledOp&, const FModel*, uint32 LodMask);
        void RunCode_InstanceAddResource(const FScheduledOp&, const TSharedPtr<const FModel>& Model, const FParameters* );

		/** Return false incase of failure. */
        bool RunCode_ConstantResource(const FScheduledOp&, const FModel* );

        void RunCode_Mesh(const FScheduledOp&, const FModel* );
        void RunCode_Image(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Layout(const FScheduledOp&, const FModel* );
        void RunCode_Bool(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Int(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Scalar(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_String(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Colour(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Projector(const FScheduledOp&, const FParameters*, const FModel* );
        void RunCode_Matrix(const FScheduledOp&, const FParameters*, const FModel* );

		void RunCodeImageDesc(const FScheduledOp&, const FParameters*, const FModel*, uint32 LodMask);
	public:

		struct FExternalResourceId
		{
			/** If it is an image or mesh reference. */
			int32 ReferenceResourceId = -1;

			/** If it is an image or mesh parameter.*/
			FName ParameterId; 
		};

		/** Load an external image asynchronously, returns an event to wait for complition and a cleanup function that must be called once the event has completed. */
		TTuple<UE::Tasks::FTask, TFunction<void()>> LoadExternalImageAsync(FExternalResourceId Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<FImage>)>& ResultCallback);
 	    mu::FExtendedImageDesc GetExternalImageDesc(FName Id);

		/** Load an external mesh asynchronously, returns an event to wait for complition and a cleanup function that must be called once the event has completed. */
		TTuple<UE::Tasks::FTask, TFunction<void()>> LoadExternalMeshAsync(FExternalResourceId Id, int32 LODIndex, int32 SectionIndex, TFunction<void(TSharedPtr<FMesh>)>& ResultCallback);

		/** Settings that may affect the execution of some operations, like image conversion quality. */
		FSettings Settings;

    protected:
        /** 
		 * Heap of intermediate data pushed by some instructions and referred by others.
         * It is not released until no operations are pending.
		 */
		TArray<FScheduledOpData> HeapData;
		
		/** Image descriptor intermediate results. */
		TMap<OP::ADDRESS, FExtendedImageDesc> ImageDescResults;
		TArray<int32> ImageDescConstantImages; 

		/** Only used for correct mip skipping with external images. It is the LOD for which the image is build. */
		int32 ImageLOD;

		UE::Tasks::FTaskEvent RunnerCompletionEvent;

		void Run(TUniquePtr<FProfileContext>&& ProfileContext, bool bForceInlineExecution);
		void AbortRun();
	public:

		UE::Tasks::FTask StartRun(bool bForceInlineExecution);

		/** */
		const FExtendedImageDesc& GetImageDescResult(OP::ADDRESS ResultAddres);

		//!
		FProgramCache& GetMemory();


		struct FTask
		{
			FTask() {}
			FTask(const FScheduledOp& InOp) : Op(InOp) {}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0) : Op(InOp) 
			{ 
				if (InDep0.At) Deps.Add(InDep0); 
			}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1) : Op(InOp) 
			{ 
				if (InDep0.At) Deps.Add(InDep0); 
				if (InDep1.At) Deps.Add(InDep1); 
			}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2) : Op(InOp) 
			{ 
				if (InDep0.At) Deps.Add(InDep0); 
				if (InDep1.At) Deps.Add(InDep1); 
				if (InDep2.At) Deps.Add(InDep2); 
			}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2, const FScheduledOp& InDep3) : Op(InOp) 
			{ 
				if (InDep0.At) Deps.Add(InDep0); 
				if (InDep1.At) Deps.Add(InDep1); 
				if (InDep2.At) Deps.Add(InDep2); 
				if (InDep3.At) Deps.Add(InDep3); 
			}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2, const FScheduledOp& InDep3, const FScheduledOp& InDep4) : Op(InOp) 
			{ 
				if (InDep0.At) Deps.Add(InDep0); 
				if (InDep1.At) Deps.Add(InDep1); 
				if (InDep2.At) Deps.Add(InDep2); 
				if (InDep3.At) Deps.Add(InDep3); 
				if (InDep4.At) Deps.Add(InDep4); 
			}

			FScheduledOp Op;
			TArray<FCacheAddress, TInlineAllocator<3>> Deps;
		};

		class FIssuedTask
		{
		public:
			const FScheduledOp Op;

			UE::Tasks::FTask Event = {};

			FIssuedTask(const FScheduledOp& InOp) : Op(InOp) {}
			virtual ~FIssuedTask() {}

			/** */
			virtual bool Prepare(CodeRunner*, bool& bOutFailed) { bOutFailed = false; return true; }
			virtual void DoWork() {}

			/** Return true if succeeded. */
			virtual bool Complete(CodeRunner*) = 0;

			/** Return true if the task has been completed. */
			virtual bool IsComplete(CodeRunner*)
			{ 
				return !Event.IsValid() || Event.IsCompleted(); 
			}
		};

		struct FRomLoadOp
		{
			using StreamingDataContainerType = TArray<uint8, FDefaultMemoryTrackingAllocator<MemoryCounters::FStreamingMemoryCounter>>;

			int32 RomIndex = -1;
			FModelReader::FOperationID m_streamID = -1;
			StreamingDataContainerType m_streamBuffer;
			UE::Tasks::FTask Event;
		};

		class FLoadMeshRomTask : public CodeRunner::FIssuedTask
		{
		public:
			FLoadMeshRomTask( 
					const FScheduledOp& InOp, 
					int32 InFirstIndex, 
					EMeshContentFlags InRomContentFlags, 
					EMeshContentFlags InExecutionContentFlags)
				: FIssuedTask(InOp)
				, FirstIndex(InFirstIndex)
				, RomContentFlags(InRomContentFlags)
				, ExecutionContentFlags(InExecutionContentFlags)
			{
			}

			// FIssuedTask interface
			virtual bool Prepare(CodeRunner*, bool& bOutFailed) override;
			virtual bool Complete(CodeRunner*) override;

		private:

			int32 FirstIndex = -1;
			EMeshContentFlags RomContentFlags = EMeshContentFlags::None;
			EMeshContentFlags ExecutionContentFlags = EMeshContentFlags::None;

			TArray<int32, TInlineAllocator<4>> RomIndices;
		};

		class FLoadImageRomsTask : public CodeRunner::FIssuedTask
		{
		public:
			FLoadImageRomsTask(const FScheduledOp& InOp, int32 InLODIndexIndex, int32 InLODIndexCount )
				: FIssuedTask(InOp)
			{
				LODIndexIndex = InLODIndexIndex;
				LODIndexCount = InLODIndexCount;
			}

			// FIssuedTask interface
			virtual bool Prepare(CodeRunner*, bool& bOutFailed) override;
			virtual bool Complete(CodeRunner*) override;

		private:
 			int32 LODIndexIndex = -1;
			int32 LODIndexCount = -1;

			TArray<int32> RomIndices;
		};

		void AddOp(const FScheduledOp& op)
		{
			// It has no dependencies, so add it directly to the open tasks list.
			OpenTasks.Add(op);
			ScheduledStagePerOp[op] = op.Stage + 1;
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0)
		{
			ClosedTasks.Add(FTask(op, dep0));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2,
			const FScheduledOp& dep3)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2,
			const FScheduledOp& dep3,
			const FScheduledOp& dep4)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3, dep4));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
			AddChildren(dep4);
		}

		void AddOp(const FScheduledOp& Op, TArrayView<const FScheduledOp> Deps)
		{
			FTask Task(Op);

			Task.Deps.Reserve(Deps.Num());
			for (const FScheduledOp& D : Deps)
			{
				Task.Deps.Add(D);
			}

			ClosedTasks.Add(MoveTemp(Task));

			ScheduledStagePerOp[Op] = Op.Stage + 1;

			for (const FScheduledOp& D : Deps)
			{
				AddChildren(D);
			}
		}

    	/** Calculate an approximation of memory used by streaming buffers in this class. */
		int32 GetStreamingMemoryBytes() const
    	{
			return RomLoadOps.GetAllocatedSize();			
    	}
    	
		/** Calculate an approximation of memory used by manging structures in this class. */
		int32 GetInternalMemoryBytes() const
		{
			return sizeof(CodeRunner) 
				+ HeapData.GetAllocatedSize() + ImageDescResults.GetAllocatedSize()
				+ ClosedTasks.GetAllocatedSize() + OpenTasks.GetAllocatedSize() + ScheduledStagePerOp.GetAllocatedSize()
				// this contains smart pointers, approximate size like this:
				+ IssuedTasks.Max() * ( sizeof(FIssuedTask) + 16);
		}

	protected:

		/** Strategy to choose the order of execution of operations. */
		EExecutionStrategy ExecutionStrategy = EExecutionStrategy::None;

		/** If this flag is enabled, issued operation stage that use tasks will be executed in the mutable thread instead of in a generic worker thread. */
		bool bForceSerialTaskExecution = false;

		/** List of pending operations that we don't know if they cannot be run yet because of dependencies. */
		TArray< FTask > ClosedTasks;

		/** List of tasks that can be run because they don't have any unmet dependency. */
		TArray< FScheduledOp > OpenTasks;

		/** For every op, up to what stage it has been scheduled to run. */
		CodeContainer<uint8> ScheduledStagePerOp;

		/** List of tasks that are ready to run concurrently. */
		TArray< TSharedPtr<FIssuedTask> > IssuedTasksOnHold;

		/** List of tasks that have been set to run concurrently and their completion is unknown. */
		TArray< TSharedPtr<FIssuedTask> > IssuedTasks;

	public:

		inline bool LoadBool(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetBool(From);
		}

		inline float LoadInt(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetInt(From);
		}

		inline float LoadScalar(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetScalar(From);
		}

		inline FVector4f LoadColor(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetColour(From);
		}

    	inline FMatrix44f LoadMatrix(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetMatrix(From);
		}

		inline TSharedPtr<const String> LoadString(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetString(From);
		}

		inline FProjector LoadProjector(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetProjector(From);
		}

		inline TSharedPtr<const FMesh> LoadMesh(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.LoadMesh(From);
		}

		inline TSharedPtr<const FImage> LoadImage(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.LoadImage(From);
		}

		inline TSharedPtr<const FLayout> LoadLayout(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetLayout(From);
		}

		inline TSharedPtr<const FInstance> LoadInstance(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetInstance(From);
		}

		inline TSharedPtr<const FExtensionData> LoadExtensionData(const FCacheAddress& From)
		{
			return System->WorkingMemoryManager.CurrentInstanceCache->GetExtensionData(From);
		}

		inline void StoreValidDesc(const FCacheAddress& To)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetValidDesc(To);
		}

		inline void StoreBool(const FCacheAddress& To, bool Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetBool(To, Value);
		}

		inline void StoreInt(const FCacheAddress& To, int32 Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetInt(To, Value);
		}

		inline void StoreScalar(const FCacheAddress& To, float Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetScalar(To, Value);
		}

		inline void StoreString(const FCacheAddress& To, TSharedPtr<const String> Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetString(To, Value);
		}

		inline void StoreColor(const FCacheAddress& To, const FVector4f& Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetColour(To, Value);
		}

    	inline void StoreMatrix(const FCacheAddress& To, const FMatrix44f& Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetMatrix(To, Value);
		}
    	
		inline void StoreProjector(const FCacheAddress& To, const FProjector& Value)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetProjector(To, Value);
		}

		inline void StoreMesh(const FCacheAddress& To, TSharedPtr<const FMesh> Resource)
		{
			System->WorkingMemoryManager.StoreMesh(To, Resource);
		}

		inline void StoreImage(const FCacheAddress& To, TSharedPtr<const FImage> Resource)
		{
			System->WorkingMemoryManager.StoreImage(To, Resource);
		}

		inline void StoreLayout(const FCacheAddress& To, TSharedPtr<const FLayout> Resource)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetLayout(To, Resource);
		}

		inline void StoreInstance(const FCacheAddress& To, TSharedPtr<const FInstance> Resource)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetInstance(To, Resource);
		}

		inline void StoreExtensionData(const FCacheAddress& To, TSharedPtr<const FExtensionData> Resource)
		{
			System->WorkingMemoryManager.CurrentInstanceCache->SetExtensionData(To, Resource);
		}

		inline TSharedPtr<FImage> CreateImage(uint32 SizeX, uint32 SizeY, uint32 Lods, EImageFormat Format, EInitializationType Init)
		{
			TSharedPtr<FImage> Result = System->WorkingMemoryManager.CreateImage(SizeX, SizeY, Lods, Format, Init);
			return MoveTemp(Result);
		}

		TSharedPtr<FImage> CreateImageLike(const FImage* Ref, EInitializationType Init)
		{
			TSharedPtr<FImage> Result = System->WorkingMemoryManager.CreateImage(Ref->GetSizeX(), Ref->GetSizeY(), Ref->GetLODCount(), Ref->GetFormat(), Init);
			return MoveTemp(Result);
		}

		/** Ref will be nulled and relesed in any case. */
		inline TSharedPtr<FImage> CloneOrTakeOver(TSharedPtr<const FImage>& Ref)
		{
			TSharedPtr<FImage> Result = System->WorkingMemoryManager.CloneOrTakeOver(Ref);
			return MoveTemp(Result);
		}

		inline void Release(TSharedPtr<const FImage>& Resource)
		{
			return System->WorkingMemoryManager.Release(Resource);
		}

		inline void Release(TSharedPtr<FImage>& Resource)
		{
			return System->WorkingMemoryManager.Release(Resource);
		}

		[[nodiscard]] inline TSharedPtr<FMesh> CreateMesh(int32 BudgetReserveSize = 0)
		{
			return System->WorkingMemoryManager.CreateMesh(BudgetReserveSize);
		}

		[[nodiscard]] inline TSharedPtr<FMesh> CloneOrTakeOver(TSharedPtr<const FMesh>& Ref)
		{
			return System->WorkingMemoryManager.CloneOrTakeOver(Ref);
		}

		inline void Release(TSharedPtr<const FMesh>& Resource)
		{
			System->WorkingMemoryManager.Release(Resource);
		}

		inline void Release(TSharedPtr<FMesh>& Resource)
		{
			System->WorkingMemoryManager.Release(Resource);
		}

	// TODO: protect this.
	public:
		
		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

		FSystem::Private* System = nullptr;
		TSharedPtr<const FModel> Model = nullptr;
		const FParameters* Params = nullptr;
		uint32 LODMask = 0;

    private:
    	struct FRomLoadOps
    	{
   		private:
    		/** Rom read operations already in progress. */
    		TArray<FRomLoadOp> RomLoadOps;

			CodeRunner* Runner;

    	public:

			FRomLoadOps(CodeRunner& InRunner)
			{
				Runner = &InRunner;
			}

    		FRomLoadOp* Find(const int32 RomIndex)
    		{
    			for (FRomLoadOp& RomLoadOp : RomLoadOps)
    			{
    				if (RomLoadOp.RomIndex == RomIndex)
    				{
    					return &RomLoadOp;
    				}
    			}

    			return nullptr;
    		}
    	
    		FRomLoadOp& Create(const int32 RomIndex)
    		{
    			for (FRomLoadOp& RomLoadOp : RomLoadOps)
    			{
    				if (RomLoadOp.RomIndex == -1)
    				{
    					RomLoadOp.RomIndex = RomIndex;
    					return RomLoadOp;
    				}
    			}

    			FRomLoadOp& RomLoadOp = RomLoadOps.AddDefaulted_GetRef();
    			RomLoadOp.RomIndex = RomIndex;

    			return RomLoadOp;
    		}

    		void Remove(FRomLoadOp& RomLoadOp)
    		{
    			RomLoadOp.RomIndex = -1;
                RomLoadOp.m_streamBuffer.Empty();
                RomLoadOp.Event = {};
            }

    		int32 GetAllocatedSize() const
    		{
    			int32 Result = 0;
			
    			for (const FRomLoadOp& RomLoadOp : RomLoadOps)
    			{
    				Result += RomLoadOp.m_streamBuffer.GetAllocatedSize();
    			}

    			return Result;
    		}
    	};
    	
    	FRomLoadOps RomLoadOps = FRomLoadOps(*this);
    	    	
		void AddChildren(const FScheduledOp& dep);

		/** Try to create a concurrent task for the given op. Return null if not possible. */
		TSharedPtr<FIssuedTask> IssueOp(FScheduledOp item);

		/** Update debug stats. */
		void UpdateTraces();

		/** */
		bool ShouldIssueTask() const;

		/** */
		void LaunchIssuedTask(const TSharedPtr<FIssuedTask>& TaskToIssue, bool& bOutFailed);

    };


	/** Helper function to create the memory-tracked image operator. */
	extern FImageOperator MakeImageOperator(CodeRunner* Runner);

}

#if MUTABLE_DEBUG_CODERUNNER_TASK_SCHEDULE_CALLSTACK
namespace mu::Private
{
	FString DumpItemScheduledCallstack(const FScheduledOp& Item);
}
#endif

