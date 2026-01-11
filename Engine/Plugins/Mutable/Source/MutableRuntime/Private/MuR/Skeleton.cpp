// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Skeleton.h"

#include "MuR/SerialisationPrivate.h"


namespace mu
{

	void FSkeleton::Serialise(const FSkeleton* In, FOutputArchive& Arch)
	{
		Arch << *In;
	}


	TSharedPtr<FSkeleton> FSkeleton::StaticUnserialise(FInputArchive& Arch)
	{
		TSharedPtr<FSkeleton> Result = MakeShared<FSkeleton>();
		Arch >> *Result;
		return Result;
	}


	TSharedPtr<FSkeleton> FSkeleton::Clone() const
	{
		TSharedPtr<FSkeleton> Result = MakeShared<FSkeleton>();

		Result->BoneIds = BoneIds;
		Result->BoneParents = BoneParents;

		// For debug
		Result->DebugBoneNames = DebugBoneNames;

		return Result;
	}


	int32 FSkeleton::GetBoneCount() const
	{
		return BoneIds.Num();
	}


	void FSkeleton::SetBoneCount(int32 NumBones)
	{
		DebugBoneNames.SetNum(NumBones);
		BoneIds.SetNum(NumBones);
		BoneParents.Init(INDEX_NONE, NumBones);
	}


	const FName FSkeleton::GetDebugName(int32 Index) const
	{
		if (DebugBoneNames.IsValidIndex(Index))
		{
			return DebugBoneNames[Index];
		}

		return FName("Unknown Bone");
	}


	void FSkeleton::SetDebugName(const int32 Index, const FName BoneName)
	{
		if (DebugBoneNames.IsValidIndex(Index))
		{
			DebugBoneNames[Index] = BoneName;
		}
	}


	int32 FSkeleton::GetBoneParent(int32 Index) const
	{
		if (BoneParents.IsValidIndex(Index))
		{
			return BoneParents[Index];
		}

		return INDEX_NONE;
	}


	void FSkeleton::SetBoneParent(int32 Index, int32 ParentIndex)
	{
		check(ParentIndex >= -1 && ParentIndex < GetBoneCount() && ParentIndex < 0xffff);
		check(BoneParents.IsValidIndex(Index));
		
		if (BoneParents.IsValidIndex(Index))
		{
			BoneParents[Index] = (int16)ParentIndex;
		}
	}


	const FBoneName& FSkeleton::GetBoneName(int32 Index) const
	{
		check(BoneIds.IsValidIndex(Index));
		return BoneIds[Index];
	}

	
	void FSkeleton::SetBoneName(int32 Index, const FBoneName& BoneName)
	{
		check(BoneIds.IsValidIndex(Index));
		if (BoneIds.IsValidIndex(Index))
		{
			BoneIds[Index] = BoneName;
		}
	}


	int32 FSkeleton::FindBone(const FBoneName& BoneName) const
	{
		return BoneIds.Find(BoneName);
	}


	void FSkeleton::Serialise(FOutputArchive& Arch) const
	{
		Arch << BoneIds;
		Arch << BoneParents;
	}


	void FSkeleton::Unserialise(FInputArchive& Arch)
	{
		Arch >> BoneIds;
		Arch >> BoneParents;
	}
}
