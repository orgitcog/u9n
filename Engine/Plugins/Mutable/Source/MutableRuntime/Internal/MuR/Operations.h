// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Types.h"
#include "MuR/SerialisationPrivate.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Function.h"

#define MUTABLE_OP_MAX_INTERPOLATE_COUNT	6
#define MUTABLE_OP_MAX_SWIZZLE_CHANNELS		4

namespace mu
{

    //!
    enum class EOpType : uint16
    {
        /** No operation. */
        NONE,

        //-----------------------------------------------------------------------------------------
        // Generic operations
        //-----------------------------------------------------------------------------------------

        //! Constant value
        BO_CONSTANT,
        NU_CONSTANT,
        SC_CONSTANT,
        CO_CONSTANT,
        IM_CONSTANT,
        ME_CONSTANT,
        LA_CONSTANT,
        PR_CONSTANT,
        ST_CONSTANT,
		ED_CONSTANT,
    	MA_CONSTANT,

        //! User parameter
        BO_PARAMETER,
        NU_PARAMETER,
        SC_PARAMETER,
        CO_PARAMETER,
        PR_PARAMETER,
		IM_PARAMETER,
		ME_PARAMETER,
		ST_PARAMETER,
    	MA_PARAMETER,

		//! A referenced, but opaque engine resource
		IM_REFERENCE,
		ME_REFERENCE,

        //! Select one value or the other depending on a boolean input
        NU_CONDITIONAL,
        SC_CONDITIONAL,
        CO_CONDITIONAL,
        IM_CONDITIONAL,
        ME_CONDITIONAL,
        LA_CONDITIONAL,
        IN_CONDITIONAL,
		ED_CONDITIONAL,

        //! Select one of several values depending on an int input
        NU_SWITCH,
        SC_SWITCH,
        CO_SWITCH,
        IM_SWITCH,
        ME_SWITCH,
        LA_SWITCH,
        IN_SWITCH,
		ED_SWITCH,

        //-----------------------------------------------------------------------------------------
        // Boolean operations
        //-----------------------------------------------------------------------------------------

        //! Compare an integerexpression with an integer constant
        BO_EQUAL_INT_CONST,

        //! Logical and
        BO_AND,

        //! Logical or
        BO_OR,

        //! Left as an exercise to the reader to find out what this op does.
        BO_NOT,

        //-----------------------------------------------------------------------------------------
        // Scalar operations
        //-----------------------------------------------------------------------------------------

        //! Apply an arithmetic operation to two scalars
        SC_ARITHMETIC,

        //! Get a scalar value from a curve
        SC_CURVE,

        //-----------------------------------------------------------------------------------------
        // Colour operations. Colours are sometimes used as generic vectors.
        //-----------------------------------------------------------------------------------------

        //! Sample an image to get its colour.
        CO_SAMPLEIMAGE,

        //! Make a color by shuffling channels from other colours.
        CO_SWIZZLE,

        //! Compose a vector from 4 scalars
        CO_FROMSCALARS,

        //! Apply component-wise arithmetic operations to two colours
        CO_ARITHMETIC,

        //-----------------------------------------------------------------------------------------
        // Image operations
        //-----------------------------------------------------------------------------------------

        //! Combine an image on top of another one using a specific effect (Blend, SoftLight, 
		//! Hardlight, Burn...). And optionally a mask.
        IM_LAYER,

        //! Apply a colour on top of an image using a specific effect (Blend, SoftLight, 
		//! Hardlight, Burn...), optionally using a mask.
        IM_LAYERCOLOUR,        

        //! Convert between pixel formats
        IM_PIXELFORMAT,

        //! Generate mipmaps up to a provided level
        IM_MIPMAP,

        //! Resize the image to a constant size
        IM_RESIZE,

        //! Resize the image to the size of another image
        IM_RESIZELIKE,

        //! Resize the image by a relative factor
        IM_RESIZEREL,

        //! Create an empty image to hold a particular layout.
        IM_BLANKLAYOUT,

        //! Copy an image into a rect of another one.
        IM_COMPOSE,

