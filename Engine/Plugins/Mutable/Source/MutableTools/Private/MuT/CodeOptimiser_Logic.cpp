// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpMeshMerge.h"
#include "MuT/ASTOpBoolEqualIntConst.h"
#include "MuT/CodeOptimiser.h"


namespace mu
{

	bool LocalLogicOptimiserAST(ASTOpList& roots)
    {
        MUTABLE_CPUPROFILER_SCOPE(LocalLogicOptimiserAST);

        bool bModified = false;

        // The separate steps should not be combined into one traversal

        // Unwrap some typical code daisy-chains
        //-----------------------------------------------------------------------------------------
        {
            MUTABLE_CPUPROFILER_SCOPE(Unwrap);
            ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](Ptr<ASTOp>& n)
            {
                bool bRecurse = true;

                if (n->GetOpType()==EOpType::IN_CONDITIONAL)
                {
					ASTOpConditional* topConditional = static_cast<ASTOpConditional*>(n.get());
					Ptr<ASTOp> yes = topConditional->yes.child();
                    if (yes && yes->GetOpType()==EOpType::IN_ADDSURFACE)
                    {
						ASTOpInstanceAdd* addSurface = static_cast<ASTOpInstanceAdd*>(yes.get());

                        bool ended = false;
                        while (!ended)
                        {
                            ended = true;
							Ptr<ASTOp> base = addSurface->instance.child();
                            if ( base && base->GetOpType()==EOpType::IN_CONDITIONAL )
                            {
								ASTOpConditional* bottomConditional = static_cast<ASTOpConditional*>(base.get());

                                // Are the two conditions exclusive?
                                bool conditionaAreExclusive = false;

                                ASTOpList facts;
                                facts.Add(topConditional->condition.child());

                                // Check if the child condition has a value with the current facts
                                Ptr<ASTOp> pChildCond = bottomConditional->condition.child();
                                ASTOp::FBoolEvalResult result;
                                {
                                    //MUTABLE_CPUPROFILER_SCOPE(EvaluateBool);
                                    result = pChildCond->EvaluateBool( facts );
                                }
                                if ( result==ASTOp::BET_FALSE )
                                {
                                    conditionaAreExclusive = true;
                                }

                                if (conditionaAreExclusive)
                                {
                                    if (addSurface->GetParentCount()==1)
                                    {
                                        // Directly modify the instruction to skip the impossible child option
                                        addSurface->instance = bottomConditional->no.child();
                                    }
                                    else
                                    {
                                        // Other parents may not impose the same condition that allows the optimisation.
                                        Ptr<ASTOpInstanceAdd> newAddSurface = mu::Clone<ASTOpInstanceAdd>(addSurface);
                                        newAddSurface->instance = bottomConditional->no.child();
                                        topConditional->yes = newAddSurface;
										addSurface = newAddSurface.get();
                                    }

                                    bModified = true;
                                    ended = false;
                                }
                            }
                        }
                    }
                }

                else if (n->GetOpType()==EOpType::ME_CONDITIONAL)
                {
					ASTOpConditional* topConditional = static_cast<ASTOpConditional*>(n.get());
                    Ptr<ASTOp> yes = topConditional->yes.child();
                    if ( yes && yes->GetOpType()==EOpType::ME_MERGE )
                    {
                        Ptr<ASTOpMeshMerge> Merge = static_cast<ASTOpMeshMerge*>(yes.get());

                        bool ended = false;
                        while (!ended)
                        {
                            ended = true;
							Ptr<ASTOp> base = Merge->Base.child();
                            if ( base && base->GetOpType()==EOpType::ME_CONDITIONAL )
                            {
								ASTOpConditional* bottomConditional = static_cast<ASTOpConditional*>(base.get());

                                // Are the two conditions exclusive?
                                bool conditionaAreExclusive = false;

                                ASTOpList facts;
                                facts.Add(topConditional->condition.child());

                                // Check if the child condition has a value with the current facts
								Ptr<ASTOp> pChildCond = bottomConditional->condition.child();
                                ASTOp::FBoolEvalResult result = pChildCond->EvaluateBool( facts );
                                if ( result==ASTOp::BET_FALSE )
                                {
                                    conditionaAreExclusive = true;
                                }

                                if (conditionaAreExclusive)
                                {
                                    if (Merge->GetParentCount()==1)
                                    {
                                        // Directly modify the instruction to skip the impossible child option
										Merge->Base = bottomConditional->no.child();
                                    }
                                    else
                                    {
                                        // Other parents may not impose the same condition that allows
                                        // the optimisation.
                                        Ptr<ASTOpMeshMerge> NewMerge = mu::Clone<ASTOpMeshMerge>(Merge);
										NewMerge->Base = bottomConditional->no.child();
                                        topConditional->yes = NewMerge;
                                        Merge = NewMerge;
                                    }

                                    bModified = true;
                                    ended = false;
                                }
                            }
                        }
                    }
                }


                return bRecurse;
            });
        }


        // See if we can turn conditional chains into switches: all conditions must be integer
        // comparison with the same variable.
        //-----------------------------------------------------------------------------------------
        {
            MUTABLE_CPUPROFILER_SCOPE(ConditionalToSwitch);
            ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](Ptr<ASTOp>& n)
            {
                bool bRecurse = true;

				ASTOpConditional* topConditional = nullptr;
				if (n && n->IsConditional())
				{
					topConditional = static_cast<ASTOpConditional*>(n.get());
				}

                if (topConditional && topConditional->condition)
                {
                    if ( topConditional->condition->GetOpType()==EOpType::BO_EQUAL_INT_CONST
                         &&
                         topConditional->no
                         &&
                         topConditional->no->GetOpType()==topConditional->GetOpType()
                         )
                    {
                        Ptr<ASTOpSwitch> switchOp = new ASTOpSwitch();
                        switchOp->Type = GetSwitchForType(GetOpDataType(topConditional->GetOpType()));

						ASTOpBoolEqualIntConst* firstCompare = static_cast<ASTOpBoolEqualIntConst*>(topConditional->condition.child().get());
                        switchOp->Variable = firstCompare->Value.child();

						Ptr<ASTOp> current = n;
                        while(current)
                        {
                            bool bValid = false;

							ASTOpConditional* conditional = nullptr;
							if (current && current->IsConditional())
							{
								conditional = static_cast<ASTOpConditional*>(current.get());
							}

                            if ( conditional
                                 &&
                                 conditional->GetOpType()==topConditional->GetOpType()
                                 &&
                                 conditional->condition
                                 &&
                                 conditional->condition->GetOpType()==EOpType::BO_EQUAL_INT_CONST )
                            {
								ASTOpBoolEqualIntConst* compare = static_cast<ASTOpBoolEqualIntConst*>(conditional->condition.child().get());
                                check(compare);
                                if (compare)
                                {
                                    auto compareValue = compare->Value.child();
                                    check(compare);
                                    if ( compareValue == switchOp->Variable.child() )
                                    {
                                        switchOp->Cases.Emplace( compare->Constant, switchOp, conditional->yes.child() );

                                        current = conditional->no.child();
										bValid = true;
                                    }
                                }
                            }

                            if (!bValid)
                            {
                                switchOp->Default = current;
                                current = nullptr;
                            }
                        }

                        const int MIN_CONDITIONS_TO_CREATE_SWITCH = 3;
                        if (switchOp->Cases.Num()>=MIN_CONDITIONS_TO_CREATE_SWITCH)
                        {
                            ASTOp::Replace(n,switchOp);
                            n = switchOp;
                            bModified = true;
                        }
                    }

                }

                return bRecurse;
            });
        }


        // Float operations up switches, to tidy up the code and reduce its size.
		// TODO?
		/*
        {
            MUTABLE_CPUPROFILER_SCOPE(FloatSwitches);

            ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](Ptr<ASTOp>& n)
            {
                bool bRecurse = true;

				ASTOpSwitch* TopSwitch = nullptr;
				if (n && n->IsSwitch())
				{
					TopSwitch = static_cast<ASTOpSwitch*>(n.get());
				}

                if (TopSwitch)
                {
                    bool bFirst = true;
					EOpType CaseType = EOpType::NONE;
                    for (const ASTOpSwitch::FCase& c: TopSwitch->Cases)
                    {
                        if ( c.Branch )
                        {
                            if (bFirst)
                            {
								bFirst = false;
								CaseType = c.Branch->GetOpType();
                            }
                            else
                            {
                                if (CaseType !=c.Branch->GetOpType())
                                {
									CaseType = EOpType::NONE;
                                    break;
                                }
                            }
                        }
                    }
                }

                return bRecurse;
            });
        }
		*/

        return bModified;
    }

}

