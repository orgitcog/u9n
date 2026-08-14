// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/CodeRunner.h"

#include "OpMeshTransformWithMesh.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/Instance.h"
#include "MuR/InstancePrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableString.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpImageApplyComposite.h"
#include "MuR/OpImageBinarise.h"
#include "MuR/OpImageBlend.h"
#include "MuR/OpImageColourMap.h"
#include "MuR/OpImageDisplace.h"
#include "MuR/OpImageInterpolate.h"
#include "MuR/OpImageLuminance.h"
#include "MuR/OpImageNormalCombine.h"
#include "MuR/OpImageProject.h"
#include "MuR/OpImageRasterMesh.h"
#include "MuR/OpImageTransform.h"
#include "MuR/OpLayoutPack.h"
#include "MuR/OpLayoutRemoveBlocks.h"
#include "MuR/OpMeshApplyLayout.h"
#include "MuR/OpMeshPrepareLayout.h"
#include "MuR/OpMeshApplyPose.h"
#include "MuR/OpMeshBind.h"
#include "MuR/OpMeshClipDeform.h"
#include "MuR/OpMeshClipMorphPlane.h"
#include "MuR/OpMeshClipWithMesh.h"
#include "MuR/OpMeshDifference.h"
#include "MuR/OpMeshExtractLayoutBlock.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/OpMeshGeometryOperation.h"
#include "MuR/OpMeshMerge.h"
#include "MuR/OpMeshMorph.h"
#include "MuR/OpMeshRemove.h"
#include "MuR/OpMeshReshape.h"
#include "MuR/OpMeshTransform.h"
#include "MuR/OpMeshOptimizeSkinning.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Skeleton.h"
#include "MuR/SystemPrivate.h"
#include "Templates/Tuple.h"

#if MUTABLE_DEBUG_CODERUNNER_TASK_SCHEDULE_CALLSTACK
#include "GenericPlatform/GenericPlatformStackWalk.h"
#endif

namespace
{

int32 ForcedProjectionMode = -1;
static FAutoConsoleVariableRef CVarForceProjectionSamplingMode (
	TEXT("mutable.ForceProjectionMode"),
	ForcedProjectionMode,
	TEXT("force mutable to use an specific projection mode, 0 = Point + None, 1 = Bilinear + TotalAreaHeuristic, -1 uses the values provided by the projector."),
	ECVF_Default);

float GlobalProjectionLodBias = 0.0f;
static FAutoConsoleVariableRef CVarGlobalProjectionLodBias (
	TEXT("mutable.GlobalProjectionLodBias"),
	GlobalProjectionLodBias,
	TEXT("Lod bias applied to the lod resulting form the best mip computation for ImageProject operations, only used if a min filter method different than None is used."),
	ECVF_Default);

bool bUseProjectionVectorImpl = true;
static FAutoConsoleVariableRef CVarUseProjectionVectorImpl (
	TEXT("mutable.UseProjectionVectorImpl"),
	bUseProjectionVectorImpl,
	TEXT("If set to true, enables the vectorized implementation of the projection pixel processing."),
	ECVF_Default);

float GlobalImageTransformLodBias = 0.0f;
static FAutoConsoleVariableRef CVarGlobalImageTransformLodBias (
	TEXT("mutable.GlobalImageTransformLodBias"),
	GlobalImageTransformLodBias,
		TEXT("Lod bias applied to the lod resulting form the best mip computation for ImageTransform operations"),
	ECVF_Default);

bool bUseImageTransformVectorImpl = true;
static FAutoConsoleVariableRef CVarUseImageTransformVectorImpl (
	TEXT("mutable.UseImageTransformVectorImpl"),
	bUseImageTransformVectorImpl,
	TEXT("If set to true, enables the vectorized implementation of the image transform pixel processing."),
	ECVF_Default);
}

#if MUTABLE_DEBUG_CODERUNNER_TASK_SCHEDULE_CALLSTACK

#define AddOp(...) Invoke([&](){ AddOp(__VA_ARGS__); });

namespace mu::Private
{
	FString DumpItemScheduledCallstack(const FScheduledOp& Item)
	{
		constexpr SIZE_T MaxStringSize = 16 * 1024;
		ANSICHAR StackTrace[MaxStringSize];

		FString OutputString;

		constexpr uint32 EntriesToSkip = 3;
		for (uint32 Index = EntriesToSkip; Index < Item.StackDepth; ++Index)
		{
			StackTrace[0] = 0;
			FPlatformStackWalk::ProgramCounterToHumanReadableString(Index, Item.ScheduleCallstack[Index], StackTrace, MaxStringSize, nullptr);
			OutputString += FString::Printf(TEXT("\t\t%d %s\n"), Index - EntriesToSkip, ANSI_TO_TCHAR(StackTrace));
		}

		return OutputString;
	}
}

#endif

namespace mu
{

	TSharedRef<CodeRunner> CodeRunner::Create(
		const FSettings& InSettings,
		class FSystem::Private* InSystem,
		EExecutionStrategy InExecutionStrategy,
		const TSharedPtr<const FModel>& InModel,
		const FParameters* InParams,
		OP::ADDRESS At,
		uint32 InLODMask, uint8 ExecutionOptions, int32 InImageLOD, FScheduledOp::EType Type)
	{
		return MakeShared<CodeRunner>(FPrivateToken {},
				InSettings, InSystem, InExecutionStrategy, InModel, InParams, At, InLODMask, ExecutionOptions, InImageLOD, Type);
	}

    CodeRunner::CodeRunner(FPrivateToken PrivateToken, 
		const FSettings& InSettings,
		class FSystem::Private* InSystem,
		EExecutionStrategy InExecutionStrategy,
		const TSharedPtr<const FModel>& InModel,
		const FParameters* InParams,
		OP::ADDRESS At,
		uint32 InLodMask, uint8 ExecutionOptions, int32 InImageLOD, FScheduledOp::EType Type )
		: Settings(InSettings)
		, RunnerCompletionEvent(TEXT("CodeRunnerCompletioneEventInit"))
		, ExecutionStrategy(InExecutionStrategy)
		, System(InSystem)
		, Model(InModel)
		, Params(InParams)
		, LODMask(InLodMask)
	{
		MUTABLE_CPUPROFILER_SCOPE(CodeRunner_Create)
		const FProgram& Program = Model->GetPrivate()->Program;
		ScheduledStagePerOp.resize(Program.OpAddress.Num());

		
		if (Type == FScheduledOp::EType::ImageDesc)
		{
			ImageDescResults.Reserve(64);
			ImageDescConstantImages.Reserve(32);
		}
	
		// We will read this in the end, so make sure we keep it.
   		if (Type == FScheduledOp::EType::Full)
   		{
			GetMemory().IncreaseHitCount(FCacheAddress(At, 0, ExecutionOptions));
		}
    
		// Start with a completed Event. This is checked at StartRun() to make sure StartRun is not called while there is 
		// a Run in progress.
		RunnerCompletionEvent.Trigger();

		ImageLOD = InImageLOD;
	
		// Push the root operation
		FScheduledOp RootOp;
		RootOp.At = At;
		RootOp.ExecutionOptions = ExecutionOptions;
		RootOp.Type = Type;
		AddOp(RootOp);
	}


	FProgramCache& CodeRunner::GetMemory()
    {
		return *System->WorkingMemoryManager.CurrentInstanceCache;
	}


