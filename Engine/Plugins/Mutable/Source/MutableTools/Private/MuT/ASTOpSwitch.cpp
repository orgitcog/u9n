// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpSwitch.h"

#include "MuT/ASTOpParameter.h"
#include "MuT/ASTOpConstantInt.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "Containers/Map.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpSwitch::ASTOpSwitch()
		: Variable(this)
		, Default(this)
	{
	}


	ASTOpSwitch::~ASTOpSwitch()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpSwitch::IsEqual(const ASTOp& otherUntyped) const
	{
		if (GetOpType()==otherUntyped.GetOpType())
		{
			const ASTOpSwitch* other = static_cast<const ASTOpSwitch*>(&otherUntyped);
			return Type == other->Type && Variable == other->Variable &&
				Cases == other->Cases && Default == other->Default;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpSwitch::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpSwitch> n = new ASTOpSwitch();
		n->Type = Type;
		n->Variable = mapChild(Variable.child());
		n->Default = mapChild(Default.child());
		for (const FCase& c : Cases)
		{
			n->Cases.Emplace(c.Condition, n, mapChild(c.Branch.child()));
		}
		return n;
	}


	void ASTOpSwitch::Assert()
	{
		switch (Type)
		{
		case EOpType::NU_SWITCH:
		case EOpType::SC_SWITCH:
		case EOpType::CO_SWITCH:
		case EOpType::IM_SWITCH:
		case EOpType::ME_SWITCH:
		case EOpType::LA_SWITCH:
		case EOpType::IN_SWITCH:
		case EOpType::ED_SWITCH:
			break;
		default:
			// Unexpected Type
			check(false);
			break;
		}

		ASTOp::Assert();
	}


	uint64 ASTOpSwitch::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(Type));
		for (const FCase& c : Cases)
		{
			hash_combine(res, c.Condition);
			hash_combine(res, c.Branch.child().get());
		}
		return res;
	}


	mu::Ptr<ASTOp> ASTOpSwitch::GetFirstValidValue()
	{
		for (int32 i = 0; i < Cases.Num(); ++i)
		{
			if (Cases[i].Branch)
			{
				return Cases[i].Branch.child();
			}
		}
		return nullptr;
	}


	bool ASTOpSwitch::IsCompatibleWith(const ASTOpSwitch* other) const
	{
		if (!other) return false;
		if (Variable.child() != other->Variable.child()) return false;
		if (Cases.Num() != other->Cases.Num()) return false;
		for (const FCase& c : Cases)
		{
			bool found = false;
			for (const FCase& o : other->Cases)
			{
				if (c.Condition == o.Condition)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				return false;
			}
		}

		return true;
	}


	mu::Ptr<ASTOp> ASTOpSwitch::FindBranch(int32 Condition) const
	{
		for (const FCase& c : Cases)
		{
			if (c.Condition == Condition)
			{
				return c.Branch.child();
			}
		}

		return Default.child();
	}


	void ASTOpSwitch::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Variable);
		f(Default);
		for (FCase& cas : Cases)
		{
			f(cas.Branch);
		}
	}


	void ASTOpSwitch::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());

			OP::ADDRESS VarAddress = Variable ? Variable->linkedAddress : 0;
			OP::ADDRESS DefAddress = Default ? Default->linkedAddress : 0;

			AppendCode(program.ByteCode, Type);
			AppendCode(program.ByteCode, VarAddress);
			AppendCode(program.ByteCode, DefAddress);
			AppendCode(program.ByteCode, (uint32_t)Cases.Num());

			for (const FCase& Case : Cases)
			{
				OP::ADDRESS CaseBranchAddress = Case.Branch ? Case.Branch->linkedAddress : 0;
				AppendCode(program.ByteCode, Case.Condition);
				AppendCode(program.ByteCode, CaseBranchAddress);
			}
		}
	}


	FImageDesc ASTOpSwitch::GetImageDesc(bool bReturnBestOption, class FGetImageDescContext* Context) const
	{
		FImageDesc Result;

		// Local context in case it is necessary
		FGetImageDescContext LocalContext;
		if (!Context)
		{
			Context = &LocalContext;
		}
		else
		{
			// Cached result?
			FImageDesc* PtrValue = Context->m_results.Find(this);
			if (PtrValue)
			{
				return *PtrValue;
			}
		}

		// In a switch we cannot guarantee the size and format.
		// We check all the options, and if they are the same we return that.
		// Otherwise, we return a descriptor with empty fields in the conflicting ones, size or format.
		// In some places this will force re-formatting of the image.
		// The code optimiser will take care then of moving the format operations down to each
		// Branch and remove the unnecessary ones.
		FImageDesc Candidate;

		bool bSameSize = true;
		bool bSameFormat = true;
		bool bSameLods = true;
		bool bFirst = true;

		if (Default)
		{
			FImageDesc ChildDesc = Default->GetImageDesc(bReturnBestOption, Context);
			Candidate = ChildDesc;
			bFirst = false;
		}

		for (int32 CaseIndex = 0; CaseIndex < Cases.Num(); ++CaseIndex)
		{
			if (Cases[CaseIndex].Branch)
			{
				FImageDesc ChildDesc = Cases[CaseIndex].Branch->GetImageDesc(bReturnBestOption, Context);
				if (bFirst)
				{
					Candidate = ChildDesc;
					bFirst = false;
				}
				else
				{
					bSameSize = bSameSize && (Candidate.m_size == ChildDesc.m_size);
					bSameFormat = bSameFormat && (Candidate.m_format == ChildDesc.m_format);
					bSameLods = bSameLods && (Candidate.m_lods == ChildDesc.m_lods);

					if (bReturnBestOption)
					{
						Candidate.m_format = GetMostGenericFormat(Candidate.m_format, ChildDesc.m_format);

						// Return the biggest size
						Candidate.m_size[0] = FMath::Max(Candidate.m_size[0], ChildDesc.m_size[0]);
						Candidate.m_size[1] = FMath::Max(Candidate.m_size[1], ChildDesc.m_size[1]);
					}
				}
			}
		}

		Result = Candidate;

		// In case of ReturnBestOption the first valid case will be used to determine size and lods.
		// Format will be the most generic from all Cases.
		if (!bSameFormat && !bReturnBestOption)
		{
			Result.m_format = EImageFormat::None;
		}

		if (!bSameSize && !bReturnBestOption)
		{
			Result.m_size = FImageSize(0, 0);
		}

		if (!bSameLods && !bReturnBestOption)
		{
			Result.m_lods = 0;
		}

		// Cache the result
		if (Context)
		{
			Context->m_results.Add(this, Result);
		}

		return Result;
	}


	void ASTOpSwitch::GetBlockLayoutSize(uint64 BlockId, int32* pBlockX, int32* pBlockY, FBlockLayoutSizeCache* cache)
	{
		switch (Type)
		{
		case EOpType::LA_SWITCH:
		{
			Ptr<ASTOp> child = GetFirstValidValue();
			if (!child)
			{
				child = Default.child();
			}

			if (child)
			{
				child->GetBlockLayoutSizeCached(BlockId, pBlockX, pBlockY, cache);
			}
			else
			{
				*pBlockX = 0;
				*pBlockY = 0;
			}
			break;
		}

		default:
			check(false);
			break;
		}
	}


	void ASTOpSwitch::GetLayoutBlockSize(int32* pBlockX, int32* pBlockY)
	{
		switch (Type)
		{
		case EOpType::IM_SWITCH:
		{
			Ptr<ASTOp> child = GetFirstValidValue();
			if (!child)
			{
				child = Default.child();
			}

			if (child)
			{
				child->GetLayoutBlockSize(pBlockX, pBlockY);
			}
			else
			{
				checkf(false, TEXT("Image switch had no options."));
			}
			break;
		}

		default:
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	bool ASTOpSwitch::GetNonBlackRect(FImageRect& maskUsage) const
	{
		if (Type == EOpType::IM_SWITCH)
		{
			FImageRect local;
			bool localValid = false;
			if (Default)
			{
				localValid = Default->GetNonBlackRect(local);
				if (!localValid)
				{
					return false;
				}
			}

			for (const FCase& c : Cases)
			{
				if (c.Branch)
				{
					FImageRect branchRect;
					bool validBranch = c.Branch->GetNonBlackRect(branchRect);
					if (validBranch)
					{
						if (localValid)
						{
							local.Bound(branchRect);
						}
						else
						{
							local = branchRect;
							localValid = true;
						}
					}
					else
					{
						return false;
					}
				}
			}

			if (localValid)
			{
				maskUsage = local;
				return true;
			}
		}

		return false;
	}


	bool ASTOpSwitch::IsImagePlainConstant(FVector4f&) const
	{
		// We could check if every option is plain and exactly the same colour, but probably it is
		// not worth.
		return false;
	}


	mu::Ptr<ASTOp> ASTOpSwitch::OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const
	{
		// Constant Condition?
		if (Variable->GetOpType() == EOpType::NU_CONSTANT)
		{
			Ptr<ASTOp> Branch = Default.child();

			const ASTOpConstantInt* typedCondition = static_cast<const ASTOpConstantInt*>(Variable.child().get());
			for (int32 o = 0; o < Cases.Num(); ++o)
			{
				if (Cases[o].Branch &&
					typedCondition->Value == (int32)Cases[o].Condition)
				{
					Branch = Cases[o].Branch.child();
					break;
				}
			}

			return Branch;
		}

		else if (Variable->GetOpType() == EOpType::NU_PARAMETER)
		{
			// If all the branches for the possible values are the same op remove the instruction
			const ASTOpParameter* ParamOp = static_cast<const ASTOpParameter*>(Variable.child().get());
			check(ParamOp);
			if(ParamOp->Parameter.PossibleValues.IsEmpty())
			{
				return nullptr;
			}

			bool bFirstValue = true;
			bool bAllSame = true;
			Ptr<ASTOp> SameBranch = nullptr;
			for (const FParameterDesc::FIntValueDesc& Value : ParamOp->Parameter.PossibleValues)
			{
				// Look for the switch Branch it would take
				Ptr<ASTOp> Branch = Default.child();
				for (const FCase& Case : Cases)
				{
					if (Case.Condition == Value.Value)
					{
						Branch = Case.Branch.child();
						break;
					}
				}

				if (bFirstValue)
				{
					bFirstValue = false;
					SameBranch = Branch;
				}
				else
				{
					if (SameBranch != Branch)
					{
						bAllSame = false;
						SameBranch = nullptr;
						break;
					}
				}
			}

	        if (bAllSame)
	        {
				return SameBranch;
	        }
		}

		// Ad-hoc logic optimization: check if all code paths leading to this operation have a switch with the same Variable
		// and the option on those switches for the path that connects to this one is always the same. In that case, we can 
		// remove this switch and replace it by the value it has for that option. 
		// This is something the generic logic optimizer should do whan re-enabled.
		{
			// List of parent operations that we have visited, and the child we have visited them from.
			TSet<TTuple<const ASTOp*, const ASTOp*>> Visited;
			Visited.Reserve(64);

			// First is parent, second is what child we are reaching the parent from. This is necessary to find out what 
			// switch Branch we reach the parent from, if it is a switch.
			TArray< TTuple<const ASTOp*, const ASTOp*>, TInlineAllocator<16>> Pending;
			ForEachParent([this,&Pending](ASTOp* Parent)
				{
					Pending.Add({ Parent,this});
				});

			bool bAllPathsHaveMatchingSwitch = true;

			// Switch option value of all parent compatible switches (if any)
			int32 MatchingSwitchOption = -1;

			while (!Pending.IsEmpty() && bAllPathsHaveMatchingSwitch)
			{
				TTuple<const ASTOp*, const ASTOp*> ParentPair = Pending.Pop();
				bool bAlreadyVisited = false;
				Visited.Add(ParentPair, &bAlreadyVisited);

				if (!bAlreadyVisited)
				{
					const ASTOp* Parent = ParentPair.Get<0>();
					const ASTOp* ParentChild = ParentPair.Get<1>();

					bool bIsMatchingSwitch = false;

					// TODO: Probably it could be a any switch, it doesn't need to be of the same Type.
					if (Parent->GetOpType() == GetOpType())
					{
						const ASTOpSwitch* ParentSwitch = static_cast<const ASTOpSwitch*>(Parent);
						check(ParentSwitch);

						// To be compatible the switch must be on the same Variable
						if (ParentSwitch->Variable==Variable)
						{
							bIsMatchingSwitch = true;
							
							// Find what switch option we are reaching it from
							bool bIsSingleOption = true;
							int OptionIndex = -1;
							for (int32 CaseIndex = 0; CaseIndex < ParentSwitch->Cases.Num(); ++CaseIndex)
							{
								if (ParentSwitch->Cases[CaseIndex].Branch.child().get() == ParentChild)
								{
									if (OptionIndex != -1)
									{
										// This means the same child is connected to more than one switch options
										// so we cannot optimize.
										// \TODO: We could if we track a "set of options" for all switches instead of just one.
										bIsSingleOption = false;
										break;
									}
									else
									{
										OptionIndex = CaseIndex;
									}
								}
							}

							// If we did reach it from one single option
							if (bIsSingleOption && OptionIndex!=-1)
							{
								if (MatchingSwitchOption<0)
								{
									MatchingSwitchOption = ParentSwitch->Cases[OptionIndex].Condition;
								}
								else if (MatchingSwitchOption!= ParentSwitch->Cases[OptionIndex].Condition)
								{
									bAllPathsHaveMatchingSwitch = false;
								}
							}
						}
					}
					
					if (!bIsMatchingSwitch)
					{
						// If it has no parents, then the optimization cannot be applied
						bool bHasParent = false;
						Parent->ForEachParent([&bHasParent,this,&Pending,Parent](ASTOp* ParentParent)
							{
								Pending.Add({ ParentParent,Parent });
								bHasParent = true;
							});

						if (!bHasParent)
						{
							// We reached a root without a matching switch along the path.
							bAllPathsHaveMatchingSwitch = false;
						}
					}
				}
			}

			if (bAllPathsHaveMatchingSwitch && MatchingSwitchOption>=0)
			{
				// We can remove this switch, all paths leading to it have the same Condition for this switches Variable.
				return FindBranch(MatchingSwitchOption);
			}

		}

		return nullptr;
	}


	Ptr<ASTOp> ASTOpSwitch::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		// Detect if all Cases are the same op Type or they are null (same op with some branches being null).
		EOpType BranchOpType = EOpType::NONE;
		bool bSameOpTypeOrNull = true;

		if (Default)
		{
			BranchOpType = Default->GetOpType();
		}

		for (const FCase& Case : Cases)
		{
			if (!Case.Branch)
			{
				continue;
			}

			if (BranchOpType==EOpType::NONE)
			{
				BranchOpType = Case.Branch->GetOpType();
			}
			else if (Case.Branch->GetOpType() != BranchOpType)
			{
				bSameOpTypeOrNull = false;
				break;
			}
		}

		if (bSameOpTypeOrNull)
		{
			switch (BranchOpType)
			{
			case EOpType::ME_ADDTAGS:
			{
				// Move the add tags out of the switch if all tags are the same
				bool bAllTagsAreTheSame = true;
				TArray<FString> Tags;

				if (Default)
				{
					check(Default->GetOpType()==EOpType::ME_ADDTAGS);
					const ASTOpMeshAddTags* Typed = static_cast<const ASTOpMeshAddTags*>(Default.child().get());
					Tags = Typed->Tags;
				}

				for (const FCase& Case : Cases)
				{
					if (!Case.Branch)
					{
						continue;
					}

					check(Case.Branch->GetOpType() == EOpType::ME_ADDTAGS);
					const ASTOpMeshAddTags* Typed = static_cast<const ASTOpMeshAddTags*>(Case.Branch.child().get());
					if (Tags.IsEmpty())
					{
						Tags = Typed->Tags;
					}
					else if (Typed->Tags!=Tags)
					{
						bAllTagsAreTheSame = false;
						break;
					}
				}

				if (bAllTagsAreTheSame)
				{
					Ptr<ASTOpMeshAddTags> New = new ASTOpMeshAddTags();
					New->Tags = Tags;
					
					{
						Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(this);

						// Replace all branches removing the "add tags" operation.
						if (Default)
						{
							check(Default->GetOpType() == EOpType::ME_ADDTAGS);
							const ASTOpMeshAddTags* Typed = static_cast<const ASTOpMeshAddTags*>(Default.child().get());
							NewSwitch->Default = Typed->Source.child();
						}

						for (int32 CaseIndex=0; CaseIndex<Cases.Num();++CaseIndex)
						{
							const FCase& SourceCase = Cases[CaseIndex];
							if (!SourceCase.Branch)
							{
								continue;
							}

							FCase& NewCase = NewSwitch->Cases[CaseIndex];

							check(SourceCase.Branch->GetOpType() == EOpType::ME_ADDTAGS);
							const ASTOpMeshAddTags* Typed = static_cast<const ASTOpMeshAddTags*>(SourceCase.Branch.child().get());
							NewCase.Branch = Typed->Source.child();
						}

						New->Source = NewSwitch;
					}

					NewOp = New;
				}
				break;
			}

			default:
				break;
			}
		}

		return NewOp;
	}


	mu::Ptr<ImageSizeExpression> ASTOpSwitch::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;

		bool first = true;
		for (const FCase& c : Cases)
		{
			if (c.Branch)
			{
				if (first)
				{
					pRes = c.Branch->GetImageSizeExpression();
				}
				else
				{
					Ptr<ImageSizeExpression> pOther = c.Branch->GetImageSizeExpression();
					if (!(*pOther == *pRes))
					{
						pRes->type = ImageSizeExpression::ISET_UNKNOWN;
						break;
					}
				}
			}
		}

		return pRes;
	}


	FSourceDataDescriptor ASTOpSwitch::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		// Cache management
		TUniquePtr<FGetSourceDataDescriptorContext> LocalContext;
		if (!Context)
		{
			LocalContext.Reset(new FGetSourceDataDescriptorContext);
			Context = LocalContext.Get();
		}

		FSourceDataDescriptor* Found = Context->Cache.Find(this);
		if (Found)
		{
			return *Found;
		}

		// Not cached: calculate
		FSourceDataDescriptor Result;

		for (const FCase& Case : Cases)
		{
			if (Case.Branch)
			{
				FSourceDataDescriptor SourceDesc = Case.Branch->GetSourceDataDescriptor(Context);
				Result.CombineWith(SourceDesc);
			}
		}

		Context->Cache.Add(this, Result);

		return Result;
	}


}