        //! Interpolate between 2 images taken from a row of targets (2 consecutive targets).
        IM_INTERPOLATE,

        //! Change the saturation of the image.
        IM_SATURATE,

        //! Generate a one-channel image with the luminance of the source image.
        IM_LUMINANCE,

        //! Recombine the channels of several images into one.
        IM_SWIZZLE,

        //! Convert the source image colours using a "palette" image sampled with the source
        //! grey-level.
        IM_COLOURMAP,

        //! Generate a black and white image from an image and a threshold.
        IM_BINARISE,

        //! Generate a plain colour image
        IM_PLAINCOLOUR,

        //! Cut a rect from an image
        IM_CROP,

        //! Replace a subrect of an image with another one
        IM_PATCH,

        //! Render a mesh texture layout into a mask
        IM_RASTERMESH,

        //! Create an image displacement encoding the grow operation for a mask
        IM_MAKEGROWMAP,

        //! Apply an image displacement on another image.
        IM_DISPLACE,

        //! Repeately apply
        IM_MULTILAYER,

        //! Inverts the colors of an image
        IM_INVERT,

        //! Modifiy roughness channel of an image based on normal variance.
        IM_NORMALCOMPOSITE,

		//! Apply linear transform to Image content. Resulting samples outside the original image are tiled.
		IM_TRANSFORM,

        //-----------------------------------------------------------------------------------------
        // Mesh operations
        //-----------------------------------------------------------------------------------------

        //! Apply a layout to a mesh texture coordinates channel
        ME_APPLYLAYOUT,

		/** */
		ME_PREPARELAYOUT,

        //! Compare two meshes and extract a morph from the first to the second
        //! The meshes must have the same topology, etc.
        ME_DIFFERENCE,

        //! Apply a one morphs on a base. 
        ME_MORPH,

        //! Merge a mesh to a mesh
        ME_MERGE,

        //! Create a new mask mesh selecting all the faces of a source that are inside a given
        //! clip mesh.
        ME_MASKCLIPMESH,

        /** Create a new mask mesh selecting the faces of a source that have UVs inside the region marked in an image mask. */
		ME_MASKCLIPUVMASK,

        //! Create a new mask mesh selecting all the faces of a source that match another mesh.
        ME_MASKDIFF,

        //! Remove all the geometry selected by a mask.
        ME_REMOVEMASK,

        //! Change the mesh format to match the format of another one.
        ME_FORMAT,

        //! Extract a fragment of a mesh containing specific layout blocks.
        ME_EXTRACTLAYOUTBLOCK,

        //! Apply a transform in a 4x4 matrix to the geometry channels of the mesh
        ME_TRANSFORM,

        //! Clip the mesh with a plane and morph it when it is near until it becomes an ellipse on
        //! the plane.
        ME_CLIPMORPHPLANE,

        //! Clip the mesh with another mesh.
        ME_CLIPWITHMESH,

        //! Replace the skeleton data from a mesh with another one.
        ME_SETSKELETON,

        //! Project a mesh using a projector and clipping the irrelevant faces
        ME_PROJECT,

        //! Deform a skinned mesh applying a skeletal pose
        ME_APPLYPOSE,

		//! Calculate the binding of a mesh on a shape
		ME_BINDSHAPE,

		//! Apply a shape on a (previously bound) mesh
		ME_APPLYSHAPE,

		//! Clip Deform using bind data.
		ME_CLIPDEFORM,
	
        //! Mesh morph with Skeleton Reshape based on the morphed mesh.
        ME_MORPHRESHAPE,

		//! Optimize skinning before adding a mesh to the component
		ME_OPTIMIZESKINNING,

		//! Add a set of tags to a mesh
		ME_ADDTAGS,

    	//! Transform with a 4x4 matrix the geometry channels of a mesh that are bounded by another mesh
    	ME_TRANSFORMWITHMESH,

        //-----------------------------------------------------------------------------------------
        // Instance operations
        //-----------------------------------------------------------------------------------------

        //! Add a mesh to an instance
        IN_ADDMESH,

        //! Add an image to an instance
        IN_ADDIMAGE,

