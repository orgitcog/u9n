// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(__cplusplus) || COMPILER_SUPPORTS_HLSL2021

#ifdef __cplusplus

#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif

struct FBankRecordHeader
{
	uint BoneCount;
	uint FrameCount;

	float SampleRate;
	float PlayRate;

	float CurrentTime;
	float PreviousTime;

	uint TransformOffset	: 29;
	uint Playing			: 1;
	uint Interpolating		: 1;
	uint HasScale			: 1;
};

struct FBankBlockHeader
{
	uint BlockLocalIndex;
	uint BlockBoneCount;
	uint BlockTransformOffset;
	uint BankRecordOffset;
};

struct FBankScatterHeader
{
	uint BlockLocalIndex;
	uint BlockSrcTransformOffset;
	uint BlockDstTransformOffset;
	uint BlockTransformCount : 8;
	uint TotalTransformCount : 24;
};

#define ANIM_BANK_FLAG_NONE			0x0
#define ANIM_BANK_FLAG_LOOPING		0x1
#define ANIM_BANK_FLAG_AUTOSTART	0x2

#ifdef __cplusplus
} // namespace UE::HLSL
#endif

#if defined(__cplusplus)
#define UINT_TYPE unsigned int
#else
#define UINT_TYPE uint
#endif

#define USE_COMPRESSED_BONE_TRANSFORM	1

#if USE_COMPRESSED_BONE_TRANSFORM
struct FCompressedBoneTransform
{
	UINT_TYPE Data[8];
};
#else
#if defined(__cplusplus)
#define FCompressedBoneTransform FMatrix3x4
#else
#define FCompressedBoneTransform float3x4
#endif
#endif

#ifdef __cplusplus

#include "Matrix3x4.h"

#define REF_POSE_TRANSFORM_PROVIDER_GUID 0x665207E7, 0x449A4FB1, 0xA298F7AD, 0x8F989B11
#define ANIM_BANK_GPU_TRANSFORM_PROVIDER_GUID 0xA5C0027A, 0x8F884C7C, 0x9312F138, 0x71A9300F
#define ANIM_BANK_CPU_TRANSFORM_PROVIDER_GUID 0xE7D6173D, 0x246F431A, 0x912D384E, 0x156C0D2C
#define ANIM_RUNTIME_TRANSFORM_PROVIDER_GUID 0xF1508490, 0xFCC24BB9, 0xA9F277B3, 0x1AF766F0

inline void StoreCompressedBoneTransform(FCompressedBoneTransform* CompressedTransform, FMatrix44f& Transform)
{
#if USE_COMPRESSED_BONE_TRANSFORM
	//TODO: Optimize
	*(FVector3f*)&CompressedTransform->Data[0] = Transform.GetOrigin();

	float Tmp[8] = { Transform.M[0][0], Transform.M[0][1], Transform.M[0][2], Transform.M[1][0],
					 Transform.M[1][1], Transform.M[1][2], Transform.M[2][0], Transform.M[2][1] };

	uint16* Ptr = (uint16*)&CompressedTransform->Data[3];

	FPlatformMath::VectorStoreHalf(&Ptr[0], &Tmp[0]);
	FPlatformMath::VectorStoreHalf(&Ptr[4], &Tmp[4]);
	FPlatformMath::StoreHalf(&Ptr[8], Transform.M[2][2]);
	Ptr[9] = 0;
#else
	Transform.To3x4MatrixTranspose((float*)CompressedTransform);
#endif
}

inline void StoreCompressedBoneTransform(FCompressedBoneTransform& CompressedTransform, const FMatrix3x4& Transform)
{
#if USE_COMPRESSED_BONE_TRANSFORM
	*(FVector3f*)&CompressedTransform.Data[0] = FVector3f(Transform.M[0][3], Transform.M[1][3], Transform.M[2][3]);

	float Tmp[8] = { Transform.M[0][0], Transform.M[1][0], Transform.M[2][0], Transform.M[0][1],
					 Transform.M[1][1], Transform.M[2][1], Transform.M[0][2], Transform.M[1][2] };

	uint16* Ptr = (uint16*)&CompressedTransform.Data[3];

	FPlatformMath::VectorStoreHalf(&Ptr[0], &Tmp[0]);
	FPlatformMath::VectorStoreHalf(&Ptr[4], &Tmp[4]);
	FPlatformMath::StoreHalf(&Ptr[8], Transform.M[2][2]);
	Ptr[9] = 0;
#else
	CompressedTransform = Transform;
#endif
}

inline void SetCompressedBoneTransformIdentity(FCompressedBoneTransform& Transform)
{
#if USE_COMPRESSED_BONE_TRANSFORM
	Transform.Data[0] = 0;			// Origin.X = 0
	Transform.Data[1] = 0;			// Origin.Y = 0
	Transform.Data[2] = 0;			// Origin.Z = 0
	Transform.Data[3] = 0x3C00u;	// XAxis.X = 1, XAxis.Y = 0
	Transform.Data[4] = 0;			// YAxis.Z = 0, YAxis.X = 0
	Transform.Data[5] = 0x3C00u;	// YAxis.Y = 1, YAxis.Z = 0
	Transform.Data[6] = 0;			// ZAxis.X = 0, ZAxis.Y = 0
	Transform.Data[7] = 0x3C00u;	// ZAxis.Z = 1
#else
	Transform.SetIdentity();
#endif
}

#endif

#endif // defined(__cplusplus) || COMPILER_SUPPORTS_HLSL2021