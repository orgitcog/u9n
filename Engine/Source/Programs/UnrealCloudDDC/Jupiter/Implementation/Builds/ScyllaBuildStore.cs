// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation;

public class ScyllaBuildStore : IBuildStore
{
	private readonly Tracer _tracer;
	private readonly ISession _session;
	private readonly Mapper _mapper;
	private readonly PreparedStatement _getTtlStatement;

	public ScyllaBuildStore(IScyllaSessionManager scyllaSessionManager, IOptionsMonitor<ScyllaSettings> scyllaSettings, Tracer tracer)
	{
		_tracer = tracer;
		_session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
		_mapper = new Mapper(_session);

		if (!scyllaSettings.CurrentValue.AvoidSchemaChanges)
		{
			_session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS builds (
					namespace text,
					bucket_id text,
					build_id blob,
					is_finalized boolean,
					build_object blob,
					PRIMARY KEY ((namespace, bucket_id), build_id)
				) WITH CLUSTERING ORDER BY (build_id DESC)
				;"
			));

			_session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS build_parts (
					namespace text,
					bucket_id text,
					build_id blob,
					part_name text,
					part_id blob,
					PRIMARY KEY ((namespace, bucket_id, build_id), part_id)
				);"
			));
		}
		_getTtlStatement = _session.Prepare("SELECT TTL(build_object) as ttl FROM builds WHERE namespace = ? AND bucket_id = ? AND build_id = ?");
	}

	public async IAsyncEnumerable<BuildMetadata> ListBuildsAsync(NamespaceId ns, BucketId bucket, bool includeTTL = false)
	{
		using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.list_builds");
		foreach (ScyllaBuildEntry buildEntry in await _mapper.FetchAsync<ScyllaBuildEntry>("WHERE namespace = ? AND bucket_id = ?", ns.ToString(), bucket.ToString()))
		{
			BuildMetadata? metadata = buildEntry.ToBuildMetadata();
			// ignore invalid metadata
			if (metadata == null)
			{
				continue;
			}

			if (includeTTL)
			{
				uint? ttl = await GetTTL(ns, bucket, metadata.BuildId);
				metadata.Ttl = ttl;
			}

			yield return metadata;
		}
	}

	public async Task PutBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, CbObject buildObject, uint ttl)
	{
		using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.put_build");

		await _mapper.InsertAsync<ScyllaBuildEntry>(new ScyllaBuildEntry(ns, bucket, buildId, buildObject), insertNulls: false, ttl: (int)ttl);
	}

	public async Task FinalizeBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, uint ttl)
	{
		await _mapper.UpdateAsync<ScyllaBuildEntry>("USING TTL ? SET is_finalized = true WHERE namespace = ? and bucket_id = ? AND build_id = ?", (int)ttl, ns.ToString(), bucket.ToString(), buildId.ToByteArray());
	}

	public async Task<BuildRecord?> GetBuildAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId)
	{
		ScyllaBuildEntry? buildEntry = await _mapper.SingleOrDefaultAsync<ScyllaBuildEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray());

		if (buildEntry != null && buildEntry.BuildObject != null) 
		{
			return new BuildRecord {BuildObject = new CbObject(buildEntry.BuildObject), IsFinalized = buildEntry.IsFinalized};
		}

		return null;
	}

	public async Task PutBuildPartAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId, CbObjectId partId, string partName, uint ttl)
	{
		using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.put_build_part");

		await _mapper.InsertAsync<ScyllaBuildPartEntry>(new ScyllaBuildPartEntry(ns, bucket, buildId, partId, partName), insertNulls: false, ttl: (int)ttl);
	}

	public async IAsyncEnumerable<(string, CbObjectId)> GetBuildPartsAsync(NamespaceId ns, BucketId bucket, CbObjectId buildId)
	{
		using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get_build_parts");
		foreach (ScyllaBuildPartEntry buildPartEntry in await _mapper.FetchAsync<ScyllaBuildPartEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray()))
		{
			yield return (buildPartEntry.PartName, new CbObjectId(buildPartEntry.PartId));
		}
	}

	public async Task<uint?> GetTTL(NamespaceId ns, BucketId bucket, CbObjectId buildId)
	{
		using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get_build_ttl");

		BoundStatement? boundStatement = _getTtlStatement.Bind(ns.ToString(), bucket.ToString(), buildId.ToByteArray());
		RowSet rowSet = await _session.ExecuteAsync(boundStatement);
		foreach (Row row in rowSet)
		{
			// we only expect one row here so we just return once we have a value

			if (row["ttl"] is int ttl)
			{
				return (uint)ttl;
			}
		}

		return null;
	}

	public async Task UpdateTTL(NamespaceId ns, BucketId bucket, CbObjectId buildId, uint ttl)
	{
		ScyllaBuildEntry? buildEntry = await _mapper.SingleOrDefaultAsync<ScyllaBuildEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray());

		if (buildEntry == null)
		{
			// failed to find the full build, very unlikely, should have failed earlier
			return;
		}

		// scylla does not support updating the ttl, instead we reinsert the same value with a new ttl
		await _mapper.InsertAsync<ScyllaBuildEntry>(buildEntry, false, (int)ttl);

		foreach (ScyllaBuildPartEntry buildPartEntry in await _mapper.FetchAsync<ScyllaBuildPartEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray()))
		{
			await _mapper.InsertAsync<ScyllaBuildPartEntry>(buildPartEntry, false, (int)ttl);
		}
	}

	public async Task DeleteBuild(NamespaceId ns, BucketId bucket, CbObjectId buildId)
	{
		await _mapper.DeleteAsync<ScyllaBuildPartEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray());
		await _mapper.DeleteAsync<ScyllaBuildEntry>("WHERE namespace = ? AND bucket_id = ? AND build_id = ?", ns.ToString(), bucket.ToString(), buildId.ToByteArray());
	}
}

