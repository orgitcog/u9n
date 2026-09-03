// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Commits;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Jobs.TestData;
using EpicGames.Horde.Streams;
using HordeServer.Commits;
using HordeServer.Server;
using HordeServer.Utilities;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using OpenTelemetry.Trace;

namespace HordeServer.Jobs.TestData
{
	/// <summary>
	/// Collection of test data documents v2
	/// </summary>
	public class TestDataCollectionV2 : ITestDataCollectionV2
	{
		/// <summary>
		/// Interface to handle expiration
		/// </summary>
		internal interface ITestExpire
		{
			DateTime LastSeenUtc { get; }
		}

		/// <summary>
		/// Metadata key value pair
		/// </summary>
		class TestMetaEntry : ITestMetaEntry
		{
			[BsonRequired, BsonElement("k")]
			public string Key { get; set; }

			[BsonRequired, BsonElement("v")]
			public string Value { get; set; }

			private TestMetaEntry()
			{
				Key = String.Empty;
				Value = String.Empty;
			}

			public TestMetaEntry(string key, string data)
			{
				Key = key;
				Value = data;
			}
		}

		/// <summary>
		/// Test metadata document
		/// </summary>
		class TestMetadataDocument : ITestMetaRef, ITestExpire
		{
			[BsonRequired, BsonId]
			public TestMetaId Id { get; set; }

			[BsonRequired, BsonElement("m")]
			public List<TestMetaEntry> Entries { get; set; }
			IReadOnlyList<ITestMetaEntry> ITestMetaRef.Entries => Entries;

			[BsonRequired, BsonElement("s")]
			public DateTime LastSeenUtc { get; set; }

			private TestMetadataDocument()
			{
				Entries = new();
			}

			public TestMetadataDocument(IReadOnlyDictionary<string, string> entries)
			{
				Id = TestMetaId.GenerateNewId();
				Entries = entries.Select(i => new TestMetaEntry(i.Key, i.Value)).ToList();
				LastSeenUtc = DateTime.UtcNow;
			}
		}

		/// <summary>
		/// Test tag reference document
		/// </summary>
		class TestTagDocument : ITestTagRef, ITestExpire
		{
			[BsonRequired, BsonId] 
			public TestTagId Id { get; set; }

			[BsonRequired, BsonElement("n")] 
			public string Name { get; set; }

			[BsonRequired, BsonElement("s")]
			public DateTime LastSeenUtc { get; set; }

			private TestTagDocument()
			{
				Name = String.Empty;
			}

			public TestTagDocument(string name)
			{
				Id = TestTagId.GenerateNewId();
				Name = name;
				LastSeenUtc = DateTime.UtcNow;
			}
		}

		/// <summary>
		/// Test name reference document
		/// </summary>
		class TestNameDocument : ITestNameRef, ITestExpire
		{
			[BsonRequired, BsonId]
			public TestId Id { get; set; }

			[BsonRequired, BsonElement("n")]
			public string Name { get; set; }

			[BsonRequired, BsonElement("k")]
			public string Key { get; set; }

			[BsonRequired, BsonElement("s")]
			public DateTime LastSeenUtc { get; set; }

			private TestNameDocument()
			{
				Key = String.Empty;
				Name = String.Empty;
			}

			public TestNameDocument(string key, string name)
			{
				Id = TestId.GenerateNewId();
				Key = key;
				Name = name;
				LastSeenUtc = DateTime.UtcNow;
			}
		}

		/// <summary>
		/// Test phase reference document
		/// </summary>
		class TestPhaseDocument : ITestPhaseRef, ITestExpire
		{
			[BsonRequired, BsonId]
			public TestPhaseId Id { get; set; }

			[BsonRequired, BsonElement("tid")]
			public TestId TestNameRef { get; set; }

			[BsonRequired, BsonElement("n")]
			public string Name { get; set; }

			[BsonRequired, BsonElement("k")]
			public string Key { get; set; }

			[BsonRequired, BsonElement("s")]
			public DateTime LastSeenUtc { get; set; }

			private TestPhaseDocument()
			{
				Name = String.Empty;
				Key = String.Empty;
			}

			public TestPhaseDocument(TestId testId, string key, string name)
			{
				Id = TestPhaseId.GenerateNewId();
				TestNameRef = testId;
				Name = name;
				Key = key;
				LastSeenUtc = DateTime.UtcNow;
			}
		}

		/// <summary>
		/// Stores the tests running in a stream
		/// </summary>
		class TestSessionStreamDocument : ITestSessionStream
		{
			[BsonRequired, BsonId]
			public ObjectId Id { get; set; }

			[BsonRequired, BsonElement("sid")]
			public StreamId StreamId { get; set; }

			[BsonRequired, BsonElement("t")]
			public List<TestId> Tests { get; set; } = new List<TestId>();
			IReadOnlyList<TestId> ITestSessionStream.Tests => Tests;

			[BsonRequired, BsonElement("m")]
			public List<TestMetaId> Metadata { get; set; } = new List<TestMetaId>();
			IReadOnlyList<TestMetaId> ITestSessionStream.Metadata => Metadata;

			[BsonRequired, BsonElement("tg")]
			public List<TestTagId> Tags { get; set; } = new List<TestTagId>();
			IReadOnlyList<TestTagId> ITestSessionStream.Tags => Tags;

			private TestSessionStreamDocument()
			{

			}

			public TestSessionStreamDocument(StreamId streamId)
			{
				Id = ObjectId.GenerateNewId();
				StreamId = streamId;
			}
		}

		/// <summary>
		/// Store the test session overall result
		/// </summary>
		class TestSessionDocument : ITestSession
		{
			[BsonRequired, BsonId]
			public TestSessionId Id { get; set; }

			[BsonRequired, BsonElement("sid")]
			public StreamId StreamId { get; set; }

