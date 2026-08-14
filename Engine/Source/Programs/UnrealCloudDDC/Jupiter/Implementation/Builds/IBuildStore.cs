// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Jupiter.Implementation;

public interface IBuildStore
{
	Task PutBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, CbObject buildObject, uint ttl);
	Task FinalizeBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, uint ttl);
	Task<BuildRecord?> GetBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId);
	IAsyncEnumerable<BuildMetadata> ListBuildsAsync(NamespaceId ns, BucketId bucket, bool includeTTL = false);

	Task PutBuildPartAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, CbObjectId partId, string partName, uint ttl);
	IAsyncEnumerable<(string, CbObjectId)> GetBuildPartsAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId);
	Task UpdateTTL(NamespaceId ns, BucketId bucket, CbObjectId buildId, uint ttl);
	Task DeleteBuild(NamespaceId ns, BucketId bucket, CbObjectId buildId);
	Task<uint?> GetTTL(NamespaceId ns, BucketId bucket, CbObjectId buildId);
}

public class BuildMetadata
{
	public BuildMetadata(CbObjectId buildId, CbObject metadata, bool isFinalized, uint? ttl)
	{
		BuildId = buildId;
		Metadata = metadata;
		IsFinalized = isFinalized;
		Ttl = ttl;
	}

	public CbObjectId BuildId { get; set; }
	public CbObject Metadata { get; set; } = null!;
	public bool IsFinalized { get; set; }
	public uint? Ttl { get; set; } = null;
}

public class BuildRecord
{
	public bool IsFinalized { get; set; }
	public CbObject BuildObject { get; set; } = CbObject.Empty;

}