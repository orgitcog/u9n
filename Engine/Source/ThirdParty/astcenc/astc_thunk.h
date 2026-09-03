// Copyright Epic Games, Inc. All Rights Reserved.

// This header exports the aspects of astcenc we care about in a version agnostic way, 
// as well as provides a way for us to override the allocators.
#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define EXPORTDLL __declspec(dllexport)
#else
#define EXPORTDLL __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define EXTERNC extern "C"
#endif

// Swizzles
// These must match the numbers in astcenc.h for astcenc_swz
enum class EAstcEncThunk_SwizzleComp : uint8_t
{
	SELECT_R = 0,
	SELECT_G = 1,
	SELECT_B = 2,
	SELECT_A = 3,
	SELECT_0 = 4,
	SELECT_1 = 5,
	SELECT_Z = 6
};

// How much effort to spend finding higher quality matches.
// These should match the astcenc.h values (as int)
enum class EAstcEncThunk_Quality : uint8_t
{
	FASTEST = 0,
	FAST = 10,
	MEDIUM = 60,
	THOROUGH = 98,
	VERYTHOROUGH = 99,
	EXHAUSTIVE = 100
};

enum class EAstcEncThunk_Flags : uint8_t
{
	NONE = 0,

	// pass to enable RDO encoding using the lambda in the create struct.
	LZ_RDO = 0x01,

	// pass for normal map encoding
	NORMAL_MAP = 0x02,

	// pass to switch from encode to decode
	DECOMPRESS_ONLY = 0x04
};

static inline EAstcEncThunk_Flags& operator|=(EAstcEncThunk_Flags& Lhs, EAstcEncThunk_Flags Rhs) 
{ 
	return Lhs = (EAstcEncThunk_Flags)((__underlying_type(EAstcEncThunk_Flags))Lhs | (__underlying_type(EAstcEncThunk_Flags))Rhs); 
}

inline constexpr bool operator&(EAstcEncThunk_Flags Lhs, EAstcEncThunk_Flags Rhs)
{ 
	return ((__underlying_type(EAstcEncThunk_Flags))Lhs & (__underlying_type(EAstcEncThunk_Flags))Rhs) != 0;
}


enum class EAstcEncThunk_Profile : uint8_t
{
	LDR_SRGB = 0,
	LDR,
	HDR_RGB_LDR_A,
	HDR
};

enum class EAstcEncThunk_Type : uint8_t
{
	U8,
	F16
};

struct FAstcEncThunk_CreateParams
{
	EAstcEncThunk_SwizzleComp SwizzleR = EAstcEncThunk_SwizzleComp::SELECT_R;
	EAstcEncThunk_SwizzleComp SwizzleG = EAstcEncThunk_SwizzleComp::SELECT_G;
	EAstcEncThunk_SwizzleComp SwizzleB = EAstcEncThunk_SwizzleComp::SELECT_B;
	EAstcEncThunk_SwizzleComp SwizzleA = EAstcEncThunk_SwizzleComp::SELECT_A;

	EAstcEncThunk_Profile Profile = EAstcEncThunk_Profile::LDR_SRGB;
	EAstcEncThunk_Flags Flags = EAstcEncThunk_Flags::NONE;
	EAstcEncThunk_Quality Quality = EAstcEncThunk_Quality::THOROUGH;

	// ASTC format block size. We only support square 2d blocks.
	uint8_t BlockSize = 4;

	float ErrorWeightR = 1.0f;
	float ErrorWeightG = 1.0f;
	float ErrorWeightB = 1.0f;
	float ErrorWeightA = 1.0f;

	// The level of rate/distortion tradeoff when using EAstcEncThunk_Flags::LZ_RDO. Higher means worse quality.
	float LZRdoLambda = 10.0f;

	// If set, the encoder dB threshold will be set to at least 60
	bool bDbLimitGreaterThan60 = false;

	// The number of times we need to call DoWork to complete the encode.
	uint32_t TaskCount = 1;

	// Input image specification (output when decoding)
	uint32_t SizeX = 0;
	uint32_t SizeY = 0;
	uint32_t NumSlices = 0;
	void** ImageSlices = nullptr; // [NumSlices] pointers to slices of the image.
	EAstcEncThunk_Type ImageDataType = EAstcEncThunk_Type::U8;

	// Output image buffer (input when decoding)
	uint8_t* OutputImageBuffer = nullptr;
	uint64_t OutputImageBufferSize = 0;
};

typedef void* AstcEncThunk_Context;

typedef void* AstcThunk_MallocFnType(size_t Size, size_t Alignment);
typedef void AstcThunk_FreeFnType(void* Ptr);

typedef void AstcThunk_SetAllocatorsFnType(AstcThunk_MallocFnType* MallocFn, AstcThunk_FreeFnType* FreeFn);
typedef char const* AstcThunk_CreateFnType(const FAstcEncThunk_CreateParams& CreateParams, AstcEncThunk_Context* OutContext);
typedef char const* AstcThunk_DoWorkFnType(AstcEncThunk_Context Context, uint32_t TaskIndex);
typedef void AstcThunk_DestroyFnType(AstcEncThunk_Context Context);

// Should be called once before any other calls.
EXTERNC EXPORTDLL void AstcEncThunk_SetAllocators(AstcThunk_MallocFnType* MallocFn, AstcThunk_FreeFnType* FreeFn);

// Returns the underlying string representation of the ASTC error on failure, or nullptr on success.
EXTERNC EXPORTDLL char const* AstcEncThunk_Create(const FAstcEncThunk_CreateParams& CreateParams, AstcEncThunk_Context* OutContext);

// Returns the underlying string representation of the ASTC error on failure, or nullptr on success.
EXTERNC EXPORTDLL char const* AstcEncThunk_DoWork(AstcEncThunk_Context Context, uint32_t TaskIndex);

// Frees the context created by AstcEncThunk_Create. Valid (nop) to pass nullptr.
EXTERNC EXPORTDLL void AstcEncThunk_Destroy(AstcEncThunk_Context Context);

/*
	Usage:

	FAstcEncThunk_CreateParams CreateParams;
	// fill out desired encoding and image pointers.

	AstcEncThunk_Context ThunkContext;
	const char* Error = AstcEncThunk_Create(CreateParams, &ThunkContext);
	if (!Error)
	{
		foreach (task in CreateParams.TaskCount)
		{
			Error = AstcEncThunk_DoWork(ThunkContext, task);
			if (Error)
				break;
		}
	}

	AstcEncThunk_Destroy(ThunkContext);
	
	if (Error)
		printf(Error);

*/