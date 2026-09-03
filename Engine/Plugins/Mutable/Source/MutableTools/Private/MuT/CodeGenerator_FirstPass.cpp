// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator_FirstPass.h"

#include "MuT/CodeGenerator.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpBoolEqualIntConst.h"
#include "MuT/ASTOpBoolAnd.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeModifierSurfaceEdit.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeScalarEnumParameter.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	FirstPassGenerator::FirstPassGenerator()
	{
		// Default conditions when there is no restriction accumulated.
		FConditionContext noCondition;
        CurrentCondition.Add(noCondition);
        CurrentStateCondition.Add(StateCondition());
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate( TSharedPtr<FErrorLog> InErrorLog,
                                       const Node* Root,
									   bool bIgnoreStates,
									   CodeGenerator* InGenerator )
	{
		MUTABLE_CPUPROFILER_SCOPE(FirstPassGenerate);

		Generator = InGenerator;
		ErrorLog = InErrorLog;

		// Step 1: collect all objects, surfaces and object conditions
        if (Root)
		{
 			Generate_Generic(Root);
 		}

		// Step 2: Collect all tags and a list of the surfaces and modifiers that activate them
		for (int32 s=0; s<Surfaces.Num(); ++s)
		{
			// Collect the tags in new surfaces
			for (int32 t=0; t< Surfaces[s].Node->Tags.Num(); ++t)
			{
				int32 tag = -1;
                const FString& tagStr = Surfaces[s].Node->Tags[t];
                for (int32 i = 0; i<Tags.Num() && tag<0; ++i)
				{
                    if (Tags[i].Tag == tagStr)
					{
						tag = i;
					}
				}

				// New tag?
				if (tag < 0)
				{
                    tag = Tags.Num();
					FTag newTag;
                    newTag.Tag = tagStr;
                    Tags.Add(newTag);
				}

                if (Tags[tag].Surfaces.Find(s)==INDEX_NONE)
				{
                    Tags[tag].Surfaces.Add(s);
				}
			}
		}

		// TODO: Modifier's enabling tags?
		for (int32 ModifierIndex = 0; ModifierIndex < Modifiers.Num(); ++ModifierIndex)
		{
			// Collect the tags in the modifiers
			for (const FString& ModifierTag: Modifiers[ModifierIndex].Node->EnableTags)
			{
				int32 TagIndex = Tags.IndexOfByPredicate([&](const FTag& Candidate)
					{ 
						return Candidate.Tag == ModifierTag;
					});

				// New tag?
				if (TagIndex < 0)
				{
					FTag newTag;
					newTag.Tag = ModifierTag;
					TagIndex = Tags.Add(newTag);
				}

				if (Tags[TagIndex].Modifiers.Find(ModifierIndex) == INDEX_NONE)
				{
					Tags[TagIndex].Modifiers.Add(ModifierIndex);
				}
			}
		}

		// Step 3: Create default state if necessary
        if ( bIgnoreStates )
        {
            States.Empty();
        }

        if ( States.IsEmpty() )
        {
            FObjectState Data;
            Data.Name = "Default";
            States.Add( Data );
        }
	}


	void FirstPassGenerator::Generate_Generic(const Node* Root)
	{
		if (!Root)
		{
			return;
		}

		if (Root->GetType()==NodeSurfaceNew::GetStaticType())
		{
			Generate_SurfaceNew(static_cast<const NodeSurfaceNew*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceVariation::GetStaticType())
		{
			Generate_SurfaceVariation(static_cast<const NodeSurfaceVariation*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceSwitch::GetStaticType())
		{
			Generate_SurfaceSwitch(static_cast<const NodeSurfaceSwitch*>(Root));
		}
		else if (Root->GetType() == NodeComponentNew::GetStaticType())
		{
			Generate_ComponentNew(static_cast<const NodeComponentNew*>(Root));
		}
		else if (Root->GetType() == NodeComponentEdit::GetStaticType())
		{
			Generate_ComponentEdit(static_cast<const NodeComponentEdit*>(Root));
		}
		else if (Root->GetType() == NodeComponentSwitch::GetStaticType())
		{
			Generate_ComponentSwitch(static_cast<const NodeComponentSwitch*>(Root));
		}
		else if (Root->GetType() == NodeComponentVariation::GetStaticType())
		{
			Generate_ComponentVariation(static_cast<const NodeComponentVariation*>(Root));
		}
		else if (Root->GetType() == NodeObjectNew::GetStaticType())
		{
			Generate_ObjectNew(static_cast<const NodeObjectNew*>(Root));
		}
		else if (Root->GetType() == NodeObjectGroup::GetStaticType())
		{
			Generate_ObjectGroup(static_cast<const NodeObjectGroup*>(Root));
		}
		else if (Root->GetType() == NodeLOD::GetStaticType())
		{
			Generate_LOD(static_cast<const NodeLOD*>(Root));
		}
		else if (Root->GetType() == NodeModifier::GetStaticType())
		{
			Generate_Modifier(static_cast<const NodeModifier*>(Root));
		}
		else
		{
			// This node type is not supported in this pass.
			check(false);
		}
	}

	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_Modifier(const NodeModifier* InNode)
	{
		// Add the data about this modifier
		FModifier thisData;
		thisData.Node = InNode;
		thisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		thisData.StateCondition = CurrentStateCondition.Last();
		thisData.PositiveTags = CurrentPositiveTags;
		thisData.NegativeTags = CurrentNegativeTags;
		Modifiers.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceNew(const NodeSurfaceNew* InNode)
	{
		// Add the data about this surface
		FSurface thisData;
		thisData.Node = InNode;
		thisData.Component = CurrentComponent;
		thisData.LOD = CurrentLOD;
		thisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		thisData.StateCondition = CurrentStateCondition.Last();
		thisData.PositiveTags = CurrentPositiveTags;
		thisData.NegativeTags = CurrentNegativeTags;
		Surfaces.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceVariation(const NodeSurfaceVariation* InNode)
	{
        switch(InNode->Type)
        {

        case NodeSurfaceVariation::VariationType::Tag:
        {
            // Any of the tags in the variations would prevent the default surface
			TArray<FString> OldNegativeTags = CurrentNegativeTags;
            for (int32 v=0; v< InNode->Variations.Num(); ++v)
            {
                CurrentNegativeTags.Add(InNode->Variations[v].Tag);
            }

            for(const Ptr<NodeSurface>& n: InNode->DefaultSurfaces)
            {
				Generate_Generic(n.get());
			}
            for(const Ptr<NodeModifier>& n: InNode->DefaultModifiers)
            {
				Generate_Modifier(n.get());
            }

            CurrentNegativeTags = OldNegativeTags;

            for (int32 v=0; v< InNode->Variations.Num(); ++v)
            {
                CurrentPositiveTags.Add(InNode->Variations[v].Tag);
                for (const Ptr<NodeSurface>& s : InNode->Variations[v].Surfaces)
                {
					Generate_Generic(s.get());
                }

                for (const Ptr<NodeModifier>& s : InNode->Variations[v].Modifiers)
                {
					Generate_Modifier(s.get());
				}

                CurrentPositiveTags.Pop();

                // Tags have an order in a variation node: the current tag should prevent any following
                // variation surface
                CurrentNegativeTags.Add(InNode->Variations[v].Tag);
            }

            CurrentNegativeTags = OldNegativeTags;

            break;
        }


        case NodeSurfaceVariation::VariationType::State:
        {
            int32 StateCount = States.Num();

            // Default
            {
                // Store the states for the default branch here
                StateCondition DefaultStates;
                {
					StateCondition AllTrue;
					AllTrue.Init(true, StateCount);
					DefaultStates = CurrentStateCondition.Last().IsEmpty()
                            ? AllTrue
                            : CurrentStateCondition.Last();

                    for (const NodeSurfaceVariation::FVariation& v: InNode->Variations)
                    {
                        for( int32 StateIndex=0; StateIndex < StateCount; ++StateIndex)
                        {
                            if (States[StateIndex].Name==v.Tag)
                            {
                                // Remove this state from the default options, since it has its own variation
								DefaultStates[StateIndex] = false;
                            }
                        }
                    }
                }

                CurrentStateCondition.Add(DefaultStates);

                for (const Ptr<NodeSurface>& n : InNode->DefaultSurfaces)
                {
					Generate_Generic(n.get());
				}
                for (const Ptr<NodeModifier>& n : InNode->DefaultModifiers)
                {
					Generate_Modifier(n.get());
				}

                CurrentStateCondition.Pop();
            }

            // Variation branches
            for (const auto& v: InNode->Variations)
            {
                // Store the states for this variation here
				StateCondition variationStates;
				variationStates.Init(false,StateCount);

                for( int32 StateIndex=0; StateIndex< StateCount; ++StateIndex)
                {
                    if (States[StateIndex].Name==v.Tag)
                    {
                        variationStates[StateIndex] = true;
                    }
                }

                CurrentStateCondition.Add(variationStates);

                for (const Ptr<NodeSurface>& n : v.Surfaces)
                {
					Generate_Generic(n.get());
				}
                for (const Ptr<NodeModifier>& n : v.Modifiers)
                {
					Generate_Modifier(n.get());
				}

                CurrentStateCondition.Pop();
            }

            break;
        }

        default:
            // Case not implemented.
            check(false);
            break;
        }
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceSwitch(const NodeSurfaceSwitch* InNode)
	{
		if (InNode->Options.Num() == 0)
		{
			// No options in the switch!
			return;
		}

		// Prepare the enumeration parameter
		CodeGenerator::FGenericGenerationOptions Options;
		CodeGenerator::FScalarGenerationResult ScalarResult;
		if (InNode->Parameter)
		{
			Generator->GenerateScalar( ScalarResult, Options, InNode->Parameter );
		}
		else
		{
			// This argument is required
			ScalarResult.op = Generator->GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, InNode->GetMessageContext());
		}

		// Parse the options
		for (int32 t = 0; t < InNode->Options.Num(); ++t)
		{
			// Create a comparison operation as the boolean parameter for the child
			Ptr<ASTOpBoolEqualIntConst> ParamOp = new ASTOpBoolEqualIntConst;
			ParamOp->Value = ScalarResult.op;
			ParamOp->Constant = t;

			Ptr<ASTOp> CurrentOp = ParamOp;

			// Combine the new condition with previous conditions coming from parent objects
			if (CurrentCondition.Last().ObjectCondition)
			{
				Ptr<ASTOpBoolAnd> op = new ASTOpBoolAnd();
				op->A = CurrentCondition.Last().ObjectCondition;
				op->B = CurrentOp;
				CurrentOp = op;
			}

			FConditionContext data;
			data.ObjectCondition = CurrentOp;
			CurrentCondition.Push(data);

			if (InNode->Options[t])
			{
				Generate_Generic(InNode->Options[t].get());
			}

			CurrentCondition.Pop();
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentNew(const NodeComponentNew* InNode)
	{
		// Add the data about this surface
		FComponent ThisData;
		ThisData.Component = InNode;
		ThisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		ThisData.PositiveTags = CurrentPositiveTags;
		ThisData.NegativeTags = CurrentNegativeTags;
		Components.Add(ThisData);

        CurrentComponent = InNode;

		CurrentLOD = 0;
		for (const Ptr<NodeLOD>& c : InNode->LODs)
		{
			if (c)
			{
				Generate_LOD(c.get());
			}
			++CurrentLOD;
		}
		CurrentLOD = -1;

		CurrentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentEdit(const NodeComponentEdit* InNode)
	{
		CurrentComponent = InNode->GetParentComponentNew();

		CurrentLOD = 0;
		for (const Ptr<NodeLOD>& c : InNode->LODs)
		{
			if (c)
			{
				Generate_LOD(c.get());
			}
			++CurrentLOD;
		}
		CurrentLOD = -1;

		CurrentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentVariation(const NodeComponentVariation* InNode)
	{
		// Any of the tags in the variations would prevent the default surface
		TArray<FString> OldNegativeTags = CurrentNegativeTags;
		for (int32 v = 0; v < InNode->Variations.Num(); ++v)
		{
			CurrentNegativeTags.Add(InNode->Variations[v].Tag);
		}

		Generate_Generic(InNode->DefaultComponent.get());

		CurrentNegativeTags = OldNegativeTags;

		for (int32 v = 0; v < InNode->Variations.Num(); ++v)
		{
			CurrentPositiveTags.Add(InNode->Variations[v].Tag);
			Generate_Generic(InNode->Variations[v].Component.get());

			CurrentPositiveTags.Pop();

			// Tags have an order in a variation node: the current tag should prevent any following variation
			CurrentNegativeTags.Add(InNode->Variations[v].Tag);
		}

		CurrentNegativeTags = OldNegativeTags;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentSwitch(const NodeComponentSwitch* InNode)
	{
		if (InNode->Options.Num() == 0)
		{
			// No options in the switch!
			return;
		}

		// Prepare the enumeration parameter
		CodeGenerator::FGenericGenerationOptions Options;
		CodeGenerator::FScalarGenerationResult ScalarResult;
		if (InNode->Parameter)
		{
			Generator->GenerateScalar(ScalarResult, Options, InNode->Parameter);
		}
		else
		{
			// This argument is required
			ScalarResult.op = Generator->GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, InNode->GetMessageContext());
		}

		// Parse the options
		for (int32 t = 0; t < InNode->Options.Num(); ++t)
		{
			// Create a comparison operation as the boolean parameter for the child
			Ptr<ASTOpBoolEqualIntConst> ParamOp = new ASTOpBoolEqualIntConst();
			ParamOp->Value = ScalarResult.op;
			ParamOp->Constant = t;

			Ptr<ASTOp> CurrentOp = ParamOp;

			// Combine the new condition with previous conditions coming from parent objects
			if (CurrentCondition.Last().ObjectCondition)
			{
				Ptr<ASTOpBoolAnd> op = new ASTOpBoolAnd();
				op->A = CurrentCondition.Last().ObjectCondition;
				op->B = CurrentOp;
				CurrentOp = op;
			}

			FConditionContext data;
			data.ObjectCondition = CurrentOp;
			CurrentCondition.Push(data);

			if (InNode->Options[t])
			{
				Generate_Generic(InNode->Options[t].get());
			}

			CurrentCondition.Pop();
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_LOD(const NodeLOD* InNode)
	{
		for (const Ptr<NodeSurface>& c : InNode->Surfaces)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectNew(const NodeObjectNew* InNode)
	{
		// Add the data about this object
		FObject thisData;
		thisData.Node = InNode;
        thisData.Condition = CurrentCondition.Last().ObjectCondition;
		Objects.Add(thisData);

        // Accumulate the model states
        for ( const FObjectState& s: InNode->States )
        {
            States.Add( s );

            if ( s.RuntimeParams.Num() > MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE )
            {
                FString Msg = FString::Printf( TEXT("State [%s] has more than %d runtime parameters. Their update may fail."), 
					*s.Name,
                    MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE);
				ErrorLog->Add(Msg, ELMT_ERROR, InNode->GetMessageContext());
            }
        }

		// Process the components
		for (const Ptr<NodeComponent>& Component : InNode->Components)
		{
			if (Component)
			{
				Generate_Generic(Component.get());
			}
		}

		// Process the modifiers
		for (const Ptr<NodeModifier>& c : InNode->Modifiers)
		{
			if (c)
			{
				Generate_Modifier(c.get());
			}
		}

		// Process the children
		for (const Ptr<NodeObject>& Child : InNode->Children)
		{
			if (Child)
			{
				Generate_Generic(Child.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectGroup(const NodeObjectGroup* Node)
	{
		// Prepare the enumeration parameter if necessary
        Ptr<ASTOpParameter> enumOp;
        if (Node->Type==NodeObjectGroup::CS_ALWAYS_ONE ||
			Node->Type==NodeObjectGroup::CS_ONE_OR_NONE )
        {
			UE::TUniqueLock Lock(ParameterNodes.Mutex);
			Ptr<ASTOpParameter>* Found = ParameterNodes.GenericParametersCache.Find(Node);

			if (!Found)
			{
				Ptr<ASTOpParameter> op = new ASTOpParameter();
				op->Type = EOpType::NU_PARAMETER;
				op->Parameter.DefaultValue.Set<FParamIntType>(Node->DefaultValue);

				op->Parameter.Name = Node->Name;
				bool bParseOk = FGuid::Parse(Node->Uid, op->Parameter.UID);
				check(bParseOk);
				op->Parameter.Type = EParameterType::Int;
				op->Parameter.DefaultValue.Set<FParamIntType>(Node->DefaultValue);

				if (Node->Type == NodeObjectGroup::CS_ONE_OR_NONE)
				{
					FParameterDesc::FIntValueDesc nullValue;
					nullValue.Value = -1;
					nullValue.Name = "None";
					op->Parameter.PossibleValues.Add(nullValue);
				}

				ParameterNodes.GenericParametersCache.Add(Node, op);

				enumOp = op;
			}
			else
			{
				enumOp = *Found;
			}
        }


        // Parse the child objects
		for ( int32 t=0; t< Node->Children.Num(); ++t )
        {
            if ( mu::Ptr<const NodeObject> ChildNode = Node->Children[t] )
            {
                // Overwrite the implicit condition
                Ptr<ASTOp> paramOp = 0;
                switch (Node->Type )
                {
                    case NodeObjectGroup::CS_TOGGLE_EACH:
                    {           
						if (ChildNode->GetType() == NodeObjectGroup::GetStaticType())
						{
							FString Msg = FString::Printf(TEXT("The Group Node [%s] has type Toggle and its direct child is a Group node, which is not allowed. Change the type or add a Child Object node in between them."), *Node->Name);
							ErrorLog->Add(Msg, ELMT_ERROR, Node->GetMessageContext());
						}
						else
						{
							// Create a new boolean parameter			
							UE::TUniqueLock Lock(ParameterNodes.Mutex);
							Ptr<ASTOpParameter>* Found = ParameterNodes.GenericParametersCache.Find(ChildNode);

							if (!Found)
							{
								Ptr<ASTOpParameter> op = new ASTOpParameter();
								op->Type = EOpType::BO_PARAMETER;

								op->Parameter.Name = ChildNode->GetName();
								bool bParseOk = FGuid::Parse(ChildNode->GetUid(), op->Parameter.UID);
								check(bParseOk);
								op->Parameter.Type = EParameterType::Bool;
								op->Parameter.DefaultValue.Set<FParamBoolType>(false);

								ParameterNodes.GenericParametersCache.Add(ChildNode, op);

								paramOp = op;
							}
							else
							{
								paramOp = *Found;
							}
						}

                        break;
                    }

                    case NodeObjectGroup::CS_ALWAYS_ALL:
                    {
                        // Create a constant true boolean that the optimiser will remove later.
                        paramOp = new ASTOpConstantBool(true);
                        break;
                    }

                    case NodeObjectGroup::CS_ONE_OR_NONE:
                    case NodeObjectGroup::CS_ALWAYS_ONE:
                    {
                        // Add the option to the enumeration parameter
                        FParameterDesc::FIntValueDesc value;
                        value.Value = (int16)t;
                        value.Name = ChildNode->GetName();
                        enumOp->Parameter.PossibleValues.Add( value );

                        check(enumOp);

                        // Create a comparison operation as the boolean parameter for the child
                        Ptr<ASTOpBoolEqualIntConst> op = new ASTOpBoolEqualIntConst();
                        op->Value = enumOp;
                        op->Constant = t;

                        paramOp = op;
                        break;
                    }

                    default:
                        check( false );
                }

                // Combine the new condition with previous conditions coming from parent objects
                if (CurrentCondition.Last().ObjectCondition)
                {
                    Ptr<ASTOpBoolAnd> op = new ASTOpBoolAnd();
                    op->A = CurrentCondition.Last().ObjectCondition;
                    op->B = paramOp;
                    paramOp = op;
                }

				FConditionContext data;
                data.ObjectCondition = paramOp;
                CurrentCondition.Add( data );

				Generate_Generic(ChildNode.get());

                CurrentCondition.Pop();
            }
        }
 	}

}

