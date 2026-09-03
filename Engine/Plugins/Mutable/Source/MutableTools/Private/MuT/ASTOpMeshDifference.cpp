// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshDifference.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshAddTags.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	ASTOpMeshDifference::ASTOpMeshDifference()
		: Base(this), Target(this)
	{
	}


	//---------------------------------------------------------------------------------------------
	ASTOpMeshDifference::~ASTOpMeshDifference()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//---------------------------------------------------------------------------------------------
	bool ASTOpMeshDifference::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshDifference* Other = static_cast<const ASTOpMeshDifference*>(&otherUntyped);

			return Base == Other->Base && Target == Other->Target
				&& bIgnoreTextureCoords == Other->bIgnoreTextureCoords
				&& Channels == Other->Channels;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshDifference::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshDifference> n = new ASTOpMeshDifference();
		n->Base = mapChild(Base.child());
		n->Target = mapChild(Target.child());
		n->bIgnoreTextureCoords = bIgnoreTextureCoords;
		n->Channels = Channels;		
		return n;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshDifference::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Base);
		f(Target);
	}


	//---------------------------------------------------------------------------------------------
	uint64 ASTOpMeshDifference::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Base.child().get());
		hash_combine(res, Target.child().get());
		return res;
	}


	//---------------------------------------------------------------------------------------------
	void ASTOpMeshDifference::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();

			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::ME_DIFFERENCE);

			OP::ADDRESS BaseAt = Base ? Base->linkedAddress : 0;
			AppendCode(program.ByteCode, BaseAt);

			OP::ADDRESS TargetAt = Target ? Target->linkedAddress : 0;
			AppendCode(program.ByteCode, TargetAt);

			AppendCode(program.ByteCode, (uint8)bIgnoreTextureCoords);

			AppendCode(program.ByteCode, (uint8)Channels.Num());
			for (const FChannel& b : Channels)
			{
				AppendCode(program.ByteCode, b.Semantic);
				AppendCode(program.ByteCode, b.SemanticIndex);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshDifference::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		Ptr<ASTOp> BaseAt = Base.child();
		if (!BaseAt)
		{
			return nullptr;
		}

		Ptr<ASTOp> TargetAt = Target.child();
		if (!TargetAt)
		{
			return nullptr;
		}

		EOpType BaseType = BaseAt->GetOpType();
		EOpType TargetType = TargetAt->GetOpType();

		// See if both base and target have an operation that can be optimized in a combined way
		if (BaseType == TargetType)
		{
			switch (BaseType)
			{

			case EOpType::ME_SWITCH:
			{
				// If the switch variable and structure is the same
				const ASTOpSwitch* BaseSwitch = static_cast<const ASTOpSwitch*>(BaseAt.get());
				const ASTOpSwitch* TargetSwitch = static_cast<const ASTOpSwitch*>(TargetAt.get());
				bool bIsSimilarSwitch = BaseSwitch->IsCompatibleWith(TargetSwitch);
				if (!bIsSimilarSwitch)
				{
					break;
				}

				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(BaseAt);

				if (NewSwitch->Default)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseSwitch->Default.child();
					NewDiff->Target = TargetSwitch->Default.child();
					NewSwitch->Default = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->Cases.Num(); ++v)
				{
					if (NewSwitch->Cases[v].Branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Base = BaseSwitch->Cases[v].Branch.child();
						NewDiff->Target = TargetSwitch->FindBranch(BaseSwitch->Cases[v].Condition);
						NewSwitch->Cases[v].Branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}


			case EOpType::ME_CONDITIONAL:
			{
				const ASTOpConditional* BaseConditional = static_cast<const ASTOpConditional*>(BaseAt.get());
				const ASTOpConditional* TargetConditional = static_cast<const ASTOpConditional*>(TargetAt.get());
				bool bIsSimilar = BaseConditional->condition == TargetConditional->condition;
				if (!bIsSimilar)
				{
					break;
				}

				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(BaseAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseConditional->yes.child();
					NewDiff->Target = TargetConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = BaseConditional->no.child();
					NewDiff->Target = TargetConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}


			default:
				break;

			}
		}


		// If not already optimized
		if (!NewOp)
		{
			// Optimize only the mesh parameter
			switch (BaseType)
			{

			case EOpType::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(BaseAt);

				if (NewSwitch->Default)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewSwitch->Default.child();
					NewSwitch->Default = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->Cases.Num(); ++v)
				{
					if (NewSwitch->Cases[v].Branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Base = NewSwitch->Cases[v].Branch.child();
						NewSwitch->Cases[v].Branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case EOpType::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(BaseAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}

			case EOpType::ME_ADDTAGS:
			{
				Ptr<ASTOpMeshAddTags> NewAdd = mu::Clone<ASTOpMeshAddTags>(BaseAt);

				if (NewAdd->Source)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Base = NewAdd->Source.child();
					NewAdd->Source = NewDiff;
				}

				NewOp = NewAdd;
				break;
			}

			default:
				break;

			}
		}

		// If not already optimized
		if (!NewOp)
		{
			// Optimize only the shape parameter
			switch (TargetType)
			{

			case EOpType::ME_SWITCH:
			{
				// Move the operation down all the paths
				Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(TargetAt);

				if (NewSwitch->Default)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewSwitch->Default.child();
					NewSwitch->Default = NewDiff;
				}

				for (int32 v = 0; v < NewSwitch->Cases.Num(); ++v)
				{
					if (NewSwitch->Cases[v].Branch)
					{
						Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
						NewDiff->Target = NewSwitch->Cases[v].Branch.child();
						NewSwitch->Cases[v].Branch = NewDiff;
					}
				}

				NewOp = NewSwitch;
				break;
			}

			case EOpType::ME_CONDITIONAL:
			{
				// Move the operation down all the paths
				Ptr<ASTOpConditional> NewConditional = mu::Clone<ASTOpConditional>(TargetAt);

				if (NewConditional->yes)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewConditional->yes.child();
					NewConditional->yes = NewDiff;
				}

				if (NewConditional->no)
				{
					Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
					NewDiff->Target = NewConditional->no.child();
					NewConditional->no = NewDiff;
				}

				NewOp = NewConditional;
				break;
			}

			case EOpType::ME_ADDTAGS:
			{
				// Ignore tags in this branch
				const ASTOpMeshAddTags* Add = static_cast<const ASTOpMeshAddTags*>(TargetAt.get());

				Ptr<ASTOpMeshDifference> NewDiff = mu::Clone<ASTOpMeshDifference>(this);
				NewDiff->Target = Add->Source.child();
				NewOp = NewDiff;
				break;
			}

			default:
				break;

			}
		}


		return NewOp;
	}


	//-------------------------------------------------------------------------------------------------
	FSourceDataDescriptor ASTOpMeshDifference::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Base)
		{
			return Base->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
