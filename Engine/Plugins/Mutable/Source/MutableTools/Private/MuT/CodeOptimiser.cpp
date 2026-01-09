// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeOptimiser.h"

#include "MuT/ErrorLog.h"
#include "MuT/AST.h"
#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpConstantColor.h"
#include "MuT/ASTOpMeshApplyShape.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpMeshPrepareLayout.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpReferenceResource.h"

#include "MuR/ModelPrivate.h"
#include "MuR/SystemPrivate.h"
#include "MuR/Operations.h"
#include "MuR/OpMeshMerge.h"
#include "MuR/MutableRuntimeModule.h"

#include "Tasks/Task.h"
#include "Math/NumericLimits.h"

namespace mu
{

	namespace
	{

		struct FMeshEntry
		{
			TSharedPtr<const FMesh> Mesh;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FMeshEntry& o) const
			{
				return Mesh == o.Mesh || *Mesh ==*o.Mesh;
			}
		};

		struct FImageEntry
		{
			TSharedPtr<const FImage> Image;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FImageEntry& o) const
			{
				return Image == o.Image || *Image == *o.Image;
			}
		};

		struct FLayoutEntry
		{
			TSharedPtr< const FLayout> Layout;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FLayoutEntry& o) const
			{
				return Layout == o.Layout || *Layout == *o.Layout;
			}
		};

		struct custom_mesh_equal
		{
			bool operator()( const TSharedPtr<const FMesh>& a, const TSharedPtr<const FMesh>& b ) const
			{
				return a==b || *a==*b;
			}
		};

		struct custom_image_equal
		{
			bool operator()( const TSharedPtr<const FImage>& a, const TSharedPtr<const FImage>& b ) const
			{
				return a==b || *a==*b;
			}
		};

		struct custom_layout_equal
		{
			bool operator()( const TSharedPtr<const FLayout>& a, const TSharedPtr<const FLayout>& b ) const
			{
				return a==b || *a==*b;
			}
		};
	}


	bool DuplicatedDataRemoverAST( ASTOpList& roots )
	{
		MUTABLE_CPUPROFILER_SCOPE(DuplicatedDataRemoverAST);

		TArray<Ptr<ASTOpConstantResource>> AllMeshOps;
		TArray<Ptr<ASTOpConstantResource>> AllImageOps;
		TArray<Ptr<ASTOpConstantResource>> AllLayoutOps;

		bool bModified = false;

		// Gather constants
		{
			MUTABLE_CPUPROFILER_SCOPE(Gather);

			ASTOp::Traverse_TopRandom_Unique_NonReentrant( roots, [&](Ptr<ASTOp> n)
			{
				switch ( n->GetOpType() )
				{

				case EOpType::ME_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllMeshOps.Add(typedNode);
					break;
				}

				case EOpType::IM_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllImageOps.Add(typedNode);
					break;
				}

				case EOpType::LA_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllLayoutOps.Add(typedNode);
					break;
				}

				//    These should be part of the duplicated code removal, in AST.
				//            // Names
				//            case EOpType::IN_ADDMESH:
				//            case EOpType::IN_ADDIMAGE:
				//            case EOpType::IN_ADDVECTOR:
				//            case EOpType::IN_ADDSCALAR:
				//            case EOpType::IN_ADDCOMPONENT:
				//            case EOpType::IN_ADDSURFACE:

				default:
					break;
				}

				return true;
			});
		}


		// Compare meshes
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareMeshes);

			TMultiMap< SIZE_T, FMeshEntry > Meshes;

			for (Ptr<ASTOpConstantResource>& typedNode : AllMeshOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FMeshEntry*, TInlineAllocator<4>> Candidates;
				Meshes.MultiFindPointer(Key, Candidates, false);

				if (!Candidates.IsEmpty())
				{
					TSharedPtr<const FMesh> mesh = StaticCastSharedPtr<const FMesh>(typedNode->GetValue());

					for (FMeshEntry* It : Candidates)
					{
						if (!It->Mesh)
						{
							It->Mesh = StaticCastSharedPtr<const FMesh>(It->Op->GetValue());
						}

						if (custom_mesh_equal()(mesh, It->Mesh))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
					bModified = true;
				}
				else
				{
					// The mesh will be loaded only if it needs to be compared
					FMeshEntry e;
					e.Op = typedNode;
					Meshes.Add(Key, e);
				}
			}
		}

		// Compare images
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareImages);

			TMultiMap< SIZE_T, FImageEntry > Images;

			for (Ptr<ASTOpConstantResource>& typedNode : AllImageOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FImageEntry*,TInlineAllocator<4>> Candidates;
				Images.MultiFindPointer(Key, Candidates, false);
				
				if (!Candidates.IsEmpty())
				{
					TSharedPtr<const FImage> image = StaticCastSharedPtr<const FImage>(typedNode->GetValue());

					for (FImageEntry* It: Candidates)
					{
						if (!It->Image)
						{
							It->Image = StaticCastSharedPtr<const FImage>(It->Op->GetValue());
						}

						if (custom_image_equal()(image, It->Image))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
					bModified = true;
				}
				else
				{
					// The image will be loaded only if it needs to be compared
					FImageEntry e;
					e.Op = typedNode;
					Images.Add(Key, e);
				}
			}
		}

		// Compare layouts
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareLayouts);

			TMultiMap< SIZE_T, FLayoutEntry > Layouts;

			for (Ptr<ASTOpConstantResource>& typedNode : AllLayoutOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FLayoutEntry*, TInlineAllocator<4>> Candidates;
				Layouts.MultiFindPointer(Key, Candidates, false);

				if (!Candidates.IsEmpty())
				{
					TSharedPtr<const FLayout> layout = StaticCastSharedPtr<const FLayout>(typedNode->GetValue());

					for (FLayoutEntry* It : Candidates)
					{
						if (!It->Layout)
						{
							It->Layout = StaticCastSharedPtr<const FLayout>(It->Op->GetValue());
						}

						if (custom_layout_equal()(layout, It->Layout))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
					bModified = true;
				}
				else
				{
					FLayoutEntry e;
					e.Op = typedNode;
					Layouts.Add(Key, e);
				}
			}
		}

		return bModified;
	}


	bool DuplicatedCodeRemoverAST( ASTOpList& roots )
	{
		MUTABLE_CPUPROFILER_SCOPE(DuplicatedCodeRemoverAST);

		bool bModified = false;

		struct FKeyFuncs : BaseKeyFuncs<Ptr<ASTOp>, Ptr<ASTOp>, false>
		{
			static KeyInitType GetSetKey(ElementInitType Element) { return Element; }
			static bool Matches(const Ptr<ASTOp>& lhs, const Ptr<ASTOp>& rhs) { return lhs == rhs || *lhs == *rhs; }
			static uint32 GetKeyHash(const Ptr<ASTOp>& Key) { return Key->Hash(); }
		};

		// Visited nodes, per type
		TSet<Ptr<ASTOp>, FKeyFuncs, TInlineSetAllocator<32>> Visited[int32(EOpType::COUNT)];

		ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, 
			[&bModified,&Visited, &roots]
			(Ptr<ASTOp>& n)
			{
				TSet<Ptr<ASTOp>, FKeyFuncs, TInlineSetAllocator<32>>& Container = Visited[(int32)n->GetOpType()];

				// Insert will tell us if it was already there
				bool bIsAlreadyInSet = false;
				Ptr<ASTOp>& Found = Container.FindOrAdd(n, &bIsAlreadyInSet);
				if( bIsAlreadyInSet)
				{
					// It wasn't inserted, so it was already there
					ASTOp::Replace(n, Found);

					// Is it one of the roots? Then we also need to update it
					for (Ptr<ASTOp>& Root : roots)
					{
						if (Root == n)
						{
							Root = Found;
						}
					}

					bModified = true;
				}
			});

		return bModified;
	}


	class FConstantTask 
	{
	public:

		// input
		Ptr<ASTOp> Source;
		FProxyFileContext* DiskCacheContext = nullptr;
		int32 ImageCompressionQuality = 0;
		int32 OptimizationPass = 0;
		FReferencedMeshResourceFunc ReferencedMeshResourceProvider;
		FReferencedImageResourceFunc ReferencedImageResourceProvider;

		// Intermediate
		Ptr<ASTOp> SourceCloned;

		// Result
		Ptr<ASTOp> Result;

	public:

		FConstantTask( const Ptr<ASTOp>& InSource, const CompilerOptions::Private* InOptions, int32 InOptimizationPass )
		{
			OptimizationPass = InOptimizationPass;
			Source = InSource;
			DiskCacheContext = InOptions->OptimisationOptions.DiskCacheContext;
			ImageCompressionQuality = InOptions->ImageCompressionQuality;
			ReferencedMeshResourceProvider = InOptions->OptimisationOptions.ReferencedMeshResourceProvider;
			ReferencedImageResourceProvider = InOptions->OptimisationOptions.ReferencedImageResourceProvider;
		}

		void Run(FImageOperator ImOp)
		{
			MUTABLE_CPUPROFILER_SCOPE(ConstantTask_Run);

			// This runs in a worker thread

			EOpType type = SourceCloned->GetOpType();
			EDataType DataType = GetOpDataType(type);

			FSettings Settings;
			Settings.SetProfile( false );
			Settings.SetImageCompressionQuality( ImageCompressionQuality );
			TSharedPtr<FSystem> System = MakeShared<FSystem>(Settings);

			System->GetPrivate()->ImagePixelFormatOverride = ImOp.FormatImageOverride;

			FSourceDataDescriptor SourceDataDescriptor;
			if (DataType == EDataType::Image || DataType == EDataType::Mesh)
			{
				SourceDataDescriptor = SourceCloned->GetSourceDataDescriptor();
				check(!SourceDataDescriptor.IsInvalid());
			}

			// Don't generate mips during linking here.
			FLinkerOptions LinkerOptions(ImOp);
			LinkerOptions.MinTextureResidentMipCount = 255;
			LinkerOptions.bSeparateImageMips = false;

			TSharedPtr<const FModel> model = MakeShared<FModel>();
			OP::ADDRESS at = ASTOp::FullLink(SourceCloned, model->GetPrivate()->Program, &LinkerOptions);

			FProgram::FState state;
			state.Root = at;
			model->GetPrivate()->Program.States.Add(state);

			TSharedPtr<FParameters> LocalParams = FModel::NewParameters(model);
			System->GetPrivate()->BeginBuild( model );

			// Calculate the value and replace this op by a constant
			switch( DataType )
			{
			case EDataType::Mesh:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantMesh);

				TSharedPtr<const FMesh> MeshBuild = System->GetPrivate()->BuildMesh(model, LocalParams.Get(), at, EMeshContentFlags::AllFlags);

				if (MeshBuild)
				{
					mu::Ptr<ASTOpConstantResource> ConstantOp = new ASTOpConstantResource();
					ConstantOp->SourceDataDescriptor = SourceDataDescriptor;
					ConstantOp->Type = EOpType::ME_CONSTANT;
					ConstantOp->SetValue(MeshBuild, DiskCacheContext);
					Result = ConstantOp;
				  }
				break;
			}

			case EDataType::Image:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantImage);

				TSharedPtr<const FImage> pImage = System->GetPrivate()->BuildImage( model, LocalParams.Get(), at, 0, 0 );

				if (pImage)
				{
					mu::Ptr<ASTOpConstantResource> ConstantOp = new ASTOpConstantResource();
					ConstantOp->SourceDataDescriptor = SourceDataDescriptor;
					ConstantOp->Type = EOpType::IM_CONSTANT;
					ConstantOp->SetValue( pImage, DiskCacheContext );
					Result = ConstantOp;
				}
				break;
			}

			case EDataType::Layout:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantLayout);

				TSharedPtr<const FLayout> pLayout = System->GetPrivate()->BuildLayout( model, LocalParams.Get(), at );

				if (pLayout)
				{
					mu::Ptr<ASTOpConstantResource> constantOp = new ASTOpConstantResource();
					constantOp->Type = EOpType::LA_CONSTANT;
					constantOp->SetValue( pLayout, DiskCacheContext);
					Result = constantOp;
				}
				break;
			}

			case EDataType::Bool:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantBool);

				bool value = System->GetPrivate()->BuildBool( model, LocalParams.Get(), at );
				Result = new ASTOpConstantBool(value);
				break;
			}

			case EDataType::Color:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantBool);

				FVector4f ResultColor(0, 0, 0, 0);
				ResultColor = System->GetPrivate()->BuildColour( model, LocalParams.Get(), at );

				{
					mu::Ptr<ASTOpConstantColor> ConstantOp = new ASTOpConstantColor();
					ConstantOp->Value = ResultColor;
					Result = ConstantOp;
				}
				break;
			}

			case EDataType::Int:
			case EDataType::Scalar:
			case EDataType::String:
			case EDataType::Projector:
				// TODO
				break;

			default:
				break;
			}

			System->GetPrivate()->EndBuild();
		}

	};


	bool ConstantGenerator( const CompilerOptions::Private* InOptions, Ptr<ASTOp>& Root, int32 Pass )
	{
		MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator);

		// don't do this if constant optimization has been disabled, usually for debugging.
		if (!InOptions->OptimisationOptions.bConstReduction)
		{
			return false;
		}

		// Gather the roots of all constant operations
		struct FConstantSubgraph
		{
			Ptr<ASTOp> Root;
			UE::Tasks::FTaskEvent CompletedEvent;
		};
		TArray< FConstantSubgraph > ConstantSubgraphs;
		ConstantSubgraphs.Reserve(256);
		{
			MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator_GenerateTasks);

			ASTOp::Traverse_BottomUp_Unique(Root,
				[&ConstantSubgraphs, Pass]
				(Ptr<ASTOp>& SubgraphRoot)
				{
					EOpType SubgraphType = SubgraphRoot->GetOpType();

					bool bGetFromChildren = false;

					bool bIsConstantSubgraph = true;
					switch (SubgraphType)
					{
					case EOpType::BO_PARAMETER:
					case EOpType::NU_PARAMETER:
					case EOpType::SC_PARAMETER:
					case EOpType::CO_PARAMETER:
					case EOpType::PR_PARAMETER:
					case EOpType::IM_PARAMETER:
					case EOpType::ME_PARAMETER:
					case EOpType::MA_PARAMETER:
						bIsConstantSubgraph = false;
						break;
					default:
						// Propagate from children
						SubgraphRoot->ForEachChild([&bIsConstantSubgraph](ASTChild& c)
							{
								if (c)
								{
									bIsConstantSubgraph = bIsConstantSubgraph && c->bIsConstantSubgraph;
								}
							});
						break;
					}
					SubgraphRoot->bIsConstantSubgraph = bIsConstantSubgraph;

					// We avoid generating constants for these operations, to avoid the memory explosion.
					// TODO: Make compiler options for some of them
					// TODO: Some of them are worth if the code below them is unique.
					bool bHasSpecialOpInSubgraph = false;
					switch (SubgraphType)
					{
					case EOpType::IM_BLANKLAYOUT:
					case EOpType::IM_COMPOSE:
					case EOpType::ME_MERGE:
					case EOpType::ME_CLIPWITHMESH:
					case EOpType::ME_CLIPMORPHPLANE:
					case EOpType::ME_APPLYPOSE:
					case EOpType::ME_REMOVEMASK:
					case EOpType::ME_PREPARELAYOUT:
					case EOpType::IM_PLAINCOLOUR:
						bHasSpecialOpInSubgraph = true;
						break;

					case EOpType::ME_ADDTAGS:
					{
						const ASTOpMeshAddTags* AddTags = static_cast<const ASTOpMeshAddTags*>(SubgraphRoot.get());
						if (AddTags->Source && AddTags->Source->GetParentCount()==1)
						{
							// We could resolve the constant if there are no other constraints since this op's mesh op is only referenced once.
							bGetFromChildren = true;
						}
						else
						{
							// Don't reduce constant to avoid duplicating mesh data.
							bHasSpecialOpInSubgraph = true;
						}
						break;
					}

					case EOpType::IM_RASTERMESH:
					{
						const ASTOpImageRasterMesh* Raster = static_cast<const ASTOpImageRasterMesh*>(SubgraphRoot.get());
						// If this operation is only rastering the mesh UVs, reduce it to constant. Otherwise avoid reducing it
						// for the case of a constant projector of a large set of possible images. We don't want to generate all the
						// projected version of the images beforehand. TODO: Make it a compile-time option?
						bHasSpecialOpInSubgraph = Raster->image.child().get() != nullptr;
						break;
					}

					case EOpType::LA_FROMMESH:
					case EOpType::ME_EXTRACTLAYOUTBLOCK:
					case EOpType::ME_APPLYLAYOUT:
					{
						// We want to reduce this type of operation regardless of it having special ops below.
						bHasSpecialOpInSubgraph = false;
						break;
					}

					case EOpType::ME_REFERENCE:
					case EOpType::IM_REFERENCE:
						// If we are in a reference-resolution optimization phase, then the ops are not special.
						if (Pass < 2)
						{
							bHasSpecialOpInSubgraph = true;
						}
						else
						{
							const ASTOpReferenceResource* Typed = static_cast<const ASTOpReferenceResource*>(SubgraphRoot.get());
							bHasSpecialOpInSubgraph = !Typed->bForceLoad;
						}
						break;

					default:
						bGetFromChildren = true;
						break;
					}

					if (bGetFromChildren)
					{
						// Propagate from children
						SubgraphRoot->ForEachChild([&](ASTChild& c)
							{
								if (c)
								{
									bHasSpecialOpInSubgraph = bHasSpecialOpInSubgraph || c->bHasSpecialOpInSubgraph;
								}
							});
					}

					SubgraphRoot->bHasSpecialOpInSubgraph = bHasSpecialOpInSubgraph;

					bool bIsDataTypeThanCanTurnIntoConst = false;
					EDataType DataType = GetOpDataType(SubgraphType);
					switch (DataType)
					{
					case EDataType::Mesh:
					case EDataType::Image:
					case EDataType::Layout:
					case EDataType::Bool:
					case EDataType::Color:
						bIsDataTypeThanCanTurnIntoConst = true;
						break;
					default:
						break;
					}


					// See if it is worth generating this as constant
					// ---------------------------------------------
					bool bWorthGenerating = SubgraphRoot->bIsConstantSubgraph
						&& !SubgraphRoot->bHasSpecialOpInSubgraph
						&& !SubgraphRoot->IsConstantOp()
						&& bIsDataTypeThanCanTurnIntoConst;

					if (bWorthGenerating)						 
					{
						bool bCanBeGenerated = true;

						// Check source data incompatiblities: when generating constants don't mix data that has different source descriptors (tags and other properties).
						if (DataType == EDataType::Image || DataType == EDataType::Mesh)
						{
							FSourceDataDescriptor SourceDescriptor = SubgraphRoot->GetSourceDataDescriptor();
							if (SourceDescriptor.IsInvalid())
							{
								bCanBeGenerated = false;
							}
						}

						if (bCanBeGenerated)
						{
							ConstantSubgraphs.Add({ SubgraphRoot, UE::Tasks::FTaskEvent(TEXT("MutableConstantSubgraph")) });
						}
					}
				});
		}

		auto GetRequisites = [&ConstantSubgraphs](const Ptr<ASTOp>& SubgraphRoot, TArray< UE::Tasks::FTask, TInlineAllocator<8> >& OutRequisites)
		{
			MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator_GetRequisites);

			TArray< Ptr<ASTOp> > ScanRoots;
			ScanRoots.Add(SubgraphRoot);
			ASTOp::Traverse_TopDown_Unique_Imprecise(ScanRoots, [&SubgraphRoot, &OutRequisites, &ConstantSubgraphs](Ptr<ASTOp>& ChildNode)
				{
					bool bRecurse = true;

					// Subgraph root?
					if (SubgraphRoot == ChildNode)
					{
						return bRecurse;
					}

					FConstantSubgraph* DependencyFound = ConstantSubgraphs.FindByPredicate([&ChildNode](const FConstantSubgraph& Candidate) { return Candidate.Root == ChildNode; });
					if (DependencyFound)
					{
						bRecurse = false;
						OutRequisites.Add(DependencyFound->CompletedEvent);
					}

					return bRecurse;
				});
		};


		// Launch the tasks.
		UE::Tasks::FTask LaunchTask = UE::Tasks::Launch(TEXT("ConstantGeneratorLaunchTasks"),
			[&ConstantSubgraphs, &GetRequisites, Pass, InOptions]()
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator_LaunchTasks);

				FImageOperator ImOp = FImageOperator::GetDefault(InOptions->ImageFormatFunc);

				// Traverse list of constants to generate. It is ordered in a bottom-up way.
				int32 SubgraphCount = ConstantSubgraphs.Num();
				for (int32 OrderIndex = 0; OrderIndex < SubgraphCount; ++OrderIndex)
				{
					int32 Index = SubgraphCount - 1 - OrderIndex;

					Ptr<ASTOp> SubgraphRoot = ConstantSubgraphs[Index].Root;
					UE::Tasks::FTaskEvent& SubgraphCompletionEvent = ConstantSubgraphs[Index].CompletedEvent;

					bool bIsReference = false;
					EOpType SubgraphType = SubgraphRoot->GetOpType();

					if (SubgraphType == EOpType::IM_REFERENCE
						||
						SubgraphType == EOpType::IM_CONSTANT)
					{
						uint32 ImageID = 0;
						if (SubgraphType == EOpType::IM_REFERENCE)
						{
							bIsReference = true;
							const ASTOpReferenceResource* Typed = static_cast<const ASTOpReferenceResource*>(SubgraphRoot.get());
							ImageID = Typed->ID;
						}
						else if (SubgraphType == EOpType::IM_CONSTANT)
						{
							const ASTOpConstantResource* Typed = static_cast<const ASTOpConstantResource*>(SubgraphRoot.get());
							const FImage* Value = static_cast<const FImage*>(Typed->GetValue().Get());
							bIsReference = Value->IsReference();
							if (bIsReference)
							{
								ImageID = Value->GetReferencedTexture();
							}
						}

						// Instead of generating the constant we resolve the reference, which also replaces the ASTOp.
						if (bIsReference)
						{
							TSharedPtr< TSharedPtr<FImage> > ResolveImage = MakeShared<TSharedPtr<FImage>>();

							constexpr bool bRunImmediatlyIfPossible = false;
							UE::Tasks::FTask ReferenceCompletion = InOptions->OptimisationOptions.ReferencedImageResourceProvider(ImageID, ResolveImage, bRunImmediatlyIfPossible);

							UE::Tasks::FTask CompleteTask = UE::Tasks::Launch(TEXT("MutableResolveComplete"),
								[SubgraphRoot, InOptions, ResolveImage]()
								{
									Ptr<ASTOpConstantResource> ConstantOp;
									{
										MUTABLE_CPUPROFILER_SCOPE(MutableResolveComplete_CreateConstant);
										ConstantOp = new ASTOpConstantResource;
										ConstantOp->Type = EOpType::IM_CONSTANT;
										{
											MUTABLE_CPUPROFILER_SCOPE(GetSourceDataDescriptor);
											ConstantOp->SourceDataDescriptor = SubgraphRoot->GetSourceDataDescriptor();
										}
										ConstantOp->SetValue(*ResolveImage, InOptions->OptimisationOptions.DiskCacheContext);
									}
									{
										MUTABLE_CPUPROFILER_SCOPE(MutableResolveComplete_Replace);
										ASTOp::Replace(SubgraphRoot, ConstantOp);
									}
								},
								ReferenceCompletion,
								LowLevelTasks::ETaskPriority::BackgroundNormal);

							SubgraphCompletionEvent.AddPrerequisites(CompleteTask);
						}

					}

					if (SubgraphType == EOpType::ME_REFERENCE
						||
						SubgraphType == EOpType::ME_CONSTANT)
					{
						uint32 MeshID = 0;
						FString MeshMorph;
						if (SubgraphType == EOpType::ME_REFERENCE)
						{
							bIsReference = true;
							const ASTOpReferenceResource* Typed = static_cast<const ASTOpReferenceResource*>(SubgraphRoot.get());
							MeshID = Typed->ID;
						}
						else if (SubgraphType == EOpType::ME_CONSTANT)
						{
							const ASTOpConstantResource* Typed = static_cast<const ASTOpConstantResource*>(SubgraphRoot.get());
							const FMesh* Value = static_cast<const FMesh*>(Typed->GetValue().Get());
							bIsReference = Value->IsReference();
							if (bIsReference)
							{
								MeshID = Value->GetReferencedMesh();
								MeshMorph = Value->GetReferencedMorph();
							}
						}

						// Instead of generating the constant we resolve the reference, which also replaces the ASTOp.
						if (bIsReference)
						{
							TSharedPtr< TSharedPtr<FMesh> > ResolveMesh = MakeShared<TSharedPtr<FMesh>>();

							constexpr bool bRunImmediatlyIfPossible = false;
							UE::Tasks::FTask ReferenceCompletion = InOptions->OptimisationOptions.ReferencedMeshResourceProvider(MeshID, MeshMorph, ResolveMesh, bRunImmediatlyIfPossible);

							UE::Tasks::FTask CompleteTask = UE::Tasks::Launch(TEXT("MutableResolveComplete"),
								[SubgraphRoot, InOptions, ResolveMesh]()
								{
									Ptr<ASTOpConstantResource> ConstantOp;
									{
										MUTABLE_CPUPROFILER_SCOPE(MutableResolveComplete_CreateConstant);
										ConstantOp = new ASTOpConstantResource;
										ConstantOp->Type = EOpType::ME_CONSTANT;
										{
											MUTABLE_CPUPROFILER_SCOPE(GetSourceDataDescriptor);
											ConstantOp->SourceDataDescriptor = SubgraphRoot->GetSourceDataDescriptor();
										}
										ConstantOp->SetValue(*ResolveMesh, InOptions->OptimisationOptions.DiskCacheContext);
									}
									{
										MUTABLE_CPUPROFILER_SCOPE(MutableResolveComplete_Replace);
										ASTOp::Replace(SubgraphRoot, ConstantOp);
									}
								},
								ReferenceCompletion,
								LowLevelTasks::ETaskPriority::BackgroundNormal);

							SubgraphCompletionEvent.AddPrerequisites(CompleteTask);
						}
					}

					if (!bIsReference)
					{
						// Scan for requisites
						TArray< UE::Tasks::FTask, TInlineAllocator<8> > Requisites;
						GetRequisites(SubgraphRoot, Requisites);

						TUniquePtr<FConstantTask> Task(new FConstantTask(SubgraphRoot, InOptions, Pass));

						// Launch the preparation on the AST-modification pipe
						UE::Tasks::FTask CompleteTask = UE::Tasks::Launch(TEXT("MutableConstant"), [TaskPtr = MoveTemp(Task), ImOp]()
							{
								MUTABLE_CPUPROFILER_SCOPE(MutableConstantPrepare);

								// We need the clone because linking modifies ASTOp state and also to be safe for concurrency.
								TaskPtr->SourceCloned = ASTOp::DeepClone(TaskPtr->Source);

								TaskPtr->Run(ImOp);

								ASTOp::Replace(TaskPtr->Source, TaskPtr->Result);
								TaskPtr->Result = nullptr;
								TaskPtr->Source = nullptr;
							},
							Requisites,
							LowLevelTasks::ETaskPriority::BackgroundHigh);

						SubgraphCompletionEvent.AddPrerequisites(CompleteTask);
					}

					ConstantSubgraphs[Index].Root = nullptr;
					SubgraphCompletionEvent.Trigger();

					UE::Tasks::AddNested(SubgraphCompletionEvent);
				}

			});

		// Wait for pending tasks
		{
			MUTABLE_CPUPROFILER_SCOPE(Waiting);
			LaunchTask.Wait();
		}


		bool bSomethingModified = ConstantSubgraphs.Num() > 0;
		return bSomethingModified;
	}


	CodeOptimiser::CodeOptimiser(Ptr<CompilerOptions> InOptions, TArray<FStateCompilationData>& InStates )
		: States( InStates )
	{
		Options = InOptions;
	}


	void CodeOptimiser::FullOptimiseAST( ASTOpList& roots, int32 Pass )
	{
		bool bModified = true;
		int32 NumIterations = 0;
		while (bModified && (OptimizeIterationsLeft>0 || !NumIterations))
		{
			bool bModifiedInInnerLoop = true;
			while (bModifiedInInnerLoop && (OptimizeIterationsLeft>0 || !NumIterations))
			{
				--OptimizeIterationsLeft;
				++NumIterations;
				UE_LOG(LogMutableCore, Verbose, TEXT("Main optimise iteration %d, left %d"), NumIterations, OptimizeIterationsLeft);

				bModifiedInInnerLoop = false;

				// All kind of optimisations that depend on the meaning of each operation
				// \TODO: We are doing it for all states.
				UE_LOG(LogMutableCore, Verbose, TEXT(" - semantic optimiser"));
				bModifiedInInnerLoop |= SemanticOptimiserAST(roots, Options->GetPrivate()->OptimisationOptions, Pass);
				ASTOp::LogHistogram(roots);

				UE_LOG(LogMutableCore, Verbose, TEXT(" - sink optimiser"));
				bModifiedInInnerLoop |= SinkOptimiserAST(roots, Options->GetPrivate()->OptimisationOptions);
				ASTOp::LogHistogram(roots);

				// Image size operations are treated separately
				UE_LOG(LogMutableCore, Verbose, TEXT(" - size optimiser"));
				bModifiedInInnerLoop |= SizeOptimiserAST(roots);
			}

			bModified = bModifiedInInnerLoop;

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			bModified |= DuplicatedCodeRemoverAST(roots);
			//UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

			ASTOp::LogHistogram(roots);

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			bModified |= DuplicatedDataRemoverAST(roots);
			//UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

			ASTOp::LogHistogram(roots);

			// Generate constants
			bool bModifiedInConstants = false;
			for (Ptr<ASTOp>& Root : roots)
			{
				//UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				UE_LOG(LogMutableCore, Verbose, TEXT(" - constant generator"));

				// Constant subtree generation
				bModifiedInConstants |= ConstantGenerator(Options->GetPrivate(), Root, Pass);
			}

			ASTOp::LogHistogram(roots);

			if (bModifiedInConstants)
			{
				bModified = true;

				UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
				DuplicatedDataRemoverAST(roots);
			}

			//if (!bModified)
			{
				UE_LOG(LogMutableCore, Verbose, TEXT(" - logic optimiser"));
				bModified |= LocalLogicOptimiserAST(roots);
			}

			ASTOp::LogHistogram(roots);
		}
	}


	// The state represents if there is a parent operation requiring skeleton for current mesh subtree.
	class CollectAllMeshesForSkeletonVisitorAST : public Visitor_TopDown_Unique_Const<uint8>
	{
	public:

		CollectAllMeshesForSkeletonVisitorAST( const ASTOpList& roots  )
		{
			Traverse( roots, false );
		}

		// List of meshes that require a skeleton
		TArray<mu::Ptr<ASTOpConstantResource>> MeshesRequiringSkeleton;

	private:

		// Visitor_TopDown_Unique_Const<uint8_t> interface
		bool Visit( const mu::Ptr<ASTOp>& node ) override
		{
			// \todo: refine to avoid instruction branches with irrelevant skeletons.

			uint8_t currentProtected = GetCurrentState();

			switch (node->GetOpType())
			{

			case EOpType::ME_CONSTANT:
			{
				mu::Ptr<ASTOpConstantResource> typedOp = static_cast<ASTOpConstantResource*>(node.get());

				if (currentProtected)
				{
					MeshesRequiringSkeleton.AddUnique(typedOp);
				}

				return false;
			}

			case EOpType::ME_CLIPMORPHPLANE:
			{
				ASTOpMeshClipMorphPlane* typedOp = static_cast<ASTOpMeshClipMorphPlane*>(node.get());
				if (typedOp->VertexSelectionType == EClipVertexSelectionType::BoneHierarchy)
				{
					// We need the skeleton for the source mesh
					RecurseWithState( typedOp->Source.child(), true );
					return false;
				}

				return true;
			}

			case EOpType::ME_APPLYPOSE:
			{
				ASTOpMeshApplyPose* typedOp = static_cast<ASTOpMeshApplyPose*>(node.get());

				// We need the skeleton for both meshes
				RecurseWithState(typedOp->Base.child(), true);
				RecurseWithState(typedOp->Pose.child(), true);
				return false;
			}

			case EOpType::ME_BINDSHAPE:
			{
				ASTOpMeshBindShape* typedOp = static_cast<ASTOpMeshBindShape*>(node.get());
				if (typedOp->bReshapeSkeleton)
				{
					RecurseWithState(typedOp->Mesh.child(), true);
					return false;
				}

				break;
			}

			case EOpType::ME_APPLYSHAPE:
			{
				ASTOpMeshApplyShape* typedOp = static_cast<ASTOpMeshApplyShape*>(node.get());
				if (typedOp->bReshapeSkeleton)
				{
					RecurseWithState(typedOp->Mesh.child(), true);
					return false;
				}

				break;
			}

			default:
				break;
			}

			return true;
		}

	};


	// This stores an ADD_MESH op with the child meshes collected and the final skeleton to use
	// for this op.
	struct FAddMeshSkeleton
	{
		mu::Ptr<ASTOp> AddMeshOp;
		TArray<mu::Ptr<ASTOpConstantResource>> ContributingMeshes;
		TSharedPtr<FSkeleton> FinalSkeleton;

		FAddMeshSkeleton( const mu::Ptr<ASTOp>& InAddMeshOp,
						  TArray<mu::Ptr<ASTOpConstantResource>>& InContributingMeshes,
						  const TSharedPtr<FSkeleton>& InFinalSkeleton )
		{
			AddMeshOp = InAddMeshOp;
			ContributingMeshes = MoveTemp(InContributingMeshes);
			FinalSkeleton = InFinalSkeleton;
		}
	};


	void SkeletonCleanerAST( TArray<mu::Ptr<ASTOp>>& roots, const FModelOptimizationOptions& options )
	{
		// This collects all the meshes that require a skeleton because they are used in operations
		// that require it.
		CollectAllMeshesForSkeletonVisitorAST requireSkeletonCollector( roots );

		TArray<FAddMeshSkeleton> replacementsFound;

		ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](mu::Ptr<ASTOp>& at )
		{
			// Only recurse instance construction ops.
			bool processChildren = GetOpDataType(at->GetOpType())== EDataType::Instance;

			if ( at->GetOpType() == EOpType::IN_ADDMESH )
			{
				ASTOpInstanceAdd* typedNode = static_cast<ASTOpInstanceAdd*>(at.get());
				mu::Ptr<ASTOp> meshRoot = typedNode->value.child();

				if (meshRoot)
				{
					// Gather constant meshes contributing to the final mesh
					TArray<mu::Ptr<ASTOpConstantResource>> subtreeMeshes;
					TArray<mu::Ptr<ASTOp>> tempRoots;
					tempRoots.Add(meshRoot);
					ASTOp::Traverse_TopDown_Unique_Imprecise( tempRoots, [&](mu::Ptr<ASTOp>& lat )
					{
						// \todo: refine to avoid instruction branches with irrelevant skeletons.
						if ( lat->GetOpType() == EOpType::ME_CONSTANT )
						{
							mu::Ptr<ASTOpConstantResource> typedOp = static_cast<ASTOpConstantResource*>(lat.get());
							if ( subtreeMeshes.Find(typedOp)
								 ==
								 INDEX_NONE )
							{
								subtreeMeshes.Add(typedOp);
							}
						}
						return true;
					});

					// Create a mesh just with the unified skeleton
					TSharedPtr<FSkeleton> FinalSkeleton = MakeShared<FSkeleton>();
					for (const auto& meshAt: subtreeMeshes)
					{
						TSharedPtr<const FMesh> pMesh = StaticCastSharedPtr<const FMesh>(meshAt->GetValue());
						TSharedPtr<const FSkeleton> SourceSkeleton = pMesh ? pMesh->GetSkeleton() : nullptr;
						if (SourceSkeleton)
						{
							ExtendSkeleton(FinalSkeleton.Get(),SourceSkeleton.Get());
						}
					}

					replacementsFound.Emplace( at, subtreeMeshes, FinalSkeleton );
				}
			}

			return processChildren;
		});


		// Iterate all meshes again
		ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](mu::Ptr<ASTOp>& at )
		{
			if (at->GetOpType()==EOpType::ME_CONSTANT)
			{
				ASTOpConstantResource* typedOp = static_cast<ASTOpConstantResource*>(at.get());

				for(FAddMeshSkeleton& Rep: replacementsFound)
				{
					if (Rep.ContributingMeshes.Contains(at))
					{
						TSharedPtr<const FMesh> pMesh = StaticCastSharedPtr<const FMesh>(typedOp->GetValue());
						pMesh->CheckIntegrity();

						TSharedPtr<FMesh> NewMesh = MakeShared<FMesh>();
						bool bOutSuccess = false;
						MeshRemapSkeleton(NewMesh.Get(), pMesh.Get(), Rep.FinalSkeleton, bOutSuccess);

						if (bOutSuccess)
						{
							NewMesh->CheckIntegrity();
							mu::Ptr<ASTOpConstantResource> newOp = new ASTOpConstantResource();
							newOp->Type = EOpType::ME_CONSTANT;
							newOp->SetValue(NewMesh, options.DiskCacheContext);
							newOp->SourceDataDescriptor = at->GetSourceDataDescriptor();

							ASTOp::Replace(at, newOp);
						}
					}
				}
			}
			return true;
		});
	}


	void CodeOptimiser::Optimise()
	{
		MUTABLE_CPUPROFILER_SCOPE(Optimise);

		// Gather all the roots (one for each state)
		TArray<Ptr<ASTOp>> roots;
		for(const FStateCompilationData& s:States)
		{
			roots.Add(s.root);
		}

		//UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

		if ( Options->GetPrivate()->OptimisationOptions.bEnabled )
		{
			// We use 4 times the count because at the time we moved to sharing this count it
			// was being used 4 times, and we want to keep the tests consistent.
			int32 MaxIterations = Options->GetPrivate()->OptimisationOptions.MaxOptimisationLoopCount;
			OptimizeIterationsLeft = MaxIterations ? MaxIterations * 4 : TNumericLimits<int32>::Max();

			// The first duplicated data remover has the special mission of removing
			// duplicated data (meshes) that may have been specified in the source
			// data, before we make it diverge because of different uses, like layout
			// creation
			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );

			ASTOp::LogHistogram(roots);

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );

			// Special optimization stages
			if ( Options->GetPrivate()->OptimisationOptions.bUniformizeSkeleton )
			{
				UE_LOG(LogMutableCore, Verbose, TEXT(" - skeleton cleaner"));
				ASTOp::LogHistogram(roots);

				SkeletonCleanerAST( roots, Options->GetPrivate()->OptimisationOptions );
				ASTOp::LogHistogram(roots);
			}

			// First optimisation stage. It tries to resolve all the image sizes. This is necessary
			// because some operations cannot be applied correctly until the image size is known
			// like the grow-map generation.
			bool bModified = true;
			int32 NumIterations = 0;
			while (bModified)
			{
				MUTABLE_CPUPROFILER_SCOPE(FirstStage);

				--OptimizeIterationsLeft;
				++NumIterations;
				UE_LOG(LogMutableCore, Verbose, TEXT("First optimise iteration %d, left %d"), NumIterations, OptimizeIterationsLeft);

				bModified = false;

				UE_LOG(LogMutableCore, Verbose, TEXT(" - size optimiser"));
				bModified |= SizeOptimiserAST( roots );
			}

			// Main optimisation stage
			{
				MUTABLE_CPUPROFILER_SCOPE(MainStage);
				FullOptimiseAST( roots, 0 );

				FullOptimiseAST( roots, 1 );
			}

			// Constant resolution stage: resolve referenced assets.
			{
				MUTABLE_CPUPROFILER_SCOPE(ReferenceResolution);
				
				constexpr int32 Pass = 2;

				//FullOptimiseAST(roots, 2);

				// Generate constants
				for (Ptr<ASTOp>& Root : roots)
				{
					// Constant subtree generation
					bModified = ConstantGenerator(Options->GetPrivate(), Root, Pass);
				}

				DuplicatedDataRemoverAST(roots);
			}

			// Main optimisation stage again for data-aware optimizations
			{
				MUTABLE_CPUPROFILER_SCOPE(FinalStage);
				FullOptimiseAST(roots, 0);
				ASTOp::LogHistogram(roots);

				FullOptimiseAST(roots, 1);
				ASTOp::LogHistogram(roots);
			}

			// Analyse mesh constants to see which of them are in optimised mesh formats, and set the flags.
			ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, [&](Ptr<ASTOp>& n)
			{
				if (n->GetOpType()==EOpType::ME_CONSTANT)
				{
					ASTOpConstantResource* typed = static_cast<ASTOpConstantResource*>(n.get());
					auto pMesh = StaticCastSharedPtr<const FMesh>(typed->GetValue());
					pMesh->ResetStaticFormatFlags();
					typed->SetValue( pMesh, Options->GetPrivate()->OptimisationOptions.DiskCacheContext);
				}
			});

			ASTOp::LogHistogram(roots);

			// Reset the state root operations in case they have changed due to optimization
			for (int32 RootIndex = 0; RootIndex < States.Num(); ++RootIndex)
			{
				States[RootIndex].root = roots[RootIndex];
			}

			{
				MUTABLE_CPUPROFILER_SCOPE(StatesStage);

				// Optimise for every state
				OptimiseStatesAST( );

				// Optimise the data formats (TODO)
				//OperationFlagGenerator flagGen( pResult.get() );
			}

			ASTOp::LogHistogram(roots);
		}

		// Minimal optimisation of constant subtrees
		else if ( Options->GetPrivate()->OptimisationOptions.bConstReduction )
		{
			// The first duplicated data remover has the special mission of removing
			// duplicated data (meshes) that may have been specified in the source
			// data, before we make it diverge because of different uses, like layout
			// creation
			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );

			// Constant resolution stage: resolve referenced assets.
			{
				MUTABLE_CPUPROFILER_SCOPE(ReferenceResolution);
				FullOptimiseAST(roots, 2);
			}

			for ( int32 StateIndex=0; StateIndex <States.Num(); ++StateIndex)
			{
				constexpr int32 Pass = 1;

				UE_LOG(LogMutableCore, Verbose, TEXT(" - constant generator"));
				ConstantGenerator( Options->GetPrivate(), roots[StateIndex], Pass);
			}

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );

			// Reset the state root operations in case they have changed due to optimization
			for (int32 RootIndex = 0; RootIndex < States.Num(); ++RootIndex)
			{
				States[RootIndex].root = roots[RootIndex];
			}
		}

		ASTOp::LogHistogram(roots);

	}

}
