// Copyright Epic Games, Inc. All Rights Reserved.

#include "astc_thunk.h"

#include "astcenc.h"

#include <new>

static AstcThunk_MallocFnType* ThunkMalloc = nullptr;
static AstcThunk_FreeFnType* ThunkFree = nullptr;

struct FAstcEncThunk_ContextInternal
{
	astcenc_context* Context;
	astcenc_image Image;
	astcenc_swizzle Swizzle;
	astcenc_config Config;

	FAstcEncThunk_CreateParams CreateParams;
};


#if !defined(_MSC_VER)
#	if __has_feature(cxx_noexcept)
#		define OPERATOR_NEW_THROW_SPEC
#	else
#		define OPERATOR_NEW_THROW_SPEC		throw (std::bad_alloc)
#	endif
#else
#	define OPERATOR_NEW_THROW_SPEC
#endif

#define OPERATOR_DELETE_THROW_SPEC		noexcept
#define OPERATOR_NEW_NOTHROW_SPEC		noexcept
#define OPERATOR_DELETE_NOTHROW_SPEC	noexcept

// Allocator overriding so astcenc routes through our allocators. This was all cribbed from ModuleBoilerplate.h
	void* operator new  ( size_t Size                                                    ) OPERATOR_NEW_THROW_SPEC      { return ThunkMalloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	void* operator new[]( size_t Size                                                    ) OPERATOR_NEW_THROW_SPEC      { return ThunkMalloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	void* operator new  ( size_t Size,                             const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return ThunkMalloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	void* operator new[]( size_t Size,                             const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return ThunkMalloc( Size ? Size : 1, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); } \
	void* operator new  ( size_t Size, std::align_val_t Alignment                        ) OPERATOR_NEW_THROW_SPEC      { return ThunkMalloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	void* operator new[]( size_t Size, std::align_val_t Alignment                        ) OPERATOR_NEW_THROW_SPEC      { return ThunkMalloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	void* operator new  ( size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return ThunkMalloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	void* operator new[]( size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return ThunkMalloc( Size ? Size : 1, (std::size_t)Alignment ); } \
	void operator delete  ( void* Ptr                                                                             ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr                                                                             ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,                                                      const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,                                                      const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size                                                    ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size                                                    ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size,                             const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size,                             const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,                          std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,                          std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,                          std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,                          std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size, std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size, std::align_val_t Alignment                        ) OPERATOR_DELETE_THROW_SPEC   { ThunkFree( Ptr ); } \
	void operator delete  ( void* Ptr,             size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); } \
	void operator delete[]( void* Ptr,             size_t Size, std::align_val_t Alignment, const std::nothrow_t& ) OPERATOR_DELETE_NOTHROW_SPEC { ThunkFree( Ptr ); }

EXTERNC void AstcEncThunk_SetAllocators(AstcThunk_MallocFnType* MallocFn, AstcThunk_FreeFnType* FreeFn)
{
	ThunkMalloc = MallocFn;
	ThunkFree = FreeFn;
}

EXTERNC char const* AstcEncThunk_Create(const FAstcEncThunk_CreateParams& CreateParams, AstcEncThunk_Context* OutContext)
{
	if (!ThunkMalloc)
	{
		return "No ASTC thunk allocator installed!";
	}

	FAstcEncThunk_ContextInternal* Internal = new FAstcEncThunk_ContextInternal;

	*OutContext = (AstcEncThunk_Context)Internal;

	if (!Internal)
	{
		return "Failed to allocate ASTC thunk context!";
	}

	Internal->CreateParams = CreateParams;
	Internal->Context = nullptr;

	uint32_t EncFlags = 0;
	if (CreateParams.Flags & EAstcEncThunk_Flags::NORMAL_MAP)
	{
		EncFlags |= ASTCENC_FLG_MAP_NORMAL;
	}

	if (CreateParams.Flags & EAstcEncThunk_Flags::DECOMPRESS_ONLY)
	{
		EncFlags |= ASTCENC_FLG_DECOMPRESS_ONLY;
	}

#if ASTC_SUPPORTS_RDO
	if (CreateParams.Profile != EAstcEncThunk_Profile::HDR_RGB_LDR_A &&
		(CreateParams.Flags & EAstcEncThunk_Flags::LZ_RDO))
	{
		EncFlags |= ASTCENC_FLG_USE_LZ_RDO;
	}
#endif

	astcenc_error EncStatus = astcenc_config_init(
		(astcenc_profile)CreateParams.Profile,
		CreateParams.BlockSize,
		CreateParams.BlockSize,
		1, // Always 2D blocks.
		(float)CreateParams.Quality,
		EncFlags,
		&Internal->Config);
	if (EncStatus != ASTCENC_SUCCESS)
	{
		return astcenc_get_error_string(EncStatus);
	}

	// Set up the input image data.
	Internal->Image.dim_x = CreateParams.SizeX;
	Internal->Image.dim_y = CreateParams.SizeY;
	Internal->Image.dim_z = CreateParams.NumSlices;
	Internal->Image.data = CreateParams.ImageSlices;
	Internal->Image.data_type = (astcenc_type)CreateParams.ImageDataType;

	// Set up the encode swizzle
	Internal->Swizzle.r = (astcenc_swz)CreateParams.SwizzleR;
	Internal->Swizzle.g = (astcenc_swz)CreateParams.SwizzleG;
	Internal->Swizzle.b = (astcenc_swz)CreateParams.SwizzleB;
	Internal->Swizzle.a = (astcenc_swz)CreateParams.SwizzleA;
	
	
	if (CreateParams.bDbLimitGreaterThan60 &&
		Internal->Config.tune_db_limit < 60.0f)
	{
		Internal->Config.tune_db_limit = 60.0f;
	}

	Internal->Config.cw_r_weight = CreateParams.ErrorWeightR;
	Internal->Config.cw_g_weight = CreateParams.ErrorWeightG;
	Internal->Config.cw_b_weight = CreateParams.ErrorWeightB;
	Internal->Config.cw_a_weight = CreateParams.ErrorWeightA;

#if ASTC_SUPPORTS_RDO
	Internal->Config.lz_rdo_lambda = CreateParams.LZRdoLambda;
#endif

	EncStatus = astcenc_context_alloc(&Internal->Config, CreateParams.TaskCount, &Internal->Context);
	if (EncStatus != ASTCENC_SUCCESS)
	{
		return astcenc_get_error_string(EncStatus);
	}

	return nullptr;
}

// Returns the underlying string representation of the ASTC error on failure, or nullptr on success.
EXTERNC char const* AstcEncThunk_DoWork(AstcEncThunk_Context Context, uint32_t TaskIndex)
{
	FAstcEncThunk_ContextInternal* Internal = (FAstcEncThunk_ContextInternal*)Context;

	astcenc_error EncStatus = ASTCENC_SUCCESS;
	
	if (Internal->CreateParams.Flags & EAstcEncThunk_Flags::DECOMPRESS_ONLY)
	{
		EncStatus = astcenc_decompress_image(
			Internal->Context,
			Internal->CreateParams.OutputImageBuffer,
			Internal->CreateParams.OutputImageBufferSize,
			&Internal->Image,
			&Internal->Swizzle,
			TaskIndex);
	}
	else
	{
		EncStatus = astcenc_compress_image(
			Internal->Context,
			&Internal->Image,
			&Internal->Swizzle,
			Internal->CreateParams.OutputImageBuffer,
			Internal->CreateParams.OutputImageBufferSize,
			TaskIndex);
	}
	if (EncStatus != ASTCENC_SUCCESS)
	{
		return astcenc_get_error_string(EncStatus);
	}
	return nullptr;
}

// Frees the context created by AstcEncThunk_Create. Valid (nop) to pass nullptr.
EXTERNC void AstcEncThunk_Destroy(AstcEncThunk_Context Context)
{
	if (!Context)
	{
		return;
	}

	FAstcEncThunk_ContextInternal* Internal = (FAstcEncThunk_ContextInternal*)Context;
	if (Internal->Context)
	{
		astcenc_context_free(Internal->Context);
		Internal->Context = nullptr;
	}

	delete Internal;
}