			[BsonRequired, BsonElement("m")]
			public TestMetaId Metadata { get; set; }

			[BsonIgnoreIfNull, BsonElement("tg")]
			public IReadOnlyList<TestTagId>? Tags { get; set; }

			[BsonRequired, BsonElement("did")]
			public ObjectId TestDataId { get; set; }

			public CommitIdWithOrder BuildCommitId
			{
				get => (BuildCommitName != null) ? new CommitIdWithOrder(BuildCommitName, BuildCommitOrder) : CommitIdWithOrder.FromPerforceChange(BuildCommitOrder);
				set => (BuildCommitName, BuildCommitOrder) = (value.Name, value.Order);
			}

			[BsonIgnoreIfNull, BsonElement("bcn")]
			public string? BuildCommitName { get; set; }

			[BsonRequired, BsonElement("bcl")]
			public int BuildCommitOrder { get; set; }

			[BsonRequired, BsonElement("d")]
			public TimeSpan Duration { get; set; }

			[BsonRequired, BsonElement("s")]
			public DateTime StartDateTime { get; set; }

			[BsonRequired, BsonElement("tid")]
			public TestId NameRef { get; set; }

			[BsonRequired, BsonElement("o")]
			public TestOutcome Outcome { get; set; }

			[BsonRequired, BsonElement("ptc")]
			public int PhasesTotalCount { get; set; }

			[BsonRequired, BsonElement("psc")]
			public int PhasesSucceededCount { get; set; }

			[BsonRequired, BsonElement("puc")]
			public int PhasesUndefinedCount { get; set; }

			[BsonRequired, BsonElement("pfc")]
			public int PhasesFailedCount { get; set; }

			[BsonRequired, BsonElement("jid")]
			public JobId JobId { get; set; }

			[BsonRequired, BsonElement("sjid")]
			public JobStepId StepId { get; set; }

			private TestSessionDocument()
			{

			}

			public TestSessionDocument(StreamId streamId, ObjectId testDataId, TestId testId, TestMetaId metaId)
			{
				Id = TestSessionId.GenerateNewId();
				StreamId = streamId;
				TestDataId = testDataId;
				NameRef = testId;
				Metadata = metaId;
			}
		}

		/// <summary>
		/// Store the test phase result
		/// </summary>
		class TestPhaseSessionDocument : ITestPhaseSession
		{
			[BsonRequired, BsonId]
			public TestPhaseSessionId Id { get; set; }

			[BsonRequired, BsonElement("tid")]
			public TestPhaseId PhaseRef { get; set; }

			[BsonRequired, BsonElement("tsid")]
			public TestSessionId SessionId { get; set; }

			[BsonRequired, BsonElement("sid")]
			public StreamId StreamId { get; set; }

			[BsonRequired, BsonElement("m")]
			public TestMetaId Metadata { get; set; }

			public CommitIdWithOrder BuildCommitId
			{
				get => (BuildCommitName != null) ? new CommitIdWithOrder(BuildCommitName, BuildCommitOrder) : CommitIdWithOrder.FromPerforceChange(BuildCommitOrder);
				set => (BuildCommitName, BuildCommitOrder) = (value.Name, value.Order);
			}

			[BsonIgnoreIfNull, BsonElement("bcn")]
			public string? BuildCommitName { get; set; }

			[BsonRequired, BsonElement("bcl")]
			public int BuildCommitOrder { get; set; }

			[BsonRequired, BsonElement("d")]
			public TimeSpan Duration { get; set; }

			[BsonRequired, BsonElement("s")]
			public DateTime StartDateTime { get; set; }

			[BsonRequired, BsonElement("o")]
			public TestPhaseOutcome Outcome { get; set; }

			[BsonIgnoreIfNull, BsonElement("esp")]
			public string? EventStreamPath { get; set; }

			[BsonIgnoreIfNull, BsonElement("hw")]
			public bool? HasWarning { get; set; }

			[BsonIgnoreIfNull, BsonElement("ef")]
			public string? ErrorFingerprint { get; set; }

			[BsonRequired, BsonElement("jid")]
			public JobId JobId { get; set; }

			[BsonRequired, BsonElement("sjid")]
			public JobStepId StepId { get; set; }

			private TestPhaseSessionDocument()
			{

			}

			public TestPhaseSessionDocument(StreamId streamId, TestSessionId sessionId, TestPhaseId phaseId, TestMetaId metaId)
			{
				Id = TestPhaseSessionId.GenerateNewId();
				StreamId = streamId;
				SessionId = sessionId;
				PhaseRef = phaseId;
				Metadata = metaId;
			}
		}

		/// <summary>
		/// Information about a test data document
		/// </summary>
		class TestDataDocument : ITestData
		{
			public ObjectId Id { get; set; }
			public StreamId StreamId { get; set; }
			public TemplateId TemplateRefId { get; set; }
			public JobId JobId { get; set; }
			public JobStepId StepId { get; set; }

			[BsonIgnore]
			public CommitIdWithOrder CommitId
			{
				get => (CommitName != null) ? new CommitIdWithOrder(CommitName, CommitOrder) : CommitIdWithOrder.FromPerforceChange(CommitOrder);
				set => (CommitName, CommitOrder) = (value.Name, value.Order);
			}

			public string? CommitName { get; set; }

			[BsonElement("Change")]
			public int CommitOrder { get; set; }

			public string Key { get; set; }
			public BsonDocument Data { get; set; }

			private TestDataDocument()
			{
				Key = String.Empty;
				Data = new BsonDocument();
			}

			public TestDataDocument(IJob job, IJobStep jobStep, string key, BsonDocument value)
			{
				Id = ObjectId.GenerateNewId();
				StreamId = job.StreamId;
				TemplateRefId = job.TemplateId;
				JobId = job.Id;
				StepId = jobStep.Id;
				CommitId = job.CommitId;
				Key = key;
				Data = value;
			}
		}

