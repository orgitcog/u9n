// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/Trace/PoseSearchTraceLogger.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Serialization/MemoryWriter.h"
#include "Trace/Trace.inl"
#include "TraceFilter.h"
#include "PoseSearch/PoseSearchCustomVersion.h"

UE_TRACE_CHANNEL_DEFINE(PoseSearchChannel);

// No custom versions applied when loading.
UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

// We apply the FPoseSearchCustomVersion::DeprecatedTrajectoryTypes version when loading.
UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState2)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

// We apply the FPoseSearchCustomVersion::AddedInterruptModeToDebugger version when loading.
UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState3)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

namespace UE::PoseSearch
{

// @todo: These should be deprecated. No need to expose them.
const FName FTraceLogger::Name("PoseSearch");
const FName FTraceMotionMatchingStateMessage::Name("MotionMatchingState3");

FArchive& operator<<(FArchive& Ar, FTraceMessage& State)
{
	Ar << State.Cycle;
	Ar << State.AnimInstanceId;
	Ar << State.NodeId;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStatePoseEntry& Entry)
{
	Ar << Entry.DbPoseIdx;
	FPoseSearchCost::StaticStruct()->SerializeItem(Ar, &Entry.Cost, nullptr);
	Ar << Entry.PoseCandidateFlags;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateDatabaseEntry& Entry)
{
	Ar << Entry.DatabaseId;
	Ar << Entry.QueryVector;
	Ar << Entry.PoseEntries;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateMessage& State)
{
	Ar << static_cast<FTraceMessage&>(State);

	Ar << State.ElapsedPoseSearchTime;
	Ar << State.AssetPlayerTime;
	Ar << State.DeltaTime;
	Ar << State.SimLinearVelocity;
	Ar << State.SimAngularVelocity;
	Ar << State.AnimLinearVelocity;
	Ar << State.AnimAngularVelocity;
	Ar << State.Playrate;
	Ar << State.AnimLinearVelocityNoTimescale;
	Ar << State.AnimAngularVelocityNoTimescale;
	Ar << State.RecordingTime;
	Ar << State.SearchBestCost;
	Ar << State.SearchBruteForceCost;
	Ar << State.SearchBestPosePos;
	Ar << State.SkeletalMeshComponentIds;
	Ar << State.Roles;
	Ar << State.DatabaseEntries;
	Ar << State.PoseHistories;	
	Ar << State.CurrentDbEntryIdx;
	Ar << State.CurrentPoseEntryIdx;

	// InterruptMode has been introduced with AddedInterruptModeToDebugger version
	if (Ar.CustomVer(FPoseSearchCustomVersion::GUID) >= FPoseSearchCustomVersion::AddedInterruptModeToDebugger)
	{
		Ar << State.InterruptMode;
	}
	return Ar;
}

void FTraceMotionMatchingStateMessage::Output()
{
#if OBJECT_TRACE_ENABLED
	TArray<uint8> ArchiveData;
	FMemoryWriter Archive(ArchiveData);

	// Ensure we have the latest version.
	FCustomVersionContainer CustomVersionContainer;
	CustomVersionContainer.SetVersion(FPoseSearchCustomVersion::GUID, FPoseSearchCustomVersion::LatestVersion, TEXT("Dev-PoseSearch-Version"));
	Archive.SetCustomVersions(CustomVersionContainer);
	
	Archive << *this;
	UE_TRACE_LOG(PoseSearch, MotionMatchingState3, PoseSearchChannel) << MotionMatchingState3.Data(ArchiveData.GetData(), ArchiveData.Num());
#endif
}

const UPoseSearchDatabase* FTraceMotionMatchingStateMessage::GetCurrentDatabase() const
{
	const UPoseSearchDatabase* Database = nullptr;
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		Database = GetObjectFromId<UPoseSearchDatabase>(DatabaseEntries[CurrentDbEntryIdx].DatabaseId);
	}
	return Database;
}

int32 FTraceMotionMatchingStateMessage::GetCurrentDatabasePoseIndex() const
{
	if (const FTraceMotionMatchingStatePoseEntry* PoseEntry = GetCurrentPoseEntry())
	{
		return PoseEntry->DbPoseIdx;
	}
	return INDEX_NONE;
}

const FTraceMotionMatchingStatePoseEntry* FTraceMotionMatchingStateMessage::GetCurrentPoseEntry() const
{
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		const FTraceMotionMatchingStateDatabaseEntry& DbEntry = DatabaseEntries[CurrentDbEntryIdx];
		if (DbEntry.PoseEntries.IsValidIndex(CurrentPoseEntryIdx))
		{
			return &DbEntry.PoseEntries[CurrentPoseEntryIdx];
		}
	}
	return nullptr;
}

FText FTraceMotionMatchingStateMessage::GenerateSearchName() const
{
	TStringBuilder<256> StringBuilder;
	bool bAddDatabasesSeparator = false;
	for (const FTraceMotionMatchingStateDatabaseEntry& DbEntry : DatabaseEntries)
	{
		if (bAddDatabasesSeparator)
		{
			StringBuilder.Append(" - ");
		}

		if (const UPoseSearchDatabase* Database = GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId))
		{
			StringBuilder.Append(Database->GetName());
		}
		else
		{
			StringBuilder.Append("Unknown");
		}
		
		bAddDatabasesSeparator = true;
	}

	if (Roles.Num() > 1)
	{
		if (Roles.Num() != SkeletalMeshComponentIds.Num())
		{
			StringBuilder.Append("Error!");
		}
		else
		{
			StringBuilder.Append(" [");

			bool bAddRolesSeparator = false;

			for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
			{
				if (bAddRolesSeparator)
				{
					StringBuilder.Append(" - ");
				}

				StringBuilder.Append(Roles[RoleIndex].ToString());

				StringBuilder.Append(": ");

				if (const USkeletalMeshComponent* SkeletalMeshComponent = GetObjectFromId<USkeletalMeshComponent>(SkeletalMeshComponentIds[RoleIndex]))
				{
					if (const AActor* Actor = SkeletalMeshComponent->GetOwner())
					{
						StringBuilder.Append(Actor->GetName());
					}
					else
					{
						StringBuilder.Append("Error!");
					}
				}
				else
				{
					StringBuilder.Append("Error!");
				}

				bAddRolesSeparator = true;
			}

			StringBuilder.Append("]");
		}
	}

	return FText::FromString(StringBuilder.ToString());
}

} // namespace UE::PoseSearch