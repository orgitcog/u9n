// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpLayoutRemoveBlocks.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpSwitch.h"


namespace mu
{

	ASTOpLayoutRemoveBlocks::ASTOpLayoutRemoveBlocks()
		: Source(this)
		, ReferenceLayout(this)
	{
	}


	ASTOpLayoutRemoveBlocks::~ASTOpLayoutRemoveBlocks()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpLayoutRemoveBlocks::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpLayoutRemoveBlocks* other = static_cast<const ASTOpLayoutRemoveBlocks*>(&otherUntyped);
			return Source == other->Source && ReferenceLayout == other->ReferenceLayout;
		}
		return false;
	}


	uint64 ASTOpLayoutRemoveBlocks::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, ReferenceLayout.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpLayoutRemoveBlocks::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpLayoutRemoveBlocks> n = new ASTOpLayoutRemoveBlocks();
		n->Source = mapChild(Source.child());
		n->ReferenceLayout = mapChild(ReferenceLayout.child());
		return n;
	}


	void ASTOpLayoutRemoveBlocks::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
		f(ReferenceLayout);
	}


	void ASTOpLayoutRemoveBlocks::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::LayoutRemoveBlocksArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.Source = Source->linkedAddress;
			if (ReferenceLayout) Args.ReferenceLayout = ReferenceLayout->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::LA_REMOVEBLOCKS);
			AppendCode(program.ByteCode, Args);
		}

	}


	void ASTOpLayoutRemoveBlocks::GetBlockLayoutSize(uint64 BlockId, int32* pBlockX, int32* pBlockY, FBlockLayoutSizeCache* cache)
	{
		if (Source)
		{
			Source->GetBlockLayoutSize(BlockId, pBlockX, pBlockY, cache);
		}
	}

}
