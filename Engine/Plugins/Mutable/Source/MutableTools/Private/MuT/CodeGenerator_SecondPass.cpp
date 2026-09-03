// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator_SecondPass.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpBoolAnd.h"
#include "MuT/ASTOpBoolNot.h"
#include "MuT/ErrorLog.h"
#include "MuT/CodeGenerator_FirstPass.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	SecondPassGenerator::SecondPassGenerator(
		FirstPassGenerator* firstPass,
		const CompilerOptions::Private* options)
	{
		check(firstPass);
		check(options);
		FirstPass = firstPass;
		CompilerOptions = options;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> SecondPassGenerator::GenerateTagCondition(int32 tagIndex,
		const TSet<int32>& posSurf,
		const TSet<int32>& negSurf,
		const TSet<int32>& posTag,
		const TSet<int32>& negTag)
	{
		FirstPassGenerator::FTag& t = FirstPass->Tags[tagIndex];

		// If this tag is already in the list of positive tags, return true as condition
		if (posTag.Contains(tagIndex))
		{
			return OpPool.Add(new ASTOpConstantBool(true));
		}

		// If this tag is already in the list of negative tags, return false as condition
		if (negTag.Contains(tagIndex))
		{
			return OpPool.Add(new ASTOpConstantBool(false));
		}

		// Cached?
		FConditionGenerationKey key;
		key.tagOrSurfIndex = tagIndex;
		//    key.negSurf = negSurf;
		//    key.posSurf = posSurf;
		//    key.negTag = negTag;
		//    key.posTag = posTag;
		for (auto s : negTag) { if (TagsPerTag[tagIndex].Contains(s)) { key.negTag.Add(s); } }
		for (auto s : posTag) { if (TagsPerTag[tagIndex].Contains(s)) { key.posTag.Add(s); } }
		for (auto s : negSurf) { if (SurfacesPerTag[tagIndex].Contains(s)) { key.negSurf.Add(s); } }
		for (auto s : posSurf) { if (SurfacesPerTag[tagIndex].Contains(s)) { key.posSurf.Add(s); } }

		{
			Ptr<ASTOp>* Found = TagConditionGenerationCache.Find(key);
			if (Found)
			{
				return *Found;
			}
		}
		
		Ptr<ASTOp> c;

		// Condition expression for all the surfaces that activate the tag
		for (int32 surfIndex : t.Surfaces)
		{
			if (posSurf.Contains(surfIndex))
			{
				// This surface is already a positive requirement higher up in the condition so
				// we can ignore it here.
				continue;
			}

			if (negSurf.Contains(surfIndex))
			{
				// This surface is a negative requirement higher up in the condition so
				// this branch never be true.
				continue;
			}

			const FirstPassGenerator::FSurface& surface = FirstPass->Surfaces[surfIndex];

			auto PositiveTags = posTag;
			PositiveTags.Add(tagIndex);

			Ptr<ASTOp> surfCondition = GenerateDataCodition(surfIndex,
				FirstPass->Surfaces[surfIndex].PositiveTags,
				FirstPass->Surfaces[surfIndex].NegativeTags,
				posSurf,
				negSurf,
				PositiveTags,
				negTag);

			// If the surface is a constant false, we can skip adding it
			if (surfCondition && surfCondition->GetOpType()==EOpType::BO_CONSTANT)
			{
				const ASTOpConstantBool* constOp = static_cast<const ASTOpConstantBool*>(surfCondition.get());
				if (constOp->bValue == false)
				{
					continue;
				}
			}

			Ptr<ASTOp> fullCondition;
			if (surfCondition)
			{
				Ptr<ASTOpBoolAnd> f = new ASTOpBoolAnd;
				f->A = surface.ObjectCondition;
				f->B = surfCondition;
				fullCondition = OpPool.Add(f);
			}
			else
			{
				fullCondition = OpPool.Add(surface.ObjectCondition);
			}


			if (!c)
			{
				c = fullCondition;
			}
			else
			{
				Ptr<ASTOpBoolAnd> o = new ASTOpBoolAnd();
				o->A = fullCondition;
				o->B = c;
				c = OpPool.Add(o);
			}

			// Optimise the condition now.
			//PartialOptimise( c, CompilerOptions->OptimisationOptions );
		}

		TagConditionGenerationCache.Add(key, c);

		return c;
	}

	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> SecondPassGenerator::GenerateDataCodition(int32 Index,
		const TArray<FString>& PositiveTags,
		const TArray<FString>& NegativeTags,
		const TSet<int32>& posSurf,
		const TSet<int32>& negSurf,
		const TSet<int32>& posTag,
		const TSet<int32>& negTag)
	{
		// If this surface is already in the list of positive surfaces, return true as condition
		if (posSurf.Contains(Index))
		{
			return OpPool.Add(new ASTOpConstantBool(true));
		}

		// If this surface is already in the list of negative surfaces, return false as condition
		if (negSurf.Contains(Index))
		{
			return OpPool.Add(new ASTOpConstantBool(false));
		}

		Ptr<ASTOp> c;

		for (const FString& t : PositiveTags)
		{
			const FirstPassGenerator::FTag* it = FirstPass->Tags.FindByPredicate([&](const FirstPassGenerator::FTag& e) { return e.Tag == t; });
			if (!it)
			{
				// This could happen if a tag is in a variation but noone defines it.
				// This surface depends on a tag that will never be active, so it will never be used.
				return OpPool.Add(new ASTOpConstantBool(false));
			}

			int32 tagIndex = int32(it - &FirstPass->Tags[0]);

			TSet<int32> positiveSurfacesVisited = posSurf;
			positiveSurfacesVisited.Add(Index);

			Ptr<ASTOp> tagCondition = GenerateTagCondition(tagIndex,
				positiveSurfacesVisited,
				negSurf,
				posTag,
				negTag);

			if (!tagCondition)
			{
				// This tag is unconditionally activated, so there's no condition logic to add
				continue;
			}

			// TODO: Optimise the tag condition here

			// If the tag is a constant ...
			bool isConstant = false;
			bool constantValue = false;
			if (tagCondition->GetOpType()==EOpType::BO_CONSTANT)
			{
				const ASTOpConstantBool* constOp = static_cast<const ASTOpConstantBool*>(tagCondition.get());
				isConstant = true;
				constantValue = constOp->bValue;
			}

			if (!isConstant)
			{
				if (!c)
				{
					c = tagCondition;
				}
				else
				{
					Ptr<ASTOpBoolAnd> o = new ASTOpBoolAnd;
					o->A = tagCondition;
					o->B = c;
					c = OpPool.Add(o);
				}
			}
			else if (constantValue == true)
			{
				// No need to add it to the AND
			}
			else //if (constantValue==false)
			{
				// Entire expression will be false
				Ptr<ASTOpConstantBool> f = new ASTOpConstantBool(false);

				// No need to evaluate anything else.
				c = OpPool.Add(f);
				break;
			}
		}


		for (const FString& t : NegativeTags)
		{
			const FirstPassGenerator::FTag* it = FirstPass->Tags.FindByPredicate([&](const FirstPassGenerator::FTag& e) { return e.Tag == t; });
			if (!it)
			{
				// This could happen if a tag is in a variation but noone defines it.
				continue;
			}

			int32 tagIndex = int32(it - FirstPass->Tags.GetData());

			TSet<int32> positiveSurfacesVisited = negSurf;
			TSet<int32> negativeSurfacesVisited = posSurf;
			negativeSurfacesVisited.Add(Index);
			TSet<int32> positiveTagsVisited = negTag;
			TSet<int32> negativeTagsVisited = posTag;
			Ptr<ASTOp> tagCondition = GenerateTagCondition(tagIndex,
				positiveSurfacesVisited,
				negativeSurfacesVisited,
				positiveTagsVisited,
				negativeTagsVisited);

			// No condition is equal to a conditional with a true constant
			if (!tagCondition)
			{
				tagCondition = new ASTOpConstantBool(true);
			}

			// TODO: Optimise the tag condition here

			// If the tag is a constant ...
			bool isConstant = false;
			bool constantValue = false;
			if (tagCondition && tagCondition->GetOpType()== EOpType::BO_CONSTANT)
			{
				const ASTOpConstantBool* constOp = static_cast<const ASTOpConstantBool*>(tagCondition.get());
				isConstant = true;
				constantValue = constOp->bValue;
			}


			if (!isConstant && tagCondition)
			{
				Ptr<ASTOpBoolNot> n = new ASTOpBoolNot;
				n->A = tagCondition;

				if (!c)
				{
					c = OpPool.Add(n);
				}
				else
				{
					Ptr<ASTOpBoolAnd> o = new ASTOpBoolAnd;
					o->A = n;
					o->B = c;
					c = OpPool.Add(o);
				}
			}
			else if (isConstant && constantValue == true)
			{
				// No expression here means always true which becomes always false
				Ptr<ASTOpConstantBool> f = new ASTOpConstantBool(false);

				// No need to evaluate anything else.
				c = OpPool.Add(f);
				break;
			}
		}

		return c;
	}


	//---------------------------------------------------------------------------------------------
	bool SecondPassGenerator::Generate(TSharedPtr<FErrorLog> InErrorLog, const Node* root)
	{
		MUTABLE_CPUPROFILER_SCOPE(SecondPassGenerate);

		check(root);
		ErrorLog = InErrorLog;

		// Find the list of surfaces every tag depends on
		SurfacesPerTag.Empty();
		SurfacesPerTag.SetNum(FirstPass->Tags.Num());
		TagsPerTag.Empty();
		TagsPerTag.SetNum(FirstPass->Tags.Num());
		for (int32 t = 0; t < FirstPass->Tags.Num(); ++t)
		{
			TSet<int32> PendingSurfaces;
			for (int32 s : FirstPass->Tags[t].Surfaces)
			{
				PendingSurfaces.Add(s);
			}

			TSet<int32> processedSurfs;
			while (!PendingSurfaces.IsEmpty())
			{
				int32 cs = *PendingSurfaces.begin();
				PendingSurfaces.Remove(cs);

				if (processedSurfs.Contains(cs))
				{
					continue;
				}

				processedSurfs.Add(cs);

				SurfacesPerTag[t].Add(cs);

				FirstPassGenerator::FSurface& csurf = FirstPass->Surfaces[cs];
				for (const FString& sct : csurf.PositiveTags)
				{
					const FirstPassGenerator::FTag* it = FirstPass->Tags.FindByPredicate([&](const FirstPassGenerator::FTag& e) { return e.Tag == sct; });
					if (!it)
					{
						// This could happen if a tag is in a variation but noone defines it.
						continue;
					}

					int32 ct = int32(it - &FirstPass->Tags[0]);

					TagsPerTag[t].Add(ct);

					for (int32 s : FirstPass->Tags[ct].Surfaces)
					{
						if (SurfacesPerTag[t].Contains(s))
						{
							PendingSurfaces.Add(s);
						}
					}
				}
				for (const FString& sct : csurf.NegativeTags)
				{
					const FirstPassGenerator::FTag* it = FirstPass->Tags.FindByPredicate([&](const FirstPassGenerator::FTag& e) { return e.Tag == sct; });
					if (!it)
					{
						// This could happen if a tag is in a variation but noone defines it.
						continue;
					}

					size_t ct = it - &FirstPass->Tags[0];

					TagsPerTag[t].Add(ct);

					for (size_t s : FirstPass->Tags[ct].Surfaces)
					{
						if (SurfacesPerTag[t].Contains(s))
						{
							PendingSurfaces.Add(s);
						}
					}
				}
			}
		}

		// Create the conditions for every surface, modifier, component and individual tag.
		TagConditionGenerationCache.Reset();

		TSet<int32> Empty;

		for (int32 SurfaceIndex = 0; SurfaceIndex < FirstPass->Surfaces.Num(); ++SurfaceIndex)
		{
			FirstPassGenerator::FSurface& Surface = FirstPass->Surfaces[SurfaceIndex];

			{
				Ptr<ASTOp> c = GenerateDataCodition(
					SurfaceIndex, 
					Surface.PositiveTags,
					Surface.NegativeTags,
					Empty, Empty, Empty, Empty);

				Ptr<ASTOpBoolAnd> ConditionOp = new ASTOpBoolAnd();
				ConditionOp->A = Surface.ObjectCondition;
				ConditionOp->B = c;

				Surface.FinalCondition = ConditionOp;
			}
			
			// TODO: Convert to modifiers that enable tags?
			//for (int32 EditIndex = 0; EditIndex < Surface.Edits.Num(); ++EditIndex)
			//{
			//	FirstPassGenerator::FSurface::FEdit& Edit = Surface.Edits[EditIndex];
			//	
			//	Ptr<ASTOp> c = GenerateDataCodition(
			//		EditIndex, 
			//		Surface.Edits[EditIndex].PositiveTags,
			//		Surface.Edits[EditIndex].NegativeTags,
			//		Empty, Empty, Empty, Empty);

			//	Ptr<ASTOpBoolAnd> OpAnd = new ASTOpBoolAnd;
			//	OpAnd->a = Edit.Condition;
			//	OpAnd->b = c;
			//	c = OpPool.Add(OpAnd);
			//	
			//	Edit.Condition = OpAnd;
			//}
		}

		for (int32 ModifierIndex = 0; ModifierIndex < FirstPass->Modifiers.Num(); ++ModifierIndex)
		{
			Ptr<ASTOp> c = GenerateDataCodition(
				ModifierIndex, 
				FirstPass->Modifiers[ModifierIndex].PositiveTags,
				FirstPass->Modifiers[ModifierIndex].NegativeTags,
				Empty, Empty, Empty, Empty);
			
			Ptr<ASTOpBoolAnd> ConditionOp = new ASTOpBoolAnd;
			ConditionOp->A = FirstPass->Modifiers[ModifierIndex].ObjectCondition;
			ConditionOp->B = c;

			FirstPass->Modifiers[ModifierIndex].FinalCondition = ConditionOp;
		}

		for (int32 ComponentIndex = 0; ComponentIndex < FirstPass->Components.Num(); ++ComponentIndex)
		{
			Ptr<ASTOp> c = GenerateDataCodition(
				ComponentIndex, 
				FirstPass->Components[ComponentIndex].PositiveTags,
				FirstPass->Components[ComponentIndex].NegativeTags,
				Empty, Empty, Empty, Empty);

			FirstPass->Components[ComponentIndex].ComponentCondition = c;
		}

		// TODO: Do we really need the tag conditions from here on?
		for (int32 s = 0; s < FirstPass->Tags.Num(); ++s)
		{
			Ptr<ASTOp> c = GenerateTagCondition(s, Empty, Empty, Empty, Empty);
			FirstPass->Tags[s].GenericCondition = c;
		}

		FirstPass = nullptr;

		return true;
	}


}