        //! Add a vector to an instance
        IN_ADDVECTOR,

        //! Add a scalar to an instance
        IN_ADDSCALAR,

        //! Add a string to an instance
        IN_ADDSTRING,

        //! Add a surface to an instance component
        IN_ADDSURFACE,

        //! Add a component to an instance LOD
        IN_ADDCOMPONENT,

        //! Add all LODs to an instance. This operation can only appear once in a model.
        IN_ADDLOD,

		//! Add extension data to an instance
		IN_ADDEXTENSIONDATA,

		//! Add overlay material to an instance
		IN_ADDOVERLAYMATERIAL,

        //-----------------------------------------------------------------------------------------
        // Layout operations
        //-----------------------------------------------------------------------------------------

        //! Pack all the layout blocks from the source in the grid without overlapping
        LA_PACK,

        //! Merge two layouts
        LA_MERGE,

        //! Remove all layout blocks not used by any vertex of the mesh.
        //! This operation is for the new way of managing layout blocks.
        LA_REMOVEBLOCKS,

		//! Extract a layout from a mesh
		LA_FROMMESH,

        //-----------------------------------------------------------------------------------------
        // Utility values
        //-----------------------------------------------------------------------------------------

        //!
        COUNT

    };

	enum class EMeshBindShapeFlags : uint32
	{
		None				   = 0,
		ReshapeSkeleton		   = 1 << 0,
		EnableRigidParts       = 1 << 2,
		ReshapePhysicsVolumes  = 1 << 4,
		ReshapeVertices		   = 1 << 5,
		ApplyLaplacian		   = 1 << 6,
		RecomputeNormals	   = 1 << 7,
	};
	ENUM_CLASS_FLAGS(EMeshBindShapeFlags);

	enum class EMeshBindColorChannelUsage : uint8
	{
		None       = 0,
		ClusterId  = 1,
		MaskWeight = 2,
	};

	struct FMeshBindColorChannelUsages
	{
		EMeshBindColorChannelUsage R;
		EMeshBindColorChannelUsage G;
		EMeshBindColorChannelUsage B;
		EMeshBindColorChannelUsage A;
	};

	static_assert(sizeof(FMeshBindColorChannelUsages) == sizeof(uint32));


    /** */
    template<typename ADDRESS_TYPE>
    struct TOperation
    {
        typedef ADDRESS_TYPE ADDRESS;

        /** Arguments for every operation type. */
        struct BoolConstantArgs
        {
            bool bValue;
        };

        struct IntConstantArgs
        {
            int32 Value;
        };

        struct ScalarConstantArgs
        {
            float Value;
        };

        struct ColorConstantArgs
        {
            FVector4f Value;
        };

    	struct MatrixConstantArgs
    	{
    		ADDRESS value;
    	};

        struct ResourceConstantArgs
        {
            ADDRESS value;
        };

        struct MeshConstantArgs
        {
            // Index of the mesh in the mesh constant array
            ADDRESS Value;

            // If not negative, index of the skeleton to set to the mesh from the skeleton
            // constant array.
            int32 Skeleton;

            // If not negative, index of the physics body to set to the mesh from the physics body
            // constant array.
			int32 PhysicsBody;
        };

		struct ParameterArgs
		{
			ADDRESS variable;
		};

		struct MeshParameterArgs
		{
			ADDRESS variable;
			uint8 LOD = 0;
			uint8 Section = 0;
			uint32 MeshID = 0;
		};

        struct ConditionalArgs
        {
            ADDRESS condition, yes, no;
        };

		struct ResourceReferenceArgs
		{
			FImageDesc ImageDesc;
			int32 ID;
			int8 ForceLoad;
		};

        //-------------------------------------------------------------------------------------
        struct BoolEqualScalarConstArgs
        {
            ADDRESS Value;
            int32 Constant;
        };

        struct BoolBinaryArgs
        {
            ADDRESS A,B;
        };

        struct BoolNotArgs
        {
            ADDRESS A;
        };

        struct ScalarCurveArgs
        {
            ADDRESS time;   // Operation generating the time value to sample the curve
            ADDRESS curve;  // Constant curve (not an op)
        };

