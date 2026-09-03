// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Operations.h"

#include "MuR/ModelPrivate.h"


namespace mu
{
    MUTABLE_IMPLEMENT_POD_SERIALISABLE(OP);
    MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(OP);

    // clang-format off
    static const FOpDesc SOperationRuntimeDescs[] =
	{ 
		// DataType				
        { EDataType::None,			},	// NONE

		{ EDataType::Bool,			},	// BO_CONSTANT
		{ EDataType::Int,			},	// NU_CONSTANT
		{ EDataType::Scalar,		},	// SC_CONSTANT
		{ EDataType::Color,			},	// CO_CONSTANT
		{ EDataType::Image,			},	// IM_CONSTANT
		{ EDataType::Mesh,			},	// ME_CONSTANT
		{ EDataType::Layout,		},	// LA_CONSTANT
		{ EDataType::Projector,		},	// PR_CONSTANT
		{ EDataType::String,		},	// ST_CONSTANT
		{ EDataType::ExtensionData,	},	// ED_CONSTANT
		{ EDataType::Matrix,		},	// MA_CONSTANT

		{ EDataType::Bool,			},	// BO_PARAMETER
		{ EDataType::Int,			},	// NU_PARAMETER
		{ EDataType::Scalar,		},	// SC_PARAMETER
		{ EDataType::Color,			},	// CO_PARAMETER
		{ EDataType::Projector,		},	// PR_PARAMETER
		{ EDataType::Image,			},	// IM_PARAMETER
		{ EDataType::Mesh,			},	// ME_PARAMETER
		{ EDataType::String,		},	// ST_PARAMETER
		{ EDataType::Matrix,		},	// MA_PARAMETER

		{ EDataType::Image,			},	// IM_REFERENCE
		{ EDataType::Mesh,			},	// ME_REFERENCE

		{ EDataType::Int,			},	// NU_CONDITIONAL
		{ EDataType::Scalar,		},	// SC_CONDITIONAL
		{ EDataType::Color,			},	// CO_CONDITIONAL
		{ EDataType::Image,			},	// IM_CONDITIONAL
		{ EDataType::Mesh,			},	// ME_CONDITIONAL
		{ EDataType::Layout,		},	// LA_CONDITIONAL
		{ EDataType::Instance,		},	// IN_CONDITIONAL
		{ EDataType::ExtensionData,	},	// ED_CONDITIONAL

		{ EDataType::Int,			},	// NU_SWITCH
		{ EDataType::Scalar,		},	// SC_SWITCH
		{ EDataType::Color,			},	// CO_SWITCH
		{ EDataType::Image,			},	// IM_SWITCH
		{ EDataType::Mesh,			},	// ME_SWITCH
		{ EDataType::Layout,		},	// LA_SWITCH
		{ EDataType::Instance,		},	// IN_SWITCH
		{ EDataType::ExtensionData,	},	// ED_SWITCH

		{ EDataType::Bool,			},	// BO_EQUAL_SC_CONST
		{ EDataType::Bool,			},	// BO_AND
		{ EDataType::Bool,			},	// BO_OR
		{ EDataType::Bool,			},	// BO_NOT

		{ EDataType::Scalar,		},	// SC_ARITHMETIC
		{ EDataType::Scalar,		},	// SC_CURVE

		{ EDataType::Color,			},	// CO_SAMPLEIMAGE
		{ EDataType::Color,			},	// CO_SWIZZLE
		{ EDataType::Color,			},	// CO_FROMSCALARS
		{ EDataType::Color,			},	// CO_ARITHMETIC

		{ EDataType::Image,			},	// IM_LAYER
		{ EDataType::Image,			},	// IM_LAYERCOLOUR
		{ EDataType::Image,			},	// IM_PIXELFORMAT
		{ EDataType::Image,			},	// IM_MIPMAP
		{ EDataType::Image,			},	// IM_RESIZE
		{ EDataType::Image,			},	// IM_RESIZELIKE
		{ EDataType::Image,			},	// IM_RESIZEREL
		{ EDataType::Image,			},	// IM_BLANKLAYOUT
		{ EDataType::Image,			},	// IM_COMPOSE
		{ EDataType::Image,			},	// IM_INTERPOLATE
		{ EDataType::Image,			},	// IM_SATURATE
		{ EDataType::Image,			},	// IM_LUMINANCE
		{ EDataType::Image,			},	// IM_SWIZZLE
		{ EDataType::Image,			},	// IM_COLOURMAP
		{ EDataType::Image,			},	// IM_BINARISE
		{ EDataType::Image,			},	// IM_PLAINCOLOUR
		{ EDataType::Image,			},	// IM_CROP
		{ EDataType::Image,			},	// IM_PATCH
		{ EDataType::Image,			},	// IM_RASTERMESH
		{ EDataType::Image,			},	// IM_MAKEGROWMAP
		{ EDataType::Image,			},	// IM_DISPLACE
		{ EDataType::Image,			},	// IM_MULTILAYER
		{ EDataType::Image,			},	// IM_INVERT
		{ EDataType::Image,			},	// IM_NORMALCOMPOSITE
		{ EDataType::Image,			},	// IM_TRANSFORM

		{ EDataType::Mesh,			},	// ME_APPLYLAYOUT
		{ EDataType::Mesh,			},	// ME_PREPARELAYOUT
		{ EDataType::Mesh,			},	// ME_DIFFERENCE
		{ EDataType::Mesh,			},	// ME_MORPH
		{ EDataType::Mesh,			},	// ME_MERGE
		{ EDataType::Mesh,			},	// ME_MASKCLIPMESH
		{ EDataType::Mesh,			},	// ME_MASKCLIPUVMASK
		{ EDataType::Mesh,			},	// ME_MASKDIFF
		{ EDataType::Mesh,			},	// ME_REMOVEMASK
		{ EDataType::Mesh,			},	// ME_FORMAT
		{ EDataType::Mesh,			},	// ME_EXTRACTLAYOUTBLOCK
		{ EDataType::Mesh,			},	// ME_TRANSFORM
		{ EDataType::Mesh,			},	// ME_CLIPMORPHPLANE
		{ EDataType::Mesh,			},	// ME_CLIPWITHMESH
		{ EDataType::Mesh,			},	// ME_SETSKELETON
		{ EDataType::Mesh,			},	// ME_PROJECT
		{ EDataType::Mesh,			},	// ME_APPLYPOSE
		{ EDataType::Mesh,			},	// ME_BINDSHAPE
		{ EDataType::Mesh,			},	// ME_APPLYSHAPE
		{ EDataType::Mesh,			},	// ME_CLIPDEFORM
		{ EDataType::Mesh,			},	// ME_MORPHRESHAPE
		{ EDataType::Mesh,			},	// ME_OPTIMIZESKINNING
		{ EDataType::Mesh,			},	// ME_ADDTAGS
		{ EDataType::Mesh,			},	// ME_TRANSFORMWITHMESH

		{ EDataType::Instance,		},	// IN_ADDMESH
		{ EDataType::Instance,		},	// IN_ADDIMAGE
		{ EDataType::Instance,		},	// IN_ADDVECTOR
		{ EDataType::Instance,		},	// IN_ADDSCALAR
		{ EDataType::Instance,		},	// IN_ADDSTRING
		{ EDataType::Instance,		},	// IN_ADDSURFACE
		{ EDataType::Instance,		},	// IN_ADDCOMPONENT
		{ EDataType::Instance,		},	// IN_ADDLOD
		{ EDataType::Instance,		},	// IN_ADDEXTENSIONDATA
		{ EDataType::Instance,		},	// IN_ADDOVERLAYMATERIAL

		{ EDataType::Layout,		},	// LA_PACK
		{ EDataType::Layout,		},	// LA_MERGE
		{ EDataType::Layout,		},	// LA_REMOVEBLOCKS
		{ EDataType::Layout,		},	// LA_FROMMESH
	};