	TTuple<UE::Tasks::FTask, TFunction<void()>> CodeRunner::LoadExternalImageAsync(FExternalResourceId Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<FImage>)>& ResultCallback)
    {
		MUTABLE_CPUPROFILER_SCOPE(LoadExternalImageAsync);

		check(System);

		if (System->ExternalResourceProvider)
		{
			if (Id.ReferenceResourceId < 0)
			{
				// It's a parameter image
				return System->ExternalResourceProvider->GetImageAsync(Id.ParameterId, MipmapsToSkip, ResultCallback);
			}
			else
			{
				// It's an image reference
				return System->ExternalResourceProvider->GetReferencedImageAsync(Model.Get(), Id.ReferenceResourceId, MipmapsToSkip, ResultCallback);
			}
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	}


	TTuple<UE::Tasks::FTask, TFunction<void()>> CodeRunner::LoadExternalMeshAsync(FExternalResourceId Id, int32 LODIndex, int32 SectionIndex, TFunction<void(TSharedPtr<FMesh>)>& ResultCallback)
	{
		MUTABLE_CPUPROFILER_SCOPE(LoadExternalImageAsync);

		check(System);

		if (System->ExternalResourceProvider)
		{
			if (Id.ReferenceResourceId < 0)
			{
				// It's a parameter mesh
				return System->ExternalResourceProvider->GetMeshAsync(Id.ParameterId, LODIndex, SectionIndex, ResultCallback);
			}
			else
			{
				// It's a mesh reference
				check(false);
			}
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	}

	
	FExtendedImageDesc CodeRunner::GetExternalImageDesc(FName Id)
	{
		MUTABLE_CPUPROFILER_SCOPE(GetExternalImageDesc);

		check(System);

		if (System->ExternalResourceProvider)
		{
			return System->ExternalResourceProvider->GetImageDesc(Id);
		}
		else
		{
			// Not found and there is no generator!
			check(false);
		}

		return FExtendedImageDesc();
	}

	
    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Conditional( const FScheduledOp& Item, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Conditional);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
		OP::ConditionalArgs Args = Program.GetOpArgs<OP::ConditionalArgs>(Item.At);

        // Conditionals have the following execution stages:
        // 0: we need to run the condition
        // 1: we need to run the branch
        // 2: we need to fetch the result and store it in this op
        switch( Item.Stage )
        {
        case 0:
        {
            AddOp( FScheduledOp( Item.At,Item,1 ),
                   FScheduledOp( Args.condition, Item ) );
            break;
        }

        case 1:
        {
            // Get the condition result

            // If there is no expression, we'll assume true.
            bool value = true;
            value = LoadBool( FCacheAddress(Args.condition, Item.ExecutionIndex, Item.ExecutionOptions) );

            OP::ADDRESS resultAt = value ? Args.yes : Args.no;

            // Schedule the end of this instruction if necessary
            AddOp( FScheduledOp( Item.At, Item, 2, (uint32)value),
				FScheduledOp( resultAt, Item) );

            break;
        }

        case 2:
        {
            OP::ADDRESS resultAt = Item.CustomState ? Args.yes : Args.no;

            // Store the final result
            FCacheAddress cat( Item );
            FCacheAddress rat( resultAt, Item );
            switch (GetOpDataType(type))
            {
            case EDataType::Bool:			StoreBool( cat, LoadBool(rat) ); break;
            case EDataType::Int:			StoreInt( cat, LoadInt(rat) ); break;
            case EDataType::Scalar:			StoreScalar( cat, LoadScalar(rat) ); break;
			case EDataType::String:			StoreString( cat, LoadString( rat ) ); break;
            case EDataType::Color:			StoreColor( cat, LoadColor( rat ) ); break;
            case EDataType::Projector:		StoreProjector( cat, LoadProjector(rat) ); break;
            case EDataType::Mesh:			StoreMesh( cat, LoadMesh(rat) ); break;
            case EDataType::Image:			StoreImage( cat, LoadImage(rat) ); break;
            case EDataType::Layout:			StoreLayout( cat, LoadLayout(rat) ); break;
            case EDataType::Instance:		StoreInstance( cat, LoadInstance(rat) ); break;
			case EDataType::ExtensionData:	StoreExtensionData( cat, LoadExtensionData(rat) ); break;
            default:
                // Not implemented
                check( false );
            }

            break;
        }

        default:
            check(false);
        }
    }


	//---------------------------------------------------------------------------------------------
	void CodeRunner::RunCode_Switch(const FScheduledOp& Item, const FModel* InModel )
	{
		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);

		const uint8* data = Program.GetOpArgsPointer(Item.At);

		OP::ADDRESS VarAddress;
		FMemory::Memcpy(&VarAddress, data, sizeof(OP::ADDRESS));
		data += sizeof(OP::ADDRESS);

		OP::ADDRESS DefAddress;
		FMemory::Memcpy(&DefAddress, data, sizeof(OP::ADDRESS));
		data += sizeof(OP::ADDRESS);

		uint32 CaseCount;
		FMemory::Memcpy(&CaseCount, data, sizeof(uint32));
		data += sizeof(uint32);

		switch (Item.Stage)
		{
		case 0:
		{
			if (VarAddress)
			{
				AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp(VarAddress, Item));
			}
			else
			{
				switch (GetOpDataType(type))
				{
				case EDataType::Bool:			StoreBool(Item, false); break;
				case EDataType::Int:			StoreInt(Item, 0); break;
				case EDataType::Scalar:			StoreScalar(Item, 0.0f); break;
				case EDataType::String:			StoreString(Item, nullptr); break;
				case EDataType::Color:			StoreColor(Item, FVector4f()); break;
				case EDataType::Projector:		StoreProjector(Item, FProjector()); break;
				case EDataType::Mesh:			StoreMesh(Item, nullptr); break;
				case EDataType::Image:			StoreImage(Item, nullptr); break;
				case EDataType::Layout:			StoreLayout(Item, nullptr); break;
				case EDataType::Instance:		StoreInstance(Item, nullptr); break;
				case EDataType::ExtensionData:	StoreExtensionData(Item, MakeShared<FExtensionData>()); break;
				default:
					// Not implemented
					check(false);
				}
			}
			break;
		}

		case 1:
		{
			// Get the variable result
			int32 var = LoadInt(FCacheAddress(VarAddress, Item));

			OP::ADDRESS valueAt = DefAddress;
			for (uint32 C = 0; C < CaseCount; ++C)
			{
				int32 Condition;
				FMemory::Memcpy(&Condition, data, sizeof(int32));
				data += sizeof(int32);

				OP::ADDRESS At;
				FMemory::Memcpy(&At, data, sizeof(OP::ADDRESS));
				data += sizeof(OP::ADDRESS);

				if (At && var == (int32)Condition)
				{
					valueAt = At;
					break; 
				}
			}

            // Schedule the end of this instruction if necessary
            AddOp( FScheduledOp( Item.At, Item, 2, valueAt ),
				   FScheduledOp( valueAt, Item ) );

            break;
        }

        case 2:
        {
			OP::ADDRESS resultAt = OP::ADDRESS(Item.CustomState);

            // Store the final result
            FCacheAddress cat( Item );
            FCacheAddress rat( resultAt, Item );
            switch (GetOpDataType(type))
            {
            case EDataType::Bool:			StoreBool( cat, LoadBool(rat) ); break;
            case EDataType::Int:			StoreInt( cat, LoadInt(rat) ); break;
            case EDataType::Scalar:			StoreScalar( cat, LoadScalar(rat) ); break;
            case EDataType::String:			StoreString( cat, LoadString( rat ) ); break;
            case EDataType::Color:			StoreColor( cat, LoadColor( rat ) ); break;
            case EDataType::Projector:		StoreProjector( cat, LoadProjector(rat) ); break;
			case EDataType::Mesh:			StoreMesh( cat, LoadMesh(rat) ); break;
            case EDataType::Image:			StoreImage( cat, LoadImage(rat) ); break;
            case EDataType::Layout:			StoreLayout( cat, LoadLayout(rat) ); break;
            case EDataType::Instance:		StoreInstance( cat, LoadInstance(rat) ); break;
			case EDataType::ExtensionData:	StoreExtensionData( cat, LoadExtensionData(rat) ); break;
            default:
                // Not implemented
                check( false );
            }

            break;
        }

        default:
            check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Instance(const FScheduledOp& Item, const FModel* InModel, uint32 lodMask )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Instance);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::IN_ADDVECTOR:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);

            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.instance, Item),
                           FScheduledOp( Args.value, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );
				TSharedPtr<FInstance> Result;
                if (!Base)
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
					Result = mu::CloneOrTakeOver<FInstance>(Base);
                }

                if ( Args.value )
                {
					FVector4f value = LoadColor( FCacheAddress(Args.value,Item) );

                    OP::ADDRESS nameAd = Args.name;
                    check(  nameAd < (uint32)Program.ConstantStrings.Num() );
                    const FString& Name = Program.ConstantStrings[ nameAd ];

                    Result->GetPrivate()->AddVector( 0, 0, 0, value, FName(Name) );
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IN_ADDSCALAR:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.instance, Item),
                           FScheduledOp( Args.value, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );
                TSharedPtr<FInstance> Result;
                if (!Base)
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
                    Result = mu::CloneOrTakeOver<FInstance>(Base);
                }

                if ( Args.value )
                {
                    float value = LoadScalar( FCacheAddress(Args.value,Item) );

                    OP::ADDRESS nameAd = Args.name;
                    check(  nameAd < (uint32)Program.ConstantStrings.Num() );
                    const FString& Name = Program.ConstantStrings[ nameAd ];

                    Result->GetPrivate()->AddScalar( 0, 0, 0, value, FName(Name));
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IN_ADDSTRING:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>( Item.At );
            switch ( Item.Stage )
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1 ), FScheduledOp( Args.instance, Item ),
                           FScheduledOp( Args.value, Item ) );
                break;

            case 1:
            {
                TSharedPtr<const FInstance> Base =
                    LoadInstance( FCacheAddress( Args.instance, Item ) );
                TSharedPtr<FInstance> Result;
                if ( !Base )
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}

                if ( Args.value )
                {
                    TSharedPtr<const String> value = LoadString( FCacheAddress( Args.value, Item ) );

                    OP::ADDRESS nameAd = Args.name;
                    check( nameAd < (uint32)Program.ConstantStrings.Num() );
                    const FString& Name = Program.ConstantStrings[nameAd];

                    Result->GetPrivate()->AddString( 0, 0, 0, value->GetValue(), FName(Name) );
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check( false );
            }

            break;
        }

        case EOpType::IN_ADDCOMPONENT:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.instance, Item),
                           FScheduledOp( Args.value, Item) );
                break;

            case 1:
            {
				TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );
				TSharedPtr<FInstance> Result;
                if (!Base)
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}

                if ( Args.value )
                {
                    TSharedPtr<const FInstance> pComp = LoadInstance( FCacheAddress(Args.value,Item) );

                    int32 NewComponentIndex = Result->GetPrivate()->AddComponent();

                    if ( !pComp->GetPrivate()->Components.IsEmpty() )
                    {
                        Result->GetPrivate()->Components[NewComponentIndex] = pComp->GetPrivate()->Components[0];

						// Id
                    	Result->GetPrivate()->Components[NewComponentIndex].Id = Args.ExternalId;
                    }
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IN_ADDSURFACE:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.instance, Item),
                           FScheduledOp( Args.value, Item) );
                break;

            case 1:
            {
				TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );

				TSharedPtr<FInstance> Result;
				if (Base)
				{
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}
				else
				{
					Result = MakeShared<FInstance>();
				}

                // Empty surfaces are ok, they still need to be created, because they may contain
                // additional information like internal or external IDs
                //if ( Args.value )
                {
					TSharedPtr<const FInstance> pSurf = LoadInstance( FCacheAddress(Args.value,Item) );

                    int32 sindex = Result->GetPrivate()->AddSurface( 0, 0 );

                    // Surface data
                    if (pSurf
                            &&
                            pSurf->GetPrivate()->Components.Num()
                            &&
                            pSurf->GetPrivate()->Components[0].LODs.Num()
                            &&
                            pSurf->GetPrivate()->Components[0].LODs[0].Surfaces.Num())
                    {
                        Result->GetPrivate()->Components[0].LODs[0].Surfaces[sindex] =
                            pSurf->GetPrivate()->Components[0].LODs[0].Surfaces[0];
                    }

                    // Name
                    OP::ADDRESS nameAd = Args.name;
                    check( nameAd < (uint32)Program.ConstantStrings.Num() );
                    const FString& Name = Program.ConstantStrings[ nameAd ];
                    Result->GetPrivate()->SetSurfaceName( 0, 0, sindex, FName(Name) );

                    // IDs
                    Result->GetPrivate()->Components[0].LODs[0].Surfaces[sindex].InternalId = Args.id;
                    Result->GetPrivate()->Components[0].LODs[0].Surfaces[sindex].ExternalId = Args.ExternalId;
                    Result->GetPrivate()->Components[0].LODs[0].Surfaces[sindex].SharedId = Args.SharedSurfaceId;
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IN_ADDLOD:
        {
			const uint8* Data = Program.GetOpArgsPointer(Item.At);

			uint8 LODCount;
			FMemory::Memcpy(&LODCount, Data, sizeof(uint8));
			Data += sizeof(uint8);

			switch (Item.Stage)
            {
            case 0:
            {                
                TArray<FScheduledOp> deps;
                for (uint8 LODIndex=0; LODIndex <LODCount; ++LODIndex)
                {
					OP::ADDRESS LODAddress;
					FMemory::Memcpy(&LODAddress, Data, sizeof(OP::ADDRESS));
					Data += sizeof(OP::ADDRESS);

                    if (LODAddress)
                    {
                        bool bSelectedLod = ( (1<< LODIndex) & lodMask ) != 0;

                        if (bSelectedLod)
                        {
                            deps.Emplace(LODAddress, Item);
                        }
                    }
                }

                AddOp( FScheduledOp( Item.At, Item, 1), deps );

                break;
            }

            case 1:
            {
                // Assemble result
				TSharedPtr<FInstance> Result = MakeShared<FInstance>();
				int32 ComponentIndex = Result->GetPrivate()->AddComponent();

                for (uint8 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
                {
					OP::ADDRESS LODAddress;
					FMemory::Memcpy(&LODAddress, Data, sizeof(OP::ADDRESS));
					Data += sizeof(OP::ADDRESS);
					
					if ( LODAddress )
                    {						
						bool bIsSelectedLod = ( (1<<LODIndex) & lodMask ) != 0;

						// Add an empty LOD even if not selected.
						int32 InstanceLODIndex = Result->GetPrivate()->AddLOD(ComponentIndex);
						
						if (bIsSelectedLod)
                        {
							TSharedPtr<const FInstance> pLOD = LoadInstance( FCacheAddress(LODAddress,Item) );

                            // In a degenerated case, the returned pLOD may not have an LOD inside
 							if (!pLOD->GetPrivate()->Components.IsEmpty()
								&&
								!pLOD->GetPrivate()->Components[0].LODs.IsEmpty())
							{
								Result->GetPrivate()->Components[ComponentIndex].LODs[InstanceLODIndex] = pLOD->GetPrivate()->Components[0].LODs[0];
							}
						}
                    }
                }

                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::IN_ADDEXTENSIONDATA:
		{
			OP::InstanceAddExtensionDataArgs Args = Program.GetOpArgs<OP::InstanceAddExtensionDataArgs>(Item.At);
			switch (Item.Stage)
			{
				case 0:
				{
					// Must pass in an Instance op and FExtensionData op
					check(Args.Instance);
					check(Args.ExtensionData);

					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Instance, Item),
						FScheduledOp(Args.ExtensionData, Item));

					break;
				}

				case 1:
				{
					// Assemble result
					TSharedPtr<const FInstance> InstanceOpResult = LoadInstance(FCacheAddress(Args.Instance, Item));

					TSharedPtr<FInstance> Result = mu::CloneOrTakeOver<FInstance>(InstanceOpResult);

					if (TSharedPtr<const FExtensionData> ExtensionData = LoadExtensionData(FCacheAddress(Args.ExtensionData, Item)))
					{
						const OP::ADDRESS NameAddress = Args.ExtensionDataName;
						check(NameAddress < (uint32)Program.ConstantStrings.Num());
						const FString& NameString = Program.ConstantStrings[NameAddress];

						Result->GetPrivate()->AddExtensionData(ExtensionData, FName(NameString) );
					}

					StoreInstance(Item, Result);
					break;
				}

				default:
					check(false);
			}
			
			break;
		}
		case EOpType::IN_ADDOVERLAYMATERIAL:
		{
			OP::InstanceAddOverlayMaterialArgs Args = Program.GetOpArgs<OP::InstanceAddOverlayMaterialArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				// Must pass in a Material op
				check(Args.OverlayMaterialId);

				AddOp(FScheduledOp(Item.At, Item, 1),
					FScheduledOp(Args.Instance, Item),
					FScheduledOp(Args.OverlayMaterialId, Item));

				break;
			}

			case 1:
			{
				TSharedPtr<const FInstance> Base = LoadInstance(FCacheAddress(Args.Instance, Item));
				TSharedPtr<FInstance> Result;
				if (!Base)
				{
					Result = MakeShared<FInstance>();
				}
				else
				{
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}

				Result->GetPrivate()->AddOverlayMaterial(0, LoadScalar(FCacheAddress(Args.OverlayMaterialId, Item)));

				StoreInstance(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        default:
                check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_InstanceAddResource(const FScheduledOp& Item, const TSharedPtr<const FModel>& InModel, const FParameters* InParams )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_InstanceAddResource);

		if (!InModel || !System)
		{
			return;
		}

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::IN_ADDMESH:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
				// We don't build the resources when building instance: just store ids for them.
				AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.instance, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );
                TSharedPtr<FInstance> Result;
                if (!Base)
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}

                if ( Args.value )
                {
					FResourceID MeshId = System->WorkingMemoryManager.GetResourceKey(InModel,InParams,Args.relevantParametersListIndex, Args.value);
					OP::ADDRESS NameAd = Args.name;
					check(NameAd < (uint32)Program.ConstantStrings.Num());
					const FString& Name = Program.ConstantStrings[NameAd];
					Result->GetPrivate()->SetMesh(0, 0, MeshId, FName(Name));
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case EOpType::IN_ADDIMAGE:
        {
			OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
				// We don't build the resources when building instance: just store ids for them.
				AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.instance, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FInstance> Base = LoadInstance( FCacheAddress(Args.instance,Item) );
                TSharedPtr<FInstance> Result;
                if (!Base)
                {
                    Result = MakeShared<FInstance>();
                }
                else
                {
					Result = mu::CloneOrTakeOver<FInstance>(Base);
				}

                if ( Args.value )
                {
					FResourceID ImageId = System->WorkingMemoryManager.GetResourceKey(InModel, InParams, Args.relevantParametersListIndex, Args.value);
					OP::ADDRESS NameAd = Args.name;
					check(NameAd < (uint32)Program.ConstantStrings.Num());
					const FString& Name = Program.ConstantStrings[NameAd];
					Result->GetPrivate()->AddImage(0, 0, 0, ImageId, FName(Name) );
                }
                StoreInstance( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        default:
			check(false);
        }
    }


    //---------------------------------------------------------------------------------------------
    bool CodeRunner::RunCode_ConstantResource(const FScheduledOp& Item, const FModel* InModel)
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Constant);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::ME_CONSTANT:
        {
			OP::MeshConstantArgs Args = Program.GetOpArgs<OP::MeshConstantArgs>(Item.At);
			EMeshContentFlags MeshContentFlags = (EMeshContentFlags)Item.ExecutionOptions;

			TSharedPtr<const FMesh> Source;
			Program.GetConstant(Args.Value, Args.Skeleton, Args.PhysicsBody, Source, MeshContentFlags, 
					[this](int32 BudgetReserve){ return CreateMesh(BudgetReserve); });
			
			if (!Source)
			{
				return false;
			}

            StoreMesh(Item, Source);

			//UE_LOG(LogMutableCore, Log, TEXT("Set mesh constant %d."), Item.At);
			break;
        }

        case EOpType::IM_CONSTANT:
        {
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>(Item.At);
            OP::ADDRESS cat = Args.value;

			int32 MipsToSkip = Item.ExecutionOptions;
            TSharedPtr<const FImage> Source;
			Program.GetConstant(cat, Source, MipsToSkip, [this](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i)
				{
					return CreateImage(x, y, m, f, i);
				});

			// Assume the ROM has been loaded previously in a task generated at IssueOp
			if (!Source)
			{
				return false;
			}

            StoreImage( Item, Source );
			//UE_LOG(LogMutableCore, Log, TEXT("Set image constant %d."), Item.At);
			break;
        }

		case EOpType::ED_CONSTANT:
		{
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>(Item.At);

			// Assume the ROM has been loaded previously
			TSharedPtr<const mu::FExtensionData> SourceConst;
			Program.GetExtensionDataConstant(Args.value, SourceConst);

			check(SourceConst);

            StoreExtensionData(Item, SourceConst);
            break;
		}

        default:
            if (type!=EOpType::NONE)
            {
                // Operation not implemented
                check( false );
            }
            break;
        }

		// Success
		return true;
    }

	void CodeRunner::RunCode_Mesh(const FScheduledOp& Item, const FModel* InModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Mesh);

		check((EMeshContentFlags)Item.ExecutionOptions != EMeshContentFlags::None); 

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);

        switch (type)
        {

		case EOpType::ME_REFERENCE:
		{
			OP::ResourceReferenceArgs Args = Program.GetOpArgs<OP::ResourceReferenceArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				TSharedPtr<FMesh> Result;
				if (Args.ForceLoad)
				{
					// This should never be reached because it should have been caught as a Task in IssueOp
					check(false);
				}
				else
				{
					Result = FMesh::CreateAsReference(Args.ID, false);
				}
				StoreMesh(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_APPLYLAYOUT:
        {
			OP::MeshApplyLayoutArgs Args = Program.GetOpArgs<OP::MeshApplyLayoutArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Mesh, Item));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Mesh, Item),
						FScheduledOp(Args.Layout, Item));
				}
				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYLAYOUT)
            		
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(Args.Mesh, Item));
					StoreMesh(Item, Base);
				}
				else
				{
					TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(Args.Mesh, Item));
					TSharedPtr<const FLayout> Layout = LoadLayout(FCacheAddress(Args.Layout, Item));

					if (Base)
					{
						TSharedPtr<FMesh> Result = CloneOrTakeOver(Base);

						int32 texCoordsSet = Args.Channel;

						MeshApplyLayout(Result.Get(), Layout.Get(), texCoordsSet);
						
						StoreMesh(Item, Result);
					}
					else
					{
						StoreMesh(Item, nullptr);
					}
				}

                break;
            }

            default:
                check(false);
            }

            break;
        }


		case EOpType::ME_PREPARELAYOUT:
		{
			OP::MeshPrepareLayoutArgs Args = Program.GetOpArgs<OP::MeshPrepareLayoutArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Mesh, Item));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Mesh, Item),
						FScheduledOp(Args.Layout, Item));
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_PREPARELAYOUT);

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(Args.Mesh, Item));
					StoreMesh(Item, Base);
				}
				else
				{
					TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(Args.Mesh, Item));
					TSharedPtr<const FLayout> Layout = LoadLayout(FCacheAddress(Args.Layout, Item));

					if (Base && Layout)
					{
						TSharedPtr<FMesh> Result = CloneOrTakeOver(Base);

						MeshPrepareLayout(*Result, *Layout, Args.LayoutChannel, Args.bNormalizeUVs, Args.bClampUVIslands, Args.bEnsureAllVerticesHaveLayoutBlock, Args.bUseAbsoluteBlockIds);

						StoreMesh(Item, Result);
					}
					else
					{
						StoreMesh(Item, Base);
					}
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_DIFFERENCE:
        {
			const uint8* data = Program.GetOpArgsPointer(Item.At);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

            switch (Item.Stage)
            {
            case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
                	TSharedPtr<const FMesh> Result = CreateMesh();
					StoreMesh(Item, Result);
				}
				else
				{
					if (BaseAt && TargetAt)
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(BaseAt, Item),
							FScheduledOp(TargetAt, Item));
					}
					else
					{
						StoreMesh(Item, nullptr);
					}
				}
				break;
			}
            case 1:
            {
       	        MUTABLE_CPUPROFILER_SCOPE(ME_DIFFERENCE)

				check(EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))

				TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(BaseAt,Item));
				TSharedPtr<const FMesh> pTarget = LoadMesh(FCacheAddress(TargetAt,Item));

				TArray<EMeshBufferSemantic, TInlineAllocator<8>> Semantics;
				TArray<int32, TInlineAllocator<8>> SemanticIndices;

				uint8 bIgnoreTextureCoords = 0;
				FMemory::Memcpy(&bIgnoreTextureCoords, data, sizeof(uint8)); 
				data += sizeof(uint8);

				uint8 NumChannels = 0;
				FMemory::Memcpy(&NumChannels, data, sizeof(uint8)); 
				data += sizeof(uint8);

				for (uint8 i = 0; i < NumChannels; ++i)
				{
					uint8 Semantic = 0;
					FMemory::Memcpy(&Semantic, data, sizeof(uint8)); 
					data += sizeof(uint8);
					
					uint8 SemanticIndex = 0;
					FMemory::Memcpy(&SemanticIndex, data, sizeof(uint8)); 
					data += sizeof(uint8);

					Semantics.Add(EMeshBufferSemantic(Semantic));
					SemanticIndices.Add(SemanticIndex);
				}

				TSharedPtr<FMesh> Result = CreateMesh();
				bool bOutSuccess = false;
				MeshDifference(Result.Get(), Base.Get(), pTarget.Get(),
							   NumChannels, Semantics.GetData(), SemanticIndices.GetData(),
							   bIgnoreTextureCoords != 0, bOutSuccess);
				Release(Base);
				Release(pTarget);

				StoreMesh(Item, Result);

				break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_MORPH:
        {
			const uint8* Data = Program.GetOpArgsPointer(Item.At);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, Data, sizeof(OP::ADDRESS)); 
			Data += sizeof(OP::ADDRESS);
			
			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, Data, sizeof(OP::ADDRESS)); 
			Data += sizeof(OP::ADDRESS);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, Data, sizeof(OP::ADDRESS)); 
			Data += sizeof(OP::ADDRESS);

			switch (Item.Stage)
            {
            case 0:
			{
				if (BaseAt)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(BaseAt, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(FactorAt, Item));
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				
                break;
			}
            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(ME_MORPH_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
                	TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(BaseAt, Item));
					StoreMesh(Item, Base);
				}
				else	
				{
					float Factor = LoadScalar(FCacheAddress(FactorAt, Item));

					// Factor goes from -1 to 1 across all targets. [0 - 1] represents positive morphs, while [-1, 0) represent negative morphs.
					Factor = FMath::Clamp(Factor, -1.0f, 1.0f); // Is the factor not in range [-1, 1], it will index a non existing morph.

					FScheduledOpData OpHeapData;
					OpHeapData.Interpolate.Bifactor = Factor;
					uint32 DataAddress = uint32(HeapData.Add(OpHeapData));

					// No morph
					if (FMath::IsNearlyZero(Factor))
					{                        
						AddOp(FScheduledOp(Item.At, Item, 2, DataAddress),
							FScheduledOp(BaseAt, Item));
					}
					// The Morph, partial or full
					else
					{
						// We will need the base again
						AddOp(FScheduledOp(Item.At, Item, 2, DataAddress),
							FScheduledOp(BaseAt, Item),
							FScheduledOp(TargetAt, Item));
					}
				}
                break;
            }

            case 2:
            {
       		    MUTABLE_CPUPROFILER_SCOPE(ME_MORPH_2)
				check(EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				
				TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(BaseAt, Item));

				// Factor from 0 to 1 between the two targets
				const FScheduledOpData& OpHeapData = HeapData[Item.CustomState];
				float Factor = OpHeapData.Interpolate.Bifactor;

				if (BaseMesh)
				{
					// No morph
					if (FMath::IsNearlyZero(Factor))
					{
						StoreMesh(Item, BaseMesh);
					}
					// The Morph, partial or full
					else 
					{
						TSharedPtr<const FMesh> MorphMesh = LoadMesh(FCacheAddress(TargetAt, Item));
						
						if (MorphMesh)
						{
							TSharedPtr<FMesh> Result = CloneOrTakeOver(BaseMesh);
							MeshMorph(Result.Get(), MorphMesh.Get(), Factor);

							Release(MorphMesh);
							StoreMesh(Item, Result);
						}
						else
						{
							StoreMesh(Item, BaseMesh);
						}
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_MERGE:
        {
			OP::MeshMergeArgs Args = Program.GetOpArgs<OP::MeshMergeArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1),
					FScheduledOp(Args.Base, Item),
					FScheduledOp(Args.Added, Item));
				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_MERGE_1)

                TSharedPtr<const FMesh> pA = LoadMesh(FCacheAddress(Args.Base, Item));
                TSharedPtr<const FMesh> pB = LoadMesh(FCacheAddress(Args.Added, Item));

                if (pA && pB && pA->GetVertexCount() && pB->GetVertexCount())
                {
					check(!pA->IsReference() && !pB->IsReference());

					FMeshMergeScratchMeshes Scratch;
					Scratch.FirstReformat = CreateMesh();
					Scratch.SecondReformat = CreateMesh();

					TSharedPtr<FMesh> Result = CreateMesh(pA->GetDataSize() + pB->GetDataSize());

					MeshMerge(Result.Get(), pA.Get(), pB.Get(), !Args.NewSurfaceID, Scratch);

					Release(Scratch.FirstReformat);
					Release(Scratch.SecondReformat);

                    if (Args.NewSurfaceID)
                    {
						check(pB->GetSurfaceCount() == 1);
						Result->Surfaces.Last().Id = Args.NewSurfaceID;
                    }

					Release(pA);
					Release(pB);
					StoreMesh(Item, Result);
                }
                else if (pA && (pA->GetVertexCount() || pA->IsReference()))
                {
					Release(pB);
					StoreMesh(Item, pA);
                }
                else if (pB && (pB->GetVertexCount() || pB->IsReference()))
                {
					TSharedPtr<FMesh> Result = CloneOrTakeOver(pB);

                    check(Result->IsReference() || (Result->GetSurfaceCount() == 1) );

                    if (Result->GetSurfaceCount() > 0 && Args.NewSurfaceID)
                    {
                        Result->Surfaces.Last().Id = Args.NewSurfaceID;
                    }

					Release(pA);
					StoreMesh(Item, Result);
                }
                else
                {
					Release(pA);
					Release(pB);
					StoreMesh(Item, CreateMesh());
                }

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_MASKCLIPMESH:
        {
			OP::MeshMaskClipMeshArgs Args = Program.GetOpArgs<OP::MeshMaskClipMeshArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.source, Item));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.source, Item),
						FScheduledOp(Args.clip, Item));
				}
				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_MASKCLIPMESH_1)
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source, Item));
					StoreMesh(Item, Source);
				}
				else
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source, Item));
					TSharedPtr<const FMesh> pClip = LoadMesh(FCacheAddress(Args.clip, Item));

					// Only if both are valid.
					if (Source.Get() && pClip.Get())
					{
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MeshMaskClipMesh(Result.Get(), Source.Get(), pClip.Get(), bOutSuccess);
						
						Release(Source);
						Release(pClip);
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, nullptr);
						}
						else
						{
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(Source);
						Release(pClip);
						StoreMesh(Item, nullptr);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::ME_MASKCLIPUVMASK:
		{
			OP::MeshMaskClipUVMaskArgs Args = Program.GetOpArgs<OP::MeshMaskClipUVMaskArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item),
						FScheduledOp(Args.UVSource, Item),
						FScheduledOp(Args.MaskImage, Item),
						FScheduledOp(Args.MaskLayout, Item));
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_MASKCLIPUVMASK_1);

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					StoreMesh(Item, Source);
				}
				else
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					TSharedPtr<const FMesh> UVSource = LoadMesh(FCacheAddress(Args.UVSource, Item));
					TSharedPtr<const FImage> MaskImage = LoadImage(FCacheAddress(Args.MaskImage, Item));
					TSharedPtr<const FLayout> MaskLayout = LoadLayout(FCacheAddress(Args.MaskLayout, Item));

					// Only if both are valid.
					if (Source.Get() && MaskImage.Get())
					{
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MakeMeshMaskFromUVMask(Result.Get(), Source.Get(), UVSource.Get(), MaskImage.Get(), Args.LayoutIndex, bOutSuccess);

						Release(Source);
						Release(UVSource);
						Release(MaskImage);
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, nullptr);
						}
						else
						{
							StoreMesh(Item, Result);
						}
					}
					else if (Source.Get() && MaskLayout.Get())
					{					
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MakeMeshMaskFromLayout(Result.Get(), Source.Get(), UVSource.Get(), MaskLayout.Get(), Args.LayoutIndex, bOutSuccess);

						Release(Source);
						Release(UVSource);
						Release(MaskImage);
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, nullptr);
						}
						else
						{
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(Source);
						Release(UVSource);
						Release(MaskImage);
						StoreMesh(Item, nullptr);
					}
				}
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::ME_MASKDIFF:
		{
			OP::MeshMaskDiffArgs Args = Program.GetOpArgs<OP::MeshMaskDiffArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item),
						FScheduledOp(Args.Fragment, Item));
				}

				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_MASKDIFF_1)
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					StoreMesh(Item, Source);
				}
				else
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					TSharedPtr<const FMesh> pClip = LoadMesh(FCacheAddress(Args.Fragment, Item));

					// Only if both are valid.
					if (Source.Get() && pClip.Get())
					{
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MeshMaskDiff(Result.Get(), Source.Get(), pClip.Get(), bOutSuccess);

						Release(Source);
						Release(pClip);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, nullptr);
						}
						else
						{
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(Source);
						Release(pClip);
						StoreMesh(Item, nullptr);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_FORMAT:
        {
			OP::MeshFormatArgs Args = Program.GetOpArgs<OP::MeshFormatArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.source && Args.format)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.source, Item),
						FScheduledOp(Args.format, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_FORMAT_1)
				TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source,Item));
				TSharedPtr<const FMesh> Format = LoadMesh(FCacheAddress(Args.format,Item));

				if (Source && Source->IsReference())
				{
					Release(Format);
					StoreMesh(Item, Source);
				}
				else if (Source)
				{
					uint8 Flags = Args.Flags;
					if (!Format && !(Flags & OP::MeshFormatArgs::ResetBufferIndices))
					{
						StoreMesh(Item, Source);
					}
					else if (!Format)
					{
						TSharedPtr<FMesh> Result = CloneOrTakeOver(Source);

						if (Flags & OP::MeshFormatArgs::ResetBufferIndices)
						{
							Result->ResetBufferIndices();
						}

						StoreMesh(Item, Result);
					}
					else
					{
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MeshFormat(Result.Get(), Source.Get(), Format.Get(),
							true,
							(Flags & OP::MeshFormatArgs::Vertex) != 0,
							(Flags & OP::MeshFormatArgs::Index) != 0,
							(Flags & OP::MeshFormatArgs::IgnoreMissing) != 0,
							bOutSuccess);

						check(bOutSuccess);

						if (Flags & OP::MeshFormatArgs::ResetBufferIndices)
						{
							Result->ResetBufferIndices();
						}

						if (Flags & OP::MeshFormatArgs::OptimizeBuffers)
						{
							MUTABLE_CPUPROFILER_SCOPE(MeshOptimizeBuffers)
							MeshOptimizeBuffers(Result.Get());
						}

						Release(Source);
						Release(Format);
						StoreMesh(Item, Result);
					}
				}
				else
				{
					Release(Format);
					StoreMesh(Item, nullptr);
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_EXTRACTLAYOUTBLOCK:
        {
            const uint8* data = Program.GetOpArgsPointer(Item.At);

            OP::ADDRESS source;
            FMemory::Memcpy( &source, data, sizeof(OP::ADDRESS) );
            data += sizeof(OP::ADDRESS);

            uint16 LayoutIndex;
			FMemory::Memcpy( &LayoutIndex, data, sizeof(uint16) );
            data += sizeof(uint16);

            uint16 blockCount;
			FMemory::Memcpy( &blockCount, data, sizeof(uint16) );
            data += sizeof(uint16);

            switch (Item.Stage)
            {
            case 0:
			{
				if (source)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(source, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_EXTRACTLAYOUTBLOCK_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> SourceMesh = LoadMesh(FCacheAddress(source, Item));
					StoreMesh(Item, SourceMesh);
				}
				else
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(source, Item));

					// Access with memcpy necessary for unaligned memory access issues.
					uint64 blocks[512];
					check(blockCount< 512);
					FMemory::Memcpy(blocks, data, sizeof(uint64)*FMath::Min(512,int32(blockCount)));

					if (Source)
					{
						TSharedPtr<FMesh> Result = CreateMesh();
						bool bOutSuccess;

						if (blockCount > 0)
						{
							MeshExtractLayoutBlock(Result.Get(), Source.Get(), LayoutIndex, blockCount, blocks, bOutSuccess);
						}
						else
						{
							MeshExtractLayoutBlock(Result.Get(), Source.Get(), LayoutIndex, bOutSuccess);
						}

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Source);
						}
						else
						{
							Release(Source);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						StoreMesh(Item, nullptr);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_TRANSFORM:
        {
			OP::MeshTransformArgs Args = Program.GetOpArgs<OP::MeshTransformArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.source)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.source, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_TRANSFORM_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
                	TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source, Item));
					StoreMesh(Item, Source);
				}
				else
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source,Item));

					const FMatrix44f& mat = Program.ConstantMatrices[Args.matrix];

					TSharedPtr<FMesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshTransform(Result.Get(), Source.Get(), mat, bOutSuccess);

					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(Item, Source);
					}
					else
					{
						Release(Source);
						StoreMesh(Item, Result);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_CLIPMORPHPLANE:
        {
			OP::MeshClipMorphPlaneArgs Args = Program.GetOpArgs<OP::MeshClipMorphPlaneArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.Source)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}

				break;
			}
            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(ME_CLIPMORPHPLANE_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
                	TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					StoreMesh(Item, Source);
				}
				else
				{

					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));

					check(Args.MorphShape < (uint32)InModel->GetPrivate()->Program.ConstantShapes.Num());

					// Should be an ellipse
					const FShape& MorphShape = Program.ConstantShapes[Args.MorphShape];

					const FVector3f& Origin = MorphShape.position;
					const FVector3f& Normal = MorphShape.up;

					bool bRemoveFaceIfAllVerticesCulled = Args.FaceCullStrategy==EFaceCullStrategy::AllVerticesCulled;

					if (Args.VertexSelectionType == EClipVertexSelectionType::Shape)
					{
						check(Args.VertexSelectionShapeOrBone < (uint32)InModel->GetPrivate()->Program.ConstantShapes.Num());

						// Should be None or an axis aligned box
						const FShape& SelectionShape = Program.ConstantShapes[Args.VertexSelectionShapeOrBone];

						TSharedPtr<FMesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

						bool bOutSuccess = false;
						MeshClipMorphPlane(Result.Get(), Source.Get(), Origin, Normal, Args.Dist, Args.Factor, MorphShape.size[0], MorphShape.size[1], MorphShape.size[2], SelectionShape, bRemoveFaceIfAllVerticesCulled, bOutSuccess, nullptr, -1);
						
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Source);
						}
						else
						{
							Release(Source);
							StoreMesh(Item, Result);
						}
					}

					else if (Args.VertexSelectionType == EClipVertexSelectionType::BoneHierarchy)
					{
						FShape SelectionShape;
						SelectionShape.type = (uint8)FShape::Type::None;

						TSharedPtr<FMesh> Result = CreateMesh(Source->GetDataSize());

						check(Args.VertexSelectionShapeOrBone <= MAX_uint32);
						const FBoneName Bone(Args.VertexSelectionShapeOrBone);

						bool bOutSuccess = false;
						MeshClipMorphPlane(Result.Get(), Source.Get(), Origin, Normal, Args.Dist, Args.Factor, MorphShape.size[0], MorphShape.size[1], MorphShape.size[2], SelectionShape, bRemoveFaceIfAllVerticesCulled, bOutSuccess, &Bone, Args.MaxBoneRadius);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Source);
						}
						else
						{
							Release(Source);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						// No vertex selection
						FShape SelectionShape;
						SelectionShape.type = (uint8)FShape::Type::None;

						TSharedPtr<FMesh> Result = CreateMesh(Source ? Source->GetDataSize() : 0);

						bool bOutSuccess = false;
						MeshClipMorphPlane(Result.Get(), Source.Get(), Origin, Normal, Args.Dist, Args.Factor, MorphShape.size[0], MorphShape.size[1], MorphShape.size[2], SelectionShape, bRemoveFaceIfAllVerticesCulled, bOutSuccess, nullptr, -1.0f);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Source);
						}
						else
						{
							Release(Source);
							StoreMesh(Item, Result);
						}
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }


        case EOpType::ME_CLIPWITHMESH:
        {
			OP::MeshClipWithMeshArgs Args = Program.GetOpArgs<OP::MeshClipWithMeshArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.Source)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.Source, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.Source, Item),
							FScheduledOp(Args.ClipMesh, Item));
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}

				break;
			}
            case 1:
            {
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPWITHMESH_1)
				
				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					StoreMesh(Item, Source);
				}
				else
				{

					TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
					TSharedPtr<const FMesh> Clip = LoadMesh(FCacheAddress(Args.ClipMesh, Item));

					// Only if both are valid.
					if (Source && Clip)
					{
						TSharedPtr<FMesh> Result = CreateMesh(Source->GetDataSize());

						bool bOutSuccess = false;
						MeshClipWithMesh(Result.Get(), Source.Get(), Clip.Get(), bOutSuccess);

						Release(Clip);
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Source);
						}
						else
						{
							Release(Source);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(Clip);
						StoreMesh(Item, Source);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }
		case EOpType::ME_CLIPDEFORM:
		{
			OP::MeshClipDeformArgs Args = Program.GetOpArgs<OP::MeshClipDeformArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (Args.mesh)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.mesh, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.mesh, Item),
							FScheduledOp(Args.clipShape, Item));
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_CLIPDEFORM_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
					StoreMesh(Item, BaseMesh);
				}
				else
				{
					TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
					TSharedPtr<const FMesh> ClipShape = LoadMesh(FCacheAddress(Args.clipShape, Item));

					if (BaseMesh && ClipShape)
					{
						TSharedPtr<FMesh> Result = CreateMesh(BaseMesh->GetDataSize());

						bool bRemoveIfAllVerticesCulled = Args.FaceCullStrategy == EFaceCullStrategy::AllVerticesCulled;

						bool bOutSuccess = false;
						MeshClipDeform(Result.Get(), BaseMesh.Get(), ClipShape.Get(), Args.clipWeightThreshold, bRemoveIfAllVerticesCulled, bOutSuccess);

						Release(ClipShape);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, BaseMesh);
						}
						else
						{
							Release(BaseMesh);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(ClipShape);
						StoreMesh(Item, BaseMesh);
					}
				}
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_APPLYPOSE:
        {
			OP::MeshApplyPoseArgs Args = Program.GetOpArgs<OP::MeshApplyPoseArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.base)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.base, Item),
						FScheduledOp(Args.pose, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
            case 1:
            {
          		MUTABLE_CPUPROFILER_SCOPE(ME_APPLYPOSE_1)

                TSharedPtr<const FMesh> Base = LoadMesh(FCacheAddress(Args.base, Item));
                TSharedPtr<const FMesh> pPose = LoadMesh(FCacheAddress(Args.pose, Item));

                // Only if both are valid.
                if (Base && pPose)
                {
					TSharedPtr<FMesh> Result = CreateMesh(Base->GetSkeleton() ? Base->GetDataSize() : 0);

					bool bOutSuccess = false;
					MeshApplyPose(Result.Get(), Base.Get(), pPose.Get(), bOutSuccess);

					Release(pPose);
					if (!bOutSuccess)
					{
						Release(Result);
						StoreMesh(Item, Base);
					}
					else
					{
						Release(Base);
						StoreMesh(Item, Result);
					}
                }
                else
                {
					Release(pPose);
					StoreMesh(Item, Base);
                }

                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::ME_BINDSHAPE:
		{
			OP::MeshBindShapeArgs Args = Program.GetOpArgs<OP::MeshBindShapeArgs>(Item.At);
			const uint8* Data = Program.GetOpArgsPointer(Item.At);

			constexpr uint8 ShapeContentFilter = (uint8)(EMeshContentFlags::GeometryData | EMeshContentFlags::PoseData);
			const EShapeBindingMethod BindingMethod = static_cast<EShapeBindingMethod>(Args.bindingMethod); 

			switch (Item.Stage)
			{
			case 0:
			{
				if (Args.mesh)
				{
					if (BindingMethod == EShapeBindingMethod::ReshapeClosestProject)
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.mesh, Item),
							FScheduledOp::FromOpAndOptions(Args.shape, Item, ShapeContentFilter));
					}
					else
					{
						if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
						{
							AddOp(FScheduledOp(Item.At, Item, 1),
								FScheduledOp(Args.mesh, Item));
						}
						else
						{
							AddOp(FScheduledOp(Item.At, Item, 1),
								FScheduledOp(Args.mesh, Item),
								FScheduledOp::FromOpAndOptions(Args.shape, Item, ShapeContentFilter));
						}
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_BINDSHAPE_1)
				
				if (BindingMethod == EShapeBindingMethod::ReshapeClosestProject)
				{
					TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
					TSharedPtr<const FMesh> Shape = 
					LoadMesh(FScheduledOp::FromOpAndOptions(Args.shape, Item, ShapeContentFilter));
					// Bones are stored after the Args
					Data += sizeof(Args);

					// Rebuilding array of bone names ----
					int32 NumBones;
					FMemory::Memcpy(&NumBones, Data, sizeof(int32)); 
					Data += sizeof(int32);
					
					TArray<FBoneName> BonesToDeform;
					BonesToDeform.SetNumUninitialized(NumBones);
					FMemory::Memcpy(BonesToDeform.GetData(), Data, NumBones * sizeof(FBoneName));
					Data += NumBones * sizeof(FBoneName);

					int32 NumPhysicsBodies;
					FMemory::Memcpy(&NumPhysicsBodies, Data, sizeof(int32)); 
					Data += sizeof(int32);

					TArray<FBoneName> PhysicsToDeform;
					PhysicsToDeform.SetNumUninitialized(NumPhysicsBodies);
					FMemory::Memcpy(PhysicsToDeform.GetData(), Data, NumPhysicsBodies * sizeof(FBoneName));
					Data += NumPhysicsBodies * sizeof(FBoneName);

					EMeshBindShapeFlags BindFlags = static_cast<EMeshBindShapeFlags>(Args.flags);
					EMeshContentFlags MeshContentFilter = (EMeshContentFlags)Item.ExecutionOptions;

					if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::GeometryData))
					{
						EnumRemoveFlags(BindFlags, 
								EMeshBindShapeFlags::EnableRigidParts |
								EMeshBindShapeFlags::ReshapeVertices  |
								EMeshBindShapeFlags::ApplyLaplacian   |
								EMeshBindShapeFlags::RecomputeNormals);
					}

					if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::PhysicsData))
					{
						EnumRemoveFlags(BindFlags, EMeshBindShapeFlags::ReshapePhysicsVolumes);
					}

					if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::PoseData))
					{
						EnumRemoveFlags(BindFlags, EMeshBindShapeFlags::ReshapeSkeleton);
					}

					FMeshBindColorChannelUsages ColorChannelUsages;
					FMemory::Memcpy(&ColorChannelUsages, &Args.ColorUsage, sizeof(ColorChannelUsages));
					static_assert(sizeof(ColorChannelUsages) == sizeof(Args.ColorUsage));

					TSharedPtr<FMesh> BindMeshResult = CreateMesh();

					bool bOutSuccess = false;
					MeshBindShapeReshape(BindMeshResult.Get(), BaseMesh.Get(), Shape.Get(), BonesToDeform, PhysicsToDeform, BindFlags, ColorChannelUsages, bOutSuccess);
				
					Release(Shape);
					// not success indicates nothing has bond so the base mesh can be reused.
					if (!bOutSuccess)
					{
						Release(BindMeshResult);
						StoreMesh(Item, BaseMesh);
					}
					else
					{
						if (!EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ReshapeVertices))
						{
							TSharedPtr<FMesh> BindMeshNoVertsResult = CloneOrTakeOver(BaseMesh);
							BindMeshNoVertsResult->AdditionalBuffers = MoveTemp(BindMeshResult->AdditionalBuffers);
							Release(BaseMesh);
							Release(BindMeshResult);
							StoreMesh(Item, BindMeshNoVertsResult);
						}
						else
						{
							Release(BaseMesh);
							StoreMesh(Item, BindMeshResult);
						}
					}
				}	
				else
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
						StoreMesh(Item, BaseMesh);
					}
					else
					{
						TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
						TSharedPtr<const FMesh> Shape = 
								LoadMesh(FScheduledOp::FromOpAndOptions(Args.shape, Item, ShapeContentFilter));

						TSharedPtr<FMesh> Result = CreateMesh(BaseMesh ? BaseMesh->GetDataSize() : 0);

						bool bOutSuccess = false;
						MeshBindShapeClipDeform(Result.Get(), BaseMesh.Get(), Shape.Get(), BindingMethod, bOutSuccess);

						Release(Shape);
						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, BaseMesh);
						}
						else
						{
							Release(BaseMesh);
							StoreMesh(Item, Result);
						}
					}
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}


		case EOpType::ME_APPLYSHAPE:
		{
			OP::MeshApplyShapeArgs Args = Program.GetOpArgs<OP::MeshApplyShapeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (Args.mesh)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.mesh, Item),
						FScheduledOp(Args.shape, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_APPLYSHAPE_1)
					
				TSharedPtr<const FMesh> BaseMesh = LoadMesh(FCacheAddress(Args.mesh, Item));
				TSharedPtr<const FMesh> Shape = LoadMesh(FCacheAddress(Args.shape, Item));

				EMeshBindShapeFlags ReshapeFlags = static_cast<EMeshBindShapeFlags>(Args.flags);
				const EMeshContentFlags MeshContentFilter = (EMeshContentFlags)Item.ExecutionOptions;

				if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::GeometryData))
				{
					EnumRemoveFlags(ReshapeFlags, 
							EMeshBindShapeFlags::EnableRigidParts |
							EMeshBindShapeFlags::ReshapeVertices  |
							EMeshBindShapeFlags::ApplyLaplacian   |
							EMeshBindShapeFlags::RecomputeNormals);
				}

				if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::PhysicsData))
				{
					EnumRemoveFlags(ReshapeFlags, EMeshBindShapeFlags::ReshapePhysicsVolumes);
				}

				if (!EnumHasAnyFlags(MeshContentFilter, EMeshContentFlags::PoseData))
				{
					EnumRemoveFlags(ReshapeFlags, EMeshBindShapeFlags::ReshapeSkeleton);
				}

				const bool bReshapeVertices = EnumHasAnyFlags(ReshapeFlags, EMeshBindShapeFlags::ReshapeVertices);

				TSharedPtr<FMesh> ReshapedMeshResult = CreateMesh(BaseMesh ? BaseMesh->GetDataSize() : 0);

				bool bOutSuccess = false;
				MeshApplyShape(ReshapedMeshResult.Get(), BaseMesh.Get(), Shape.Get(), ReshapeFlags, bOutSuccess);

				Release(Shape);
				
				if (!bOutSuccess)
				{
					Release(ReshapedMeshResult);
					StoreMesh(Item, BaseMesh);
				}
				else
				{
					if (!bReshapeVertices)
					{
						// Clone without Skeleton, Physics or Poses 
						EMeshCopyFlags CopyFlags = ~(
							EMeshCopyFlags::WithSkeleton |
							EMeshCopyFlags::WithPhysicsBody |
							EMeshCopyFlags::WithAdditionalPhysics |
							EMeshCopyFlags::WithPoses);

						TSharedPtr<FMesh> NoVerticesReshpedMesh = CloneOrTakeOver(BaseMesh);

						NoVerticesReshpedMesh->SetSkeleton(ReshapedMeshResult->GetSkeleton());
						NoVerticesReshpedMesh->SetPhysicsBody(ReshapedMeshResult->GetPhysicsBody());
						NoVerticesReshpedMesh->AdditionalPhysicsBodies = ReshapedMeshResult->AdditionalPhysicsBodies;
						NoVerticesReshpedMesh->BonePoses = ReshapedMeshResult->BonePoses;

						Release(BaseMesh);
						Release(ReshapedMeshResult);
						StoreMesh(Item, NoVerticesReshpedMesh);
					}
					else
					{
						Release(BaseMesh);
						StoreMesh(Item, ReshapedMeshResult);
					}
				}
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::ME_MORPHRESHAPE:
		{
			OP::MeshMorphReshapeArgs Args = Program.GetOpArgs<OP::MeshMorphReshapeArgs>(Item.At);
			switch(Item.Stage)
			{
			case 0:
			{
				if (Args.Morph)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Morph, Item),
						FScheduledOp(Args.Reshape, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_MORPHRESHAPE_1)
					
				TSharedPtr<const FMesh> MorphedMesh = LoadMesh(FCacheAddress(Args.Morph, Item));
				TSharedPtr<const FMesh> ReshapeMesh = LoadMesh(FCacheAddress(Args.Reshape, Item));

				if (ReshapeMesh && MorphedMesh)
				{
					// Copy without Skeleton, Physics or Poses 
					EMeshCopyFlags CopyFlags = ~(
							EMeshCopyFlags::WithSkeleton    | 
							EMeshCopyFlags::WithPhysicsBody | 
							EMeshCopyFlags::WithPoses);

					TSharedPtr<FMesh> Result = CreateMesh(MorphedMesh->GetDataSize());
					Result->CopyFrom(*MorphedMesh, CopyFlags);

					Result->SetSkeleton(ReshapeMesh->GetSkeleton());
					Result->SetPhysicsBody(ReshapeMesh->GetPhysicsBody());
					Result->BonePoses = ReshapeMesh->BonePoses;

					Release(MorphedMesh);
					Release(ReshapeMesh);
					StoreMesh(Item, Result);
				}
				else
				{
					StoreMesh(Item, MorphedMesh);
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_SETSKELETON:
        {
			OP::MeshSetSkeletonArgs Args = Program.GetOpArgs<OP::MeshSetSkeletonArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.Source)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item),
						FScheduledOp(Args.Skeleton, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_SETSKELETON_1)
            		
                TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.Source, Item));
                TSharedPtr<const FMesh> Skeleton = LoadMesh(FCacheAddress(Args.Skeleton, Item));

                // Only if both are valid.
                if (Source && Skeleton)
                {
                    if ( Source->GetSkeleton()
                         &&
                         Source->GetSkeleton()->GetBoneCount() > 0 )
                    {
                        // For some reason we already have bone data, so we can't just overwrite it
                        // or the skinning may break. This may happen because of a problem in the
                        // optimiser that needs investigation.
                        // \TODO Be defensive, for now.
                        UE_LOG(LogMutableCore, Warning, TEXT("Performing a MeshRemapSkeleton, instead of MeshSetSkeletonData because source mesh already has some skeleton."));

						TSharedPtr<FMesh> Result = CreateMesh(Source->GetDataSize());

						bool bOutSuccess = false;
                        MeshRemapSkeleton(Result.Get(), Source.Get(), Skeleton->GetSkeleton(), bOutSuccess);

						Release(Skeleton);

                        if (!bOutSuccess)
                        {
							Release(Result);
							StoreMesh(Item, Source);
                        }
						else
						{
							//Result->GetPrivate()->CheckIntegrity();
							Release(Source);
							StoreMesh(Item, Result);
						}
                    }
                    else
                    {
						TSharedPtr<FMesh> Result = CloneOrTakeOver(Source);

                        Result->SetSkeleton(Skeleton->GetSkeleton());

						//Result->GetPrivate()->CheckIntegrity();
						Release(Skeleton);
						StoreMesh(Item, Result);
                    }
                }
                else
                {
					Release(Skeleton);
					StoreMesh(Item, Source);
                }

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_REMOVEMASK:
        {
       		MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK)
        		
            // Decode op
            // TODO: Partial decode for each stage
            const uint8* data = Program.GetOpArgsPointer(Item.At);

            OP::ADDRESS source;
            FMemory::Memcpy(&source,data,sizeof(OP::ADDRESS)); 
			data += sizeof(OP::ADDRESS);

			EFaceCullStrategy FaceCullStrategy;
			FMemory::Memcpy(&FaceCullStrategy, data, sizeof(EFaceCullStrategy));
			data += sizeof(EFaceCullStrategy);

            TArray<FScheduledOp> conditions;
			TArray<OP::ADDRESS> masks;

            uint16 removes;
			FMemory::Memcpy(&removes,data,sizeof(uint16)); 
			data += sizeof(uint16);

            for( uint16 r=0; r<removes; ++r)
            {
                OP::ADDRESS condition;
				FMemory::Memcpy(&condition,data,sizeof(OP::ADDRESS)); 
				data += sizeof(OP::ADDRESS);
                
				conditions.Emplace(condition, Item);

                OP::ADDRESS mask;
				FMemory::Memcpy(&mask,data,sizeof(OP::ADDRESS)); 
				data += sizeof(OP::ADDRESS);

                masks.Add(mask);
            }


            // Schedule next stages
            switch (Item.Stage)
            {
            case 0:
			{
				if (source)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1), 
								FScheduledOp(source, Item));
					}
					else
					{
						// Request the conditions
						AddOp(FScheduledOp(Item.At, Item, 1), conditions);
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
            case 1:
            {
        		MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
                {
                	TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(source, Item));
					StoreMesh(Item, Source);
				}
				else
				{
					// Request the source and the necessary masks
					// \todo: store condition values in heap?
					TArray<FScheduledOp> deps;
					deps.Emplace( source, Item );
					for( int32 r=0; source && r<conditions.Num(); ++r )
					{
						// If there is no expression, we'll assume true.
						bool value = true;
						if (conditions[r].At)
						{
							value = LoadBool(FCacheAddress(conditions[r].At, Item));
						}

						if (value)
						{
							deps.Emplace(masks[r], Item);
						}
					}

					if (source)
					{
						AddOp(FScheduledOp(Item.At, Item, 2), deps);
					}
				}
                break;
            }

            case 2:
            {
            	MUTABLE_CPUPROFILER_SCOPE(ME_REMOVEMASK_2)
            	
                // \todo: single remove operation with all masks?
                TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(source, Item));

				if (Source)
				{
					TSharedPtr<FMesh> Result = CloneOrTakeOver(Source);

					for (int32 r = 0; r < conditions.Num(); ++r)
					{
						// If there is no expression, we'll assume true.
						bool value = true;
						if (conditions[r].At)
						{
							value = LoadBool(FCacheAddress(conditions[r].At, Item));
						}

						if (value)
						{
							TSharedPtr<const FMesh> Mask = LoadMesh(FCacheAddress(masks[r], Item));
							if (Mask)
							{
								bool bRemoveIfAllVerticesCulled = FaceCullStrategy == EFaceCullStrategy::AllVerticesCulled;
								MeshRemoveMaskInline(Result.Get(), Mask.Get(), bRemoveIfAllVerticesCulled);

								Release(Mask);
							}
						}
					}

					StoreMesh(Item, Result);
				}
				else
				{
					StoreMesh(Item, nullptr);
				}

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::ME_ADDTAGS:
		{
			MUTABLE_CPUPROFILER_SCOPE(ME_ADDTAGS)

			// Decode op
			// TODO: Partial decode for each stage
			const uint8* Data = Program.GetOpArgsPointer(Item.At);

			OP::ADDRESS Source;
			FMemory::Memcpy(&Source, Data, sizeof(OP::ADDRESS));
			Data += sizeof(OP::ADDRESS);

			// Schedule next stages
			switch (Item.Stage)
			{
			case 0:
			{
				if (Source)
				{
					// Request the source
					AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp(Source, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_ADDTAGS_2)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::MetaData))
				{
					TSharedPtr<const FMesh> SourceMesh = LoadMesh(FCacheAddress(Source, Item));
					StoreMesh(Item, SourceMesh);
				}
				else
				{
					TSharedPtr<const FMesh> SourceMesh = LoadMesh(FCacheAddress(Source, Item));

					if (!SourceMesh)
					{
						StoreMesh(Item, nullptr);
					}
					else
					{
						TSharedPtr<FMesh> Result = CloneOrTakeOver(SourceMesh);

						// Decode the tags
						uint16 TagCount;
						FMemory::Memcpy(&TagCount, Data, sizeof(uint16));
						Data += sizeof(uint16);

						int32 FirstMeshTagIndex = Result->Tags.Num();
						Result->Tags.SetNum(FirstMeshTagIndex+TagCount);
						for (uint16 TagIndex = 0; TagIndex < TagCount; ++TagIndex)
						{
							OP::ADDRESS TagConstant;
							FMemory::Memcpy(&TagConstant, Data, sizeof(OP::ADDRESS));
							Data += sizeof(OP::ADDRESS);

							check(TagConstant < (uint32)InModel->GetPrivate()->Program.ConstantStrings.Num());
							const FString& Name = Program.ConstantStrings[TagConstant];
							Result->Tags[FirstMeshTagIndex+TagIndex] = Name;
						}

						StoreMesh(Item, Result);
					}
				}
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_PROJECT:
        {
			OP::MeshProjectArgs Args = Program.GetOpArgs<OP::MeshProjectArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
			{
				if (Args.Mesh)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.Mesh, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.Mesh, Item),
							FScheduledOp(Args.Projector, Item));
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_PROJECT_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> Mesh = LoadMesh(FCacheAddress(Args.Mesh, Item));
					StoreMesh(Item, Mesh);
				}
				else
				{
					TSharedPtr<const FMesh> Mesh = LoadMesh(FCacheAddress(Args.Mesh, Item));
					const FProjector Projector = LoadProjector(FCacheAddress(Args.Projector, Item));

					// Only if both are valid.
					if (Mesh && Mesh->GetVertexBuffers().GetBufferCount() > 0)
					{
						TSharedPtr<FMesh> Result = CreateMesh();

						bool bOutSuccess = false;
						MeshProject(Result.Get(), Mesh.Get(), Projector, bOutSuccess);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, Mesh);
						}
						else
						{
							// Result->GetPrivate()->CheckIntegrity();
							Release(Mesh);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(Mesh);
						StoreMesh(Item, nullptr);
					}
				}
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::ME_OPTIMIZESKINNING:
		{
			OP::MeshOptimizeSkinningArgs Args = Program.GetOpArgs<OP::MeshOptimizeSkinningArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (Args.source)
				{
					AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp(Args.source, Item));
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_OPTIMIZESKINNING_1)

				TSharedPtr<const FMesh> Source = LoadMesh(FCacheAddress(Args.source, Item));

				if (Source && Source->IsReference())
				{
					StoreMesh(Item, Source);
				}

				TSharedPtr<FMesh> Result = CreateMesh();

				bool bOutSuccess = false;
				MeshOptimizeSkinning(Result.Get(), Source.Get(), bOutSuccess);

				if (!bOutSuccess)
				{
					Release(Result);
					StoreMesh(Item, Source);
				}
				else
				{
					Release(Source);
					StoreMesh(Item, Result);
				}

				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::ME_TRANSFORMWITHMESH:
		{
			OP::MeshTransformWithinMeshArgs Args = Program.GetOpArgs<OP::MeshTransformWithinMeshArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				if (Args.sourceMesh)
				{
					if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.sourceMesh, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.sourceMesh, Item),
							FScheduledOp(Args.boundingMesh, Item),
							FScheduledOp(Args.matrix, Item));
					}
				}
				else
				{
					StoreMesh(Item, nullptr);
				}
				break;
			}
			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(ME_TRANSFORMWITHMESH_1)

				if (!EnumHasAnyFlags(EMeshContentFlags(Item.ExecutionOptions), EMeshContentFlags::GeometryData))
				{
					TSharedPtr<const FMesh> SourceMesh = LoadMesh(FCacheAddress(Args.sourceMesh, Item));
					StoreMesh(Item, SourceMesh);
				}
				else
				{
					TSharedPtr<const FMesh> SourceMesh = LoadMesh(FCacheAddress(Args.sourceMesh,Item));
					TSharedPtr<const FMesh> BoundingMesh = LoadMesh(FCacheAddress(Args.boundingMesh, Item));
					const FMatrix44f& Transform = LoadMatrix(FCacheAddress(Args.matrix, Item));

					if (SourceMesh)
					{
						TSharedPtr<FMesh> Result = CreateMesh(SourceMesh->GetDataSize());

						bool bOutSuccess = false;
						MeshTransformWithMesh(Result.Get(), SourceMesh.Get(), BoundingMesh.Get(), Transform, bOutSuccess);
						Release(BoundingMesh);

						if (!bOutSuccess)
						{
							Release(Result);
							StoreMesh(Item, SourceMesh);
						}
						else
						{
							Release(SourceMesh);
							StoreMesh(Item, Result);
						}
					}
					else
					{
						Release(BoundingMesh);
						StoreMesh(Item, SourceMesh);
					}
				}
				break;
			}

			default:
				check(false);
			}
			break;
		}

		default:
			if (type!=EOpType::NONE)
			{
				// Operation not implemented
				check( false );
			}
			break;
		}
	}

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Image(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Image);

		FImageOperator ImOp = MakeImageOperator(this);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
		switch (type)
        {

        case EOpType::IM_LAYERCOLOUR:
        {
			OP::ImageLayerColourArgs Args = Program.GetOpArgs<OP::ImageLayerColourArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.base, Item),
                           FScheduledOp::FromOpAndOptions( Args.colour, Item, 0),
                           FScheduledOp( Args.mask, Item) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_LAYER:
        {
			OP::ImageLayerArgs Args = Program.GetOpArgs<OP::ImageLayerArgs>(Item.At);

			if (ExecutionStrategy == EExecutionStrategy::MinimizeMemory)
			{
				switch (Item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.base, Item));
					break;

				case 1:
					// Request the rest of the data.
					AddOp(FScheduledOp(Item.At, Item, 2),
						FScheduledOp(Args.blended, Item),
						FScheduledOp(Args.mask, Item));
					break;

				case 2:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
			}
			else
			{
				switch (Item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.base, Item),
						FScheduledOp(Args.blended, Item),
						FScheduledOp(Args.mask, Item));
					break;

				case 1:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
			}

            break;
        }

        case EOpType::IM_MULTILAYER:
        {
			OP::ImageMultiLayerArgs Args = Program.GetOpArgs<OP::ImageMultiLayerArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                       FScheduledOp( Args.rangeSize, Item ),
					   FScheduledOp(Args.base, Item));
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_MULTILAYER_1)
            		
                // We now know the number of iterations
                int32 Iterations = 0;
                if (Args.rangeSize)
                {
                    FCacheAddress RangeAddress(Args.rangeSize,Item);

                    // We support both integers and scalars here, which is not common.
                    // \todo: review if this is necessary or we can enforce it at compile time.
                    EDataType RangeSizeType = GetOpDataType( InModel->GetPrivate()->Program.GetOpType(Args.rangeSize) );
                    if (RangeSizeType == EDataType::Int)
                    {
						Iterations = LoadInt(RangeAddress);
                    }
                    else if (RangeSizeType == EDataType::Scalar)
                    {
						Iterations = int32( LoadScalar(RangeAddress) );
                    }
                }

				TSharedPtr<const FImage> Base = LoadImage(FCacheAddress(Args.base, Item));

				if (Iterations <= 0)
				{
					// There are no layers: return the base
					StoreImage(Item, Base);
				}
				else
				{
					// Store the base
					TSharedPtr<FImage> New = CloneOrTakeOver(Base);
					EImageFormat InitialBaseFormat = New->GetFormat();

					// Reset relevancy map.
					New->Flags &= ~FImage::EImageFlags::IF_HAS_RELEVANCY_MAP;

					// This shouldn't happen in optimised models, but it could happen in editors, etc.
					// \todo: raise a performance warning?
					EImageFormat BaseFormat = GetUncompressedFormat(New->GetFormat());
					if (New->GetFormat() != BaseFormat)
					{
						TSharedPtr<FImage> Formatted = CreateImage( New->GetSizeX(), New->GetSizeY(), New->GetLODCount(), BaseFormat, EInitializationType::NotInitialized );

						bool bSuccess = false;
						ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), New.Get());
						check(bSuccess); // Decompression cannot fail

						Release(New);
						New = Formatted;
					}

					FScheduledOpData Data;
					Data.Resource = New;
					Data.MultiLayer.Iterations = Iterations;
					Data.MultiLayer.OriginalBaseFormat = InitialBaseFormat;
					Data.MultiLayer.bBlendOnlyOneMip = false;
					int32 DataPos = HeapData.Add(Data);

					// Request the first layer
					int32 CurrentIteration = 0;
					FScheduledOp ItemCopy = Item;
					ExecutionIndex Index = GetMemory().GetRangeIndex(Item.ExecutionIndex);
					Index.SetFromModelRangeIndex(Args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(Index);
					AddOp(FScheduledOp(Item.At, Item, 2, DataPos), FScheduledOp(Args.base, Item), FScheduledOp(Args.blended, ItemCopy), FScheduledOp(Args.mask, ItemCopy));
				}

                break;
            }

            default:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_MULTILAYER_default)

				FScheduledOpData& Data = HeapData[Item.CustomState];

				int32 Iterations = Data.MultiLayer.Iterations;
				int32 CurrentIteration = Item.Stage - 2;
				check(CurrentIteration >= 0 && CurrentIteration < 120);

				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Layer %d of %d"), CurrentIteration, Iterations));

				// Process the current layer

				TSharedPtr<FImage> Base = StaticCastSharedPtr<FImage>(ConstCastSharedPtr<FResource>(Data.Resource));
 
                FScheduledOp itemCopy = Item;
                ExecutionIndex index = GetMemory().GetRangeIndex( Item.ExecutionIndex );
				
                {
                    index.SetFromModelRangeIndex( Args.rangeId, CurrentIteration);
                    itemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(index);
					itemCopy.CustomState = 0;

                    TSharedPtr<const FImage> Blended = LoadImage( FCacheAddress(Args.blended,itemCopy) );

                    // This shouldn't happen in optimised models, but it could happen in editors, etc.
                    // \todo: raise a performance warning?
                    if (Blended && Blended->GetFormat()!=Base->GetFormat() )
                    {
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedReformat);

						TSharedPtr<FImage> Formatted = CreateImage(Blended->GetSizeX(), Blended->GetSizeY(), Blended->GetLODCount(), Base->GetFormat(), EInitializationType::NotInitialized);

						bool bSuccess = false;
						ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), Blended.Get());
						check(bSuccess);

						Release(Blended);
						Blended = Formatted;
                    }

					// TODO: This shouldn't happen, but be defensive.
					FImageSize ResultSize = Base->GetSize();
					if (Blended && Blended->GetSize() != ResultSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_BlendedFixForMultilayer);

						TSharedPtr<FImage> Resized = CreateImage(ResultSize[0], ResultSize[1], Blended->GetLODCount(), Blended->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.Get(), 0, Blended.Get());
						Release(Blended);
						Blended = Resized;
					}

					if (Blended->GetLODCount() < Base->GetLODCount())
					{
						Data.MultiLayer.bBlendOnlyOneMip = true;
					}

					bool bApplyColorBlendToAlpha = false;

					bool bDone = false;

					// This becomes true if we need to update the mips of the resulting image
					// This could happen in the base image has mips, but one of the blended one doesn't.
					bool bBlendOnlyOneMip = Data.MultiLayer.bBlendOnlyOneMip;
					bool bUseBlendSourceFromBlendAlpha = false; // (Args.flags& OP::ImageLayerArgs::F_BLENDED_RGB_FROM_ALPHA) != 0;

					if (!Args.mask && Args.bUseMaskFromBlended
						&&
						Args.blendType == uint8(EBlendType::BT_BLEND)
						&&
						Args.blendTypeAlpha == uint8(EBlendType::BT_LIGHTEN) )
					{
						// This is a frequent critical-path case because of multilayer projectors.
						bDone = true;
					
						constexpr bool bUseVectorImpl = false;
						if constexpr (bUseVectorImpl)
						{
							BufferLayerCompositeVector<VectorBlendChannelMasked, VectorLightenChannel, false>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, Args.BlendAlphaSourceChannel);
						}
						else
						{
							BufferLayerComposite<BlendChannelMasked, LightenChannel, false>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, Args.BlendAlphaSourceChannel);
						}
					}

                    if (!bDone && Args.mask)
                    {
                        TSharedPtr<const FImage> Mask = LoadImage( FCacheAddress(Args.mask,itemCopy) );

						// TODO: This shouldn't happen, but be defensive.
						if (Mask && Mask->GetSize() != ResultSize)
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForMultilayer);

							TSharedPtr<FImage> Resized = CreateImage(ResultSize[0], ResultSize[1], Mask->GetLODCount(), Mask->GetFormat(), EInitializationType::NotInitialized);
							ImOp.ImageResizeLinear(Resized.Get(), 0, Mask.Get());
							Release(Mask);
							Mask = Resized;
						}

						// Not implemented yet
						check(!bUseBlendSourceFromBlendAlpha);

                        switch (EBlendType(Args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannelMasked, SoftLightChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannelMasked, HardLightChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannelMasked, BurnChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannelMasked, DodgeChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannelMasked, ScreenChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannelMasked, OverlayChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannelMasked, LightenChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannelMasked, MultiplyChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannelMasked, BlendChannel, false>(Base.Get(), Base.Get(), Mask.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
                        default: check(false);
                        }

						Release(Mask);
                    }
					else if (!bDone && Args.bUseMaskFromBlended)
					{
						// Not implemented yet
						check(!bUseBlendSourceFromBlendAlpha);

						switch (EBlendType(Args.blendType))
						{
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
						case EBlendType::BT_SOFTLIGHT: BufferLayerEmbeddedMask<SoftLightChannelMasked, SoftLightChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerEmbeddedMask<HardLightChannelMasked, HardLightChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BURN: BufferLayerEmbeddedMask<BurnChannelMasked, BurnChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_DODGE: BufferLayerEmbeddedMask<DodgeChannelMasked, DodgeChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_SCREEN: BufferLayerEmbeddedMask<ScreenChannelMasked, ScreenChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_OVERLAY: BufferLayerEmbeddedMask<OverlayChannelMasked, OverlayChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_LIGHTEN: BufferLayerEmbeddedMask<LightenChannelMasked, LightenChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_MULTIPLY: BufferLayerEmbeddedMask<MultiplyChannelMasked, MultiplyChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						case EBlendType::BT_BLEND: BufferLayerEmbeddedMask<BlendChannelMasked, BlendChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip); break;
						default: check(false);
						}
					}
                    else if (!bDone)
                    {
                        switch (EBlendType(Args.blendType))
                        {
						case EBlendType::BT_NORMAL_COMBINE: check(false); break;
                        case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_BURN: BufferLayer<BurnChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_DODGE: BufferLayer<DodgeChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_SCREEN: BufferLayer<ScreenChannel, false>(Base.Get(), Base.Get(),  Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        case EBlendType::BT_BLEND: BufferLayer<BlendChannel, false>(Base.Get(), Base.Get(), Blended.Get(), bApplyColorBlendToAlpha, bBlendOnlyOneMip, bUseBlendSourceFromBlendAlpha); break;
                        default: check(false);
                        }
                    }

					// Apply the separate blend operation for alpha
					if (!bDone && !bApplyColorBlendToAlpha && Args.blendTypeAlpha != uint8(EBlendType::BT_NONE) )
					{
						// Separate alpha operation ignores the mask.
						switch (EBlendType(Args.blendTypeAlpha))
						{
						case EBlendType::BT_SOFTLIGHT: BufferLayerInPlace<SoftLightChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_HARDLIGHT: BufferLayerInPlace<HardLightChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_BURN: BufferLayerInPlace<BurnChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_DODGE: BufferLayerInPlace<DodgeChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_SCREEN: BufferLayerInPlace<ScreenChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_OVERLAY: BufferLayerInPlace<OverlayChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_LIGHTEN: BufferLayerInPlace<LightenChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_MULTIPLY: BufferLayerInPlace<MultiplyChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						case EBlendType::BT_BLEND: BufferLayerInPlace<BlendChannel, false, 1>(Base.Get(), Blended.Get(), bBlendOnlyOneMip, 3, Args.BlendAlphaSourceChannel); break;
						default: check(false);
						}
					}

					Release(Blended);
				}

				// Are we done?
				if (CurrentIteration + 1 == Iterations)
				{
					if (Data.MultiLayer.bBlendOnlyOneMip)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageLayer_MipFix);
						FMipmapGenerationSettings DummyMipSettings{};
						ImageMipmapInPlace(Settings.ImageCompressionQuality, Base.Get(), DummyMipSettings);
					}

					// TODO: Reconvert to OriginalBaseFormat if necessary?

					Data.Resource = nullptr;
					StoreImage(Item, Base);
					break;
				}
				else
				{
					// Request a new layer
					++CurrentIteration;
					FScheduledOp ItemCopy = Item;
					ExecutionIndex Index = GetMemory().GetRangeIndex(Item.ExecutionIndex);
					Index.SetFromModelRangeIndex(Args.rangeId, CurrentIteration);
					ItemCopy.ExecutionIndex = GetMemory().GetRangeIndexIndex(Index);
					AddOp(FScheduledOp(Item.At, Item, 2+CurrentIteration, Item.CustomState), FScheduledOp(Args.blended, ItemCopy), FScheduledOp(Args.mask, ItemCopy));

				}

                break;
            }

            } // switch stage

            break;
        }

		case EOpType::IM_NORMALCOMPOSITE:
		{
			OP::ImageNormalCompositeArgs Args = Program.GetOpArgs<OP::ImageNormalCompositeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				if (Args.base && Args.normal)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
							FScheduledOp(Args.base, Item),
							FScheduledOp(Args.normal, Item));
				}
				else
				{
					StoreImage(Item, nullptr);
				}
				break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_NORMALCOMPOSITE_1)

				TSharedPtr<const FImage> Base = LoadImage(FCacheAddress(Args.base, Item));
				TSharedPtr<const FImage> Normal = LoadImage(FCacheAddress(Args.normal, Item));

				if (Normal->GetLODCount() < Base->GetLODCount())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageNormalComposite_EmergencyFix);

					int32 StartLevel = Normal->GetLODCount() - 1;
					int32 LevelCount = Base->GetLODCount();
					
					TSharedPtr<FImage> NormalFix = CloneOrTakeOver(Normal);
					NormalFix->DataStorage.SetNumLODs(LevelCount);

					FMipmapGenerationSettings MipSettings{};
					ImOp.ImageMipmap(Settings.ImageCompressionQuality, NormalFix.Get(), NormalFix.Get(), StartLevel, LevelCount, MipSettings);

					Normal = NormalFix;
				}


                TSharedPtr<FImage> Result = CreateImage(Base->GetSizeX(), Base->GetSizeY(), Base->GetLODCount(), Base->GetFormat(), EInitializationType::NotInitialized);
				ImageNormalComposite(Result.Get(), Base.Get(), Normal.Get(), Args.mode, Args.power);

				Release(Base);
				Release(Normal);
				StoreImage(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::IM_PIXELFORMAT:
        {
			OP::ImagePixelFormatArgs Args = Program.GetOpArgs<OP::ImagePixelFormatArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.source, Item) );
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_MIPMAP:
        {
			OP::ImageMipmapArgs Args = Program.GetOpArgs<OP::ImageMipmapArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
				AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.source, Item) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_RESIZE:
        {
			OP::ImageResizeArgs Args = Program.GetOpArgs<OP::ImageResizeArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.Source, Item) );
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_RESIZELIKE:
        {
			OP::ImageResizeLikeArgs Args = Program.GetOpArgs<OP::ImageResizeLikeArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp(FScheduledOp(Item.At, Item, 1),
                      	FScheduledOp(Args.Source, Item),
                        FScheduledOp(Args.SizeSource, Item));
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_RESIZELIKE_1)
            	
                TSharedPtr<const FImage> Base = LoadImage( FCacheAddress(Args.Source,Item) );
                TSharedPtr<const FImage> SizeBase = LoadImage( FCacheAddress(Args.SizeSource,Item) );
				FImageSize DestSize = SizeBase->GetSize();
				Release(SizeBase);

                if (Base->GetSize() != DestSize)
                {
					int32 BaseLODCount = Base->GetLODCount();
					TSharedPtr<FImage> Result = CreateImage(DestSize[0], DestSize[1], BaseLODCount, Base->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Result.Get(), Settings.ImageCompressionQuality, Base.Get());
					Release(Base);

                    // If the source image had mips, generate them as well for the resized image.
                    // This shouldn't happen often since "ResizeLike" should be usually optimised out
                    // during model compilation. The mipmap generation below is not very precise with
                    // the number of mips that are needed and will probably generate too many
                    bool bSourceHasMips = BaseLODCount > 1;
                    
					if (bSourceHasMips)
                    {
						int32 LevelCount = FImage::GetMipmapCount(Result->GetSizeX(), Result->GetSizeY());	
						Result->DataStorage.SetNumLODs(LevelCount);

						FMipmapGenerationSettings MipSettings{};
						ImOp.ImageMipmap(Settings.ImageCompressionQuality, Result.Get(), Result.Get(), 0, LevelCount, MipSettings);
                    }				

					StoreImage(Item, Result);
				}
                else
                {
					StoreImage(Item, Base);
				}
				
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_RESIZEREL:
        {
			OP::ImageResizeRelArgs Args = Program.GetOpArgs<OP::ImageResizeRelArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.Source, Item) );
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
            }

            default:
                check(false);
            }


            break;
        }
        case EOpType::IM_BLANKLAYOUT:
        {
			OP::ImageBlankLayoutArgs Args = Program.GetOpArgs<OP::ImageBlankLayoutArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp::FromOpAndOptions(Args.Layout, Item, 0));
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_BLANKLAYOUT_1)
            		
                TSharedPtr<const FLayout> pLayout = LoadLayout(FScheduledOp::FromOpAndOptions(Args.Layout, Item, 0));

                FIntPoint SizeInBlocks = pLayout->GetGridSize();

				FIntPoint BlockSizeInPixels(Args.BlockSize[0], Args.BlockSize[1]);

				// Image size if we don't skip any mipmap
				FIntPoint FullImageSizeInPixels = SizeInBlocks * BlockSizeInPixels;
				int32 FullImageMipCount = FImage::GetMipmapCount(FullImageSizeInPixels.X, FullImageSizeInPixels.Y);

				FIntPoint ImageSizeInPixels = FullImageSizeInPixels;
				int32 MipsToSkip = Item.ExecutionOptions;
				MipsToSkip = FMath::Min(MipsToSkip, FullImageMipCount);
				if (MipsToSkip > 0)
				{
					//FIntPoint ReducedBlockSizeInPixels;

					// This method tries to reduce only the block size, but it fails if the image is still too big
					// If we want to generate only a subset of mipmaps, reduce the layout block size accordingly.
					//ReducedBlockSizeInPixels.X = BlockSizeInPixels.X >> MipsToSkip;
					//ReducedBlockSizeInPixels.Y = BlockSizeInPixels.Y >> MipsToSkip;
					//const FImageFormatData& FormatData = GetImageFormatData((EImageFormat)Args.format);
					//int32 MinBlockSize = FMath::Max(FormatData.PixelsPerBlockX, FormatData.PixelsPerBlockY);
					//ReducedBlockSizeInPixels.X = FMath::Max<int32>(ReducedBlockSizeInPixels.X, FormatData.PixelsPerBlockX);
					//ReducedBlockSizeInPixels.Y = FMath::Max<int32>(ReducedBlockSizeInPixels.Y, FormatData.PixelsPerBlockY);
					//FIntPoint ReducedImageSizeInPixels = SizeInBlocks * ReducedBlockSizeInPixels;

					// This method simply reduces the size and assumes all the other operations will handle degeenrate cases.
					ImageSizeInPixels = FullImageSizeInPixels / (1 << MipsToSkip);
					
					//if (ReducedImageSizeInPixels!= ImageSizeInPixels)
					//{
					//	check(false);
					//}
				}

                int32 MipsToGenerate = 1;
                if (Args.GenerateMipmaps)
                {
                    if (Args.MipmapCount == 0)
                    {
						MipsToGenerate = FImage::GetMipmapCount(ImageSizeInPixels.X, ImageSizeInPixels.Y);
                    }
                    else
                    {
						MipsToGenerate = FMath::Max(Args.MipmapCount - MipsToSkip, 1);
                    }
                }

				// It needs to be initialized in case it has gaps.
                TSharedPtr<FImage> New = CreateImage(ImageSizeInPixels.X, ImageSizeInPixels.Y, MipsToGenerate, EImageFormat(Args.Format), EInitializationType::Black );
                StoreImage(Item, New);
                break;
            }

            default:
                check(false);
            }


            break;
        }

        case EOpType::IM_COMPOSE:
        {
			OP::ImageComposeArgs Args = Program.GetOpArgs<OP::ImageComposeArgs>(Item.At);

			if (ExecutionStrategy == EExecutionStrategy::MinimizeMemory)
			{
            	switch (Item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp::FromOpAndOptions(Args.layout, Item, 0));
					break;
				case 1:
				{
					TSharedPtr<const FLayout> ComposeLayout = 
							LoadLayout(FCacheAddress(Args.layout, FScheduledOp::FromOpAndOptions(Args.layout, Item, 0)));

					FScheduledOpData Data;
					Data.Resource = ComposeLayout;
					int32 DataPos = HeapData.Add(Data);

					int32 RelBlockIndex = ComposeLayout->FindBlock(Args.BlockId);

					if (RelBlockIndex >= 0)
					{
						AddOp(FScheduledOp(Item.At, Item, 2, DataPos), FScheduledOp(Args.base, Item));
					}
					else
					{
						// Jump directly to stage 3, no need to load mask or blockImage.
						AddOp(FScheduledOp(Item.At, Item, 3, DataPos), FScheduledOp(Args.base, Item));
					}

					break;
				}
				case 2:
				{
					AddOp(FScheduledOp(Item.At, Item, 3, Item.CustomState),
						  FScheduledOp(Args.blockImage, Item),
						  FScheduledOp(Args.mask, Item));
					break;
				}

				case 3:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
			}
			else
			{
            	switch (Item.Stage)
				{
				case 0:
					AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp::FromOpAndOptions(Args.layout, Item, 0));
					break;

				case 1:
				{	
					TSharedPtr<const FLayout> ComposeLayout = 
							LoadLayout(FCacheAddress(Args.layout, FScheduledOp::FromOpAndOptions(Args.layout, Item, 0)));

					FScheduledOpData Data;
					Data.Resource = ComposeLayout;
					int32 DataPos = HeapData.Add(Data);

					int32 RelBlockIndex = ComposeLayout->FindBlock(Args.BlockId);
					if (RelBlockIndex >= 0)
					{
						AddOp(FScheduledOp(Item.At, Item, 2, DataPos),
							  FScheduledOp(Args.base, Item),
							  FScheduledOp(Args.blockImage, Item),
							  FScheduledOp(Args.mask, Item));
					}
					else
					{
						AddOp(FScheduledOp(Item.At, Item, 2, DataPos), FScheduledOp(Args.base, Item));
					}
					break;
				}

				case 2:
					// This has been moved to a task. It should have been intercepted in IssueOp.
					check(false);
					break;

				default:
					check(false);
				}
			}

            break;
        }

        case EOpType::IM_INTERPOLATE:
        {
			OP::ImageInterpolateArgs Args = Program.GetOpArgs<OP::ImageInterpolateArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                       FScheduledOp( Args.Factor, Item) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_INTERPOLATE_1)
            	
                // Targets must be consecutive
                int32 count = 0;
                for ( int32 i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT && Args.Targets[i]
                    ; ++i )
                {
                    count++;
                }

                float factor = LoadScalar( FCacheAddress(Args.Factor,Item) );

                float delta = 1.0f/(count-1);
                int32 min = (int32)floorf( factor/delta );
                int32 max = (int32)ceilf( factor/delta );

                float bifactor = factor/delta - min;

                FScheduledOpData data;
                data.Interpolate.Bifactor = bifactor;
				data.Interpolate.Min = FMath::Clamp(min, 0, count - 1);
				data.Interpolate.Max = FMath::Clamp(max, 0, count - 1);
				uint32 dataPos = uint32(HeapData.Add(data));

                if ( bifactor < UE_SMALL_NUMBER )
                {
                    AddOp( FScheduledOp( Item.At, Item, 2, dataPos),
                            FScheduledOp( Args.Targets[min], Item) );
                }
                else if ( bifactor > 1.0f-UE_SMALL_NUMBER )
                {
                    AddOp( FScheduledOp( Item.At, Item, 2, dataPos),
                            FScheduledOp( Args.Targets[max], Item) );
                }
                else
                {
                    AddOp( FScheduledOp( Item.At, Item, 2, dataPos),
                            FScheduledOp( Args.Targets[min], Item),
                            FScheduledOp( Args.Targets[max], Item) );
                }
                break;
            }

            case 2:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_INTERPOLATE_2)
            		
                // Targets must be consecutive
                int32 count = 0;
                for ( int32 i=0
                    ; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT && Args.Targets[i]
                    ; ++i )
                {
                    count++;
                }

                // Factor from 0 to 1 between the two targets
                const FScheduledOpData& Data = HeapData[Item.CustomState];
                float Bifactor = Data.Interpolate.Bifactor;
                int32 Min = Data.Interpolate.Min;
                int32 Max = Data.Interpolate.Max;

                if (Bifactor < UE_SMALL_NUMBER)
                {
                    TSharedPtr<const FImage> Source = LoadImage(FCacheAddress(Args.Targets[Min], Item));
					StoreImage(Item, Source);
				}
                else if (Bifactor > 1.0f - UE_SMALL_NUMBER)
                {
                    TSharedPtr<const FImage> Source = LoadImage(FCacheAddress(Args.Targets[Max], Item));
					StoreImage(Item, Source);
				}
                else
                {
					TSharedPtr<const FImage> pMin = LoadImage(FCacheAddress(Args.Targets[Min], Item));
                    TSharedPtr<const FImage> pMax = LoadImage(FCacheAddress(Args.Targets[Max], Item));

                    if (pMin && pMax)
                    {						
						TSharedPtr<FImage> pNew = CloneOrTakeOver(pMin);

						// Be defensive: ensure image sizes match.
						if (pNew->GetSize() != pMax->GetSize())
						{
							MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForInterpolate);
							TSharedPtr<FImage> Resized = CreateImage(pNew->GetSizeX(), pNew->GetSizeY(), pMax->GetLODCount(), pMax->GetFormat(), EInitializationType::NotInitialized);
							ImOp.ImageResizeLinear(Resized.Get(), 0, pMax.Get());
							Release(pMax);
							pMax = Resized;
						}

						// Be defensive: ensure format matches.
						if (pNew->GetFormat() != pMax->GetFormat())
						{
							MUTABLE_CPUPROFILER_SCOPE(Format_ForInterpolate);

							TSharedPtr<FImage> Formatted = CreateImage(pMax->GetSizeX(), pMax->GetSizeY(), pMax->GetLODCount(), pNew->GetFormat(), EInitializationType::NotInitialized);
							
							bool bSuccess = false;
							ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), pMax.Get());
							check(bSuccess);
							
							Release(pMax);
							pMax = Formatted;
						}

						int32 LevelCount = FMath::Max(pNew->GetLODCount(), pMax->GetLODCount());

						if (pNew->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);
						
							int32 StartLevel = pNew->GetLODCount() - 1;
							// pNew is local owned, no need to CloneOrTakeOver.
							pNew->DataStorage.SetNumLODs(LevelCount);

							FMipmapGenerationSettings MipSettings{};
							ImOp.ImageMipmap(Settings.ImageCompressionQuality, pNew.Get(), pNew.Get(), StartLevel, LevelCount, MipSettings);

						}

						if (pMax->GetLODCount() != LevelCount)
						{
							MUTABLE_CPUPROFILER_SCOPE(Mipmap_ForInterpolate);

							int32 StartLevel = pMax->GetLODCount() - 1;

							TSharedPtr<FImage> MaxFix = CloneOrTakeOver(pMax);
							MaxFix->DataStorage.SetNumLODs(LevelCount);
							
							FMipmapGenerationSettings MipSettings{};
							ImOp.ImageMipmap(Settings.ImageCompressionQuality, MaxFix.Get(), MaxFix.Get(), StartLevel, LevelCount, MipSettings);

							pMax = MaxFix;
						}

                        ImageInterpolate(pNew.Get(), pMax.Get(), Bifactor);

						Release(pMax);
						StoreImage(Item, pNew);
					}
                    else if (pMin)
                    {
						StoreImage(Item, pMin);
					}
                    else if (pMax)
                    {
						StoreImage(Item, pMax);
					}
				}

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_SATURATE:
        {
			OP::ImageSaturateArgs Args = Program.GetOpArgs<OP::ImageSaturateArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.Base, Item ),
                        FScheduledOp::FromOpAndOptions( Args.Factor, Item, 0 ));
                break;

            case 1:
            {
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_LUMINANCE:
        {
			OP::ImageLuminanceArgs Args = Program.GetOpArgs<OP::ImageLuminanceArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.Base, Item ) );
                break;

            case 1:
            {
           		MUTABLE_CPUPROFILER_SCOPE(IM_LUMINANCE_1)
            		
                TSharedPtr<const FImage> Base = LoadImage( FCacheAddress(Args.Base,Item) );

				TSharedPtr<FImage> Result = CreateImage(Base->GetSizeX(), Base->GetSizeY(), Base->GetLODCount(), EImageFormat::L_UByte, EInitializationType::NotInitialized);
                ImageLuminance( Result.Get(),Base.Get() );

				Release(Base);
				StoreImage( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_SWIZZLE:
        {
			OP::ImageSwizzleArgs Args = Program.GetOpArgs<OP::ImageSwizzleArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.sources[0], Item ),
                        FScheduledOp( Args.sources[1], Item ),
                        FScheduledOp( Args.sources[2], Item ),
                        FScheduledOp( Args.sources[3], Item ) );
                break;

            case 1:
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
				break;

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_COLOURMAP:
        {
			OP::ImageColourMapArgs Args = Program.GetOpArgs<OP::ImageColourMapArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.Base, Item ),
                           FScheduledOp( Args.Mask, Item ),
                           FScheduledOp( Args.Map, Item ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_COLOURMAP_1)
            		
                TSharedPtr<const FImage> Source = LoadImage( FCacheAddress(Args.Base,Item) );
                TSharedPtr<const FImage> Mask = LoadImage( FCacheAddress(Args.Mask,Item) );
                TSharedPtr<const FImage> Map = LoadImage( FCacheAddress(Args.Map,Item) );

				bool bOnlyOneMip = (Mask->GetLODCount() < Source->GetLODCount());

				// Be defensive: ensure image sizes match.
				if (Mask->GetSize() != Source->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForColourmap);
					TSharedPtr<FImage> Resized = CreateImage(Source->GetSizeX(), Source->GetSizeY(), 1, Mask->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Resized.Get(), 0, Mask.Get());
					Release(Mask);
					Mask = Resized;
				}

				TSharedPtr<FImage> Result = CreateImage(Source->GetSizeX(), Source->GetSizeY(), Source->GetLODCount(), Source->GetFormat(), EInitializationType::NotInitialized);
				ImageColourMap( Result.Get(), Source.Get(), Mask.Get(), Map.Get(), bOnlyOneMip);

				if (bOnlyOneMip)
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageColourMap_MipFix);
					FMipmapGenerationSettings DummyMipSettings{};
					ImageMipmapInPlace(Settings.ImageCompressionQuality, Result.Get(), DummyMipSettings);
				}

				Release(Source);
				Release(Mask);
				Release(Map);
				StoreImage( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_BINARISE:
        {
			OP::ImageBinariseArgs Args = Program.GetOpArgs<OP::ImageBinariseArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.Base, Item ),
                        FScheduledOp::FromOpAndOptions( Args.Threshold, Item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_BINARISE_1)
            		
                TSharedPtr<const FImage> pA = LoadImage( FCacheAddress(Args.Base,Item) );

                float c = LoadScalar(FScheduledOp::FromOpAndOptions(Args.Threshold, Item, 0));

                TSharedPtr<FImage> Result = CreateImage(pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::L_UByte, EInitializationType::NotInitialized);
				ImageBinarise( Result.Get(), pA.Get(), c );

				Release(pA);
				StoreImage( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::IM_INVERT:
		{
			OP::ImageInvertArgs Args = Program.GetOpArgs<OP::ImageInvertArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp(Args.Base, Item));
				break;

			case 1:
			{
				// This has been moved to a task. It should have been intercepted in IssueOp.
				check(false);
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::IM_PLAINCOLOUR:
        {
			OP::ImagePlainColorArgs Args = Program.GetOpArgs<OP::ImagePlainColorArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
				AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp::FromOpAndOptions( Args.Color, Item, 0 ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_PLAINCOLOUR_1)
            		
				FVector4f c = LoadColor(FScheduledOp::FromOpAndOptions(Args.Color, Item, 0));

				uint16 SizeX = Args.Size[0];
				uint16 SizeY = Args.Size[1];
				int32 LODs = Args.LODs;
				
				// This means all the mip chain
				if (LODs == 0)
				{
					LODs = FMath::CeilLogTwo(FMath::Max(SizeX,SizeY));
				}

				for (int32 l=0; l<Item.ExecutionOptions; ++l)
				{
					SizeX = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeY, uint16(2)));
					--LODs;
				}

                TSharedPtr<FImage> pA = CreateImage( SizeX, SizeY, FMath::Max(LODs,1), EImageFormat(Args.Format), EInitializationType::NotInitialized );

				ImOp.FillColor(pA.Get(), c);

				StoreImage( Item, pA );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::IM_REFERENCE:
		{
			OP::ResourceReferenceArgs Args = Program.GetOpArgs<OP::ResourceReferenceArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				TSharedPtr<FImage> Result;
				if (Args.ForceLoad)
				{
					// This should never be reached because it should have been caught as a Task in IssueOp
					check(false);
				}
				else
				{
					Result = FImage::CreateAsReference(Args.ID, Args.ImageDesc, false);
				}
				StoreImage(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        case EOpType::IM_CROP:
        {
			OP::ImageCropArgs Args = Program.GetOpArgs<OP::ImageCropArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.source, Item ) );
                break;

            case 1:
            {
            	MUTABLE_CPUPROFILER_SCOPE(IM_CROP_1)
            		
                TSharedPtr<const FImage> pA = LoadImage( FCacheAddress(Args.source,Item) );

                box< UE::Math::TIntVector2<int32> > rect;
                rect.min[0] = Args.minX;
                rect.min[1] = Args.minY;
                rect.size[0] = Args.sizeX;
                rect.size[1] = Args.sizeY;

				// Apply ther mipmap reduction to the crop rectangle.
				int32 MipsToSkip = Item.ExecutionOptions;
				while ( MipsToSkip>0 && rect.size[0]>0 && rect.size[1]>0 )
				{
					rect.min[0] /= 2;
					rect.min[1] /= 2;
					rect.size[0] /= 2;
					rect.size[1] /= 2;
					MipsToSkip--;
				}

				TSharedPtr<FImage> Result;
				if (!rect.IsEmpty())
				{
					Result = CreateImage( rect.size[0], rect.size[1], 1, pA->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageCrop(Result, Settings.ImageCompressionQuality, pA.Get(), rect);
				}

				Release(pA);
				StoreImage( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_PATCH:
        {
			// TODO: This is optimized for memory-usage but base and patch could be requested at the same time
			OP::ImagePatchArgs Args = Program.GetOpArgs<OP::ImagePatchArgs>(Item.At);
            switch (Item.Stage)
            {
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1), FScheduledOp(Args.base, Item));
				break;

			case 1:
				AddOp(FScheduledOp(Item.At, Item, 2), FScheduledOp(Args.patch, Item));
				break;

			case 2:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_PATCH_1)

                TSharedPtr<const FImage> pA = LoadImage( FCacheAddress(Args.base,Item) );
                TSharedPtr<const FImage> pB = LoadImage( FCacheAddress(Args.patch,Item) );

				// Failsafe
				if (!pA || !pB)
				{
					Release(pB);
					StoreImage(Item, pA);
					break;
				}

				// Apply the mipmap reduction to the crop rectangle.
				int32 MipsToSkip = Item.ExecutionOptions;
				box<FIntVector2> rect;
				rect.min[0] = Args.minX / (1 << MipsToSkip);
				rect.min[1] = Args.minY / (1 << MipsToSkip);
				rect.size[0] = pB->GetSizeX();
				rect.size[1] = pB->GetSizeY();

                TSharedPtr<FImage> Result = CloneOrTakeOver(pA);

				bool bApplyPatch = !rect.IsEmpty();
				if (bApplyPatch)
				{
					// Change the block image format if it doesn't match the composed image
					// This is usually enforced at object compilation time.
					if (Result->GetFormat() != pB->GetFormat())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImagPatchReformat);

						EImageFormat format = GetMostGenericFormat(Result->GetFormat(), pB->GetFormat());

						const FImageFormatData& finfo = GetImageFormatData(format);
						if (finfo.PixelsPerBlockX == 0)
						{
							format = GetUncompressedFormat(format);
						}

						if (Result->GetFormat() != format)
						{
							TSharedPtr<FImage> Formatted = CreateImage(Result->GetSizeX(), Result->GetSizeY(), Result->GetLODCount(), format, EInitializationType::NotInitialized);
							bool bSuccess = false;
							ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), Result.Get());
							check(bSuccess);
							Release(Result);
							Result = Formatted;
						}
						if (pB->GetFormat() != format)
						{
							TSharedPtr<FImage> Formatted = CreateImage(pB->GetSizeX(), pB->GetSizeY(), pB->GetLODCount(), format, EInitializationType::NotInitialized);
							bool bSuccess = false;
							ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), pB.Get());
							check(bSuccess);
							Release(pB);
							pB = Formatted;
						}
					}

					// Don't patch if below the image compression block size.
					const FImageFormatData& finfo = GetImageFormatData(Result->GetFormat());
					bApplyPatch =
						(rect.min[0] % finfo.PixelsPerBlockX == 0) &&
						(rect.min[1] % finfo.PixelsPerBlockY == 0) &&
						(rect.size[0] % finfo.PixelsPerBlockX == 0) &&
						(rect.size[1] % finfo.PixelsPerBlockY == 0) &&
						(rect.min[0] + rect.size[0]) <= Result->GetSizeX() &&
						(rect.min[1] + rect.size[1]) <= Result->GetSizeY()
						;
				}

				if (bApplyPatch)
				{
					ImOp.ImageCompose(Result.Get(), pB.Get(), rect);
					Result->Flags = 0;
				}
				else
				{
					// This happens very often when skipping mips, and floods the log.
					//UE_LOG( LogMutableCore, Verbose, TEXT("Skipped patch operation for image not fitting the block compression size. Small image? Patch rect is (%d, %d), (%d, %d), base is (%d, %d)"),
					//	rect.min[0], rect.min[1], rect.size[0], rect.size[1], Result->GetSizeX(), Result->GetSizeY());
				}

				Release(pB);
				StoreImage( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_RASTERMESH:
        {
			OP::ImageRasterMeshArgs Args = Program.GetOpArgs<OP::ImageRasterMeshArgs>(Item.At);

			constexpr uint8 MeshContentFilter = (uint8)(EMeshContentFlags::GeometryData | EMeshContentFlags::PoseData);
            switch (Item.Stage)
            {
            case 0:
				if (Args.image)
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp::FromOpAndOptions(Args.mesh, Item, MeshContentFilter),
						FScheduledOp::FromOpAndOptions(Args.projector, Item, 0));
				}
				else
				{
					AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp::FromOpAndOptions(Args.mesh, Item, MeshContentFilter));
				}
                break;

			case 1:
			{
				MUTABLE_CPUPROFILER_SCOPE(IM_RASTERMESH_1)

				
				TSharedPtr<const FMesh> Mesh = 
						LoadMesh(FScheduledOp::FromOpAndOptions(Args.mesh, Item, MeshContentFilter));

				// If no image, we are generating a flat mesh UV raster. This is the final stage in this case.
				if (!Args.image)
				{
					uint16 SizeX = Args.sizeX;
					uint16 SizeY = Args.sizeY;
					UE::Math::TIntVector2<uint16> CropMin(Args.CropMinX, Args.CropMinY);
					UE::Math::TIntVector2<uint16> UncroppedSize(Args.UncroppedSizeX, Args.UncroppedSizeY);

					// Drop mips while possible
					int32 MipsToDrop = Item.ExecutionOptions;
					bool bUseCrop = UncroppedSize[0] > 0;
					while (MipsToDrop && !(SizeX % 2) && !(SizeY % 2))
					{
						SizeX = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeX, uint16(2)));
						SizeY = FMath::Max(uint16(1), FMath::DivideAndRoundUp(SizeY, uint16(2)));
						if (bUseCrop)
						{
							CropMin[0] = FMath::DivideAndRoundUp(CropMin[0], uint16(2));
							CropMin[1] = FMath::DivideAndRoundUp(CropMin[1], uint16(2));
							UncroppedSize[0] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[0], uint16(2)));
							UncroppedSize[1] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[1], uint16(2)));
						}
						--MipsToDrop;
					}

                    // Flat mesh UV raster
					TSharedPtr<FImage> ResultImage = CreateImage(SizeX, SizeY, 1, EImageFormat::L_UByte, EInitializationType::Black);
					if (Mesh)
					{
						ImageRasterMesh(Mesh.Get(), ResultImage.Get(), Args.LayoutIndex, Args.BlockId, CropMin, UncroppedSize);
						Release(Mesh);
					}

					// Stop execution.
					StoreImage(Item, ResultImage);
					break;
				}

				const int32 MipsToSkip = Item.ExecutionOptions;
				int32 ProjectionMip = MipsToSkip;

				FScheduledOpData Data;
				Data.RasterMesh.Mip = ProjectionMip;
				Data.RasterMesh.MipValue = static_cast<float>(ProjectionMip);
				FProjector Projector = LoadProjector(FScheduledOp::FromOpAndOptions(Args.projector, Item, 0));

				EMinFilterMethod MinFilterMethod = Invoke([&]() -> EMinFilterMethod
				{
					if (ForcedProjectionMode == 0)
					{
						return EMinFilterMethod::None;
					}
					else if (ForcedProjectionMode == 1)
					{
						return EMinFilterMethod::TotalAreaHeuristic;
					}
						
					return static_cast<EMinFilterMethod>(Args.MinFilterMethod);
				});

				if (MinFilterMethod == EMinFilterMethod::TotalAreaHeuristic)
				{
					FVector2f TargetImageSizeF = FVector2f(
						FMath::Max(Args.sizeX >> MipsToSkip, 1),
						FMath::Max(Args.sizeY >> MipsToSkip, 1));
					FVector2f SourceImageSizeF = FVector2f(Args.SourceSizeX, Args.SourceSizeY);
						
					if (Mesh)
					{ 
						const float ComputedMip = ComputeProjectedFootprintBestMip(Mesh.Get(), Projector, TargetImageSizeF, SourceImageSizeF);

						Data.RasterMesh.MipValue = FMath::Max(0.0f, ComputedMip + GlobalProjectionLodBias);
						Data.RasterMesh.Mip = static_cast<uint8>(FMath::FloorToInt32(Data.RasterMesh.MipValue));
					}
				}
		
				const int32 DataHeapAddress = HeapData.Add(Data);

				// Mesh is need again in the next stage, store it in the heap.
				HeapData[DataHeapAddress].Resource = Mesh;

				AddOp(FScheduledOp(Item.At, Item, 2, DataHeapAddress),
					FScheduledOp::FromOpAndOptions(Args.projector, Item, 0),
					FScheduledOp::FromOpAndOptions(Args.image, Item, Data.RasterMesh.Mip),
					FScheduledOp(Args.mask, Item),
					FScheduledOp::FromOpAndOptions(Args.angleFadeProperties, Item, 0));

				break;
			}

            case 2:
            {
				MUTABLE_CPUPROFILER_SCOPE(IM_RASTERMESH_2)

				if (!Args.image)
				{
					// This case is treated at the previous stage.
					check(false);
					StoreImage(Item, nullptr);
					break;
				}

				FScheduledOpData& Data = HeapData[Item.CustomState];

				// Unsafe downcast, should be fine as it is known to be a Mesh.
				TSharedPtr<const FMesh> Mesh = StaticCastSharedPtr<FMesh>(ConstCastSharedPtr<FResource>(Data.Resource));
				Data.Resource = nullptr;

				if (!Mesh)
				{
					check(false);
					StoreImage(Item, nullptr);
					break;
				}

				uint16 SizeX = Args.sizeX;
				uint16 SizeY = Args.sizeY;
				UE::Math::TIntVector2<uint16> CropMin(Args.CropMinX, Args.CropMinY);
				UE::Math::TIntVector2<uint16> UncroppedSize(Args.UncroppedSizeX, Args.UncroppedSizeY);

				// Drop mips while possible
				int32 MipsToDrop = Item.ExecutionOptions;
				bool bUseCrop = UncroppedSize[0] > 0;
				while (MipsToDrop && !(SizeX % 2) && !(SizeY % 2))
				{
					SizeX = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeX, uint16(2)));
					SizeY = FMath::Max(uint16(1),FMath::DivideAndRoundUp(SizeY, uint16(2)));
					if (bUseCrop)
					{
						CropMin[0] = FMath::DivideAndRoundUp(CropMin[0], uint16(2));
						CropMin[1] = FMath::DivideAndRoundUp(CropMin[1], uint16(2));
						UncroppedSize[0] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[0], uint16(2)));
						UncroppedSize[1] = FMath::Max(uint16(1), FMath::DivideAndRoundUp(UncroppedSize[1], uint16(2)));
					}
					--MipsToDrop;
				}

				// Raster with projection
				TSharedPtr<const FImage> Source = LoadImage(FCacheAddress(Args.image, Item.ExecutionIndex, Data.RasterMesh.Mip));

				TSharedPtr<const FImage> Mask = nullptr;
				if (Args.mask)
				{
					Mask = LoadImage(FCacheAddress(Args.mask, Item));

					// TODO: This shouldn't happen, but be defensive.
					FImageSize ResultSize(SizeX, SizeY);
					if (Mask && Mask->GetSize()!= ResultSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_MaskFixForProjection);

						TSharedPtr<FImage> Resized = CreateImage(SizeX, SizeY, Mask->GetLODCount(), Mask->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.Get(), 0, Mask.Get());
						Release(Mask);
						Mask = Resized;
					}
				}

				float fadeStart = 180.0f;
				float fadeEnd = 180.0f;
				if ( Args.angleFadeProperties )
				{
					FVector4f fadeProperties = LoadColor(FScheduledOp::FromOpAndOptions(Args.angleFadeProperties, Item, 0));
					fadeStart = fadeProperties[0];
					fadeEnd = fadeProperties[1];
				}
				const float FadeStartRad = FMath::DegreesToRadians(fadeStart);
				const float FadeEndRad = FMath::DegreesToRadians(fadeEnd);

				EImageFormat Format = Source ? GetUncompressedFormat(Source->GetFormat()) : EImageFormat::L_UByte;

				if (Source && Source->GetFormat()!=Format)
				{
					MUTABLE_CPUPROFILER_SCOPE(RunCode_RasterMesh_ReformatSource);
					TSharedPtr<FImage> Formatted = CreateImage(Source->GetSizeX(), Source->GetSizeY(), Source->GetLODCount(), Format, EInitializationType::NotInitialized);
					bool bSuccess = false;
					ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), Source.Get());
					check(bSuccess); 
					Release(Source);
					Source = Formatted;
				}
			
				EMinFilterMethod MinFilterMethod = Invoke([&]() -> EMinFilterMethod
				{
					if (ForcedProjectionMode == 0)
					{
						return EMinFilterMethod::None;
					}
					else if (ForcedProjectionMode == 1)
					{
						return EMinFilterMethod::TotalAreaHeuristic;
					}
						
					return static_cast<EMinFilterMethod>(Args.MinFilterMethod);
				});

				if (MinFilterMethod == EMinFilterMethod::TotalAreaHeuristic)
				{
					const uint16 Mip = Data.RasterMesh.Mip;
					const FImageSize ExpectedSourceSize = FImageSize(
							FMath::Max<uint16>(Args.SourceSizeX >> Mip, 1), 
							FMath::Max<uint16>(Args.SourceSizeY >> Mip, 1));

					if (Source->GetSize() != ExpectedSourceSize)
					{
						MUTABLE_CPUPROFILER_SCOPE(RunCode_ImageRasterMesh_SizeFixup);	
						
						TSharedPtr<FImage> Resized = CreateImage(ExpectedSourceSize.X, ExpectedSourceSize.Y, 1, Format, EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.Get(), Settings.ImageCompressionQuality, Source.Get());
						
						Release(Source);
						Source = Resized;	
					}
				}

				// Allocate memory for the temporary buffers
				FScratchImageProject Scratch;
				Scratch.Vertices.SetNum(Mesh->GetVertexCount());
				Scratch.CulledVertex.SetNum(Mesh->GetVertexCount());

				ESamplingMethod SamplingMethod = Invoke([&]() -> ESamplingMethod
				{
					if (ForcedProjectionMode == 0)
					{
						return ESamplingMethod::Point;
					}
					else if (ForcedProjectionMode == 1)
					{
						return ESamplingMethod::BiLinear;
					}
					
					return static_cast<ESamplingMethod>(Args.SamplingMethod);
				});

				if (SamplingMethod == ESamplingMethod::BiLinear)
				{
					if (Source->GetLODCount() < 2 && Source->GetSizeX() > 1 && Source->GetSizeY() > 1)
					{
						MUTABLE_CPUPROFILER_SCOPE(RunCode_RasterMesh_BilinearMipGen);

						TSharedPtr<FImage> OwnedSource = CloneOrTakeOver(Source);

						OwnedSource->DataStorage.SetNumLODs(2);
						ImageMipmapInPlace(0, OwnedSource.Get(), FMipmapGenerationSettings{});

						Source = OwnedSource;
					}
				}

				// Allocate new image after bilinear mip generation to reduce operation memory peak.
				TSharedPtr<FImage> New = CreateImage(SizeX, SizeY, 1, Format, EInitializationType::Black);

				if (Args.projector && Source && Source->GetSizeX() > 0 && Source->GetSizeY() > 0)
				{
					FProjector Projector = LoadProjector(FScheduledOp::FromOpAndOptions(Args.projector, Item, 0));

					switch (Projector.type)
					{
					case EProjectorType::Planar:
						ImageRasterProjectedPlanar(Mesh.Get(), New.Get(),
							Source.Get(), Mask.Get(),
							Args.bIsRGBFadingEnabled, Args.bIsAlphaFadingEnabled,
							SamplingMethod,
							FadeStartRad, FadeEndRad, FMath::Frac(Data.RasterMesh.MipValue),
							Args.LayoutIndex, Args.BlockId,
							CropMin, UncroppedSize,
							&Scratch, bUseProjectionVectorImpl);
						break;

					case EProjectorType::Wrapping:
						ImageRasterProjectedWrapping(Mesh.Get(), New.Get(),
							Source.Get(), Mask.Get(),
							Args.bIsRGBFadingEnabled, Args.bIsAlphaFadingEnabled,
							SamplingMethod,
							FadeStartRad, FadeEndRad, FMath::Frac(Data.RasterMesh.MipValue),
							Args.LayoutIndex, Args.BlockId,
							CropMin, UncroppedSize,
							&Scratch, bUseProjectionVectorImpl);
						break;

					case EProjectorType::Cylindrical:
						ImageRasterProjectedCylindrical(Mesh.Get(), New.Get(),
							Source.Get(), Mask.Get(),
							Args.bIsRGBFadingEnabled, Args.bIsAlphaFadingEnabled,
							SamplingMethod,
							FadeStartRad, FadeEndRad, FMath::Frac(Data.RasterMesh.MipValue),
							Args.LayoutIndex,
							Projector.projectionAngle,
							CropMin, UncroppedSize,
							&Scratch, bUseProjectionVectorImpl);
						break;

					default:
						check(false);
						break;
					}
				}

				Release(Mesh);
				Release(Source);
				Release(Mask);
				StoreImage(Item, New);

                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_MAKEGROWMAP:
        {
			OP::ImageMakeGrowMapArgs Args = Program.GetOpArgs<OP::ImageMakeGrowMapArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1), FScheduledOp( Args.mask, Item) );
                break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_MAKEGROWMAP_1)

                TSharedPtr<const FImage> Mask = LoadImage( FCacheAddress(Args.mask,Item) );

                TSharedPtr<FImage> Result = CreateImage( Mask->GetSizeX(), Mask->GetSizeY(), Mask->GetLODCount(), EImageFormat::L_UByte, EInitializationType::NotInitialized);

                ImageMakeGrowMap(Result.Get(), Mask.Get(), Args.border );
				Result->Flags |= FImage::IF_CANNOT_BE_SCALED;

				Release(Mask);
                StoreImage( Item, Result);
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_DISPLACE:
        {
			OP::ImageDisplaceArgs Args = Program.GetOpArgs<OP::ImageDisplaceArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.Source, Item ),
                        FScheduledOp( Args.DisplacementMap, Item ) );
				break;

            case 1:
            {
                MUTABLE_CPUPROFILER_SCOPE(IM_DISPLACE_1)

                TSharedPtr<const FImage> Source = LoadImage( FCacheAddress(Args.Source,Item) );
                TSharedPtr<const FImage> pMap = LoadImage( FCacheAddress(Args.DisplacementMap,Item) );

				if (!Source)
				{
					Release(pMap);
					StoreImage(Item, nullptr);
					break;
				}

				// TODO: This shouldn't happen: displacement maps cannot be scaled because their information
				// is resolution sensitive (pixel offsets). If the size doesn't match, scale the source, apply 
				// displacement and then unscale it.
				FImageSize OriginalSourceScale = Source->GetSize();
				if (OriginalSourceScale[0]>0 && OriginalSourceScale[1]>0 && OriginalSourceScale != pMap->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep1);

					TSharedPtr<FImage> Resized = CreateImage(pMap->GetSizeX(), pMap->GetSizeY(), Source->GetLODCount(), Source->GetFormat(), EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Resized.Get(), 0, Source.Get());
					Release(Source);
					Source = Resized;
				}

				// This works based on the assumption that displacement maps never read from a position they actually write to.
				// Since they are used for UV border expansion, this should always be the case.
				TSharedPtr<FImage> Result = CloneOrTakeOver(Source);

				if (OriginalSourceScale[0] > 0 && OriginalSourceScale[1] > 0)
				{
					ImageDisplace(Result.Get(), Result.Get(), pMap.Get());

					if (OriginalSourceScale != Result->GetSize())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyHackForDisplacementStep2);
						TSharedPtr<FImage> Resized = CreateImage(OriginalSourceScale[0], OriginalSourceScale[1], Result->GetLODCount(), Result->GetFormat(), EInitializationType::NotInitialized);
						ImOp.ImageResizeLinear(Resized.Get(), 0, Result.Get());
						Release(Result);
						Result = Resized;
					}
				}

				Release(pMap);
                StoreImage( Item, Result);
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::IM_TRANSFORM:
        {
            const OP::ImageTransformArgs Args = Program.GetOpArgs<OP::ImageTransformArgs>(Item.At);

            switch (Item.Stage)
            {
            case 0:
			{
				const TArray<FScheduledOp, TFixedAllocator<2>> Deps = 
				{
					FScheduledOp(Args.ScaleX, Item),
					FScheduledOp(Args.ScaleY, Item),
				};

                AddOp(FScheduledOp(Item.At, Item, 1), Deps);

				break;
			}
			case 1:
			{
            	MUTABLE_CPUPROFILER_SCOPE(IM_TRANSFORM_1)

				FVector2f Scale = FVector2f(
                        Args.ScaleX ? LoadScalar(FCacheAddress(Args.ScaleX, Item)) : 1.0f,
                        Args.ScaleY ? LoadScalar(FCacheAddress(Args.ScaleY, Item)) : 1.0f);
	
				using FUint16Vector2 = UE::Math::TIntVector2<uint16>;
				const FUint16Vector2 DestSizeI = Invoke([&]() 
				{
					int32 MipsToDrop = Item.ExecutionOptions;
					
					FUint16Vector2 Size = FUint16Vector2(
							Args.SizeX > 0 ? Args.SizeX : Args.SourceSizeX, 
							Args.SizeY > 0 ? Args.SizeY : Args.SourceSizeY); 

					while (MipsToDrop && !(Size.X % 2) && !(Size.Y % 2))
					{
						Size.X = FMath::Max(uint16(1), FMath::DivideAndRoundUp(Size.X, uint16(2)));
						Size.Y = FMath::Max(uint16(1), FMath::DivideAndRoundUp(Size.Y, uint16(2)));
						--MipsToDrop;
					}

					return FUint16Vector2(FMath::Max(Size.X, uint16(1)), FMath::Max(Size.Y, uint16(1)));
				});

				const FVector2f DestSize   = FVector2f(DestSizeI.X, DestSizeI.Y);
				const FVector2f SourceSize = FVector2f(FMath::Max(Args.SourceSizeX, uint16(1)), FMath::Max(Args.SourceSizeY, uint16(1)));

				FVector2f AspectCorrectionScale = FVector2f(1.0f, 1.0f);
				if (Args.bKeepAspectRatio)
				{
					const float DestAspectOverSrcAspect = (DestSize.X * SourceSize.Y) / (DestSize.Y * SourceSize.X);

					AspectCorrectionScale = DestAspectOverSrcAspect > 1.0f 
										  ? FVector2f(1.0f/DestAspectOverSrcAspect, 1.0f) 
										  : FVector2f(1.0f, DestAspectOverSrcAspect); 
				}
			
				const FTransform2f Transform = FTransform2f(FVector2f(-0.5f))
					.Concatenate(FTransform2f(FScale2f(Scale)))
					.Concatenate(FTransform2f(FScale2f(AspectCorrectionScale)))
					.Concatenate(FTransform2f(FVector2f(0.5f)));

				FBox2f NormalizedCropRect(ForceInit);
				NormalizedCropRect += Transform.TransformPoint(FVector2f(0.0f, 0.0f));
				NormalizedCropRect += Transform.TransformPoint(FVector2f(1.0f, 0.0f));
				NormalizedCropRect += Transform.TransformPoint(FVector2f(0.0f, 1.0f));
				NormalizedCropRect += Transform.TransformPoint(FVector2f(1.0f, 1.0f));

				const FVector2f ScaledSourceSize = NormalizedCropRect.GetSize() * DestSize;

				const float BestMip = 
					FMath::Log2(FMath::Max(1.0f, FMath::Square(SourceSize.GetMin()))) * 0.5f - 
				    FMath::Log2(FMath::Max(1.0f, FMath::Square(ScaledSourceSize.GetMin()))) * 0.5f;

				FScheduledOpData OpHeapData;
				OpHeapData.ImageTransform.SizeX = DestSizeI.X;
				OpHeapData.ImageTransform.SizeY = DestSizeI.Y;
				FPlatformMath::StoreHalf(&OpHeapData.ImageTransform.ScaleXEncodedHalf, Scale.X);
				FPlatformMath::StoreHalf(&OpHeapData.ImageTransform.ScaleYEncodedHalf, Scale.Y);
				OpHeapData.ImageTransform.MipValue = BestMip + GlobalImageTransformLodBias;

				const int32 HeapDataAddress = HeapData.Add(OpHeapData);

				const uint8 Mip = static_cast<uint8>(FMath::Max(0, FMath::FloorToInt(OpHeapData.ImageTransform.MipValue)));
				const TArray<FScheduledOp, TFixedAllocator<4>> Deps = 
				{
					FScheduledOp::FromOpAndOptions(Args.Base, Item, Mip),
					FScheduledOp(Args.OffsetX,  Item),
					FScheduledOp(Args.OffsetY,  Item),
					FScheduledOp(Args.Rotation, Item) 
				};
				
                AddOp(FScheduledOp(Item.At, Item, 2, HeapDataAddress), Deps);

				break;
			}
            case 2:
            {
				MUTABLE_CPUPROFILER_SCOPE(IM_TRANSFORM_2);
			
				const FScheduledOpData OpHeapData = HeapData[Item.CustomState];

				const uint8 Mip = static_cast<uint8>(FMath::Max(0, FMath::FloorToInt(OpHeapData.ImageTransform.MipValue)));
				TSharedPtr<const FImage> Source = LoadImage(FCacheAddress(Args.Base, Item.ExecutionIndex, Mip));

				const FVector2f Offset = FVector2f(
                        Args.OffsetX ? LoadScalar(FCacheAddress(Args.OffsetX, Item)) : 0.0f,
                        Args.OffsetY ? LoadScalar(FCacheAddress(Args.OffsetY, Item)) : 0.0f);

                FVector2f Scale = FVector2f(
						FPlatformMath::LoadHalf(&OpHeapData.ImageTransform.ScaleXEncodedHalf),
						FPlatformMath::LoadHalf(&OpHeapData.ImageTransform.ScaleYEncodedHalf));

				FVector2f AspectCorrectionScale = FVector2f(1.0f, 1.0f);
				if (Args.bKeepAspectRatio)
				{
					const FVector2f DestSize   = FVector2f(OpHeapData.ImageTransform.SizeX, OpHeapData.ImageTransform.SizeY);
					const FVector2f SourceSize = FVector2f(FMath::Max(Args.SourceSizeX, uint16(1)), FMath::Max(Args.SourceSizeY, uint16(1)));
					
					const float DestAspectOverSrcAspect = (DestSize.X * SourceSize.Y) / (DestSize.Y * SourceSize.X);
					
					AspectCorrectionScale = DestAspectOverSrcAspect > 1.0f 
										  ? FVector2f(1.0f/DestAspectOverSrcAspect, 1.0f) 
										  : FVector2f(1.0f, DestAspectOverSrcAspect); 
				}

				// Map Range [0..1] to a full rotation
                const float RotationRad = LoadScalar(FCacheAddress(Args.Rotation, Item)) * UE_TWO_PI;
	
				EImageFormat SourceFormat = Source->GetFormat();
				EImageFormat Format = GetUncompressedFormat(SourceFormat);

				if (Format != SourceFormat)
				{
					MUTABLE_CPUPROFILER_SCOPE(RunCode_ImageTransform_FormatFixup);	
					TSharedPtr<FImage> Formatted = CreateImage(Source->GetSizeX(), Source->GetSizeY(), Source->GetLODCount(), Format, EInitializationType::NotInitialized);
					bool bSuccess = false;
					ImOp.ImagePixelFormat(bSuccess, Settings.ImageCompressionQuality, Formatted.Get(), Source.Get());
					check(bSuccess); 

					Release(Source);
					Source = Formatted;
				}

				const FImageSize ExpectedSourceSize = FImageSize(
						FMath::Max<uint16>(Args.SourceSizeX >> (uint16)Mip, 1), 
						FMath::Max<uint16>(Args.SourceSizeY >> (uint16)Mip, 1));
				if (Source->GetSize() != ExpectedSourceSize)
				{
					MUTABLE_CPUPROFILER_SCOPE(RunCode_ImageTransform_SizeFixup);	
					
					TSharedPtr<FImage> Resized = CreateImage(ExpectedSourceSize.X, ExpectedSourceSize.Y, 1, Format, EInitializationType::NotInitialized);
					ImOp.ImageResizeLinear(Resized.Get(), Settings.ImageCompressionQuality, Source.Get());
					
					Release(Source);
					Source = Resized;	
				}

				if (Source->GetLODCount() < 2 && Source->GetSizeX() > 1 && Source->GetSizeY() > 1)
				{
					MUTABLE_CPUPROFILER_SCOPE(RunCode_ImageTransform_BilinearMipGen);

					TSharedPtr<FImage> OwnedSource = CloneOrTakeOver(Source);
					OwnedSource->DataStorage.SetNumLODs(2);

					ImageMipmapInPlace(0, OwnedSource.Get(), FMipmapGenerationSettings{});

					Source = OwnedSource;
				}

				Scale.X = FMath::IsNearlyZero(Scale.X, UE_KINDA_SMALL_NUMBER) ? UE_KINDA_SMALL_NUMBER : Scale.X;
				Scale.Y = FMath::IsNearlyZero(Scale.Y, UE_KINDA_SMALL_NUMBER) ? UE_KINDA_SMALL_NUMBER : Scale.Y;

				AspectCorrectionScale.X = FMath::IsNearlyZero(AspectCorrectionScale.X, UE_KINDA_SMALL_NUMBER) 
									    ? UE_KINDA_SMALL_NUMBER 
										: AspectCorrectionScale.X;

				AspectCorrectionScale.Y = FMath::IsNearlyZero(AspectCorrectionScale.Y, UE_KINDA_SMALL_NUMBER) 
										? UE_KINDA_SMALL_NUMBER 
										: AspectCorrectionScale.Y;

				const FTransform2f Transform = FTransform2f(FVector2f(-0.5f))
						.Concatenate(FTransform2f(FScale2f(Scale)))
						.Concatenate(FTransform2f(FQuat2f(RotationRad)))
						.Concatenate(FTransform2f(FScale2f(AspectCorrectionScale)))
						.Concatenate(FTransform2f(Offset + FVector2f(0.5f)));

				const EAddressMode AddressMode = static_cast<EAddressMode>(Args.AddressMode);

				const EInitializationType InitType = AddressMode == EAddressMode::ClampToBlack 
											       ? EInitializationType::Black
											       : EInitializationType::NotInitialized;

				TSharedPtr<FImage> Result = CreateImage(
						OpHeapData.ImageTransform.SizeX, OpHeapData.ImageTransform.SizeY, 1, Format, InitType);

				const float MipFactor = FMath::Frac(FMath::Max(0.0f, OpHeapData.ImageTransform.MipValue));
				ImageTransform(Result.Get(), Source.Get(), Transform, MipFactor, AddressMode, bUseImageTransformVectorImpl);

				Release(Source);
				StoreImage(Item, Result);

                break;
            }

            default:
                check(false);
            }

			break;
		}

        default:
            if (type!=EOpType::NONE)
            {
                // Operation not implemented
                check( false );
            }
            break;
        }
    }	
	
	//---------------------------------------------------------------------------------------------
    TSharedPtr<FRangeIndex> CodeRunner::BuildCurrentOpRangeIndex( const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel, int32 parameterIndex )
    {
        if (!Item.ExecutionIndex)
        {
            return nullptr;
        }

        // \todo: optimise to avoid allocating the index here, we could access internal
        // data directly.
		TSharedPtr<FRangeIndex> Index = pParams->NewRangeIndex( parameterIndex );
        if (!Index)
        {
            return nullptr;
        }

		const FProgram& Program = Model->GetPrivate()->Program;
		const FParameterDesc& paramDesc = Program.Parameters[ parameterIndex ];
        for( int32 rangeIndexInParam=0;
             rangeIndexInParam<paramDesc.Ranges.Num();
             ++rangeIndexInParam )
        {
            uint32 rangeIndexInModel = paramDesc.Ranges[rangeIndexInParam];
            const ExecutionIndex& currentIndex = GetMemory().GetRangeIndex( Item.ExecutionIndex );
            int32 Position = currentIndex.GetFromModelRangeIndex(rangeIndexInModel);
			Index->Values[rangeIndexInParam] = Position;
        }

        return Index;
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Bool(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Bool);

		const FProgram& Program = Model->GetPrivate()->Program;
		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::BO_CONSTANT:
        {
			OP::BoolConstantArgs Args = Program.GetOpArgs<OP::BoolConstantArgs>(Item.At);
            bool result = Args.bValue;
            StoreBool( Item, result );
            break;
        }

        case EOpType::BO_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
            bool result = false;
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
            result = pParams->GetBoolValue( Args.variable, Index.Get() );
            StoreBool( Item, result );
            break;
        }

        case EOpType::BO_AND:
        {
			OP::BoolBinaryArgs Args = Program.GetOpArgs<OP::BoolBinaryArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                {
                    // Try to avoid the op entirely if we have some children cached
                    bool skip = false;
                    if ( Args.A && GetMemory().IsValid( FCacheAddress(Args.A,Item) ) )
                    {
                         bool a = LoadBool( FCacheAddress(Args.A,Item) );
                         if (!a)
                         {
                            StoreBool( Item, false );
                            skip=true;
                         }
                    }

                    if ( !skip && Args.B && GetMemory().IsValid( FCacheAddress(Args.B,Item) ) )
                    {
                         bool b = LoadBool( FCacheAddress(Args.B,Item) );
                         if (!b)
                         {
                            StoreBool( Item, false );
                            skip=true;
                         }
                    }

                    if (!skip)
                    {
                        AddOp( FScheduledOp( Item.At, Item, 1),
                               FScheduledOp( Args.A, Item));
                    }
				break;
                }

            case 1:
            {
                bool a = Args.A ? LoadBool( FCacheAddress(Args.A,Item) ) : true;
                if (!a)
                {
                    StoreBool( Item, false );
                }
                else
                {
                    AddOp( FScheduledOp( Item.At, Item, 2),
                           FScheduledOp( Args.B, Item));
                }
                break;
            }

            case 2:
            {
                // We arrived here because a is true
                bool b = Args.B ? LoadBool( FCacheAddress(Args.B,Item) ) : true;
                StoreBool( Item, b );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case EOpType::BO_OR:
        {
			OP::BoolBinaryArgs Args = Program.GetOpArgs<OP::BoolBinaryArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                {
                    // Try to avoid the op entirely if we have some children cached
                    bool skip = false;
                    if ( Args.A && GetMemory().IsValid( FCacheAddress(Args.A,Item) ) )
                    {
                         bool a = LoadBool( FCacheAddress(Args.A,Item) );
                         if (a)
                         {
                            StoreBool( Item, true );
                            skip=true;
                         }
                    }

                    if ( !skip && Args.B && GetMemory().IsValid( FCacheAddress(Args.B,Item) ) )
                    {
                         bool b = LoadBool( FCacheAddress(Args.B,Item) );
                         if (b)
                         {
                            StoreBool( Item, true );
                            skip=true;
                         }
                    }

                    if (!skip)
                    {
                        AddOp( FScheduledOp( Item.At, Item, 1),
                               FScheduledOp( Args.A, Item));
                    }
				break;
                }

            case 1:
            {
                bool a = Args.A ? LoadBool( FCacheAddress(Args.A,Item) ) : false;
                if (a)
                {
                    StoreBool( Item, true );
                }
                else
                {
                    AddOp( FScheduledOp( Item.At, Item, 2),
                           FScheduledOp( Args.B, Item));
                }
                break;
            }

            case 2:
            {
                // We arrived here because a is false
                bool b = Args.B ? LoadBool( FCacheAddress(Args.B,Item) ) : false;
                StoreBool( Item, b );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case EOpType::BO_NOT:
        {
			OP::BoolNotArgs Args = Program.GetOpArgs<OP::BoolNotArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.A, Item) );
                break;

            case 1:
            {
                bool result = !LoadBool( FCacheAddress(Args.A,Item) );
                StoreBool( Item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case EOpType::BO_EQUAL_INT_CONST:
        {
			OP::BoolEqualScalarConstArgs Args = Program.GetOpArgs<OP::BoolEqualScalarConstArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.Value, Item) );
                break;

            case 1:
            {
                int32 a = LoadInt( FCacheAddress(Args.Value,Item) );
                bool result = a == Args.Constant;
                StoreBool( Item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        default:
            check( false );
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Int(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Int);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::NU_CONSTANT:
        {
			OP::IntConstantArgs Args = Program.GetOpArgs<OP::IntConstantArgs>(Item.At);
            int32 result = Args.Value;
            StoreInt( Item, result );
            break;
        }

        case EOpType::NU_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
            int32 result = pParams->GetIntValue( Args.variable, Index.Get());

            // Check that the value is actually valid. Otherwise set the default.
            if ( pParams->GetIntPossibleValueCount( Args.variable ) )
            {
                bool valid = false;
                for ( int32 i=0;
                      (!valid) && i<pParams->GetIntPossibleValueCount( Args.variable );
                      ++i )
                {
                    if ( result == pParams->GetIntPossibleValue( Args.variable, i ) )
                    {
                        valid = true;
                    }
                }

                if (!valid)
                {
                    result = pParams->GetIntPossibleValue( Args.variable, 0 );
                }
            }

            StoreInt( Item, result );
            break;
        }

        default:
            check( false );
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Scalar(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Scalar);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::SC_CONSTANT:
        {
			OP::ScalarConstantArgs Args = Program.GetOpArgs<OP::ScalarConstantArgs>(Item.At);
            float result = Args.Value;
            StoreScalar( Item, result );
            break;
        }

        case EOpType::SC_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
            float result = pParams->GetFloatValue( Args.variable, Index.Get());
            StoreScalar( Item, result );
            break;
        }

        case EOpType::SC_CURVE:
        {
			OP::ScalarCurveArgs Args = Program.GetOpArgs<OP::ScalarCurveArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                AddOp( FScheduledOp( Item.At, Item, 1),
                        FScheduledOp( Args.time, Item) );
                break;

            case 1:
            {
                float time = LoadScalar( FCacheAddress(Args.time,Item) );

                const FRichCurve& Curve = Program.ConstantCurves[Args.curve];
                float Result = Curve.Eval(time);

                StoreScalar( Item, Result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        case EOpType::SC_ARITHMETIC:
        {
			OP::ArithmeticArgs Args = Program.GetOpArgs<OP::ArithmeticArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.A, Item),
                           FScheduledOp( Args.B, Item) );
                break;

            case 1:
            {
                float a = LoadScalar( FCacheAddress(Args.A,Item) );
                float b = LoadScalar( FCacheAddress(Args.B,Item) );

                float result = 1.0f;
                switch (Args.Operation)
                {
                case OP::ArithmeticArgs::ADD:
                    result = a + b;
                    break;

                case OP::ArithmeticArgs::MULTIPLY:
                    result = a * b;
                    break;

                case OP::ArithmeticArgs::SUBTRACT:
                    result = a - b;
                    break;

                case OP::ArithmeticArgs::DIVIDE:
                    result = a / b;
                    break;

                default:
                    checkf(false, TEXT("Arithmetic operation not implemented."));
                    break;
                }

                StoreScalar( Item, result );
                break;
            }

            default:
                check(false);
            }
            break;
        }

        default:
            check( false );
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_String(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_String );

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType( Item.At );
        switch ( type )
        {

        case EOpType::ST_CONSTANT:
        {
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>( Item.At );
            check( Args.value < (uint32)InModel->GetPrivate()->Program.ConstantStrings.Num() );

            const FString& result = Program.ConstantStrings[Args.value];
            StoreString( Item, MakeShared<String>(result) );

            break;
        }

        case EOpType::ST_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>( Item.At );
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
			FString result;
			pParams->GetStringValue(Args.variable, result, Index.Get());
            StoreString( Item, MakeShared<String>(result) );
            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Colour(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Colour);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);

        switch ( type )
        {

        case EOpType::CO_CONSTANT:
        {
			OP::ColorConstantArgs Args = Program.GetOpArgs<OP::ColorConstantArgs>(Item.At);
            StoreColor( Item, Args.Value );
            break;
        }

        case EOpType::CO_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
            FVector4f V;
            pParams->GetColourValue( Args.variable, V, Index.Get());
            StoreColor( Item, V );
            break;
        }

        case EOpType::CO_SAMPLEIMAGE:
        {
			OP::ColourSampleImageArgs Args = Program.GetOpArgs<OP::ColourSampleImageArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.X, Item),
                           FScheduledOp( Args.Y, Item),
						   // Don't skip mips for the texture to sample
                           FScheduledOp::FromOpAndOptions( Args.Image, Item, 0) );
                break;

            case 1:
            {
                float x = Args.X ? LoadScalar( FCacheAddress(Args.X,Item) ) : 0.5f;
                float y = Args.Y ? LoadScalar( FCacheAddress(Args.Y,Item) ) : 0.5f;

                TSharedPtr<const FImage> pImage = LoadImage(FScheduledOp::FromOpAndOptions(Args.Image, Item, 0));

				FVector4f result;
                if (pImage)
                {
                    if (Args.Filter)
                    {
                        // TODO
                        result = pImage->Sample(FVector2f(x, y));
                    }
                    else
                    {
                        result = pImage->Sample(FVector2f(x, y));
                    }
                }
                else
                {
                    result = FVector4f();
                }

				Release(pImage);
                StoreColor( Item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::CO_SWIZZLE:
        {
			OP::ColourSwizzleArgs Args = Program.GetOpArgs<OP::ColourSwizzleArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.sources[0], Item),
                           FScheduledOp( Args.sources[1], Item),
                           FScheduledOp( Args.sources[2], Item),
                           FScheduledOp( Args.sources[3], Item) );
                break;

            case 1:
            {
				FVector4f result;

                for (int32 t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
                {
                    if ( Args.sources[t] )
                    {
                        FVector4f p = LoadColor( FCacheAddress(Args.sources[t],Item) );
                        result[t] = p[ Args.sourceChannels[t] ];
                    }
                }

                StoreColor( Item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::CO_FROMSCALARS:
        {
			OP::ColourFromScalarsArgs Args = Program.GetOpArgs<OP::ColourFromScalarsArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.V[0], Item),
                           FScheduledOp( Args.V[1], Item),
                           FScheduledOp( Args.V[2], Item),
                           FScheduledOp( Args.V[3], Item));
                break;

            case 1:
            {
				FVector4f Result = FVector4f(0, 0, 0, 1);

				for (int32 t = 0; t < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
				{
					if (Args.V[t])
					{
						Result[t] = LoadScalar(FCacheAddress(Args.V[t], Item));
					}
				}

                StoreColor( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::CO_ARITHMETIC:
        {
			OP::ArithmeticArgs Args = Program.GetOpArgs<OP::ArithmeticArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.A, Item),
                           FScheduledOp( Args.B, Item));
                break;

            case 1:
            {
				EOpType otype = Program.GetOpType( Args.A );
                EDataType dtype = GetOpDataType( otype );
                check( dtype == EDataType::Color );
                otype = Program.GetOpType( Args.B );
                dtype = GetOpDataType( otype );
                check( dtype == EDataType::Color );
				FVector4f a = Args.A ? LoadColor( FCacheAddress( Args.A, Item ) )
                                 : FVector4f( 0, 0, 0, 0 );
				FVector4f b = Args.B ? LoadColor( FCacheAddress( Args.B, Item ) )
                                 : FVector4f( 0, 0, 0, 0 );

				FVector4f result = FVector4f(0,0,0,0);
                switch (Args.Operation)
                {
                case OP::ArithmeticArgs::ADD:
                    result = a + b;
                    break;

                case OP::ArithmeticArgs::MULTIPLY:
                    result = a * b;
                    break;

                case OP::ArithmeticArgs::SUBTRACT:
                    result = a - b;
                    break;

                case OP::ArithmeticArgs::DIVIDE:
                    result = a / b;
                    break;

                default:
                    checkf(false, TEXT("Arithmetic operation not implemented."));
                    break;
                }

                StoreColor( Item, result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        default:
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Projector(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
        MUTABLE_CPUPROFILER_SCOPE(RunCode_Projector);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::PR_CONSTANT:
        {
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>(Item.At);
            FProjector Result = Program.ConstantProjectors[Args.value];
            StoreProjector( Item, Result );
            break;
        }

        case EOpType::PR_PARAMETER:
        {
			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
			TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
            FProjector Result = pParams->GetPrivate()->GetProjectorValue(Args.variable,Index.Get());

            // The type cannot be changed, take it from the default value
            const FProjector& def = Program.Parameters[Args.variable].DefaultValue.Get<FParamProjectorType>();
            Result.type = def.type;

            StoreProjector( Item, Result );
            break;
        }

        default:
            check( false );
            break;
        }
    }

    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Matrix(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel )
    {
		MUTABLE_CPUPROFILER_SCOPE(RunCode_Transform);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);

		switch ( type )
		{
		case EOpType::MA_CONSTANT:
			{
				OP::MatrixConstantArgs Args = Program.GetOpArgs<OP::MatrixConstantArgs>(Item.At);
				StoreMatrix( Item, Program.ConstantMatrices[Args.value] );
				break;
			}

		case EOpType::MA_PARAMETER:
			{
				OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
				TSharedPtr<FRangeIndex> Index = BuildCurrentOpRangeIndex( Item, pParams, InModel, Args.variable );
				FMatrix44f Value;
				pParams->GetMatrixValue( Args.variable, Value, Index.Get());
				StoreMatrix( Item, Value );
				break;
			}
		}
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode_Layout(const FScheduledOp& Item, const FModel* InModel )
    {
        //MUTABLE_CPUPROFILER_SCOPE(RunCode_Layout);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
        switch (type)
        {

        case EOpType::LA_CONSTANT:
        {
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>(Item.At);
            check( Args.value < (uint32)InModel->GetPrivate()->Program.ConstantLayouts.Num() );

            TSharedPtr<const FLayout> Result = Program.ConstantLayouts
                    [ Args.value ];
            StoreLayout( Item, Result );
            break;
        }

        case EOpType::LA_MERGE:
        {
			OP::LayoutMergeArgs Args = Program.GetOpArgs<OP::LayoutMergeArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.Base, Item),
                           FScheduledOp( Args.Added, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FLayout> pA = LoadLayout( FCacheAddress(Args.Base,Item) );
                TSharedPtr<const FLayout> pB = LoadLayout( FCacheAddress(Args.Added,Item) );

                TSharedPtr<const FLayout> Result;

                if (pA && pB)
                {
					Result = LayoutMerge(pA.Get(),pB.Get());
                }
                else if (pA)
                {
                    Result = pA->Clone();
                }
                else if (pB)
                {
                    Result = pB->Clone();
                }

                StoreLayout( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

        case EOpType::LA_PACK:
        {
			OP::LayoutPackArgs Args = Program.GetOpArgs<OP::LayoutPackArgs>(Item.At);
            switch (Item.Stage)
            {
            case 0:
                    AddOp( FScheduledOp( Item.At, Item, 1),
                           FScheduledOp( Args.Source, Item) );
                break;

            case 1:
            {
                TSharedPtr<const FLayout> Source = LoadLayout( FCacheAddress(Args.Source,Item) );

				TSharedPtr<FLayout> Result;

				if (Source)
				{
					Result = Source->Clone();

					LayoutPack3(Result.Get(), Source.Get() );
				}

                StoreLayout( Item, Result );
                break;
            }

            default:
                check(false);
            }

            break;
        }

		case EOpType::LA_FROMMESH:
		{
			OP::LayoutFromMeshArgs Args = Program.GetOpArgs<OP::LayoutFromMeshArgs>(Item.At);

			constexpr uint8 MeshContentFilter = (uint8)(EMeshContentFlags::AllFlags);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1),
					FScheduledOp::FromOpAndOptions(Args.Mesh, Item, MeshContentFilter));
				break;

			case 1:
			{
				TSharedPtr<const FMesh> Mesh = LoadMesh(
						FScheduledOp::FromOpAndOptions(Args.Mesh, Item, MeshContentFilter));

				TSharedPtr<const FLayout> Result = LayoutFromMesh_RemoveBlocks(Mesh.Get(), Args.LayoutIndex);

				Release(Mesh);
				StoreLayout(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::LA_REMOVEBLOCKS:
		{
			OP::LayoutRemoveBlocksArgs Args = Program.GetOpArgs<OP::LayoutRemoveBlocksArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1),
					FScheduledOp(Args.Source, Item),
					FScheduledOp(Args.ReferenceLayout, Item));
				break;

			case 1:
			{
				TSharedPtr<const FLayout> Source = LoadLayout(FCacheAddress(Args.Source, Item));
				TSharedPtr<const FLayout> ReferenceLayout = LoadLayout(FCacheAddress(Args.ReferenceLayout, Item));

				TSharedPtr<const FLayout> Result;

				if (Source && ReferenceLayout)
				{
					Result = LayoutRemoveBlocks(Source.Get(), ReferenceLayout.Get());
				}
				else if (Source)
				{
					Result = Source;
				}

				StoreLayout(Item, Result);
				break;
			}

			default:
				check(false);
			}

			break;
		}

        default:
            // Operation not implemented
            check( false );
            break;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CodeRunner::RunCode( const FScheduledOp& Item, const FParameters* pParams, const TSharedPtr<const FModel>& InModel, uint32 LodMask)
    {
		//UE_LOG( LogMutableCore, Log, TEXT("Running :%5d , %d "), Item.At, Item.Stage );
		check( Item.Type == FScheduledOp::EType::Full );
		
		const FProgram& Program = InModel->GetPrivate()->Program;

		EOpType type = Program.GetOpType(Item.At);
		//UE_LOG(LogMutableCore, Log, TEXT("Running :%5d , %d, of type %d "), Item.At, Item.Stage, type);

		// Very spammy, for debugging purposes.
		//if (System)
		//{
		//	System->WorkingMemoryManager.LogWorkingMemory( this );
		//}

		switch ( type )
        {
        case EOpType::NONE:
            break;

        case EOpType::NU_CONDITIONAL:
        case EOpType::SC_CONDITIONAL:
        case EOpType::CO_CONDITIONAL:
        case EOpType::IM_CONDITIONAL:
        case EOpType::ME_CONDITIONAL:
        case EOpType::LA_CONDITIONAL:
        case EOpType::IN_CONDITIONAL:
		case EOpType::ED_CONDITIONAL:
            RunCode_Conditional(Item, InModel.Get());
            break;

        case EOpType::ME_CONSTANT:
		case EOpType::IM_CONSTANT:
		case EOpType::ED_CONSTANT:
            RunCode_ConstantResource(Item, InModel.Get());
            break;

        case EOpType::NU_SWITCH:
        case EOpType::SC_SWITCH:
        case EOpType::CO_SWITCH:
        case EOpType::IM_SWITCH:
        case EOpType::ME_SWITCH:
        case EOpType::LA_SWITCH:
        case EOpType::IN_SWITCH:
		case EOpType::ED_SWITCH:
            RunCode_Switch(Item, InModel.Get());
            break;

        case EOpType::IN_ADDMESH:
        case EOpType::IN_ADDIMAGE:
            RunCode_InstanceAddResource(Item, InModel, pParams);
            break;

		default:
		{
			EDataType DataType = GetOpDataType(type);
			switch (DataType)
			{
			case EDataType::Instance:
				RunCode_Instance(Item, InModel.Get(), LodMask);
				break;

			case EDataType::Mesh:
				RunCode_Mesh(Item, InModel.Get());
				break;

			case EDataType::Image:
				RunCode_Image(Item, pParams, InModel.Get());
				break;

			case EDataType::Layout:
				RunCode_Layout(Item, InModel.Get());
				break;

			case EDataType::Bool:
				RunCode_Bool(Item, pParams, InModel.Get());
				break;

			case EDataType::Scalar:
				RunCode_Scalar(Item, pParams, InModel.Get());
				break;

			case EDataType::String:
				RunCode_String(Item, pParams, InModel.Get());
				break;

			case EDataType::Int:
				RunCode_Int(Item, pParams, InModel.Get());
				break;

			case EDataType::Projector:
				RunCode_Projector(Item, pParams, InModel.Get());
				break;

			case EDataType::Color:
				RunCode_Colour(Item, pParams, InModel.Get());
				break;

			case EDataType::Matrix:
				RunCode_Matrix(Item, pParams, InModel.Get());
				break;
				
			default:
				check(false);
				break;
			}
			break;
		}

        }
    }

	/** */
	void CodeRunner::RunCodeImageDesc(const FScheduledOp& Item, const FParameters* pParams, const FModel* InModel,  uint32 LodMask )
	{
		MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc);
		check(Item.Type == FScheduledOp::EType::ImageDesc);

		const FProgram& Program = Model->GetPrivate()->Program;

		EOpType Type = Program.GetOpType(Item.At);
		switch (Type)
		{

		case EOpType::IM_CONSTANT:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_CONSTANT);

			check(Item.Stage == 0);
			OP::ResourceConstantArgs Args = Program.GetOpArgs<OP::ResourceConstantArgs>(Item.At);
			int32 ImageIndex = Args.value;

			FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);

			Result.m_format = Program.ConstantImages[ImageIndex].ImageFormat;
			Result.m_size[0] = Program.ConstantImages[ImageIndex].ImageSizeX;
			Result.m_size[1] = Program.ConstantImages[ImageIndex].ImageSizeY;
			Result.m_lods = Program.ConstantImages[ImageIndex].LODCount;

			int32 LODIndexIndex = Program.ConstantImages[ImageIndex].FirstIndex;
			{
				int32 LODIndex = 0;
				for (; LODIndex < Result.m_lods; ++LODIndex)
				{
					const int32 CurrentIndexIndex = LODIndexIndex + LODIndex;
					const FConstantResourceIndex CurrentIndex = Program.ConstantImageLODIndices[CurrentIndexIndex];

					bool bIsLODAvailable = false;
					if (!CurrentIndex.Streamable)
					{
						bIsLODAvailable = true;
					}
					else
					{
						uint32 RomId = CurrentIndex.Index;
						bIsLODAvailable = System->StreamInterface->DoesBlockExist(Model.Get(), RomId);
					}

					if (bIsLODAvailable)
					{
						break;
					}
				}

				Result.FirstLODAvailable = LODIndex;
			}
			ImageDescConstantImages.Add(ImageIndex);

			StoreValidDesc(Item);
			break;
		}

		case EOpType::IM_PARAMETER:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_PARAMETER);
			check(Item.Stage == 0);

			OP::ParameterArgs Args = Program.GetOpArgs<OP::ParameterArgs>(Item.At);
			FName Id = pParams->GetImageValue(Args.variable);

			FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
			Result = GetExternalImageDesc(Id);

			StoreValidDesc(Item);
			break;
		}

		case EOpType::IM_REFERENCE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_REFERENCE);
			check(Item.Stage == 0);

			OP::ResourceReferenceArgs Args = Program.GetOpArgs<OP::ResourceReferenceArgs>(Item.At);
			FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
			
			Result = FExtendedImageDesc{ Args.ImageDesc, 0 };

			StoreValidDesc(Item);
			break;
		}

		case EOpType::IM_CONDITIONAL:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_CONDITIONAL);
			OP::ConditionalArgs Args = Program.GetOpArgs<OP::ConditionalArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				// We need to run the full condition result
				FScheduledOp FullConditionOp(Args.condition, Item);
				FullConditionOp.Type = FScheduledOp::EType::Full;

				AddOp(FScheduledOp(Item.At, Item, 1), FullConditionOp);
				break;
			}

			case 1:
			{
				bool bValue = LoadBool(FCacheAddress(Args.condition, Item.ExecutionIndex, Item.ExecutionOptions));
				OP::ADDRESS ResultAt = bValue ? Args.yes : Args.no;

				AddOp(FScheduledOp(Item.At, Item, 2, ResultAt), 
						FScheduledOp(ResultAt, Item, 0));
				break;
			}

			case 2: 
			{
				FExtendedImageDesc& Result =  ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Item.CustomState);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_SWITCH:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_SWITCH);
			const uint8* Data = Program.GetOpArgsPointer(Item.At);
		
			OP::ADDRESS VarAddress;
			FMemory::Memcpy(&VarAddress, Data, sizeof(OP::ADDRESS));
			Data += sizeof(OP::ADDRESS);

			OP::ADDRESS DefAddress;
			FMemory::Memcpy(&DefAddress, Data, sizeof(OP::ADDRESS));
			Data += sizeof(OP::ADDRESS);

			uint32 CaseCount;
			FMemory::Memcpy(&CaseCount, Data, sizeof(uint32));
			Data += sizeof(uint32);
	
			switch (Item.Stage)
			{
			case 0:
			{
				if (VarAddress)
				{
					// We need to run the full condition result
					FScheduledOp FullVariableOp(VarAddress, Item);
					FullVariableOp.Type = FScheduledOp::EType::Full;
					AddOp(FScheduledOp(Item.At, Item, 1), FullVariableOp);
				}
				else
				{
					ImageDescResults.FindOrAdd(Item.At);
					StoreValidDesc(Item);
				}
				break;
			}

			case 1:
			{
				// Get the variable result
				int32 Var = LoadInt(FCacheAddress(VarAddress, Item.ExecutionIndex, Item.ExecutionOptions, FScheduledOp::EType::Full));

				OP::ADDRESS ValueAt = DefAddress;
				for (uint32 C = 0; C < CaseCount; ++C)
				{
					int32 Condition;
					FMemory::Memcpy(&Condition, Data, sizeof(int32));
					Data += sizeof(int32);
					
					OP::ADDRESS At;
					FMemory::Memcpy(&At, Data, sizeof(OP::ADDRESS));
					Data += sizeof(OP::ADDRESS);

					if (At && Var == (int32)Condition)
					{
						ValueAt = At;
						break;
					}
				}

				if (ValueAt)
				{
					AddOp(FScheduledOp(Item.At, Item, 2, ValueAt),
							FScheduledOp(ValueAt, Item, 0));
				}
				else
				{
					ImageDescResults.FindOrAdd(Item.At);
					StoreValidDesc(Item); 
				}
				break;
			}

			case 2: 
			{
				check(Item.CustomState);

				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Item.CustomState);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false); break;
			}
			break;
		}

		case EOpType::IM_LAYERCOLOUR:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_LAYERCOLOUR);
			OP::ImageLayerColourArgs Args = Program.GetOpArgs<OP::ImageLayerColourArgs>(Item.At);

			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.base, Item, 0), 
						FScheduledOp(Args.mask, Item, 0));

				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.base);

				if (Args.mask)
				{
					const FExtendedImageDesc& MaskResult = ImageDescResults.FindChecked(Args.mask);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, MaskResult.FirstLODAvailable);
				}

				StoreValidDesc(Item); 

				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_LAYER:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_LAYER);
			OP::ImageLayerArgs Args = Program.GetOpArgs<OP::ImageLayerArgs>(Item.At);	
			switch (Item.Stage)
			{
			case 0:
			{	
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.base, Item, 0),
						FScheduledOp(Args.mask, Item, 0),
						FScheduledOp(Args.blended, Item, 0));

				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.base);

				if (Args.mask)
				{
					const FExtendedImageDesc& MaskResult = ImageDescResults.FindChecked(Args.mask);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, MaskResult.FirstLODAvailable);
				}

				if (Args.blended)
				{
					const FExtendedImageDesc& BlendedResult = ImageDescResults.FindChecked(Args.blended);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, BlendedResult.FirstLODAvailable);
				}
		
				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_MULTILAYER:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_MULTILAYER);
			OP::ImageMultiLayerArgs Args = Program.GetOpArgs<OP::ImageMultiLayerArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				//TODO: For now multilayer operations will only check the base to get the descriptor.
				// but all iterations should be checked for available mips. 
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.base, Item, 0));
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.base);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_NORMALCOMPOSITE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_NORMALCOMPOSITE);
			OP::ImageNormalCompositeArgs Args = Program.GetOpArgs<OP::ImageNormalCompositeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.base, Item, 0)); 
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.base);
				
				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_PIXELFORMAT:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_PIXELFORMAT);
			OP::ImagePixelFormatArgs Args = Program.GetOpArgs<OP::ImagePixelFormatArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.source, Item, 0));
				break;

			case 1:
			{

				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.source);

				EImageFormat OldFormat = Result.m_format;
				EImageFormat NewFormat = Args.format;
				if (Args.formatIfAlpha != EImageFormat::None && GetImageFormatData(OldFormat).Channels > 3)
				{
					NewFormat = Args.formatIfAlpha;
				}

				Result.m_format = NewFormat;

				StoreValidDesc(Item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_MIPMAP:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_MIPMAP);
			OP::ImageMipmapArgs Args = Program.GetOpArgs<OP::ImageMipmapArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.source, Item, 0));
				break;

			case 1:
			{
				// Somewhat synched with Full op execution code.
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.source);

				int32 LevelCount = Args.levels;
				int32 MaxLevelCount = FImage::GetMipmapCount(Result.m_size[0], Result.m_size[1]);
				if (LevelCount == 0)
				{
					LevelCount = MaxLevelCount;
				}
				else if (LevelCount > MaxLevelCount)
				{
					// If code generation is smart enough, this should never happen.
					// \todo But apparently it does, sometimes.
					LevelCount = MaxLevelCount;
				}

				// At least keep the levels we already have.
				int32 StartLevel = Result.m_lods;
				LevelCount = FMath::Max(StartLevel, LevelCount);

				// Update result.
				Result.m_lods = LevelCount;

				StoreValidDesc(Item);
				break;
			}

			default:
				check(false);
			}
			break;
		}

		case EOpType::IM_RESIZE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_RESIZE);
			OP::ImageResizeArgs Args = Program.GetOpArgs<OP::ImageResizeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Source, Item, 0));
				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Source);

				Result.m_size[0] = Args.Size[0];
				Result.m_size[1] = Args.Size[1];

				StoreValidDesc(Item);

				break;
			}
			default:
				check(false);
			}
			break;
		}

		case EOpType::IM_RESIZELIKE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_RESIZELIKE);
			OP::ImageResizeLikeArgs Args = Program.GetOpArgs<OP::ImageResizeLikeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Source, Item, 0), 
						FScheduledOp(Args.SizeSource, Item, 0));

				break;
			}

			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Source);
	
				if (Args.SizeSource)
				{
					const FExtendedImageDesc& SizeSourceResult = ImageDescResults.FindChecked(Args.SizeSource);
					Result.m_size = SizeSourceResult.m_size;
				}

				StoreValidDesc(Item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_RESIZEREL:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_RESIZEREL);
			OP::ImageResizeRelArgs Args = Program.GetOpArgs<OP::ImageResizeRelArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Source, Item, 0));
				break;

			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Source);
				
				FImageSize DestSize(
					uint16(Result.m_size[0] * Args.Factor[0] + 0.5f),
					uint16(Result.m_size[1] * Args.Factor[1] + 0.5f));

				Result.m_size = DestSize;

				StoreValidDesc(Item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_BLANKLAYOUT:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_BLANKLAYOUT);
			OP::ImageBlankLayoutArgs Args = Program.GetOpArgs<OP::ImageBlankLayoutArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				// We need to run the full layout
				FScheduledOp FullLayoutOp(Args.Layout, Item);
				FullLayoutOp.Type = FScheduledOp::EType::Full;
				AddOp(FScheduledOp(Item.At, Item, 1), FullLayoutOp);
				break;
			}

			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				
				TSharedPtr<const FLayout> Layout = LoadLayout(FCacheAddress(Args.Layout, Item.ExecutionIndex, Item.ExecutionOptions, FScheduledOp::EType::Full));

				FIntPoint SizeInBlocks = Layout->GetGridSize();
				FIntPoint BlockSizeInPixels(Args.BlockSize[0], Args.BlockSize[1]);
				FIntPoint ImageSizeInPixels = SizeInBlocks * BlockSizeInPixels;

				
				FImageSize DestSize(uint16(ImageSizeInPixels.X), uint16(ImageSizeInPixels.Y));
				Result.m_size = DestSize;
				Result.m_format = Args.Format;
				
				if (Args.GenerateMipmaps)
				{
					if (Args.MipmapCount == 0)
					{
						Result.m_lods = FImage::GetMipmapCount(ImageSizeInPixels.X, ImageSizeInPixels.Y);
					}
					else
					{
						Result.m_lods = Args.MipmapCount;
					}
				}

				StoreValidDesc(Item);
				break;
			}

			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_COMPOSE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_COMPOSE);
			OP::ImageComposeArgs Args = Program.GetOpArgs<OP::ImageComposeArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.base, Item, 0),
						FScheduledOp(Args.blockImage, Item, 0)); 
				break;
			} 
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At); 
				Result = ImageDescResults.FindChecked(Args.base);

				if (Args.blockImage)
				{
					const FExtendedImageDesc& BlockResult = ImageDescResults.FindChecked(Args.blockImage);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, BlockResult.FirstLODAvailable);
				}

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_INTERPOLATE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_INTERPOLATE);
			OP::ImageInterpolateArgs Args = Program.GetOpArgs<OP::ImageInterpolateArgs>(Item.At);

			int32 NumImages = 0;
			for (; NumImages < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++NumImages)
			{
				if (!Args.Targets[NumImages])
				{
					break;
				}
			}

			switch (Item.Stage)
			{
			case 0: 
			{
				TArray<FScheduledOp, TFixedAllocator<MUTABLE_OP_MAX_INTERPOLATE_COUNT>> Deps;
				for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
				{
					Deps.Add(FScheduledOp(Args.Targets[ImageIndex], Item, 0));
				}

				AddOp(FScheduledOp(Item.At, Item, 1), Deps); 
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);

				check(Args.Targets[0]);

				Result = ImageDescResults.FindChecked(Args.Targets[0]);
				
				for (int32 ImageIndex = 1; ImageIndex < NumImages; ++ImageIndex)
				{
					const FExtendedImageDesc& TargetResult = ImageDescResults.FindChecked(Args.Targets[ImageIndex]);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, TargetResult.FirstLODAvailable);
				}

				StoreValidDesc(Item); break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_SATURATE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_SATURATE);
			OP::ImageSaturateArgs Args = Program.GetOpArgs<OP::ImageSaturateArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0)); 
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Base);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_LUMINANCE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_LUMINANCE);
			OP::ImageLuminanceArgs Args = Program.GetOpArgs<OP::ImageLuminanceArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0));
				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Base);
				Result.m_format = EImageFormat::L_UByte;
				
				StoreValidDesc(Item);
				break;
			}
			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_SWIZZLE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_SWIZZLE);
			OP::ImageSwizzleArgs Args = Program.GetOpArgs<OP::ImageSwizzleArgs>(Item.At);
			
			TArray<OP::ADDRESS, TFixedAllocator<4>> ValidArgs;
			for (int32 SourceIndex = 0; SourceIndex < 4; ++SourceIndex)
			{
				if (Args.sources[SourceIndex])
				{
					ValidArgs.AddUnique(Args.sources[SourceIndex]);
				}
			}

			switch (Item.Stage)
			{
			case 0:
			{
				TArray<FScheduledOp, TFixedAllocator<4>> Deps;
				for (int32 ArgIndex = 0; ArgIndex < ValidArgs.Num(); ++ArgIndex)
				{
					Deps.Add(FScheduledOp(ValidArgs[ArgIndex], Item, 0));
				}

				AddOp(FScheduledOp(Item.At, Item, 1), Deps);

				break;
			}
			case 1:
			{	
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);

				check(ValidArgs.Num() > 0);

				Result = ImageDescResults.FindChecked(ValidArgs[0]);
				Result.m_format = Args.format;
				
				for (int32 ArgIndex = 1; ArgIndex < ValidArgs.Num(); ++ArgIndex)
				{
					const FExtendedImageDesc& SourceResult = ImageDescResults.FindOrAdd(ValidArgs[ArgIndex]); 
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, SourceResult.FirstLODAvailable);
				}

				StoreValidDesc(Item);
				break;
			}
			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_COLOURMAP:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_COLOURMAP);
			OP::ImageColourMapArgs Args = Program.GetOpArgs<OP::ImageColourMapArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{	
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0));
				break;
			}
			case 1: 
			{	
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Base);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_BINARISE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_BINARIZE);
			OP::ImageBinariseArgs Args = Program.GetOpArgs<OP::ImageBinariseArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0));
				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result.m_format = EImageFormat::L_UByte;

				StoreValidDesc(Item);
				break;
			}
			default:
				check(false);
			}
			break;
		}

		case EOpType::IM_INVERT:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_INVERT);
			OP::ImageInvertArgs Args = Program.GetOpArgs<OP::ImageInvertArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0)); 
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Base);

				StoreValidDesc(Item);
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_PLAINCOLOUR:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_PLAINCOLOUR);
			OP::ImagePlainColorArgs Args = Program.GetOpArgs<OP::ImagePlainColorArgs>(Item.At);
			FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
			
			Result.m_size[0] = Args.Size[0];
			Result.m_size[1] = Args.Size[1];
			Result.m_lods = Args.LODs;
			Result.m_format = Args.Format;

			StoreValidDesc(Item);
			break;
		}

		case EOpType::IM_CROP:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_CROP);
			OP::ImageCropArgs Args = Program.GetOpArgs<OP::ImageCropArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.source, Item, 0));
				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.source);
				
				Result.m_size[0] = Args.sizeX;
				Result.m_size[1] = Args.sizeY;
				Result.m_lods = 1;

				StoreValidDesc(Item);
				break;
			}
			default:
				check(false);
			}
			break;
		}

		case EOpType::IM_PATCH:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_PATCH);
			OP::ImagePatchArgs Args = Program.GetOpArgs<OP::ImagePatchArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.base, Item, 0),
						FScheduledOp(Args.patch, Item, 0)); 
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.base);

				if (Args.patch)
				{
					FExtendedImageDesc& PatchImageDesc = ImageDescResults.FindChecked(Args.patch);
					Result.FirstLODAvailable = FMath::Max(Result.FirstLODAvailable, PatchImageDesc.FirstLODAvailable);
				}

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

		case EOpType::IM_RASTERMESH:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_RASTERMESH);
			OP::ImageRasterMeshArgs Args = Program.GetOpArgs<OP::ImageRasterMeshArgs>(Item.At);
			FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
			
			Result.m_size[0] = Args.sizeX;
			Result.m_size[1] = Args.sizeY;
			Result.m_lods = 1;
			Result.m_format = EImageFormat::L_UByte;

			StoreValidDesc(Item);
			break;
		}

		case EOpType::IM_MAKEGROWMAP:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_MAKEGROWMAP);
			OP::ImageMakeGrowMapArgs Args = Program.GetOpArgs<OP::ImageMakeGrowMapArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.mask, Item, 0));
				break;
			}
			case 1:
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.mask);

				Result.m_format = EImageFormat::L_UByte;
				Result.m_lods = 1;

				StoreValidDesc(Item);
				break;
			}
			default:
				check(false);
			}

			break;
		}

		case EOpType::IM_DISPLACE:
		{
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_DISPLACE);
			OP::ImageDisplaceArgs Args = Program.GetOpArgs<OP::ImageDisplaceArgs>(Item.At);
			switch (Item.Stage)
			{
			case 0: 
			{
				AddOp(FScheduledOp(Item.At, Item, 1),
						FScheduledOp(Args.Source, Item, 0));
				break;
			}
			case 1: 
			{
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Source);

				StoreValidDesc(Item); 
				break;
			}
			default: check(false);
			}
			break;
		}

        case EOpType::IM_TRANSFORM:
        {
			MUTABLE_CPUPROFILER_SCOPE(RunCodeImageDesc_IM_TRANSFORM);
			OP::ImageTransformArgs Args = Program.GetOpArgs<OP::ImageTransformArgs>(Item.At);

            switch (Item.Stage)
            {
            case 0:
			{
				AddOp(FScheduledOp(Item.At, Item, 1), 
						FScheduledOp(Args.Base, Item, 0));	
                break;
			}
            case 1:
            {
				FExtendedImageDesc& Result = ImageDescResults.FindOrAdd(Item.At);
				Result = ImageDescResults.FindChecked(Args.Base);
			
				Result.m_lods = 1;
				Result.m_format = GetUncompressedFormat(Result.m_format);
				
				if (!(Args.SizeX == 0 && Args.SizeY == 0))
				{
					Result.m_size[0] = Args.SizeX;
					Result.m_size[1] = Args.SizeY;
				}

				StoreValidDesc(Item);
                break;
            }

            default:
                check(false);
            }

			break;
		}

		default:
			if (Type != EOpType::NONE)
			{
				// Operation not implemented
				check(false);

				ImageDescResults.FindOrAdd(Item.At);
			}
			break;
		}
	}

	FImageOperator MakeImageOperator(CodeRunner* Runner)
	{
		return FImageOperator(
			// Create
			[Runner](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i)
			{
				return Runner->CreateImage(x, y, m, f, i);
			},

			// Release
			[Runner](TSharedPtr<FImage>& Image)
			{
				Runner->Release(Image);
			},

			// Clone
			[Runner](const FImage* Image)
			{
				TSharedPtr<FImage> New = Runner->CreateImage(Image->GetSizeX(), Image->GetSizeY(), Image->GetLODCount(), Image->GetFormat(), EInitializationType::NotInitialized);
				New->Copy(Image);
				return New;
			},

			Runner->System->ImagePixelFormatOverride
		);
	}
}

#if MUTABLE_DEBUG_CODERUNNER_TASK_SCHEDULE_CALLSTACK

#undef AddOp

#endif