        struct ColourSampleImageArgs
        {
            ADDRESS Image;
            ADDRESS X,Y;
            uint8 Filter;
        };

        struct ColourSwizzleArgs
        {
            uint8 sourceChannels[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
            ADDRESS sources[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
        };

		struct ColourFromScalarsArgs
		{
			ADDRESS V[MUTABLE_OP_MAX_SWIZZLE_CHANNELS];
		};

        struct ArithmeticArgs
		{
			typedef enum
			{
				NONE,
				ADD,
				SUBTRACT,
				MULTIPLY,
				DIVIDE
			} OPERATION;
            uint8 Operation;

			ADDRESS A, B;
		};


        //-------------------------------------------------------------------------------------
        struct ImageLayerArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS blended;
			uint8 blendType;		// One of EBlendType

			/** One of EBlendType. If this is different than NONE, it will be applied to the alpha with the channel BlendAlphaSourceChannel of the blended. */
			uint8 blendTypeAlpha;

			/** Channel to use from the source blended argument to apply blendTypeAlpha, if any. */
			uint8 BlendAlphaSourceChannel;

			uint8 flags;
			typedef enum
            {
                F_NONE           = 0,
                /** The mask is considered binary : 0 means 0% and any other value means 100% */
                F_BINARY_MASK    = 1 << 0,
				/** If the image has 4 channels, apply to the fourth channel as well. */
				F_APPLY_TO_ALPHA = 1 << 1,
				/** Use the alpha channel of the blended image as mask. Mask should be null.*/
				F_USE_MASK_FROM_BLENDED = 1 << 2,
				/** Use the alpha channel of the base image as its RGB.*/
				F_BASE_RGB_FROM_ALPHA = 1 << 3,
				/** Use the alpha channel of the blended image as its RGB.*/
				F_BLENDED_RGB_FROM_ALPHA = 1 << 4,
			} FLAGS;
        };

        struct ImageMultiLayerArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS blended;
            ADDRESS rangeSize;
            uint16 rangeId;
			uint8 blendType;		// One of EBlendType
			
			/** One of EBlendType. If this is different than NONE, it will be applied to the alpha with the channel BlendAlphaSourceChannel of the blended. */
			uint8 blendTypeAlpha;

			/** Channel to use from the source color argument to apply blendTypeAlpha, if any. */
			uint8 BlendAlphaSourceChannel;

			uint8 bUseMaskFromBlended;	
		};

        struct ImageLayerColourArgs
        {
            ADDRESS base;
            ADDRESS mask;
            ADDRESS colour;
			uint8 blendType;		// One of EBlendType

			/** One of EBlendType. If this is different than NONE, it will be applied to the alpha but with the channel BlendAlphaSourceChannel of the color. */
			uint8 blendTypeAlpha;	

			/** Channel to use from the source color argument to apply blendTypeAlpha, if any. */
			uint8 BlendAlphaSourceChannel;

			/** Like in ImageLayerArgs. */
			uint8 flags;
		};

        struct ImagePixelFormatArgs
        {
            ADDRESS source;
			EImageFormat format;
			EImageFormat formatIfAlpha;
        };

        struct ImageMipmapArgs
        {
            ADDRESS source;

            //! Number of mipmaps to build. If zero, it means all.
            uint8 levels;

            //! Number of mipmaps that can be generated for a single layout block.
            uint8 blockLevels;

            //! This is true if this operation is supposed to build only the tail mipmaps.
            //! It is used during the code optimisation phase, and to validate the code.
            bool onlyTail;

            //! Mipmap generation settings. 
            EMipmapFilterType FilterType;
            EAddressMode AddressMode;
        };

        struct ImageResizeArgs
        {
            ADDRESS Source;
            uint16 Size[2];
        };

        struct ImageResizeLikeArgs
        {
            //! Image that will be resized
            ADDRESS Source;

            //! Image whose size will be used to resize the source.
            ADDRESS SizeSource;
        };