    // clang-format on

    static_assert(UE_ARRAY_COUNT(SOperationRuntimeDescs) == int32(EOpType::COUNT), "OperationDescMismatch");

	const FOpDesc& GetOpDesc( EOpType type )
	{
        return SOperationRuntimeDescs[ (int32)type ];
	}


    //---------------------------------------------------------------------------------------------
    void ForEachReference( const FProgram& program, OP::ADDRESS at, const TFunctionRef<void(OP::ADDRESS)> f )
    {
        EOpType type = program.GetOpType(at);
        switch ( type )
        {
        case EOpType::NONE:
        case EOpType::BO_CONSTANT:
        case EOpType::NU_CONSTANT:
        case EOpType::SC_CONSTANT:
        case EOpType::ST_CONSTANT:
        case EOpType::CO_CONSTANT:
        case EOpType::IM_CONSTANT:
        case EOpType::ME_CONSTANT:
        case EOpType::LA_CONSTANT:
        case EOpType::PR_CONSTANT:
		case EOpType::ED_CONSTANT:
		case EOpType::MA_CONSTANT:
        case EOpType::BO_PARAMETER:
        case EOpType::NU_PARAMETER:
        case EOpType::SC_PARAMETER:
        case EOpType::CO_PARAMETER:
        case EOpType::PR_PARAMETER:
		case EOpType::IM_PARAMETER:
		case EOpType::ME_PARAMETER:
		case EOpType::MA_PARAMETER:
		case EOpType::IM_REFERENCE:
		case EOpType::ME_REFERENCE:
			break;

        case EOpType::SC_CURVE:
        {
			OP::ScalarCurveArgs args = program.GetOpArgs<OP::ScalarCurveArgs>(at);
            f(args.time );
            break;
        }

        case EOpType::NU_CONDITIONAL:
        case EOpType::SC_CONDITIONAL:
        case EOpType::CO_CONDITIONAL:
        case EOpType::IM_CONDITIONAL:
        case EOpType::ME_CONDITIONAL:
        case EOpType::LA_CONDITIONAL:
        case EOpType::IN_CONDITIONAL:
		case EOpType::ED_CONDITIONAL:
        {
			OP::ConditionalArgs args = program.GetOpArgs<OP::ConditionalArgs>(at);
            f(args.condition );
            f(args.yes );
            f(args.no );
            break;
        }

        case EOpType::NU_SWITCH:
        case EOpType::SC_SWITCH:
        case EOpType::CO_SWITCH:
        case EOpType::IM_SWITCH:
        case EOpType::ME_SWITCH:
        case EOpType::LA_SWITCH:
        case EOpType::IN_SWITCH:
		case EOpType::ED_SWITCH:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);
			
			OP::ADDRESS VarAddress;
			FMemory::Memcpy( &VarAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			OP::ADDRESS DefAddress;
			FMemory::Memcpy( &DefAddress, data, sizeof(OP::ADDRESS));
			data += sizeof(OP::ADDRESS);

			uint32 CaseCount;
			FMemory::Memcpy( &CaseCount, data, sizeof(uint32));
			data += sizeof(uint32);

			f(VarAddress);
			f(DefAddress);

			for ( uint32 C = 0; C < CaseCount; ++C )
			{	
				//int32 Condition;
				//FMemory::Memcpy( &Condition, data, sizeof(int32_t));
				data += sizeof(int32);
				
				OP::ADDRESS At;
				FMemory::Memcpy( &At, data, sizeof(OP::ADDRESS) );
				data += sizeof(OP::ADDRESS);

				f(At);
			}

            break;
        }

        //-------------------------------------------------------------------------------------
        case EOpType::BO_EQUAL_INT_CONST:
        {
			OP::BoolEqualScalarConstArgs args = program.GetOpArgs<OP::BoolEqualScalarConstArgs>(at);
            f(args.Value );
            break;
        }

        case EOpType::BO_AND:
        case EOpType::BO_OR:
        {
			OP::BoolBinaryArgs args = program.GetOpArgs<OP::BoolBinaryArgs>(at);
            f(args.A );
            f(args.B );
            break;
        }

        case EOpType::BO_NOT:
        {
			OP::BoolNotArgs args = program.GetOpArgs<OP::BoolNotArgs>(at);
            f(args.A );
            break;
        }

        case EOpType::SC_ARITHMETIC:
        {
			OP::ArithmeticArgs args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.A );
            f(args.B );
            break;
        }

