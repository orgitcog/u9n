// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpInstanceAdd.h"

#include "MuT/ASTOpConditional.h"
#include "MuT/CodeOptimiser.h"
#include "MuR/ModelPrivate.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "Misc/AssertionMacros.h"


namespace mu
{

	ASTOpInstanceAdd::ASTOpInstanceAdd()
		: instance(this)
		, value(this)
	{
	}


	ASTOpInstanceAdd::~ASTOpInstanceAdd()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpInstanceAdd::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpInstanceAdd* other = static_cast<const ASTOpInstanceAdd*>(&otherUntyped);
			return type == other->type &&
				instance == other->instance &&
				value == other->value &&
				id == other->id &&
				ExternalId == other->ExternalId &&
				SharedSurfaceId == other->SharedSurfaceId &&
				name == other->name;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpInstanceAdd::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpInstanceAdd> n = new ASTOpInstanceAdd();
		n->type = type;
		n->instance = mapChild(instance.child());
		n->value = mapChild(value.child());
		n->id = id;
		n->ExternalId = ExternalId;
		n->SharedSurfaceId = SharedSurfaceId;
		n->name = name;
		return n;
	}


	uint64 ASTOpInstanceAdd::Hash() const
	{
		uint64 res = std::hash<size_t>()(size_t(type));
		hash_combine(res, instance.child().get());
		hash_combine(res, value.child().get());
		return res;
	}


	void ASTOpInstanceAdd::Assert()
	{
		switch (type)
		{
		case EOpType::IN_ADDMESH:
		case EOpType::IN_ADDIMAGE:
		case EOpType::IN_ADDVECTOR:
		case EOpType::IN_ADDSCALAR:
		case EOpType::IN_ADDSTRING:
		case EOpType::IN_ADDSURFACE:
		case EOpType::IN_ADDCOMPONENT:
		case EOpType::IN_ADDLOD:
			break;
		default:
			// Unexpected type
			check(false);
			break;
		}

		ASTOp::Assert();
	}


	void ASTOpInstanceAdd::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(instance);
		f(value);
	}


	void ASTOpInstanceAdd::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::InstanceAddArgs Args;
			FMemory::Memzero(Args);
			Args.id = id;
			Args.ExternalId = ExternalId;
			Args.SharedSurfaceId = SharedSurfaceId;
			Args.name = program.AddConstant(name);

			if (instance) Args.instance = instance->linkedAddress;
			if (value) Args.value = value->linkedAddress;

			if (type == EOpType::IN_ADDIMAGE ||
				type == EOpType::IN_ADDMESH)
			{
				// Find out relevant parameters. \todo: this may be optimised by reusing partial
				// values in a LINK_CONTEXT or similar
				SubtreeRelevantParametersVisitorAST visitor;
				visitor.Run(value.child());

				TArray<uint16> params;
				for (const FString& paramName : visitor.Parameters)
				{
					for (int32 i = 0; i < program.Parameters.Num(); ++i)
					{
						const auto& param = program.Parameters[i];
						if (param.Name == paramName)
						{
							params.Add(uint16(i));
							break;
						}
					}
				}

				params.Sort();

				auto it = program.ParameterLists.Find(params);

				if (it != INDEX_NONE)
				{
					Args.relevantParametersListIndex = it;
				}
				else
				{
					Args.relevantParametersListIndex = uint32_t(program.ParameterLists.Num());
					program.ParameterLists.Add(params);
				}
			}

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, type);
			AppendCode(program.ByteCode, Args);
		}

	}


	Ptr<ASTOp> ASTOpInstanceAdd::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		switch (type)
		{

		case EOpType::IN_ADDMESH:
		{
			Ptr<ASTOp> ValueAt = value.child();
			if (!ValueAt)
			{
				break;
			}

			EOpType ValueType = ValueAt->GetOpType();
			switch (ValueType)
			{

			// We want to move the conditional up the op graph because this way the mesh root operation
			// can easily match other mesh root operations in the program. This is important because this operation
			// address is used for caching, and avoiding duplicate mesh work with multi-components.
			case EOpType::ME_CONDITIONAL:
			{
				const ASTOpConditional* TypedValue = static_cast<const ASTOpConditional*>(ValueAt.get());

				bool bCanSink = !instance.child();

				const ASTOpConditional* TypedSource = nullptr;
				if (instance.child() && instance.child()->GetOpType() == EOpType::IN_CONDITIONAL)
				{
					TypedSource = static_cast<const ASTOpConditional*>(instance.child().get());
					if (TypedSource->condition == TypedValue->condition)
					{
						bCanSink = true;
					}
				}

				if (bCanSink)
				{
					Ptr<ASTOpConditional> NewOp = mu::Clone<ASTOpConditional>(ValueAt);
					NewOp->type = EOpType::IN_CONDITIONAL;

					Ptr<ASTOpInstanceAdd> TrueOp = mu::Clone<ASTOpInstanceAdd>(this);
					TrueOp->instance = TypedSource ? TypedSource->yes.child() : nullptr; 
					TrueOp->value = TypedValue->yes.child();
					NewOp->yes = TrueOp;

					Ptr<ASTOpInstanceAdd> FalseOp = mu::Clone<ASTOpInstanceAdd>(this);
					FalseOp->instance = TypedSource ? TypedSource->no.child() : nullptr; 
					FalseOp->value = TypedValue->no.child();
					NewOp->no = FalseOp;

					at = NewOp;
				}
				break;
			}

			// I don't think we have switches at this point but if we did, it would be interesting to optimize
			// for the same reasons 
			case EOpType::ME_SWITCH:
			{
				// TODO
				break;
			}

			default:
				break;
			}

			break;
		}

		default:
			break;
		}

		return at;
	}


}