        struct ImageResizeVarArgs
        {
            //! Image that will be resized
            ADDRESS source;

            //! Size expression.
            ADDRESS size;
        };

        struct ImageResizeRelArgs
        {
            //! Image that will be resized
            ADDRESS Source;

            //! Factor for each axis.
            float Factor[2];
        };

        struct ImageBlankLayoutArgs
        {
            ADDRESS Layout;

            /** Size of a layout block in pixels. */
            uint16 BlockSize[2];
			EImageFormat Format;

            /** If true, generate mipmaps. */
            uint8 GenerateMipmaps;

            /** Mipmaps to generate if mipmaps are to be generated. 0 means all. */
            uint8 MipmapCount;
        };

        struct ImageComposeArgs
        {
            ADDRESS layout, base, blockImage;
            ADDRESS mask;
            uint64 BlockId;
        };

        struct ImageInterpolateArgs
        {
            ADDRESS Factor;
            ADDRESS Targets[ MUTABLE_OP_MAX_INTERPOLATE_COUNT ];
        };

        struct ImageSaturateArgs
        {
            /** Image to modify. */
            ADDRESS Base;

            /** Saturation factor : 0 desaturates, 1 leaves the same, >1 saturates */
            ADDRESS Factor;
        };

        struct ImageLuminanceArgs
        {
            //! Image to modify.
            ADDRESS Base;
        };

        struct ImageSwizzleArgs
        {
			EImageFormat format;
            uint8 sourceChannels[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
            ADDRESS sources[ MUTABLE_OP_MAX_SWIZZLE_CHANNELS ];
        };

        struct ImageColourMapArgs
        {
            ADDRESS Base;
            ADDRESS Mask;
            ADDRESS Map;
        };

        struct ImageBinariseArgs
        {
            ADDRESS Base;
            ADDRESS Threshold;
        };

        struct ImagePlainColorArgs
        {
            ADDRESS Color;
			EImageFormat Format;
            uint16 Size[2];

			/** Number of mipmaps to generate. 0 means all the chain. */
			uint8 LODs;
        };

        struct ImageCropArgs
        {
            ADDRESS source;
            uint16 minX, minY, sizeX, sizeY;
        };

        struct ImagePatchArgs
        {
            ADDRESS base;
            ADDRESS patch;
            uint16 minX, minY;
        };

        struct ImageRasterMeshArgs
        {
			uint64 BlockId;
			
			ADDRESS mesh;

			//! These are used in case of projected mesh raster.
			ADDRESS image;
			ADDRESS angleFadeProperties;

			//! Mask selecting the pixels in the destination image that may receive projection.
			ADDRESS mask;

			//! A projector may be needed for some kind of per-pixel raster operations
			//! like cylindrical projections.
			ADDRESS projector;
			
			uint16 sizeX, sizeY;
			uint16 SourceSizeX, SourceSizeY;
			uint16 CropMinX, CropMinY;
			uint16 UncroppedSizeX, UncroppedSizeY;
			uint8 bIsRGBFadingEnabled : 1;
			uint8 bIsAlphaFadingEnabled : 1;
			
			// Currently only 2 sampling methods are contemplated, but reserve 3 bits for future uses. 
			uint8 SamplingMethod : 3;
			// Currently only 2 min filter methods are contemplated, but reserve 3 bits for future uses. 
			uint8 MinFilterMethod : 3;

			uint8 LayoutIndex;
        };

        struct ImageMakeGrowMapArgs
        {
            ADDRESS mask;
            int32 border;
        };

        struct ImageDisplaceArgs
        {
            ADDRESS Source;
            ADDRESS DisplacementMap;
        };

		struct ImageInvertArgs
		{
			ADDRESS Base;
		};

        struct ImageNormalCompositeArgs
        {
            ADDRESS base;
            ADDRESS normal;

            float power;
            ECompositeImageMode mode;
        };

		struct ImageTransformArgs
		{
			ADDRESS Base = 0;
			ADDRESS OffsetX = 0;
			ADDRESS OffsetY = 0;
			ADDRESS ScaleX = 0;
			ADDRESS ScaleY = 0;
			ADDRESS Rotation = 0;

