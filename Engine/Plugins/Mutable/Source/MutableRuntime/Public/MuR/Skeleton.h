// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Math/Transform.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

#define UE_API MUTABLERUNTIME_API


namespace mu
{	
	// Bone name identifier
	struct FBoneName
	{
		FBoneName() {};
		FBoneName(uint32 InID) : Id(InID) {};

		// Hash built from the bone name (FString)
		uint32 Id = 0;

		inline void Serialise(FOutputArchive& Arch) const
		{
			Arch << Id;
		}

		inline void Unserialise(FInputArchive& Arch)
		{
			Arch >> Id;
		}

		//!
		inline bool operator==(const FBoneName& Other) const
		{
			return Id == Other.Id;
		}
	};

	inline uint32 GetTypeHash(const FBoneName& Bone)
	{
		return Bone.Id;
	}


    /** Skeleton object. */
	class FSkeleton
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        //! Deep clone this skeleton.
        UE_API TSharedPtr<FSkeleton> Clone() const;

		//! Serialisation
        static UE_API void Serialise( const FSkeleton*, FOutputArchive& );
        static UE_API TSharedPtr<FSkeleton> StaticUnserialise( FInputArchive& );


		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

        //! @return - Number of bones in the Skeleton
		UE_API int32 GetBoneCount() const;
        UE_API void SetBoneCount(int32 Count);

		//! @return - FName of the bone at 'Index'. Only valid in the editor
		UE_API const FName GetDebugName(int32 Index) const;
		UE_API void SetDebugName(const int32 Index, const FName BoneName);

        //! Get and set the parent bone of each bone. The parent can be -1 if the bone is a root.
        UE_API int32 GetBoneParent(int32 BoneIndex) const;
        UE_API void SetBoneParent(int32 BoneIndex, int32 ParentBoneIndex);

		//! @return - BoneName of the Bone at 'Index'.
		UE_API const FBoneName& GetBoneName(int32 Index) const;
		UE_API void SetBoneName(int32 Index, const FBoneName& BoneName);

		//! @return - Index in the Skeleton. INDEX_NONE if not found.
		UE_API int32 FindBone(const FBoneName& BoneName) const;
		

	public:

		//! DEBUG. FNames of the bones. Only valid in the editor. Do not serialize.
		TArray<FName> DebugBoneNames;

		//! Array of bone identifiers. 
		TArray<FBoneName> BoneIds;

		//! For each bone, index of the parent bone in the bone vectors. -1 means no parent.
		//! This array must have the same size than the m_bones array.
		TArray<int16> BoneParents;

		//!
		UE_API void Serialise(FOutputArchive&) const;

		//!
		UE_API void Unserialise(FInputArchive&);

		//!
		inline bool operator==(const FSkeleton& Other) const
		{
			return BoneIds == Other.BoneIds
				&& BoneParents == Other.BoneParents;
		}
	};

}

#undef UE_API
