// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshFormat.h"

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
#include "MuT/ASTOpMeshTransformWithBoundingMesh.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpMeshApplyLayout.h"
#include "MuT/ASTOpMeshMerge.h"
#include "MuT/ASTOpMeshSetSkeleton.h"
#include "MuT/ASTOpSwitch.h"

#include "GPUSkinPublicDefs.h"

namespace mu
{


//-------------------------------------------------------------------------------------------------
ASTOpMeshFormat::ASTOpMeshFormat()
    : Source(this)
    , Format(this)
{
}


ASTOpMeshFormat::~ASTOpMeshFormat()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


bool ASTOpMeshFormat::IsEqual(const ASTOp& otherUntyped) const
{
	if (otherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpMeshFormat* other = static_cast<const ASTOpMeshFormat*>(&otherUntyped);
        return Source==other->Source && Format==other->Format && Flags ==other->Flags;
    }
    return false;
}


uint64 ASTOpMeshFormat::Hash() const
{
	uint64 res = std::hash<void*>()(Source.child().get() );
    hash_combine( res, Format.child().get() );
    return res;
}


mu::Ptr<ASTOp> ASTOpMeshFormat::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpMeshFormat> n = new ASTOpMeshFormat();
    n->Source = mapChild(Source.child());
    n->Format = mapChild(Format.child());
	n->Flags = Flags;
	n->bOptimizeBuffers = bOptimizeBuffers;
    return n;
}


void ASTOpMeshFormat::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
    f( Source );
    f( Format );
}


void ASTOpMeshFormat::Link( FProgram& program, FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::MeshFormatArgs Args;
		FMemory::Memzero(Args);

		Args.Flags = Flags;
		if (bOptimizeBuffers)
		{
			Args.Flags = Args.Flags | OP::MeshFormatArgs::OptimizeBuffers;
		}

		if (Source) Args.source = Source->linkedAddress;
		if (Format) Args.format = Format->linkedAddress;

        linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
        program.OpAddress.Add((uint32)program.ByteCode.Num());
        AppendCode(program.ByteCode,EOpType::ME_FORMAT);
        AppendCode(program.ByteCode,Args);
    }

}


mu::Ptr<ASTOp> ASTOpMeshFormat::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
{
	mu::Ptr<ASTOp> at = context.MeshFormatSinker.Apply(this);
	return at;
}