			uint32 AddressMode      : 31;
			uint32 bKeepAspectRatio : 1;

            /** Size of the image to create. If 0, reuse size from base.*/
            uint16 SizeX = 0;
            uint16 SizeY = 0;

			uint16 SourceSizeX = 0;
			uint16 SourceSizeY = 0;
		};

        //-------------------------------------------------------------------------------------
        struct MeshApplyLayoutArgs
        {
            ADDRESS Mesh;
            ADDRESS Layout;
            uint16 Channel;
        };

		struct MeshPrepareLayoutArgs
		{
			ADDRESS Mesh;
			ADDRESS Layout;
			uint8 LayoutChannel = 0;
			uint8 bUseAbsoluteBlockIds : 1;
			uint8 bNormalizeUVs : 1;
			uint8 bClampUVIslands : 1;
			uint8 bEnsureAllVerticesHaveLayoutBlock : 1;
		};
		
        struct MeshMergeArgs
        {
            ADDRESS Base;
            ADDRESS Added;

            // If 0, it merges the surfaces, otherwise, add a new surface for the added mesh.
            uint32 NewSurfaceID;
        };

		struct MeshMaskClipMeshArgs
		{
			ADDRESS source;
			ADDRESS clip;
		};

		struct MeshMaskClipUVMaskArgs
		{
			ADDRESS Source = 0;
			ADDRESS UVSource = 0;
			ADDRESS MaskImage = 0;
			ADDRESS MaskLayout = 0;
			uint8 LayoutIndex = 0;
		};

        struct MeshMaskDiffArgs
        {
            ADDRESS Source;
            ADDRESS Fragment;
        };

        struct MeshFormatArgs
        {
            ADDRESS source;
            ADDRESS format;

            enum
            {
                Vertex				= 1 << 0,
                Index				= 1 << 1,
                // deprecated Face  = 1 << 2,

                /** This flag will not add blank channels for the channels in the format mesh but not in the source mesh. */
				IgnoreMissing		= 1 << 4,

                /** This flag will force the reset of buffer indices to 0. */
                ResetBufferIndices	= 1 << 5,

				/** This flag will add a step to reduce some buffers size by removing components and changing the types if possible. */
				OptimizeBuffers		= 1 << 6
			} EFlags;

            //! EFlags combination, selecting the buffers to reformat and other options.
            uint8 Flags;

        };

		struct MeshTransformArgs
		{
			ADDRESS source;
			ADDRESS matrix;
		};

		struct MeshClipMorphPlaneArgs
		{
			ADDRESS Source;
			ADDRESS MorphShape;
			ADDRESS VertexSelectionShapeOrBone;

			float Dist, Factor, MaxBoneRadius;

			EClipVertexSelectionType VertexSelectionType;

			EFaceCullStrategy FaceCullStrategy;
		};

        struct MeshClipWithMeshArgs
        {
            ADDRESS Source;
            ADDRESS ClipMesh;
        };

        struct MeshSetSkeletonArgs
        {
            ADDRESS Source;
            ADDRESS Skeleton;
        };

        struct MeshProjectArgs
        {
            ADDRESS Mesh;
            ADDRESS Projector;
        };

        struct MeshApplyPoseArgs
        {
            ADDRESS base;
            ADDRESS pose;
        };

		struct MeshGeometryOperationArgs
		{
			ADDRESS meshA;
			ADDRESS meshB;
			ADDRESS scalarA;
			ADDRESS scalarB;
		};

		struct MeshBindShapeArgs
		{
			ADDRESS mesh;
			ADDRESS shape;
			uint32 flags;
			uint32 bindingMethod;
			uint32 ColorUsage;
		};

		struct MeshApplyShapeArgs
		{
			ADDRESS mesh;
			ADDRESS shape;
			uint32 flags;
		};

		struct MeshMorphReshapeArgs
		{
			ADDRESS Morph;
			ADDRESS Reshape;
		};


		struct MeshClipDeformArgs
		{
			ADDRESS mesh;
			ADDRESS clipShape;