        case EOpType::CO_SAMPLEIMAGE:
        {
			OP::ColourSampleImageArgs args = program.GetOpArgs<OP::ColourSampleImageArgs>(at);
            f(args.Image );
            f(args.X );
            f(args.Y );
            break;
        }

        case EOpType::CO_SWIZZLE:
        {
			OP::ColourSwizzleArgs args = program.GetOpArgs<OP::ColourSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case EOpType::CO_FROMSCALARS:
        {
			OP::ColourFromScalarsArgs args = program.GetOpArgs<OP::ColourFromScalarsArgs>(at);
			for (int32 t = 0; t < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++t)
			{
				f(args.V[t]);
			}
            break;
        }

        case EOpType::CO_ARITHMETIC:
        {
			OP::ArithmeticArgs args = program.GetOpArgs<OP::ArithmeticArgs>(at);
            f(args.A);
            f(args.B);
            break;
        }

        //-------------------------------------------------------------------------------------
        case EOpType::IM_LAYER:
        {
			OP::ImageLayerArgs args = program.GetOpArgs<OP::ImageLayerArgs>(at);
            f(args.base );
        	f(args.mask );
            f(args.blended );
            break;
        }

        case EOpType::IM_LAYERCOLOUR:
        {
			OP::ImageLayerColourArgs args = program.GetOpArgs<OP::ImageLayerColourArgs>(at);
            f(args.base );
        	f(args.mask );
            f(args.colour );
            break;
        }

        case EOpType::IM_MULTILAYER:
        {
			OP::ImageMultiLayerArgs args = program.GetOpArgs<OP::ImageMultiLayerArgs>(at);
            f(args.rangeSize );
            f(args.base );
            f(args.mask );
            f(args.blended );
            break;
        }

		case EOpType::IM_NORMALCOMPOSITE:
		{
			OP::ImageNormalCompositeArgs args = program.GetOpArgs<OP::ImageNormalCompositeArgs>(at);
			f(args.base);
			f(args.normal);

			break;
		}

        case EOpType::IM_PIXELFORMAT:
        {
			OP::ImagePixelFormatArgs args = program.GetOpArgs<OP::ImagePixelFormatArgs>(at);
            f(args.source );
            break;
        }

        case EOpType::IM_MIPMAP:
        {
			OP::ImageMipmapArgs args = program.GetOpArgs<OP::ImageMipmapArgs>(at);
            f(args.source );
            break;
        }

        case EOpType::IM_RESIZE:
        {
			OP::ImageResizeArgs Args = program.GetOpArgs<OP::ImageResizeArgs>(at);
            f(Args.Source );
            break;
        }

        case EOpType::IM_RESIZELIKE:
        {
			OP::ImageResizeLikeArgs Args = program.GetOpArgs<OP::ImageResizeLikeArgs>(at);
            f(Args.Source );
            f(Args.SizeSource );
            break;
        }

        case EOpType::IM_RESIZEREL:
        {
			OP::ImageResizeRelArgs Args = program.GetOpArgs<OP::ImageResizeRelArgs>(at);
            f(Args.Source );
            break;
        }

        case EOpType::IM_BLANKLAYOUT:
        {
			OP::ImageBlankLayoutArgs Args = program.GetOpArgs<OP::ImageBlankLayoutArgs>(at);
            f(Args.Layout );
            break;
        }

        case EOpType::IM_COMPOSE:
        {
			OP::ImageComposeArgs args = program.GetOpArgs<OP::ImageComposeArgs>(at);
            f(args.layout );
            f(args.base );
            f(args.blockImage );
            f(args.mask );
            break;
        }

        case EOpType::IM_INTERPOLATE:
        {
			OP::ImageInterpolateArgs Args = program.GetOpArgs<OP::ImageInterpolateArgs>(at);
            f(Args.Factor );

            for (int32 TargetIndex=0; TargetIndex <MUTABLE_OP_MAX_INTERPOLATE_COUNT;++TargetIndex)
            {
                f(Args.Targets[TargetIndex]);
            }
            break;
        }

        case EOpType::IM_SWIZZLE:
        {
			OP::ImageSwizzleArgs args = program.GetOpArgs<OP::ImageSwizzleArgs>(at);
            for (int t=0;t<MUTABLE_OP_MAX_SWIZZLE_CHANNELS;++t)
            {
                f(args.sources[t] );
            }
            break;
        }

        case EOpType::IM_SATURATE:
        {
			OP::ImageSaturateArgs Args = program.GetOpArgs<OP::ImageSaturateArgs>(at);
            f(Args.Base );
            f(Args.Factor );
            break;
        }

        case EOpType::IM_LUMINANCE:
        {
			OP::ImageLuminanceArgs Args = program.GetOpArgs<OP::ImageLuminanceArgs>(at);
            f(Args.Base );
            break;
        }

        case EOpType::IM_COLOURMAP:
        {
			OP::ImageColourMapArgs Args = program.GetOpArgs<OP::ImageColourMapArgs>(at);
            f(Args.Base );
            f(Args.Mask );
            f(Args.Map );
            break;
        }

        case EOpType::IM_BINARISE:
        {
			OP::ImageBinariseArgs Args = program.GetOpArgs<OP::ImageBinariseArgs>(at);
            f(Args.Base );
            f(Args.Threshold );
            break;
        }

        case EOpType::IM_PLAINCOLOUR:
        {
			OP::ImagePlainColorArgs args = program.GetOpArgs<OP::ImagePlainColorArgs>(at);
            f(args.Color );
            break;
        }

        case EOpType::IM_CROP:
        {
			OP::ImageCropArgs args = program.GetOpArgs<OP::ImageCropArgs>(at);
            f(args.source );
            break;
        }

        case EOpType::IM_PATCH:
        {
			OP::ImagePatchArgs args = program.GetOpArgs<OP::ImagePatchArgs>(at);
            f(args.base );
            f(args.patch );
            break;
        }

        case EOpType::IM_RASTERMESH:
        {
			OP::ImageRasterMeshArgs args = program.GetOpArgs<OP::ImageRasterMeshArgs>(at);
            f(args.mesh );
            f(args.image );
            f(args.mask );
            f(args.angleFadeProperties );
            f(args.projector );
            break;
        }

        case EOpType::IM_MAKEGROWMAP:
        {
			OP::ImageMakeGrowMapArgs args = program.GetOpArgs<OP::ImageMakeGrowMapArgs>(at);
            f(args.mask );
            break;
        }

        case EOpType::IM_DISPLACE:
        {
			OP::ImageDisplaceArgs Args = program.GetOpArgs<OP::ImageDisplaceArgs>(at);
            f(Args.Source );
            f(Args.DisplacementMap );
            break;
        }

		case EOpType::IM_INVERT:
		{
			OP::ImageInvertArgs args = program.GetOpArgs<OP::ImageInvertArgs>(at);
			f(args.Base);
			break;
		}

		case EOpType::IM_TRANSFORM:
		{
			OP::ImageTransformArgs Args = program.GetOpArgs<OP::ImageTransformArgs>(at);
			f(Args.Base);
			f(Args.OffsetX);
			f(Args.OffsetY);
			f(Args.ScaleX);
			f(Args.ScaleY);
			f(Args.Rotation);
			break;
		}

        //-------------------------------------------------------------------------------------
        case EOpType::ME_APPLYLAYOUT:
        {
			OP::MeshApplyLayoutArgs args = program.GetOpArgs<OP::MeshApplyLayoutArgs>(at);
            f(args.Layout );
            f(args.Mesh );
            break;
        }

		case EOpType::ME_PREPARELAYOUT:
		{
			OP::MeshPrepareLayoutArgs Args = program.GetOpArgs<OP::MeshPrepareLayoutArgs>(at);
			f(Args.Layout);
			f(Args.Mesh);
			break;
		}

        case EOpType::ME_DIFFERENCE:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(BaseAt);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(TargetAt);
			break;
        }

