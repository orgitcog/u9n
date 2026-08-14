// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAUtilities.h"

#include "DNAReader.h"


bool FDNAUtilities::CheckCompatibility(IDNAReader* InDnaReaderA, IDNAReader* InDnaReaderB, EDNARigCompatiblityFlags InCompareFlags)
{
	FString CompatibilityMsg;

	return CheckCompatibility(InDnaReaderA, InDnaReaderB, InCompareFlags, CompatibilityMsg);
}

bool FDNAUtilities::CheckCompatibility(IDNAReader* InDnaReaderA, IDNAReader* InDnaReaderB, EDNARigCompatiblityFlags InCompareFlags, FString& OutCompatibilityMsg)
{
	if (!InDnaReaderA || !InDnaReaderB)
	{
		OutCompatibilityMsg = TEXT("Invalid DNA readers");
		return false;
	}

	// Joints
	if (EnumHasAnyFlags(InCompareFlags, EDNARigCompatiblityFlags::Joint))
	{
		const uint16 JointCountA = InDnaReaderA->GetJointCount();
		const uint16 JointCountB = InDnaReaderB->GetJointCount();

		// Compare joint count
		if (JointCountA != JointCountB)
		{
			OutCompatibilityMsg = FString::Printf(TEXT("Joint count mismatch: %u vs %u"), JointCountA, JointCountB);
			return false;
		}

		bool bJointsOk = true;
		FStringBuilderBase ResultMsg;

		for (uint16 JointIndex = 0; JointIndex < JointCountA; JointIndex++)
		{
			const uint16 JointParentA = InDnaReaderA->GetJointParentIndex(JointIndex);
			const uint16 JointParentB = InDnaReaderB->GetJointParentIndex(JointIndex);

			// Compare joint names
			if (InDnaReaderA->GetJointName(JointIndex) != InDnaReaderB->GetJointName(JointIndex))
			{
				ResultMsg.Appendf(TEXT("Joint name mismatch: '%s' vs '%s'"), *InDnaReaderA->GetJointName(JointParentA), *InDnaReaderB->GetJointName(JointParentB));
				ResultMsg.AppendChar('\n');
				bJointsOk = false;
				continue;
			}

			// Compare parents
			if (InDnaReaderA->GetJointParentIndex(JointIndex) != InDnaReaderB->GetJointParentIndex(JointIndex))
			{
				ResultMsg.Appendf(TEXT("Joint parent mismatch for joint '%s': '%s' vs '%s'"), *InDnaReaderA->GetJointName(JointIndex), *InDnaReaderA->GetJointName(JointParentA), *InDnaReaderA->GetJointName(JointParentB));
				ResultMsg.AppendChar('\n');
				bJointsOk = false;
			}
		}

		if (!bJointsOk)
		{
			OutCompatibilityMsg = ResultMsg.ToString();
			return false;
		}
	}

	// LOD
	if (EnumHasAnyFlags(InCompareFlags, EDNARigCompatiblityFlags::LOD))
	{
		// Compare LOD count
		if (InDnaReaderA->GetLODCount() != InDnaReaderB->GetLODCount())
		{
			OutCompatibilityMsg = FString::Printf(TEXT("LOD count mismatch: %u vs %u"), InDnaReaderA->GetLODCount(), InDnaReaderB->GetLODCount());
			return false;
		}
	}

	// Meshes
	if (EnumHasAnyFlags(InCompareFlags, EDNARigCompatiblityFlags::Mesh))
	{
		bool bMeshesOk = true;

		const uint16 MeshCountA = InDnaReaderA->GetMeshCount();
		const uint16 MeshCountB = InDnaReaderB->GetMeshCount();

		// Compare mesh count
		if (MeshCountA != MeshCountB)
		{
			OutCompatibilityMsg = FString::Printf(TEXT("Mesh count mismatch: %u vs %u"), MeshCountA, MeshCountB);
			return false;
		}

		FStringBuilderBase ResultMsg;

		for (uint16 MeshIndex = 0; MeshIndex < MeshCountA; MeshIndex++)
		{
			const uint16 VertexCountA = InDnaReaderA->GetVertexPositionCount(MeshIndex);
			const uint16 VertexCountB = InDnaReaderB->GetVertexPositionCount(MeshIndex);

			// Compare vertex count
			if (VertexCountA != VertexCountB)
			{
				ResultMsg.Appendf(TEXT("Vertex count mismatch on mesh '%s' (mesh index: %u): %u vs %u"), *InDnaReaderA->GetMeshName(MeshIndex), MeshIndex, VertexCountA, VertexCountB);
				ResultMsg.AppendChar('\n');
				bMeshesOk = false;
			}
		}

		if (!bMeshesOk)
		{
			OutCompatibilityMsg = ResultMsg.ToString();
			return false;
		}
	}

	return true;
}