			float clipWeightThreshold = 0.9f;
			EFaceCullStrategy FaceCullStrategy;
		};

    	struct MeshTransformWithinMeshArgs
    	{
    		ADDRESS sourceMesh;
    		ADDRESS boundingMesh;
    		ADDRESS matrix;
    	};

		struct MeshOptimizeSkinningArgs
		{
			ADDRESS source;
		};

        struct InstanceAddArgs
        {
            ADDRESS instance;
            ADDRESS value;
            uint32 id;
            uint32 ExternalId;

			// Id used to identify shared surfaces between lods.
			int32 SharedSurfaceId;

            ADDRESS name;

            // Index in the FProgram::ParameterLists with the parameters that are relevant
            // for whatever is added by this operation. This is used only for resources like
            // images or meshes.
            ADDRESS relevantParametersListIndex;
        };

		struct InstanceAddExtensionDataArgs
		{
			// This is a reference to an op that produces the Instance that the FExtensionData will
			// be added to.
			ADDRESS Instance;
			// An op that produces the FExtensionData to add to the Instance
			ADDRESS ExtensionData;
			// The name to associate with the FExtensionData
			//
			// This is an index into the string table
			ADDRESS ExtensionDataName;
		};

		struct InstanceAddOverlayMaterialArgs
		{
			// This is a reference to an op that produces the Instance that the material will
			// be added to.
			ADDRESS Instance;
			ADDRESS OverlayMaterialId;
		};

        struct LayoutPackArgs
        {
            ADDRESS Source;
        };

        struct LayoutMergeArgs
        {
            ADDRESS Base;
            ADDRESS Added;
        };

		struct LayoutRemoveBlocksArgs
		{
			/** Layout to be processedand modified. */
			ADDRESS Source;

			/** Source layout to scan for active blocks. */
			ADDRESS ReferenceLayout;
		};

		struct LayoutFromMeshArgs
		{
			/** Source mesh to retrieve the layout from. */
			ADDRESS Mesh;
			uint8 LayoutIndex;
		};
    };

    typedef TOperation<uint32> OP;


	//! Types of data handled by the Mutable runtime.
	enum class EDataType : uint8
	{
		None,
		Bool,
		Int,
		Scalar,
		Color,
		Image,
		Layout,
		Mesh,
		Instance,
		Projector,
		String,
		ExtensionData,
		Matrix,

		// Supporting data types : Never returned as an actual data type for any operation.
		Shape,
		Curve,
		Skeleton,
		PhysicsAsset,
		
		Count
	};


	// Generic data about a Mutable operation that is needed at runtime.
    struct FOpDesc
    {
		//! Type of data generated by the instruction
		EDataType type;
    };

	MUTABLERUNTIME_API extern const FOpDesc& GetOpDesc( EOpType type );


    //!
    inline static EDataType GetOpDataType( EOpType type )
    {
		return GetOpDesc(type).type;
    }

    //! Utility function to apply a function to all operation references to other operations.
	MUTABLERUNTIME_API extern void ForEachReference( OP& op, const TFunctionRef<void(OP::ADDRESS*)> );

    //! Utility function to apply a function to all operation references to other operations.
	MUTABLERUNTIME_API extern void ForEachReference( const struct FProgram& program, OP::ADDRESS at, const TFunctionRef<void(OP::ADDRESS)> );

	//!
	MUTABLERUNTIME_API inline EOpType GetSwitchForType( EDataType d )
    {
        switch (d)
        {
		case EDataType::Instance: return EOpType::IN_SWITCH;
        case EDataType::Mesh: return EOpType::ME_SWITCH;
		case EDataType::Image: return EOpType::IM_SWITCH;
		case EDataType::Layout: return EOpType::LA_SWITCH;
        case EDataType::Color: return EOpType::CO_SWITCH;
        case EDataType::Scalar: return EOpType::SC_SWITCH;
        case EDataType::Int: return EOpType::NU_SWITCH;
		case EDataType::ExtensionData: return EOpType::ED_SWITCH;

        default:
			check(false);
			break;
        }
        return EOpType::NONE;
    }
}

