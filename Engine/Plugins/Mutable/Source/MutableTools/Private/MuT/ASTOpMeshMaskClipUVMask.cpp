// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMaskClipUVMask.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpMeshPrepareLayout.h"
#include "MuT/ASTOpSwitch.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshMaskClipUVMask::ASTOpMeshMaskClipUVMask()
		: Source(this)
		, UVSource(this)
		, MaskImage(this)
		, MaskLayout(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpMeshMaskClipUVMask::~ASTOpMeshMaskClipUVMask()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpMeshMaskClipUVMask::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshMaskClipUVMask* other = static_cast<const ASTOpMeshMaskClipUVMask*>(&otherUntyped);
			return Source == other->Source
				&& UVSource == other->UVSource
				&& MaskImage == other->MaskImage
				&& MaskLayout == other->MaskLayout 
				&& LayoutIndex==other->LayoutIndex;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpMeshMaskClipUVMask::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, UVSource.child());
		hash_combine(res, MaskImage.child());
		hash_combine(res, MaskLayout.child());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMaskClipUVMask::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshMaskClipUVMask> n = new ASTOpMeshMaskClipUVMask();
		n->Source = mapChild(Source.child());
		n->UVSource = mapChild(UVSource.child());
		n->MaskImage = mapChild(MaskImage.child());
		n->MaskLayout = mapChild(MaskLayout.child());
		n->LayoutIndex = LayoutIndex;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshMaskClipUVMask::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
		f(UVSource);
		f(MaskImage);
		f(MaskLayout);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpMeshMaskClipUVMask::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshMaskClipUVMaskArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.Source = Source->linkedAddress;
			if (UVSource) Args.UVSource = UVSource->linkedAddress;
			if (MaskImage) Args.MaskImage = MaskImage->linkedAddress;
			if (MaskLayout) Args.MaskLayout = MaskLayout->linkedAddress;
			Args.LayoutIndex = LayoutIndex;

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::ME_MASKCLIPUVMASK);
			AppendCode(program.ByteCode, Args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	namespace
	{
		class Sink_MeshMaskClipUVMaskSource
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.

			mu::Ptr<ASTOp> Apply(const ASTOpMeshMaskClipUVMask* root)
			{
				Root = root;
				m_oldToNew.Empty();

				InitialSource = root->Source.child();
				mu::Ptr<ASTOp> NewSource = Visit(InitialSource);

				// If there is any change, it is the new root.
				if (NewSource != InitialSource)
				{
					return NewSource;
				}

				return nullptr;
			}

		protected:

			const ASTOpMeshMaskClipUVMask* Root;
			mu::Ptr<ASTOp> InitialSource;
			TMap<mu::Ptr<ASTOp>, mu::Ptr<ASTOp>> m_oldToNew;
			TArray<mu::Ptr<ASTOp>> m_newOps;

			mu::Ptr<ASTOp> Visit(const mu::Ptr<ASTOp>& at)
			{
				if (!at) return nullptr;

				// Newly created?
				if (m_newOps.Find(at) != INDEX_NONE)
				{
					return at;
				}

				// Already visited?
				mu::Ptr<ASTOp>* cacheIt = m_oldToNew.Find(at);
				if (cacheIt)
				{
					return *cacheIt;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				// This cannot be sunk since the result is different. Since the clipping is now correctly
				// generated at the end of the chain when really necessary, this wrong optimisation is no 
				// longer needed.
				//case EOpType::ME_MORPH:
		        //{
		        //    break;
		        //}

				case EOpType::ME_REMOVEMASK:
				{
					// Remove this op:
					// This may lead to the mask being bigger than needed since it will include
					// faces removed by the ignored removemask, but it is ok

					// TODO: Swap instead of ignore, and implement removemask on a mask?
					const ASTOpMeshRemoveMask* typedAt = static_cast<const ASTOpMeshRemoveMask*>(at.get());
					newAt = Visit(typedAt->source.child());
					break;
				}

				case EOpType::ME_ADDTAGS:
				{
					Ptr<ASTOpMeshAddTags> newOp = mu::Clone<ASTOpMeshAddTags>(at);
					newOp->Source = Visit(newOp->Source.child());
					newAt = newOp;
					break;
				}

				case EOpType::ME_PREPARELAYOUT:
				{
					// Ignore the prepare op in the source: it doesn't contribute to the mask generation (the one in the SourceUV is used for UVs).
					const ASTOpMeshPrepareLayout* typedAt = static_cast<const ASTOpMeshPrepareLayout*>(at.get());
					newAt = Visit(typedAt->Mesh.child());
					break;
				}

				case EOpType::ME_CONDITIONAL:
				{
					// We move the mask creation down the two paths
					// It always needs to be a clone because otherwise we could be modifying an
					// instruction that shouldn't if the parent was a ME_REMOVEMASK above and we
					// skipped the cloning for the parent.
					Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
					newOp->yes = Visit(newOp->yes.child());
					newOp->no = Visit(newOp->no.child());
					newAt = newOp;
					break;
				}

				case EOpType::ME_SWITCH:
				{
					// We move the mask creation down all the paths
					Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
					newOp->Default = Visit(newOp->Default.child());
					for (ASTOpSwitch::FCase& c : newOp->Cases)
					{
						c.Branch = Visit(c.Branch.child());
					}
					newAt = newOp;
					break;
				}

				// This cannot be sunk since the result is different. Since the clipping is now correctly
				// generated at the end of the chain when really necessary, this wrong optimisation is no 
				// longer needed.
				//case EOpType::ME_CLIPMORPHPLANE:
	   //         {
	   //             // We move the mask creation down the Source
	   //             auto typedAt = dynamic_cast<const ASTOpMeshClipMorphPlane*>(at.get());
	   //             newAt = Visit(typedAt->Source.child());
	   //             break;
	   //         }

				default:
				{
					//
					if (at != InitialSource)
					{
						Ptr<ASTOpMeshMaskClipUVMask> newOp = mu::Clone<ASTOpMeshMaskClipUVMask>(Root);
						newOp->Source = at;
						newAt = newOp;
					}
					break;
				}

				}

				m_oldToNew.Add(at, newAt);

				return newAt;
			}
		};


		class Sink_MeshMaskClipUVMaskClip
		{
		public:

			// \TODO This is recursive and may cause stack overflows in big models.

			mu::Ptr<ASTOp> Apply(const ASTOpMeshMaskClipUVMask* root)
			{
				Root = root;
				m_oldToNew.Empty();

				m_initialClip = root->MaskImage.child();
				mu::Ptr<ASTOp> NewClip = Visit(m_initialClip);

				// If there is any change, it is the new root.
				if (NewClip != m_initialClip)
				{
					return NewClip;
				}

				return nullptr;
			}

		protected:

			const ASTOpMeshMaskClipUVMask* Root;
			mu::Ptr<ASTOp> m_initialClip;
			TMap<mu::Ptr<ASTOp>, mu::Ptr<ASTOp>> m_oldToNew;
			TArray<mu::Ptr<ASTOp>> m_newOps;

			mu::Ptr<ASTOp> Visit(const mu::Ptr<ASTOp>& at)
			{
				if (!at) return nullptr;

				// Newly created?
				if (m_newOps.Contains(at))
				{
					return at;
				}

				// Already visited?
				mu::Ptr<ASTOp>* cacheIt = m_oldToNew.Find(at);
				if (cacheIt)
				{
					return *cacheIt;
				}

				mu::Ptr<ASTOp> newAt = at;
				switch (at->GetOpType())
				{

				case EOpType::ME_CONDITIONAL:
				{
					// We move the mask creation down the two paths
					// It always needs to be a clone because otherwise we could be modifying an
					// instruction that shouldn't if the parent was a ME_REMOVEMASK above and we
					// skipped the cloning for the parent.
					Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
					newOp->yes = Visit(newOp->yes.child());
					newOp->no = Visit(newOp->no.child());
					newAt = newOp;
					break;
				}

				case EOpType::ME_SWITCH:
				{
					// We move the mask creation down all the paths
					Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
					newOp->Default = Visit(newOp->Default.child());
					for (ASTOpSwitch::FCase& c : newOp->Cases)
					{
						c.Branch = Visit(c.Branch.child());
					}
					newAt = newOp;
					break;
				}


				default:
				{
					//
					if (at != m_initialClip)
					{
						Ptr<ASTOpMeshMaskClipUVMask> newOp = mu::Clone<ASTOpMeshMaskClipUVMask>(Root);
						newOp->MaskImage = at;
						newAt = newOp;
					}
					break;
				}

				}

				m_oldToNew.Add(at, newAt);

				return newAt;
			}
		};

	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpMeshMaskClipUVMask::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		// \TODO: Add logic state to the sinkers to avoid explosion with switches in both branches and similar cases.

		// This will sink the operation down the Source
		Sink_MeshMaskClipUVMaskSource SinkerSource;
		mu::Ptr<ASTOp> at = SinkerSource.Apply(this);

		// If we didn't sink it.
		if (!at || at == this)
		{
			// This will sink the operation down the Mask child
			Sink_MeshMaskClipUVMaskClip SinkerClip;
			at = SinkerClip.Apply(this);
		}

		return at;
	}


	//-------------------------------------------------------------------------------------------------
	FSourceDataDescriptor ASTOpMeshMaskClipUVMask::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
 