        case EOpType::ME_MORPH:
        {
			const uint8_t* data = program.GetOpArgsPointer(at);

			OP::ADDRESS FactorAt = 0;
			FMemory::Memcpy(&FactorAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(FactorAt);

			OP::ADDRESS BaseAt = 0;
			FMemory::Memcpy(&BaseAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(BaseAt);

			OP::ADDRESS TargetAt = 0;
			FMemory::Memcpy(&TargetAt, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(TargetAt);
			break;
        }

        case EOpType::ME_MERGE:
        {
			OP::MeshMergeArgs args = program.GetOpArgs<OP::MeshMergeArgs>(at);
            f(args.Base );
            f(args.Added );
            break;
        }

		case EOpType::ME_MASKCLIPMESH:
		{
			OP::MeshMaskClipMeshArgs args = program.GetOpArgs<OP::MeshMaskClipMeshArgs>(at);
			f(args.source);
			f(args.clip);
			break;
		}

		case EOpType::ME_MASKCLIPUVMASK:
		{
			OP::MeshMaskClipUVMaskArgs Args = program.GetOpArgs<OP::MeshMaskClipUVMaskArgs>(at);
			f(Args.Source);
			f(Args.UVSource);
			f(Args.MaskImage);
			f(Args.MaskLayout);
			break;
		}

        case EOpType::ME_MASKDIFF:
        {
			OP::MeshMaskDiffArgs args = program.GetOpArgs<OP::MeshMaskDiffArgs>(at);
            f(args.Source );
            f(args.Fragment );
            break;
        }

        case EOpType::ME_REMOVEMASK:
        {
            const uint8* data = program.GetOpArgsPointer(at);
            mu::OP::ADDRESS source;
            FMemory::Memcpy( &source, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
            f(source);

			EFaceCullStrategy FaceCullStrategy;
			FMemory::Memcpy(&FaceCullStrategy, data, sizeof(EFaceCullStrategy));
			data += sizeof(EFaceCullStrategy);

            uint16 removes = 0;
			FMemory::Memcpy( &removes, data, sizeof(uint16) ); data+=sizeof(uint16);
            for (uint16 r=0; r<removes; ++r)
            {
                mu::OP::ADDRESS condition;
				FMemory::Memcpy( &condition, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(condition);

                mu::OP::ADDRESS mask;
				FMemory::Memcpy( &mask, data, sizeof(OP::ADDRESS) ); data+=sizeof(OP::ADDRESS);
                f(mask);
            }
            break;
        }

		case EOpType::ME_ADDTAGS:
		{
			const uint8_t* data = program.GetOpArgsPointer(at);
			mu::OP::ADDRESS source;
			FMemory::Memcpy(&source, data, sizeof(OP::ADDRESS)); data += sizeof(OP::ADDRESS);
			f(source);
			break;
		}

        case EOpType::ME_FORMAT:
        {
			OP::MeshFormatArgs args = program.GetOpArgs<OP::MeshFormatArgs>(at);
            f(args.source );
            f(args.format );
            break;
        }

        case EOpType::ME_TRANSFORM:
        {
			OP::MeshTransformArgs args = program.GetOpArgs<OP::MeshTransformArgs>(at);
            f(args.source );
            break;
        }

        case EOpType::ME_EXTRACTLAYOUTBLOCK:
        {
            const uint8_t* data = program.GetOpArgsPointer(at);
            mu::OP::ADDRESS source;
			FMemory::Memcpy( &source, data, sizeof(OP::ADDRESS) );
            f(source);
            break;
        }

        case EOpType::ME_CLIPMORPHPLANE:
        {
			OP::MeshClipMorphPlaneArgs args = program.GetOpArgs<OP::MeshClipMorphPlaneArgs>(at);
            f(args.Source);
            break;
        }

        case EOpType::ME_CLIPWITHMESH :
        {
			OP::MeshClipWithMeshArgs Args = program.GetOpArgs<OP::MeshClipWithMeshArgs>(at);
            f(Args.Source);
            f(Args.ClipMesh);
            break;
        }

        case EOpType::ME_CLIPDEFORM:
        {
			OP::MeshClipDeformArgs args = program.GetOpArgs<OP::MeshClipDeformArgs>(at);
            f(args.mesh);
            f(args.clipShape);
            break;
        }
       
		case EOpType::ME_MORPHRESHAPE:
		{
			OP::MeshMorphReshapeArgs Args = program.GetOpArgs<OP::MeshMorphReshapeArgs>(at);
			f(Args.Morph);
			f(Args.Reshape);

			break;
		}
 
        case EOpType::ME_SETSKELETON :
        {
			OP::MeshSetSkeletonArgs Args = program.GetOpArgs<OP::MeshSetSkeletonArgs>(at);
            f(Args.Source);
            f(Args.Skeleton);
            break;
        }

        case EOpType::ME_PROJECT :
        {
			OP::MeshProjectArgs Args = program.GetOpArgs<OP::MeshProjectArgs>(at);
            f(Args.Mesh);
            f(Args.Projector);
            break;
        }

		case EOpType::ME_APPLYPOSE:
		{
			OP::MeshApplyPoseArgs Args = program.GetOpArgs<OP::MeshApplyPoseArgs>(at);
			f(Args.base);
			f(Args.pose);
			break;
		}

		case EOpType::ME_BINDSHAPE:
		{
			OP::MeshBindShapeArgs args = program.GetOpArgs<OP::MeshBindShapeArgs>(at);
			f(args.mesh);
			f(args.shape);
			break;
		}

		case EOpType::ME_APPLYSHAPE:
		{
			OP::MeshApplyShapeArgs args = program.GetOpArgs<OP::MeshApplyShapeArgs>(at);
			f(args.mesh);
			f(args.shape);
			break;
		}

		case EOpType::ME_OPTIMIZESKINNING:
		{
			OP::MeshOptimizeSkinningArgs args = program.GetOpArgs<OP::MeshOptimizeSkinningArgs>(at);
			f(args.source);
			break;
		}

        case EOpType::ME_TRANSFORMWITHMESH :
        {
        	OP::MeshTransformWithinMeshArgs args = program.GetOpArgs<OP::MeshTransformWithinMeshArgs>(at);
        	f(args.sourceMesh);
        	f(args.boundingMesh);
        	f(args.matrix);
        	break;
        }
        	
        //-------------------------------------------------------------------------------------
        case EOpType::IN_ADDMESH:
        case EOpType::IN_ADDIMAGE:
        case EOpType::IN_ADDVECTOR:
        case EOpType::IN_ADDSCALAR:
        case EOpType::IN_ADDSTRING:
        case EOpType::IN_ADDCOMPONENT:
        case EOpType::IN_ADDSURFACE:
        {
			OP::InstanceAddArgs args = program.GetOpArgs<OP::InstanceAddArgs>(at);
            f(args.instance );
            f(args.value );
            break;
        }

        case EOpType::IN_ADDLOD:
        {
			const uint8* Data = program.GetOpArgsPointer(at);

			uint8 LODCount;
			FMemory::Memcpy(&LODCount, Data, sizeof(uint8));
			Data += sizeof(uint8);

			for (int8 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
			{
				OP::ADDRESS LODAddress;
				FMemory::Memcpy(&LODAddress, Data, sizeof(OP::ADDRESS));
				Data += sizeof(OP::ADDRESS);

				f(LODAddress);
            }
            break;
        }

		case EOpType::IN_ADDEXTENSIONDATA:
		{
			const OP::InstanceAddExtensionDataArgs Args = program.GetOpArgs<OP::InstanceAddExtensionDataArgs>(at);

			f(Args.Instance);
			f(Args.ExtensionData);

			break;
		}

		case EOpType::IN_ADDOVERLAYMATERIAL:
		{
			const OP::InstanceAddOverlayMaterialArgs Args = program.GetOpArgs<OP::InstanceAddOverlayMaterialArgs>(at);

			f(Args.Instance);
			f(Args.OverlayMaterialId);

			break;
		}

        //-------------------------------------------------------------------------------------
        case EOpType::LA_PACK:
        {
			OP::LayoutPackArgs args = program.GetOpArgs<OP::LayoutPackArgs>(at);
            f(args.Source );
            break;
        }

        case EOpType::LA_MERGE:
        {
			OP::LayoutMergeArgs args = program.GetOpArgs<OP::LayoutMergeArgs>(at);
            f(args.Base );
            f(args.Added );
            break;
        }

		case EOpType::LA_REMOVEBLOCKS:
		{
			OP::LayoutRemoveBlocksArgs args = program.GetOpArgs<OP::LayoutRemoveBlocksArgs>(at);
			f(args.Source);
			f(args.ReferenceLayout);
			break;
		}

		case EOpType::LA_FROMMESH:
		{
			OP::LayoutFromMeshArgs args = program.GetOpArgs<OP::LayoutFromMeshArgs>(at);
			f(args.Mesh);
			break;
		}

        default:
			check( false );
            break;
        }

    }


}

