// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/System.h"
#include "MuR/SystemPrivate.h"

namespace mu
{

    /** Decide what operations are an "add resource" since they are handled differently sometimes. */
    inline bool VisitorIsAddResource(const EOpType& type)
    {
        return  type == EOpType::IN_ADDIMAGE ||
                type == EOpType::IN_ADDMESH;
    }


    /** Code visitor that:
    * - is top-down
    * - cannot change the visited instructions.
    * - will not visit twice the same instruction with the same state.
    * - Its iterative
	*/
    template<typename STATE=int32>
    class UniqueConstCodeVisitorIterative
    {
    public:

        UniqueConstCodeVisitorIterative( bool bInSkipResources=false )
        {
            // Default state
            States.Add(STATE());
            CurrentState = 0;
            bSkipResources = bInSkipResources;
        }

        //! Ensure virtual destruction
        virtual ~UniqueConstCodeVisitorIterative() {}

    protected:

        //!
        void SetDefaultState(const STATE& State)
        {
            States[0] = State;
        }

        //!
        const STATE& GetDefaultState() const
        {
            return States[0];
        }

        //! Use this from visit to access the state at the time of processing the current
        //! instruction.
        STATE GetCurrentState() const
        {
            return States[CurrentState];
        }

        //! For manual recursion that changes the state for a specific path.
        void RecurseWithState(OP::ADDRESS Address, const STATE& NewState)
        {
			int32 StateIndex = States.Find(NewState);
            if (StateIndex==INDEX_NONE)
            {
				StateIndex = States.Add(NewState);
            }
            Pending.Emplace( Address, StateIndex );
        }

        //! For manual recursion that doesn't change the state for a specific path.
        void RecurseWithCurrentState(OP::ADDRESS Address)
        {
            Pending.Emplace( Address, CurrentState );
        }

        //! Can be called from visit to set the state to visit all children ops
        void SetCurrentState(const STATE& NewState)
        {
			CurrentState = States.Find(NewState);
            if (CurrentState ==INDEX_NONE)
            {
				CurrentState = States.Add(NewState);
            }
        }


        void Traverse( OP::ADDRESS Root, FProgram& Program )
        {
            Pending.Reserve( Program.OpAddress.Num() );

            // Visit the given root
            Pending.Emplace(Root, 0);
            Recurse( Program );
         }

        void FullTraverse( FProgram& Program )
        {
            // Visit all the state roots
            for ( int32 StateIndex=0; StateIndex<Program.States.Num(); ++StateIndex)
            {
                Pending.Add(FPending(Program.States[StateIndex].Root,0) );
                Recurse( Program );
            }
        }


    private:

        /** Do the actual work by overriding this in the derived classes.
         * Return true if the traverse has to continue with the children of the given address.
		 */
        virtual bool Visit( OP::ADDRESS, FProgram& ) = 0;

        //! Operations to be processed
        struct FPending
        {
			FPending()
			{
				Address = 0;
				StateIndex = 0;
			}

			FPending(OP::ADDRESS InAddress, int32 InStateIndex)
			{
				Address = InAddress;
				StateIndex = InStateIndex;
			}
			
			OP::ADDRESS Address=0;
            int32 StateIndex=0;
        };
		TArray<FPending> Pending;

        //! States found so far
		TArray<STATE> States;

        //! Index of the current state, from the States array.
        int32 CurrentState;

        //! If true, operations adding resources (meshes or images) will only
        //! have the base operation recursed, but not the resources.
        bool bSkipResources;

        //! Array of states visited for each operation.
        //! Empty array means operation not visited at all.
		TArray<TArray<int32>> Visited;

        //! Process all the pending operations and visit all children if necessary
        void Recurse( FProgram& Program )
        {
			Visited.Empty();
			Visited.SetNum(Program.OpAddress.Num());

            while ( Pending.Num() )
            {
                OP::ADDRESS Address = Pending.Last().Address;
                CurrentState = Pending.Last().StateIndex;
                Pending.Pop();

                bool bRecurse = false;

                bool bVisitedInThisState = Visited[Address].Contains(CurrentState);
                if (!bVisitedInThisState)
                {
                    Visited[Address].Add(CurrentState);

                    // Visit may change current state
                    bRecurse = Visit(Address, Program );
                }

                if (bRecurse)
                {
                    ForEachReference( Program, Address, [&](OP::ADDRESS Ref)
                    {
                        if (Ref)
                        {
                            Pending.Emplace(Ref, CurrentState);
                        }
                    });
                }
            }
        }

    };