		readonly IMongoCollection<TestDataDocument> _testDataDocuments;
		readonly IMongoCollection<TestMetadataDocument> _testMeta;
		readonly IMongoCollection<TestTagDocument> _testTags;
		readonly IMongoCollection<TestNameDocument> _tests;
		readonly IMongoCollection<TestPhaseDocument> _testPhases;
		readonly IMongoCollection<TestSessionDocument> _testSessions;
		readonly IMongoCollection<TestPhaseSessionDocument> _testPhaseSessions;
		readonly IMongoCollection<TestSessionStreamDocument> _testStreams;
		readonly ICommitService _commitService;
		readonly Tracer _tracer;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public TestDataCollectionV2(IMongoService mongoService, ICommitService commitService, Tracer tracer, ILogger<TestDataCollection> logger)
		{
			_commitService = commitService;
			_tracer = tracer;
			_logger = logger;

			List<MongoIndex<TestDataDocument>> indexes = new List<MongoIndex<TestDataDocument>>();
			_testDataDocuments = mongoService.GetCollection<TestDataDocument>("TestDataV2", indexes);

			List<MongoIndex<TestMetadataDocument>> metaIndexes = new List<MongoIndex<TestMetadataDocument>>();
			metaIndexes.Add(keys => keys.Ascending("m.k").Ascending("m.v"));
			_testMeta = mongoService.GetCollection<TestMetadataDocument>("TestDataV2.Metadata", metaIndexes);

			List<MongoIndex<TestTagDocument>> tagIndexes = new List<MongoIndex<TestTagDocument>>();
			tagIndexes.Add(keys => keys.Ascending(x => x.Name), unique: true);
			_testTags = mongoService.GetCollection<TestTagDocument>("TestDataV2.TestTags", tagIndexes);

			List<MongoIndex<TestNameDocument>> testIndexes = new List<MongoIndex<TestNameDocument>>();
			testIndexes.Add(keys => keys.Ascending(x => x.Key), unique: true);
			_tests = mongoService.GetCollection<TestNameDocument>("TestDataV2.TestNames", testIndexes);

			List<MongoIndex<TestPhaseDocument>> testPhaseIndexes = new List<MongoIndex<TestPhaseDocument>>();
			testPhaseIndexes.Add(keys => keys.Ascending(x => x.TestNameRef).Ascending(x => x.Key), unique: true);
			_testPhases = mongoService.GetCollection<TestPhaseDocument>("TestDataV2.TestPhases", testPhaseIndexes);

			List<MongoIndex<TestSessionDocument>> testSessionIndexes = new List<MongoIndex<TestSessionDocument>>();
			testSessionIndexes.Add(keys => keys.Ascending(x => x.StreamId).Ascending(x => x.NameRef).Descending(x => x.BuildCommitOrder));
			_testSessions = mongoService.GetCollection<TestSessionDocument>("TestDataV2.TestSessions", testSessionIndexes);

			List<MongoIndex<TestPhaseSessionDocument>> testPhaseSessionIndexes = new List<MongoIndex<TestPhaseSessionDocument>>();
			testPhaseSessionIndexes.Add(keys => keys.Ascending(x => x.StreamId).Ascending(x => x.PhaseRef).Descending(x => x.BuildCommitOrder));
			_testPhaseSessions = mongoService.GetCollection<TestPhaseSessionDocument>("TestDataV2.TestPhaseSessions", testPhaseSessionIndexes);

			List<MongoIndex<TestSessionStreamDocument>> streamIndexes = new List<MongoIndex<TestSessionStreamDocument>>();
			streamIndexes.Add(keys => keys.Ascending(x => x.StreamId), unique: true);
			_testStreams = mongoService.GetCollection<TestSessionStreamDocument>("TestDataV2.TestStreams", streamIndexes);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestSessionStream>> FindTestSessionStreamsAsync(StreamId[] streamIds, CancellationToken cancellationToken = default)
		{
			return await _testStreams.Find(Builders<TestSessionStreamDocument>.Filter.In(x => x.StreamId, streamIds)).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestSession>> FindTestSessionsAsync(StreamId[] streamIds, TestId[]? testIds = null, TestMetaId[]? metaIds = null, DateTime? minCreateTime = null, DateTime? maxCreateTime = null, CommitId? minCommitId = null, CommitId? maxCommitId = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestSessionDocument> filter = FilterDefinition<TestSessionDocument>.Empty;
			FilterDefinitionBuilder<TestSessionDocument> filterBuilder = Builders<TestSessionDocument>.Filter;

			filter &= filterBuilder.In(x => x.StreamId, streamIds);

			if (minCreateTime != null)
			{
				TestSessionId minTime = TestSessionId.GenerateNewId(minCreateTime.Value);
				filter &= filterBuilder.Gte(x => x.Id!, minTime);

			}
			if (maxCreateTime != null)
			{
				TestSessionId maxTime = TestSessionId.GenerateNewId(maxCreateTime.Value);
				filter &= filterBuilder.Lte(x => x.Id!, maxTime);
			}

			if (metaIds != null && metaIds.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Metadata, metaIds);
			}

			if (testIds != null && testIds.Length > 0)
			{
				filter &= filterBuilder.In(x => x.NameRef!, testIds);
			}

			if (minCommitId != null)
			{
				int minCommitOrder = minCommitId.GetPerforceChange();
				filter &= filterBuilder.Gte(x => x.BuildCommitOrder, minCommitOrder);
			}
			if (maxCommitId != null)
			{
				int maxCommitOrder = maxCommitId.GetPerforceChange();
				filter &= filterBuilder.Lte(x => x.BuildCommitOrder, maxCommitOrder);
			}

			List<TestSessionDocument> results;

			using (TelemetrySpan _ = _tracer.StartActiveSpan($"{nameof(TestDataCollectionV2)}.{nameof(FindTestSessionsAsync)}"))
			{
				results = await _testSessions.Find(filter).ToListAsync(cancellationToken);
			}

			return results.ConvertAll<ITestSession>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestNameRef>> FindTestNameRefsAsync(TestId[]? testIds = null, string[]? keys = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestNameDocument> filter = FilterDefinition<TestNameDocument>.Empty;
			FilterDefinitionBuilder<TestNameDocument> filterBuilder = Builders<TestNameDocument>.Filter;

			if (testIds != null && testIds.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Id, testIds);
			}

			if (keys != null && keys.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Key, keys);
			}

			return await _tests.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestMetaRef>> FindTestMetaAsync(TestMetaId[]? metaIds = null, IReadOnlyDictionary<string, string>? keyValues = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestMetadataDocument> filter = FilterDefinition<TestMetadataDocument>.Empty;
			FilterDefinitionBuilder<TestMetadataDocument> filterDocBuilder = Builders<TestMetadataDocument>.Filter;
			FilterDefinitionBuilder<TestMetaEntry> metaFilterBuilder = Builders<TestMetaEntry>.Filter;

			if (keyValues != null && keyValues.Count > 0)
			{
				foreach (KeyValuePair<string, string> item in keyValues)
				{
					filter &= filterDocBuilder.ElemMatch(x => x.Entries, metaFilterBuilder.Eq(i => i.Key, item.Key) & metaFilterBuilder.Eq(i => i.Value, item.Value));
				}
			}

			if (metaIds != null && metaIds.Length > 0)
			{
				filter &= filterDocBuilder.In(x => x.Id, metaIds);
			}

			return await _testMeta.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestPhaseRef>> FindTestPhasesAsync(TestId[] testIds, string[]? keys = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestPhaseDocument> filter = FilterDefinition<TestPhaseDocument>.Empty;
			FilterDefinitionBuilder<TestPhaseDocument> filterBuilder = Builders<TestPhaseDocument>.Filter;

			filter &= filterBuilder.In(x => x.TestNameRef, testIds);

			if (keys != null && keys.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Key, keys);
			}

			return await _testPhases.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestTagRef>> FindTestTagsAsync(TestTagId[]? tagIds = null, string[]? names = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestTagDocument> filter = FilterDefinition<TestTagDocument>.Empty;
			FilterDefinitionBuilder<TestTagDocument> filterBuilder = Builders<TestTagDocument>.Filter;

			if (tagIds != null && tagIds.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Id, tagIds);
			}

			if (names != null && names.Length > 0)
			{
				filter &= filterBuilder.In(x => x.Name, names);
			}

			return await _testTags.Find(filter).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestPhaseSession>> FindTestPhaseSessionsAsync(StreamId[] streamIds, TestPhaseId[] phaseIds, DateTime? minCreateTime = null, DateTime? maxCreateTime = null, CommitId? minCommitId = null, CommitId? maxCommitId = null, CancellationToken cancellationToken = default)
		{
			FilterDefinition<TestPhaseSessionDocument> filter = FilterDefinition<TestPhaseSessionDocument>.Empty;
			FilterDefinitionBuilder<TestPhaseSessionDocument> filterBuilder = Builders<TestPhaseSessionDocument>.Filter;

			filter &= filterBuilder.In(x => x.StreamId, streamIds);
			filter &= filterBuilder.In(x => x.PhaseRef, phaseIds);

			if (minCreateTime != null)
			{
				TestPhaseSessionId minTime = TestPhaseSessionId.GenerateNewId(minCreateTime.Value);
				filter &= filterBuilder.Gte(x => x.Id!, minTime);

			}
			if (maxCreateTime != null)
			{
				TestPhaseSessionId maxTime = TestPhaseSessionId.GenerateNewId(maxCreateTime.Value);
				filter &= filterBuilder.Lte(x => x.Id!, maxTime);
			}

			if (minCommitId != null)
			{
				int minCommitOrder = minCommitId.GetPerforceChange();
				filter &= filterBuilder.Gte(x => x.BuildCommitOrder, minCommitOrder);
			}
			if (maxCommitId != null)
			{
				int maxCommitOrder = maxCommitId.GetPerforceChange();
				filter &= filterBuilder.Lte(x => x.BuildCommitOrder, maxCommitOrder);
			}

			List<TestPhaseSessionDocument> results;

			using (TelemetrySpan _ = _tracer.StartActiveSpan($"{nameof(TestDataCollectionV2)}.{nameof(FindTestPhaseSessionsAsync)}"))
			{
				results = await _testPhaseSessions.Find(filter).ToListAsync(cancellationToken);
			}

			return results.ConvertAll<ITestPhaseSession>(x => x);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<ITestData>> AddAsync(IJob job, IJobStep step, (string key, BsonDocument value)[] data, CancellationToken cancellationToken = default)
		{
			// detailed test data
			List <TestDataDocument> documents = new List<TestDataDocument>();
			for (int i = 0; i < data.Length; i++)
			{
				(string key, BsonDocument document) = data[i];

					int version;
				if (document.TryGetInt32("version", out version) || document.TryGetInt32("Version", out version))
				{
					if (version > 1)
					{
						documents.Add(new TestDataDocument(job, step, key, document));
					}
				}
			}

			if (documents.Count == 0)
			{
				return documents;
			}

			await _testDataDocuments.InsertManyAsync(documents, null, cancellationToken);

			try
			{
				await AddTestReportDataAsync(job, step, documents, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Exception while adding test data  report, jobId: {JobId} stepId: {StepId}", job.Id, step.Id);
			}

			return documents;
		}

		/// <inheritdoc/>
		public async Task<ITestData?> GetAsync(ObjectId id, CancellationToken cancellationToken)
		{
			return await _testDataDocuments.Find<TestDataDocument>(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
		}

		// --- (Minimal) Parsing of Automation framework tests, geared towards generating indexes for aggregate queries
		class AutomatedTestSessionData
		{
			public class TestSessionSummary
			{
				[BsonRequired, BsonElement("testName")]
				public string TestName { get; set; } = String.Empty;
				[BsonRequired, BsonElement("dateTime")]
				public string DateTime { get; set; } = String.Empty;
				[BsonRequired, BsonElement("timeElapseSec")]
				public double TimeElapseSec { get; set; }
				[BsonRequired, BsonElement("phasesTotalCount")]
				public int PhasesTotalCount { get; set; }
				[BsonRequired, BsonElement("phasesSucceededCount")]
				public int PhasesSucceededCount { get; set; }
				[BsonRequired, BsonElement("phasesUndefinedCount")]
				public int PhasesUndefinedCount { get; set; }
				[BsonRequired, BsonElement("phasesFailedCount")]
				public int PhasesFailedCount { get; set; }
			}

			public class TestPhase
			{
				[BsonRequired, BsonElement("name")]
				public string Name { get; set; } = String.Empty;
				[BsonRequired, BsonElement("key")]
				public string Key { get; set; } = String.Empty;
				[BsonRequired, BsonElement("dateTime")]
				public string DateTime { get; set; } = String.Empty;
				[BsonRequired, BsonElement("timeElapseSec")]
				public double TimeElapseSec { get; set; }
				[BsonRequired, BsonElement("outcome")]
				public TestPhaseOutcome Outcome { get; set; }
				[BsonIgnoreIfNull, BsonElement("errorFingerprint")]
				public string? ErrorFingerprint { get; set; }
				[BsonIgnoreIfNull, BsonElement("hasWarning")]
				public bool? HasWarning { get; set; }
				[BsonIgnoreIfNull, BsonElement("eventStreamPath")]
				public string? EventStreamPath { get; set; }
			}

			[BsonRequired, BsonElement("summary")]
			public TestSessionSummary? Summary { get; set; }
			[BsonRequired, BsonElement("phases")]
			public List<TestPhase> Phases { get; set; } = new List<TestPhase>();
			[BsonRequired, BsonElement("metadata")]
			public Dictionary<string, string> Metadata { get; set; } = new Dictionary<string, string>();
			[BsonIgnoreIfNull, BsonElement("tags")]
			public List<string>? Tags { get; set; }
		}

		private async Task AddTestReportDataAsync(IJob job, IJobStep step, IReadOnlyList<TestDataDocument> data, CancellationToken cancellationToken = default)
		{
			// do not add preflight to temporal data
			if (job.PreflightCommitId != null)
			{
				return;
			}

			foreach (TestDataDocument document in data)
			{
				AutomatedTestSessionData testSession = BsonSerializer.Deserialize<AutomatedTestSessionData>(document.Data);

				if (testSession.Summary == null)
				{
					throw new Exception($"Missing summary field in test session {document.Key}.");
				}

				if (testSession.Phases == null)
				{
					throw new Exception($"Missing phases field in test session {testSession.Summary.TestName}.");
				}

				if (testSession.Metadata == null)
				{
					throw new Exception($"Missing metadata field in test session {testSession.Summary.TestName}.");
				}

				IReadOnlyList<ITestTagRef>? tags = await AddTestTagsAsync(testSession.Tags, cancellationToken);
				ITestMetaRef metadata = await AddTestMetadataAsync(testSession.Metadata, cancellationToken);
				ITestNameRef testRef = await AddTestAsync(document.Key, testSession.Summary.TestName, metadata.Id, tags?.Select(t => t.Id).ToList(), document.StreamId, cancellationToken);

				// test session
				TestSessionDocument sessionDoc = new TestSessionDocument(document.StreamId, document.Id, testRef.Id, metadata.Id);
				// job and commit
				sessionDoc.JobId = job.Id;
				sessionDoc.StepId = step.Id;
				sessionDoc.BuildCommitId = document.CommitId;
				// phase outcomes and summary
				sessionDoc.Outcome = testSession.Summary.PhasesUndefinedCount > 0 ? TestOutcome.Unspecified :
					testSession.Summary.PhasesFailedCount > 0 ? TestOutcome.Failure :
						testSession.Summary.PhasesTotalCount > 0 && testSession.Summary.PhasesSucceededCount > 0 ? TestOutcome.Success : TestOutcome.Skipped;
				sessionDoc.PhasesFailedCount = testSession.Summary.PhasesFailedCount;
				sessionDoc.PhasesSucceededCount = testSession.Summary.PhasesSucceededCount;
				sessionDoc.PhasesTotalCount = testSession.Summary.PhasesTotalCount;
				sessionDoc.PhasesUndefinedCount = testSession.Summary.PhasesUndefinedCount;
				// timing
				DateTime startDateTime;
				if (DateTime.TryParse(testSession.Summary.DateTime, out startDateTime))
				{
					sessionDoc.StartDateTime = startDateTime;
				}
				sessionDoc.Duration = TimeSpan.FromSeconds(testSession.Summary.TimeElapseSec);

				// phase session
				List<TestPhaseSessionDocument> phaseSessions = new();
				foreach (AutomatedTestSessionData.TestPhase phase in testSession.Phases)
				{
					ITestPhaseRef phaseRef = await AddTestPhaseAsync(testRef.Id, phase.Key, phase.Name, cancellationToken);

					TestPhaseSessionDocument phaseDoc = new TestPhaseSessionDocument(document.StreamId, sessionDoc.Id, phaseRef.Id, metadata.Id);
					// job and commit
					phaseDoc.JobId = job.Id;
					phaseDoc.StepId = step.Id;
					phaseDoc.BuildCommitId = document.CommitId;
					// outcome and event summary
					phaseDoc.Outcome = phase.Outcome;
					phaseDoc.ErrorFingerprint = phase.ErrorFingerprint;
					phaseDoc.EventStreamPath = phase.EventStreamPath;
					phaseDoc.HasWarning = phase.HasWarning;
					// timing
					if (DateTime.TryParse(phase.DateTime, out startDateTime))
					{
						phaseDoc.StartDateTime = startDateTime;
					}
					phaseDoc.Duration = TimeSpan.FromSeconds(phase.TimeElapseSec);

					phaseSessions.Add(phaseDoc);
				}
				if (phaseSessions.Count > 0)
				{
					await _testPhaseSessions.InsertManyAsync(phaseSessions, null, cancellationToken);
				}

				await _testSessions.InsertOneAsync(sessionDoc, null, cancellationToken);
			}
		}

		private async Task<ITestMetaRef> AddTestMetadataAsync(IReadOnlyDictionary<string, string> metaData, CancellationToken cancellationToken)
		{
			FilterDefinition<TestMetadataDocument> filter = FilterDefinition<TestMetadataDocument>.Empty;
			FilterDefinitionBuilder<TestMetadataDocument> filterDocBuilder = Builders<TestMetadataDocument>.Filter;
			FilterDefinitionBuilder<TestMetaEntry> metaFilterBuilder = Builders<TestMetaEntry>.Filter;

			foreach (KeyValuePair<string, string> item in metaData)
			{
				filter &= filterDocBuilder.ElemMatch(x => x.Entries, metaFilterBuilder.Eq(i => i.Key, item.Key) & metaFilterBuilder.Eq(i => i.Value, item.Value));
			}
			// Match the exact number of entries
			filter &= filterDocBuilder.Size(x => x.Entries, metaData.Count);

			TestMetadataDocument? meta = await _testMeta.Find(filter).FirstOrDefaultAsync(cancellationToken);

			if (meta == null)
			{
				meta = new TestMetadataDocument(metaData);
				await _testMeta.InsertOneAsync(meta, null, cancellationToken);
			}
			// update only once per day
			else if (meta.LastSeenUtc < DateTime.UtcNow.AddDays(-1))
			{
				UpdateDefinitionBuilder<TestMetadataDocument> updateBuilder = Builders<TestMetadataDocument>.Update;
				List<UpdateDefinition<TestMetadataDocument>> updates = new List<UpdateDefinition<TestMetadataDocument>>();
				updates.Add(updateBuilder.Set(x => x.LastSeenUtc, DateTime.UtcNow));
				await _testMeta.FindOneAndUpdateAsync(x => x.Id == meta.Id, updateBuilder.Combine(updates), null, cancellationToken);
			}

			return meta;
		}

		private async Task<IReadOnlyList<ITestTagRef>?> AddTestTagsAsync(IReadOnlyList<string>? tags, CancellationToken cancellationToken)
		{
			if (tags == null)
			{
				return null;
			}

			FilterDefinitionBuilder<TestTagDocument> filterBuilder = Builders<TestTagDocument>.Filter;
			FilterDefinition<TestTagDocument> filter = filterBuilder.In(i => i.Name, tags);

			List<TestTagDocument> storedTags = await _testTags.Find(filter).ToListAsync(cancellationToken);
			if (storedTags.Count > 0)
			{
				// update only once per day
				List<TestTagId> needUpdateTags = storedTags.Where(i => i.LastSeenUtc < DateTime.UtcNow.AddDays(-1)).Select(i => i.Id).ToList();
				if (needUpdateTags.Count > 0)
				{
					UpdateDefinitionBuilder<TestTagDocument> updateBuilder = Builders<TestTagDocument>.Update;
					List<UpdateDefinition<TestTagDocument>> updates = new List<UpdateDefinition<TestTagDocument>>();
					updates.Add(updateBuilder.Set(x => x.LastSeenUtc, DateTime.UtcNow));
					FilterDefinition<TestTagDocument> filter2 = filterBuilder.In(i => i.Id, needUpdateTags);
					await _testTags.UpdateManyAsync(filter2, updateBuilder.Combine(updates), null, cancellationToken);
				}
			}
			// add the missing ones
			List<TestTagDocument> missingTags = tags.Except(storedTags.Select(i => i.Name)).Select(t => new TestTagDocument(t)).ToList();
			if (missingTags.Count > 0 )
			{
				await _testTags.InsertManyAsync(missingTags, null, cancellationToken);
				storedTags.AddRange(missingTags);
			}

			return storedTags;
		}

		private async Task<ITestNameRef> AddTestAsync(string key, string name, TestMetaId metadataId, IReadOnlyList<TestTagId>? tags, StreamId streamId, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<TestNameDocument> filterBuilder = Builders<TestNameDocument>.Filter;

			TestNameDocument? test = await _tests.Find(filterBuilder.Eq(i => i.Key, key)).FirstOrDefaultAsync(cancellationToken);

			if (test == null)
			{
				test = new TestNameDocument(key, name);
				await _tests.InsertOneAsync(test, null, cancellationToken);
			}
			// update only once per day
			else if (test.LastSeenUtc < DateTime.UtcNow.AddDays(-1))
			{
				UpdateDefinitionBuilder<TestNameDocument> updateBuilder = Builders<TestNameDocument>.Update;
				List<UpdateDefinition<TestNameDocument>> updates = new List<UpdateDefinition<TestNameDocument>>();
				updates.Add(updateBuilder.Set(x => x.LastSeenUtc, DateTime.UtcNow));
				await _tests.FindOneAndUpdateAsync(x => x.Id == test.Id, updateBuilder.Combine(updates), null, cancellationToken);
			}

			await AddTestToStreamSessionAsync(test.Id, metadataId, tags, streamId, cancellationToken);

			return test;
		}

		private async Task AddTestToStreamSessionAsync(TestId testRef, TestMetaId metadataId, IReadOnlyList<TestTagId>? tags, StreamId streamId, CancellationToken cancellationToken)
		{
			List<TestSessionStreamDocument> streams = await _testStreams.Find(Builders<TestSessionStreamDocument>.Filter.Eq(x => x.StreamId, streamId)).ToListAsync(cancellationToken);

			if (streams.Count == 0)
			{
				TestSessionStreamDocument streamDoc = new TestSessionStreamDocument(streamId);
				streamDoc.Tests.Add(testRef);
				streamDoc.Metadata.Add(metadataId);
				if (tags != null)
				{
					streamDoc.Tags.AddRange(tags);
				}
				await _testStreams.InsertOneAsync(streamDoc, null, cancellationToken);
			}
			else
			{
				UpdateDefinitionBuilder<TestSessionStreamDocument> updateBuilder = Builders<TestSessionStreamDocument>.Update;
				List<UpdateDefinition<TestSessionStreamDocument>> updates = new List<UpdateDefinition<TestSessionStreamDocument>>();

				TestSessionStreamDocument streamDoc = streams[0];

				if (!streamDoc.Tests.Contains(testRef))
				{
					streamDoc.Tests.Add(testRef);
					updates.Add(updateBuilder.Set(x => x.Tests, streamDoc.Tests));
				}
				if (!streamDoc.Metadata.Contains(metadataId))
				{
					streamDoc.Metadata.Add(metadataId);
					updates.Add(updateBuilder.Set(x => x.Metadata, streamDoc.Metadata));
				}
				if (tags != null)
				{
					List<TestTagId> missingTags = tags.Except(streamDoc.Tags).ToList();
					if (missingTags.Count > 0)
					{
						streamDoc.Tags.AddRange(missingTags);
						updates.Add(updateBuilder.Set(x => x.Tags, streamDoc.Tags));
					}
				}

				if (updates.Count > 0)
				{
					FilterDefinitionBuilder<TestSessionStreamDocument> ebuilder = Builders<TestSessionStreamDocument>.Filter;
					FilterDefinition<TestSessionStreamDocument> efilter = ebuilder.Eq(x => x.StreamId, streamDoc.StreamId);
					await _testStreams.UpdateOneAsync(efilter, updateBuilder.Combine(updates), null, cancellationToken);
				}
			}
		}

		private async Task<ITestPhaseRef> AddTestPhaseAsync(TestId testId, string key, string name, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<TestPhaseDocument> filterBuilder = Builders<TestPhaseDocument>.Filter;
			FilterDefinition<TestPhaseDocument> filter = FilterDefinition<TestPhaseDocument>.Empty;

			filter &= filterBuilder.Eq(i => i.TestNameRef, testId);
			filter &= filterBuilder.Eq(i => i.Key, key);

			TestPhaseDocument? phase = await _testPhases.Find(filter).FirstOrDefaultAsync(cancellationToken);

			if (phase == null)
			{
				phase = new TestPhaseDocument(testId, key, name);
				await _testPhases.InsertOneAsync(phase, null, cancellationToken);
			}
			// update only once per day
			else if (phase.LastSeenUtc < DateTime.UtcNow.AddDays(-1))
			{
				UpdateDefinitionBuilder<TestPhaseDocument> updateBuilder = Builders<TestPhaseDocument>.Update;
				List<UpdateDefinition<TestPhaseDocument>> updates = new List<UpdateDefinition<TestPhaseDocument>>();
				updates.Add(updateBuilder.Set(x => x.LastSeenUtc, DateTime.UtcNow));
				await _testPhases.FindOneAndUpdateAsync(x => x.Id == phase.Id, updateBuilder.Combine(updates), null, cancellationToken);
			}

			return phase;
		}

		/// <inheritdoc/>
		public async Task<bool> UpdateAsync(int retainMonths, CancellationToken cancellationToken)
		{
			await ExpireTestDataAsync(retainMonths, cancellationToken);

			return true;
		}

		static async Task<long> ExpireCollectionAsync<DocType>(IMongoCollection<DocType> collection, DateTime expireTime, CancellationToken cancellationToken) where DocType : ITestExpire
		{
			FilterDefinitionBuilder<DocType> filterBuilder = Builders<DocType>.Filter;
			FilterDefinition<DocType> filter = filterBuilder.Empty;
			filter &= filterBuilder.Lte(x => x.LastSeenUtc, expireTime);
			DeleteResult result = await collection.DeleteManyAsync(filter, cancellationToken);

			return result.DeletedCount;
		}

		private async Task ExpireTestDataAsync(int retainMonths, CancellationToken cancellationToken)
		{
			if (retainMonths <= 0)
			{
				return;
			}

			DateTime expireTime = DateTime.Now.AddMonths(-retainMonths);

			// expire and prune root test data
			long testsExpired = await ExpireCollectionAsync(_tests, expireTime, cancellationToken);
			long phasesExpired = await ExpireCollectionAsync(_testPhases, expireTime, cancellationToken);
			long metadataExpired = await ExpireCollectionAsync(_testMeta, expireTime, cancellationToken);
			long tagsExpired = await ExpireCollectionAsync(_testTags, expireTime, cancellationToken);

			// Check whether we need to prune
			if (testsExpired > 0 || phasesExpired > 0 || metadataExpired > 0 || tagsExpired > 0)
			{
				_logger.LogInformation("Expired {TestsExpired} tests, {PhasesExpired} suites, {MetadataExpired} meta, {TagsExpired} tags", testsExpired, phasesExpired, metadataExpired, tagsExpired);
				await PruneDataAsync(cancellationToken);
			}

			// expire test data documents
			{
				ObjectId dataExpireTime = ObjectId.GenerateNewId(expireTime);

				FilterDefinition<TestDataDocument> filter = FilterDefinition<TestDataDocument>.Empty;
				FilterDefinitionBuilder<TestDataDocument> filterBuilder = Builders<TestDataDocument>.Filter;
				filter &= filterBuilder.Lte(x => x.Id!, dataExpireTime);
				DeleteResult result = await _testDataDocuments.DeleteManyAsync(filter, null, cancellationToken);
				if (result.DeletedCount > 0)
				{
					_logger.LogInformation("Expired {NumDeleted} test data documents", result.DeletedCount);
				}
			}

			// expire test sessions
			{
				TestSessionId sessionExpireTime = TestSessionId.GenerateNewId(expireTime);

				FilterDefinition<TestSessionDocument> filter = FilterDefinition<TestSessionDocument>.Empty;
				FilterDefinitionBuilder<TestSessionDocument> filterBuilder = Builders<TestSessionDocument>.Filter;
				filter &= filterBuilder.Lte(x => x.Id!, sessionExpireTime);
				DeleteResult result = await _testSessions.DeleteManyAsync(filter, cancellationToken);
				if (result.DeletedCount > 0)
				{
					_logger.LogInformation("Expired {NumDeleted} test session documents", result.DeletedCount);
				}
			}

			// expire test phase sessions
			{
				TestPhaseSessionId sessionExpireTime = TestPhaseSessionId.GenerateNewId(expireTime);
				FilterDefinition<TestPhaseSessionDocument> filter = FilterDefinition<TestPhaseSessionDocument>.Empty;
				FilterDefinitionBuilder<TestPhaseSessionDocument> filterBuilder = Builders<TestPhaseSessionDocument>.Filter;
				filter &= filterBuilder.Lte(x => x.Id!, sessionExpireTime);
				DeleteResult result = await _testPhaseSessions.DeleteManyAsync(filter, cancellationToken);
				if (result.DeletedCount > 0)
				{
					_logger.LogInformation("Expired {NumDeleted} test phase session documents", result.DeletedCount);
				}
			}
		}

		/// <inheritdoc/>
		public async Task PruneDataAsync(CancellationToken cancellationToken = default)
		{
			HashSet<TestId> testIds = new HashSet<TestId>();
			foreach (TestId testId in await (await _tests.DistinctAsync(x => x.Id, Builders<TestNameDocument>.Filter.Empty, null, cancellationToken)).ToListAsync(cancellationToken))
			{
				testIds.Add(testId);
			}
			_logger.LogInformation("Test data pruning to {TestIdCount} test name ids", testIds.Count);

			HashSet<TestMetaId> metadataIds = new HashSet<TestMetaId>();
			foreach (TestMetaId metaId in await (await _testMeta.DistinctAsync(x => x.Id, Builders<TestMetadataDocument>.Filter.Empty, null, cancellationToken)).ToListAsync(cancellationToken))
			{
				metadataIds.Add(metaId);
			}
			_logger.LogInformation("Test data pruning to {MetadataIdCount} test metadata ids", metadataIds.Count);

			HashSet<TestTagId> tagIds = new HashSet<TestTagId>();
			foreach (TestTagId tagId in await (await _testTags.DistinctAsync(x => x.Id, Builders<TestTagDocument>.Filter.Empty, null, cancellationToken)).ToListAsync(cancellationToken))
			{
				tagIds.Add(tagId);
			}
			_logger.LogInformation("Test data pruning to {TagIdCount} test tag ids", tagIds.Count);

			int testStreamsDeleted = 0;

			// update test session streams
			{
				List<TestSessionStreamDocument> testStreams = await _testStreams.Find(Builders<TestSessionStreamDocument>.Filter.Empty).ToListAsync(cancellationToken);
				foreach (TestSessionStreamDocument stream in testStreams)
				{
					List<TestId> tests = stream.Tests.Where(x => testIds.Contains(x)).ToList();
					if (tests.Count == 0)
					{
						await _testStreams.DeleteOneAsync(x => x.Id == stream.Id, cancellationToken);
						testStreamsDeleted++;
						continue;
					}

					List<TestMetaId> metadata = stream.Metadata.Where(x => metadataIds.Contains(x)).ToList();
					List<TestTagId> tags = stream.Tags.Where(x => tagIds.Contains(x)).ToList();
					if (tests.Count != stream.Tests.Count || metadata.Count != stream.Metadata.Count || tags.Count != stream.Tags.Count)
					{
						UpdateDefinitionBuilder<TestSessionStreamDocument> updateBuilder = Builders<TestSessionStreamDocument>.Update;
						List<UpdateDefinition<TestSessionStreamDocument>> updates = new List<UpdateDefinition<TestSessionStreamDocument>>();

						if (tests.Count != stream.Tests.Count)
						{
							updates.Add(updateBuilder.Set(x => x.Tests, tests));
						}
						if (metadata.Count != stream.Metadata.Count)
						{
							updates.Add(updateBuilder.Set(x => x.Metadata, metadata));
						}
						if (tags.Count != stream.Tags.Count)
						{
							updates.Add(updateBuilder.Set(x => x.Tags, tags));
						}

						await _testStreams.FindOneAndUpdateAsync(x => x.Id == stream.Id, updateBuilder.Combine(updates), null, cancellationToken);
					}
				}
			}

			if (testStreamsDeleted > 0)
			{
				_logger.LogInformation("Pruned {TestStreamDeleteCount} test session streams", testStreamsDeleted);
			}
		}
	}
}