[Cassandra.Mapping.Attributes.Table("builds")]
public class ScyllaBuildEntry
{
	public ScyllaBuildEntry()
	{
		Namespace = null!;
		BucketId = null!;
		BuildId = null!;
		BuildObject = null!;
		IsFinalized = false;
	}
	
	public ScyllaBuildEntry(NamespaceId @namespace, BucketId bucketId, CbObjectId buildId, CbObject buildObject)
	{
		Namespace = @namespace.ToString();
		BucketId = bucketId.ToString();
		BuildId = buildId.ToByteArray();
		BuildObject = buildObject.GetView().ToArray();
		IsFinalized = false;
	}

	[Cassandra.Mapping.Attributes.PartitionKey]
	public string Namespace { get; set; }

	[Cassandra.Mapping.Attributes.Column("bucket_id")]
	[Cassandra.Mapping.Attributes.PartitionKey]
	public string BucketId { get; set; }

	[Cassandra.Mapping.Attributes.Column("build_id")]
	[Cassandra.Mapping.Attributes.ClusteringKey]
	public byte[] BuildId { get; set; }

	[Cassandra.Mapping.Attributes.Column("build_object")]
	public byte[]? BuildObject { get; set; }

	[Cassandra.Mapping.Attributes.Column("is_finalized")]
	public bool IsFinalized { get; set; }
	public BuildMetadata? ToBuildMetadata()
	{
		try
		{
			return new BuildMetadata(new CbObjectId(BuildId), new CbObject(BuildObject), IsFinalized, null);
		}
		catch (IndexOutOfRangeException)
		{
			// incorrectly structured compact binaries are ignored
			return null;
		}
	}
}

[Cassandra.Mapping.Attributes.Table("build_parts")]
public class ScyllaBuildPartEntry
{
	public ScyllaBuildPartEntry()
	{
		Namespace = null!;
		BucketId = null!;
		BuildId = null!;
		PartName = null!;
		PartId = null!;
	}
	
	public ScyllaBuildPartEntry(NamespaceId @namespace, BucketId bucketId, CbObjectId buildId, CbObjectId partId, string partName)
	{
		Namespace = @namespace.ToString();
		BucketId = bucketId.ToString();
		BuildId = buildId.ToByteArray();
		PartName = partName;
		PartId = partId.ToByteArray();
	}

	[Cassandra.Mapping.Attributes.PartitionKey]
	public string Namespace { get; set; }

	[Cassandra.Mapping.Attributes.Column("bucket_id")]
	[Cassandra.Mapping.Attributes.PartitionKey]
	public string BucketId { get; set; }

	[Cassandra.Mapping.Attributes.Column("build_id")]
	[Cassandra.Mapping.Attributes.PartitionKey]
	public byte[] BuildId { get; set; }

	[Cassandra.Mapping.Attributes.Column("part_name")]
	public string PartName { get; set; }

	[Cassandra.Mapping.Attributes.Column("part_id")]
	[Cassandra.Mapping.Attributes.ClusteringKey]
	public byte[] PartId { get; set; }
}