    //---------------------------------------------------------------------------------------------
    //! Code visitor template for visitors that:
    //! - only traverses the operations that are relevant for a given set of parameter values. It
    //! only considers the discrete parameters like integers and booleans. In the case of forks
    //! caused by continuous parameters like float weights for interpolation, all the branches are
    //! traversed.
    //! - cannot change the instructinos
    //---------------------------------------------------------------------------------------------
    struct COVERED_CODE_VISITOR_STATE
    {
        uint16 m_underResourceCount = 0;

        bool operator==(const COVERED_CODE_VISITOR_STATE& o) const
        {
            return m_underResourceCount==o.m_underResourceCount;
        }
    };

    template<typename PARENT,typename STATE>
    class DiscreteCoveredCodeVisitorBase : public PARENT
    {
    public:

        DiscreteCoveredCodeVisitorBase
            (
                FSystem::Private* InSystem,
                const TSharedPtr<const FModel>& InModel,
                const TSharedPtr<const FParameters>& InParams,
                unsigned InLodMask,
                bool bSkipResources=false
            )
            : PARENT(bSkipResources)
        {
            System = InSystem;
            Model = InModel;
            Params = InParams.Get();
            LODMask = InLodMask;

            // Visiting state
            PARENT::SetDefaultState( STATE() );
        }

        void Run( OP::ADDRESS at  )
        {
            PARENT::SetDefaultState( STATE() );

            PARENT::Traverse( at, Model->GetPrivate()->Program );
        }

    protected:

        virtual bool Visit( OP::ADDRESS Address, FProgram& Program )
        {
            bool bRecurse = true;

            EOpType Type = Program.GetOpType(Address);

            switch ( Type )
            {
            case EOpType::NU_CONDITIONAL:
            case EOpType::SC_CONDITIONAL:
            case EOpType::CO_CONDITIONAL:
            case EOpType::IM_CONDITIONAL:
            case EOpType::ME_CONDITIONAL:
            case EOpType::LA_CONDITIONAL:
            case EOpType::IN_CONDITIONAL:
			case EOpType::ED_CONDITIONAL:
            {
				OP::ConditionalArgs Args = Program.GetOpArgs<OP::ConditionalArgs>(Address);

                bRecurse = false;

                PARENT::RecurseWithCurrentState( Args.condition );

                // If there is no expression, we'll assume true.
                bool bValue = true;

                if (Args.condition)
                {
					bValue = System->BuildBool(Model, Params, Args.condition);
                }

                if (bValue)
                {
                    PARENT::RecurseWithCurrentState( Args.yes );
                }
                else
                {
                    PARENT::RecurseWithCurrentState( Args.no );
                }
                break;
            }

            case EOpType::NU_SWITCH:
            case EOpType::SC_SWITCH:
            case EOpType::CO_SWITCH:
            case EOpType::IM_SWITCH:
            case EOpType::ME_SWITCH:
            case EOpType::LA_SWITCH:
            case EOpType::IN_SWITCH:
			case EOpType::ED_SWITCH:
            {
                bRecurse = false;

				const uint8* Data = Program.GetOpArgsPointer(Address);
				
				OP::ADDRESS VarAddress;
				FMemory::Memcpy( &VarAddress, Data, sizeof(OP::ADDRESS));
				Data += sizeof(OP::ADDRESS);

                if (VarAddress)
                {
					OP::ADDRESS DefAddress;
					FMemory::Memcpy( &DefAddress, Data, sizeof(OP::ADDRESS));
					Data += sizeof(OP::ADDRESS);

					uint32 CaseCount;
					FMemory::Memcpy( &CaseCount, Data, sizeof(uint32));
					Data += sizeof(uint32);

                    PARENT::RecurseWithCurrentState( VarAddress );

                    int32 var = System->BuildInt( Model, Params, VarAddress );

					OP::ADDRESS valueAt = DefAddress;
					for (uint32 CaseIndex = 0; CaseIndex < CaseCount; ++CaseIndex)
					{
						int32 Condition;
						FMemory::Memcpy( &Condition, Data, sizeof(int32));		
						Data += sizeof(int32);

						OP::ADDRESS At;
						FMemory::Memcpy( &At, Data, sizeof(OP::ADDRESS));
						Data += sizeof(OP::ADDRESS);

						if (At && var == (int32)Condition)
						{
							valueAt = At;
							break;	
						}
					}

					PARENT::RecurseWithCurrentState( valueAt );
                }

                break;
            }


            case EOpType::IN_ADDLOD:
            {
                bRecurse = false;

				const uint8* Data = Program.GetOpArgsPointer(Address);

				uint8 LODCount;
				FMemory::Memcpy(&LODCount, Data, sizeof(uint8));
				Data += sizeof(uint8);

                STATE NewState = PARENT::GetCurrentState();
                for (int8 LODIndex=0; LODIndex < LODCount;++LODIndex)
                {
					OP::ADDRESS LODAddress;
					FMemory::Memcpy(&LODAddress, Data, sizeof(OP::ADDRESS));
					Data += sizeof(OP::ADDRESS);
					
					if (LODAddress)
                    {
                        bool bSelected = ( (1<< LODIndex) & LODMask ) != 0;
                        if (bSelected)
                        {
                            PARENT::RecurseWithState(LODAddress, NewState);
                        }
                    }
                }
                break;
            }


            case EOpType::IN_ADDMESH:
            {
				OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Address);

                bRecurse = false;

                PARENT::RecurseWithCurrentState(Args.instance);

                STATE NewState = PARENT::GetCurrentState();
                NewState.m_underResourceCount=1;

                OP::ADDRESS MeshAddress = Args.value;
                if (MeshAddress)
                {
                    PARENT::RecurseWithState(MeshAddress, NewState);
                }
                break;
            }


            case EOpType::IN_ADDIMAGE:
            {
				OP::InstanceAddArgs Args = Program.GetOpArgs<OP::InstanceAddArgs>(Address);

                bRecurse = false;

                PARENT::RecurseWithCurrentState(Args.instance);

                STATE NewState = PARENT::GetCurrentState();
				NewState.m_underResourceCount=1;

                OP::ADDRESS ImageAddress = Args.value;
                if (ImageAddress)
                {
                    PARENT::RecurseWithState(ImageAddress, NewState);
                }
                break;
            }

            default:
                break;
            }

            return bRecurse;
        }


    protected:
        FSystem::Private* System = nullptr;
		TSharedPtr<const FModel> Model;
        const FParameters* Params = nullptr;
        uint32 LODMask = 0;
    };


    /** Code visitor that :
    * - only traverses the operations that are relevant for a given set of parameter values. It
    * only considers the discrete parameters like integers and booleans. In the case of forks
    * caused by continuous parameters like float weights for interpolation, all the branches are
    * traversed.
    * - cannot change the instructions
    * - will not repeat visits to instructions with the same state
    * - the state has to be a compatible with COVERED_CODE_VISITOR_STATE
    */
    template<typename COVERED_STATE=COVERED_CODE_VISITOR_STATE>
    class UniqueDiscreteCoveredCodeVisitor :
            public DiscreteCoveredCodeVisitorBase
            <
            UniqueConstCodeVisitorIterative<COVERED_STATE>,
            COVERED_STATE
            >
    {
        using PARENT=DiscreteCoveredCodeVisitorBase<UniqueConstCodeVisitorIterative<COVERED_STATE>, COVERED_STATE>;

    public:

        UniqueDiscreteCoveredCodeVisitor
            (
                FSystem::Private* InSystem,
				const TSharedPtr<const FModel>& InModel,
                const TSharedPtr<const FParameters>& InParams,
                uint32 InLodMask
            )
            : PARENT(InSystem, InModel, InParams, InLodMask )
        {
        }

    };

}