FSourceDataDescriptor ASTOpMeshFormat::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
{
	if (Source)
	{
		return Source->GetSourceDataDescriptor(Context);
	}

	return {};
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> Sink_MeshFormatAST::Apply(const ASTOpMeshFormat* root)
{
	Root = root;

	OldToNew.Reset();

	InitialSource = Root->Source.child();
	mu::Ptr<ASTOp> newSource = Visit(InitialSource, Root);

	// If there is any change, it is the new root.
	if (newSource != InitialSource)
	{
		return newSource;
	}

	return nullptr;
}


namespace
{
	TSharedPtr<const FMesh> FindBaseMeshConstant(mu::Ptr<ASTOp> at)
	{
		TSharedPtr<const FMesh> res;

		switch (at->GetOpType())
		{
		case EOpType::ME_CONSTANT:
		{
			const ASTOpConstantResource* typed = static_cast<const ASTOpConstantResource*>(at.get());
			res = StaticCastSharedPtr<const FMesh>(typed->GetValue());
			break;
		}

		default:
			check(false);
		}

		check(res);

		return res;
	}


	// Make a mesh format suitable to morph a particular other format.
	TSharedPtr<FMesh> MakeMorphTargetFormat(TSharedPtr<const FMesh> pTargetFormat)
	{
		MUTABLE_CPUPROFILER_SCOPE(MakeMorphTargetFormat);

		// Make a morph format by adding all the vertex channels from the base into a single
		// vertex buffer

		int32 offset = 0;
		int32 numChannels = 0;
		TArray<EMeshBufferSemantic> semantics;
		TArray<int32> semanticIndices;
		TArray<EMeshBufferFormat> formats;
		TArray<int32> components;
		TArray<int32> offsets;

		// Add the vertex channels from the new format
		for (int32 vb = 0; vb < pTargetFormat->GetVertexBuffers().GetBufferCount(); ++vb)
		{
			for (int32 c = 0; c < pTargetFormat->GetVertexBuffers().GetBufferChannelCount(vb); ++c)
			{
				// Channel info
				EMeshBufferSemantic semantic;
				int semanticIndex;
				EMeshBufferFormat format;
				int component;
				pTargetFormat->GetVertexBuffers().GetChannel(vb, c, &semantic, &semanticIndex, &format, &component, nullptr);

				// TODO: Filter useless semantics for morphing.
				// Maybe some formats like the ones with a packed tangent sign need to be tweaked here, to make sense of the whole buffer.
				semantics.Add(semantic);
				semanticIndices.Add(semanticIndex);
				formats.Add(format);
				components.Add(component);
				offsets.Add(offset);
				offset += components[numChannels] * GetMeshFormatData(formats[numChannels]).SizeInBytes;
				numChannels++;
			}
		}


		TSharedPtr<FMesh> pTargetMorphFormat = MakeShared<FMesh>();
		pTargetMorphFormat->GetVertexBuffers().SetBufferCount(1);

		pTargetMorphFormat->GetVertexBuffers().SetBuffer(0, offset,
			numChannels,
			semantics.GetData(),
			semanticIndices.GetData(),
			formats.GetData(),
			components.GetData(),
			offsets.GetData());

		return pTargetMorphFormat;
	}

    TSharedPtr<const FMesh> EnsureFormatHasSkinningBuffers(TSharedPtr<const FMesh>& FormatMesh)
    {
        const FMeshBufferSet& FormatMeshVertexBuffers = FormatMesh->GetVertexBuffers();
    
        int32 SourceSkinningBufferIndex = -1;         
        int32 SourceSkinningChannelIndex = -1;

        // Assume bone indices implies it also has weights.
        FormatMeshVertexBuffers.FindChannel(EMeshBufferSemantic::BoneIndices, 0, &SourceSkinningBufferIndex, &SourceSkinningChannelIndex);

        bool bSourceHasSkinningData = SourceSkinningBufferIndex != -1;
			
        if (bSourceHasSkinningData)
        {
            return FormatMesh;
        }

		TSharedPtr<FMesh> NewMesh = FormatMesh->Clone();
        FMeshBufferSet& MeshBuffers = NewMesh->GetVertexBuffers();
        
        FMeshBuffer& Buffer = MeshBuffers.Buffers.AddDefaulted_GetRef();

        FMeshBufferChannel BoneIndices;
        BoneIndices.Semantic = EMeshBufferSemantic::BoneIndices;
        BoneIndices.Format = EMeshBufferFormat::UInt16;
        BoneIndices.SemanticIndex = 0;
        BoneIndices.Offset = 0;
        BoneIndices.ComponentCount = MAX_TOTAL_INFLUENCES;

        FMeshBufferChannel BoneWeights;
        BoneWeights.Semantic = EMeshBufferSemantic::BoneWeights;
        BoneWeights.Format = EMeshBufferFormat::NUInt16;
        BoneWeights.SemanticIndex = 0;
        BoneWeights.Offset = MAX_TOTAL_INFLUENCES*2;
        BoneWeights.ComponentCount = MAX_TOTAL_INFLUENCES;


        Buffer.ElementSize = MAX_TOTAL_INFLUENCES*4;
        Buffer.Channels.Add(BoneIndices);
        Buffer.Channels.Add(BoneWeights);

        return NewMesh;
    }

}


mu::Ptr<ASTOp> Sink_MeshFormatAST::Visit(const mu::Ptr<ASTOp>& at, const ASTOpMeshFormat* currentFormatOp)
{
	if (!at)
	{
		return nullptr;
	}

	// Already visited?
	const Ptr<ASTOp>* Cached = OldToNew.Find({ at,currentFormatOp });
	if (Cached)
	{
		return *Cached;
	}

	mu::Ptr<ASTOp> newAt = at;
	switch (at->GetOpType())
	{

	case EOpType::ME_APPLYLAYOUT:
	{
		Ptr<ASTOpMeshApplyLayout> NewOp = mu::Clone<ASTOpMeshApplyLayout>(at);
		NewOp->Mesh = Visit(NewOp->Mesh.child(), currentFormatOp);
		newAt = NewOp;
		break;
	}

	case EOpType::ME_SETSKELETON:
	{
		Ptr<ASTOpMeshSetSkeleton> NewOp = mu::Clone<ASTOpMeshSetSkeleton>(at);
		NewOp->Source = Visit(NewOp->Source.child(), currentFormatOp);
		newAt = NewOp;
		break;
	}

	case EOpType::ME_ADDTAGS:
	{
		Ptr<ASTOpMeshAddTags> newOp = mu::Clone<ASTOpMeshAddTags>(at);
		newOp->Source = Visit(newOp->Source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case EOpType::ME_CLIPMORPHPLANE:
	{
		Ptr<ASTOpMeshClipMorphPlane> newOp = mu::Clone<ASTOpMeshClipMorphPlane>(at);
		newOp->Source = Visit(newOp->Source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case EOpType::ME_TRANSFORMWITHMESH:
	{
		Ptr<ASTOpMeshTransformWithBoundingMesh> NewOp = mu::Clone<ASTOpMeshTransformWithBoundingMesh>(at);
		NewOp->source = Visit(NewOp->source.child(), currentFormatOp);

		// Don't transform the bounding mesh: it should be optimized with a different specific format elsewhere (TODO).
		// NewOp->boundingMesh = Visit(NewOp->boundingMesh.child(), currentFormatOp);

		newAt = NewOp;
		break;
	}

	case EOpType::ME_MORPH:
	{
		// Move the format down the base of the morph
		Ptr<ASTOpMeshMorph> NewOp = mu::Clone<ASTOpMeshMorph>(at);
		NewOp->Base = Visit(NewOp->Base.child(), currentFormatOp);

		// Reformat the morph targets to match the new format.
		TSharedPtr<const FMesh> pTargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
		TSharedPtr<const FMesh> pTargetMorphFormat = MakeMorphTargetFormat(pTargetFormat);

		mu::Ptr<ASTOpConstantResource> NewFormatConstant = new ASTOpConstantResource();
		NewFormatConstant->Type = EOpType::ME_CONSTANT;
		NewFormatConstant->SetValue(pTargetMorphFormat, nullptr);
		NewFormatConstant->SourceDataDescriptor = at->GetSourceDataDescriptor();

		if (NewOp->Target)
		{
			mu::Ptr<ASTOpMeshFormat> newFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
			newFormat->Flags = OP::MeshFormatArgs::Vertex | OP::MeshFormatArgs::IgnoreMissing;
			newFormat->Format = NewFormatConstant;

			NewOp->Target = Visit(NewOp->Target.child(), newFormat.get());
		}

		newAt = NewOp;
		break;
	}

	case EOpType::ME_MERGE:
	{
		Ptr<ASTOpMeshMerge> NewOp = mu::Clone<ASTOpMeshMerge>(at);
		NewOp->Base = Visit(NewOp->Base.child(), currentFormatOp);
		NewOp->Added = Visit(NewOp->Added.child(), currentFormatOp);
		newAt = NewOp;
		break;
	}

	case EOpType::ME_APPLYPOSE:
	{
		TSharedPtr<const FMesh> TargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
        TargetFormat = EnsureFormatHasSkinningBuffers(TargetFormat);

		Ptr<ASTOpMeshApplyPose> NewOp = mu::Clone<ASTOpMeshApplyPose>(at);
        mu::Ptr<ASTOpMeshFormat> NewFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
        
		mu::Ptr<ASTOpConstantResource> NewFormatConstant = new ASTOpConstantResource();
		NewFormatConstant->Type = EOpType::ME_CONSTANT;
		NewFormatConstant->SetValue(TargetFormat, nullptr);
		NewFormatConstant->SourceDataDescriptor = at->GetSourceDataDescriptor();
	
        NewFormat->Flags = NewFormat->Flags | OP::MeshFormatArgs::OptimizeBuffers;

        // TODO: Optimize, in case no skinning data is found in the format mesh a generic buffer that can represent
        // all possible skinning formats is added. This is not optimal, we may want to add a flag to the format op
        // to indicate it should copy the skinning from the base mesh.
        NewFormat->Format = NewFormatConstant;

        NewOp->Base = Visit(NewOp->Base.child(), NewFormat.get());
		
		newAt = NewOp;
		break;
	}

	case EOpType::ME_REMOVEMASK:
	{
		Ptr<ASTOpMeshRemoveMask> newOp = mu::Clone<ASTOpMeshRemoveMask>(at);
		newOp->source = Visit(newOp->source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case EOpType::ME_CONDITIONAL:
	{
		Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
		newOp->yes = Visit(newOp->yes.child(), currentFormatOp);
		newOp->no = Visit(newOp->no.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case EOpType::ME_SWITCH:
	{
		Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
		newOp->Default = Visit(newOp->Default.child(), currentFormatOp);
		for (ASTOpSwitch::FCase& c : newOp->Cases)
		{
			c.Branch = Visit(c.Branch.child(), currentFormatOp);
		}
		newAt = newOp;
		break;
	}

	case EOpType::ME_FORMAT:
		// TODO: The child format can be removed. 
		// Unless channels are removed and re-added, which would change their content?
		break;


	// This operation should not be optimized.
	case EOpType::ME_DIFFERENCE:

	// If we reach here it means the operation type has not bee optimized.
	default:
		if (at != InitialSource)
		{
			mu::Ptr<ASTOpMeshFormat> newOp = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
			newOp->Source = at;
			newAt = newOp;
		}
		break;

	}

	OldToNew.Add({ at,currentFormatOp }, newAt);

	return newAt;
}

}
