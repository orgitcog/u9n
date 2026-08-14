// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshApplyLayout.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpMeshApplyLayout::ASTOpMeshApplyLayout()
		: Mesh(this)
		, Layout(this)
	{
	}


	ASTOpMeshApplyLayout::~ASTOpMeshApplyLayout()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshApplyLayout::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshApplyLayout* Other = static_cast<const ASTOpMeshApplyLayout*>(&OtherUntyped);
			return Mesh == Other->Mesh &&
				Layout == Other->Layout &&
				Channel == Other->Channel;
		}
		return false;
	}


	uint64 ASTOpMeshApplyLayout::Hash() const
	{
		uint64 Result = std::hash<void*>()(Mesh.child().get());
		hash_combine(Result, Layout.child().get());
		hash_combine(Result, Channel);
		return Result;
	}


	Ptr<ASTOp> ASTOpMeshApplyLayout::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshApplyLayout> New = new ASTOpMeshApplyLayout();
		New->Mesh = MapChild(Mesh.child());
		New->Layout = MapChild(Layout.child());
		New->Channel = Channel;
		return New;
	}


	void ASTOpMeshApplyLayout::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Mesh);
		Func(Layout);
	}


	void ASTOpMeshApplyLayout::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshApplyLayoutArgs Args;
			FMemory::Memzero(Args);

			if (Mesh) Args.Mesh = Mesh->linkedAddress;
			if (Layout) Args.Layout = Layout->linkedAddress;
			Args.Channel = Channel;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}


	Ptr<ASTOp> ASTOpMeshApplyLayout::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		Ptr<ASTOp> MeshAt = Mesh.child();

		if (!MeshAt)
		{
			return nullptr;
		}

		EOpType MeshOpType = MeshAt->GetOpType();
		
		bool bLayoutIsConstant = !Layout || Layout->GetOpType() == EOpType::LA_CONSTANT;

		switch (MeshOpType)
		{
			case EOpType::ME_ADDTAGS:
			{
				Ptr<ASTOpMeshAddTags> New = mu::Clone<ASTOpMeshAddTags>(MeshAt);
				if (New->Source)
				{
					Ptr<ASTOpMeshApplyLayout> NewApplyPose = mu::Clone<ASTOpMeshApplyLayout>(this);
					NewApplyPose->Mesh = New->Source.child();
					New->Source = NewApplyPose;
				}

				NewOp = New;
				break;
			}

			case EOpType::ME_SWITCH:
			{
				if (bLayoutIsConstant)
				{
					// Move the apply down all the paths
					Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(MeshAt);

					if (NewSwitch->Default)
					{
						Ptr<ASTOpMeshApplyLayout> defOp = mu::Clone<ASTOpMeshApplyLayout>(this);
						defOp->Mesh = NewSwitch->Default.child();
						NewSwitch->Default = defOp;
					}

					// We need to copy the options because we change them
					for (int32 CaseIndex = 0; CaseIndex < NewSwitch->Cases.Num(); ++CaseIndex)
					{
						if (NewSwitch->Cases[CaseIndex].Branch)
						{
							Ptr<ASTOpMeshApplyLayout> bOp = mu::Clone<ASTOpMeshApplyLayout>(this);
							bOp->Mesh = NewSwitch->Cases[CaseIndex].Branch.child();
							NewSwitch->Cases[CaseIndex].Branch = bOp;
						}
					}

					NewOp = NewSwitch;
				}
				break;
			}


			default:
				break;
		}

		return NewOp;
	}


	FSourceDataDescriptor ASTOpMeshApplyLayout::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Mesh)
		{
			return Mesh->GetSourceDataDescriptor(Context);
		}

		return {};
	}
}
