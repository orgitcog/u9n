// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Commits;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Jobs.TestData;
using EpicGames.Horde.Streams;
using MongoDB.Bson;

namespace HordeServer.Jobs.TestData
{
	/// <summary>
	/// Defines a testing environment based on platforms, configurations, targets, etc.
	/// </summary>
	public interface ITestMeta
	{
		/// <summary>
		/// The test meta id
		/// </summary>
		TestMetaId Id { get; }

		/// <summary>
		/// The name of the test platform
		/// </summary>
		IReadOnlyList<string> Platforms { get; }

		/// <summary>
		/// The configuration the test was run on
		/// </summary>
		IReadOnlyList<string> Configurations { get; }

		/// <summary>
		/// The build target, editor, server, client, etc
		/// </summary>
		IReadOnlyList<string> BuildTargets { get; }

		/// <summary>
		/// The uproject name associated with this test, note: may not be directly related to Horde project
		/// </summary>
		string ProjectName { get; }

		/// <summary>
		/// The rendering hardware interface used for the test
		/// </summary>
		string RHI { get; }

		/// <summary>
		/// The variation of the meta data, for example address sanitizing
		/// </summary>
		string Variation { get; }
	}

	/// <summary>
	/// A test that runs in a stream
	/// </summary>
	public interface ITest
	{
		/// <summary>
		/// The test id
		/// </summary>
		TestId Id { get; }

		/// <summary>
		/// The fully qualified name of the test 
		/// </summary>
		string Name { get; }

		/// <summary>
		/// The display name of the test 
		/// </summary>
		string? DisplayName { get; }

		/// <summary>
		/// The name of the associated suite if any
		/// </summary>
		string? SuiteName { get; }

		/// <summary>
		/// The meta data for the test 
		/// </summary>
		IReadOnlyList<TestMetaId> Metadata { get; }
	}

	/// <summary>
	/// A test suite that runs in a stream
	/// </summary>
	public interface ITestSuite
	{
		/// <summary>
		/// The test suite id
		/// </summary>
		TestSuiteId Id { get; }

		/// <summary>
		/// The name of the test suite
		/// </summary>
		string Name { get; }

		/// <summary>
		/// The tests that compose the suite
		/// </summary>
		IReadOnlyList<TestId> Tests { get; }

		/// <summary>
		/// The meta data for the test suite
		/// </summary>
		IReadOnlyList<TestMetaId> Metadata { get; }
	}

	/// <summary>
	/// Suite test data
	/// </summary>
	public interface ISuiteTestData
	{
		/// <summary>
		/// The test id
		/// </summary>
		TestId TestId { get; }

		/// <summary>
		/// The outcome of the suite test
		/// </summary>
		TestOutcome Outcome { get; }

		/// <summary>
		/// How long the suite test ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// Test UID for looking up in test details
		/// </summary>
		string UID { get; }

		/// <summary>
		/// The number of warnings
		/// </summary>
		int? WarningCount { get; }

		/// <summary>
		/// The number of errors
		/// </summary>
		int? ErrorCount { get; }
	}

	/// <summary>
	/// Data ref with minimal data required for aggregate views
	/// </summary>
	public interface ITestDataRef
	{
		/// <summary>
		/// The test ref id
		/// </summary>
		TestRefId Id { get; }

		/// <summary>
		/// The associated stream
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The associated job
		/// </summary>
		JobId? JobId { get; }

		/// <summary>
		/// The associated job step
		/// </summary>
		JobStepId? StepId { get; }

		/// <summary>
		/// How long the test ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// The build changelist upon which the test ran, may not correspond to the job changelist
		/// </summary>
		CommitIdWithOrder BuildCommitId { get; }

		/// <summary>
		/// The environment the test ran in
		/// </summary>
		TestMetaId Metadata { get; }

		/// <summary>
		/// The ITest in stream
		/// </summary>
		TestId? TestId { get; }

		/// <summary>
		/// The outcome of the test
		/// </summary>
		TestOutcome? Outcome { get; }

		/// <summary>
		/// The ITestSuite in stream
		/// </summary>
		TestSuiteId? SuiteId { get; }

		/// <summary>
		/// Suite tests skipped
		/// </summary>
		int? SuiteSkipCount { get; }

		/// <summary>
		/// Suite test warnings
		/// </summary>
		int? SuiteWarningCount { get; }

		/// <summary>
		/// Suite test errors
		/// </summary>
		int? SuiteErrorCount { get; }

		/// <summary>
		/// Suite test successes
		/// </summary>
		int? SuiteSuccessCount { get; }
	}

	/// <summary>
	/// Test data details
	/// </summary>
	public interface ITestDataDetails
	{
		/// <summary>
		/// The corresponding test ref		
		/// </summary>
		TestRefId Id { get; }

		/// <summary>
		/// The full details test data for this ref
		/// </summary>
		IReadOnlyList<ObjectId> TestDataIds { get; }

		/// <summary>
		/// Suite test data
		/// </summary>		
		IReadOnlyList<ISuiteTestData>? SuiteTests { get; }
	}

	/// <summary>
	/// The tests and suites running in a given stream
	/// </summary>
	public interface ITestStream
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// Test ids of tests running in the stream
		/// </summary>
		IReadOnlyList<TestId> Tests { get; }

		/// <summary>
		/// Test suite ids
		/// </summary>
		IReadOnlyList<TestSuiteId> TestSuites { get; }
	}

	/// v2

	/// <summary>
	/// A test name reference
	/// </summary>
	public interface ITestNameRef
	{
		/// <summary>
		/// The test id
		/// </summary>
		TestId Id { get; }

		/// <summary>
		/// The test data key that identify the test uniquely
		/// </summary>
		string Key { get; }

		/// <summary>
		/// The display name of the test
		/// </summary>
		string Name { get; }
	}

	/// <summary>
	/// A test phase reference
	/// </summary>
	public interface ITestPhaseRef
	{
		/// <summary>
		/// The test phase id
		/// </summary>
		TestPhaseId Id { get; }

		/// <summary>
		/// The test reference associated with the phase
		/// </summary>
		TestId TestNameRef { get; }

		/// <summary>
		/// The test phase key that identify the phase uniquely
		/// </summary>
		string Key { get; }

		/// <summary>
		/// The test phase (display) name
		/// </summary>
		string Name { get; }
	}

	/// <summary>
	/// A metadata entry key value pair
	/// </summary>
	public interface ITestMetaEntry
	{
		/// <summary>
		/// The meta key
		/// </summary>
		string Key { get; }

		/// <summary>
		/// The meta value
		/// </summary>
		string Value { get; }
	}

	/// <summary>
	/// A metadata key/value pairs reference
	/// </summary>
	public interface ITestMetaRef
	{
		/// <summary>
		/// The metadata id
		/// </summary>
		TestMetaId Id { get; }

		/// <summary>
		/// The key/value pairs that this metadata references
		/// </summary>
		IReadOnlyList<ITestMetaEntry> Entries { get; }
	}

	/// <summary>
	/// A test tag reference
	/// </summary>
	public interface ITestTagRef
	{
		/// <summary>
		/// The tag id
		/// </summary>
		TestTagId Id { get; }

		/// <summary>
		/// The tag name
		/// </summary>
		string Name { get; }
	}

	/// <summary>
	/// Test session
	/// </summary>
	public interface ITestSession
	{
		/// <summary>
		/// The test session id
		/// </summary>
		TestSessionId Id { get; }

		/// <summary>
		/// The metadata associated with the test session
		/// </summary>
		TestMetaId Metadata { get; }

		/// <summary>
		/// The tags associated with the test session
		/// </summary>
		IReadOnlyList<TestTagId>? Tags { get; }

		/// <summary>
		/// The test data id for this test session
		/// </summary>
		ObjectId TestDataId { get; }

		/// <summary>
		/// The build changelist upon which the test ran, may not correspond to the job changelist
		/// </summary>
		CommitIdWithOrder BuildCommitId { get; }

		/// <summary>
		/// Reference Id to test name
		/// </summary>
		TestId NameRef { get; }

		/// <summary>
		/// The associated stream
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The associated job
		/// </summary>
		JobId JobId { get; }

		/// <summary>
		/// The associated job step
		/// </summary>
		JobStepId StepId { get; }

		/// <summary>
		/// How long the test session ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// Date time at which the test session was started
		/// </summary>
		DateTime StartDateTime { get; }

		/// <summary>
		/// The outcome of the test session
		/// if PhasesUndefinedCount > 0 => Undefiened
		/// if PhasesFailedCount > 0 => Failure
		/// if PhasesTotalCount > 0 and PhasesSucceededCount > 0 => Success
		/// else => Skipped
		/// </summary>
		TestOutcome Outcome { get; }

		/// <summary>
		/// The total number the test session contains
		/// </summary>
		int PhasesTotalCount { get; }

		/// <summary>
		/// The number of phases that succeeded during the test session
		/// </summary>
		int PhasesSucceededCount { get; }

		/// <summary>
		/// The number of phases that were left undefined during the test session
		/// </summary>
		int PhasesUndefinedCount { get; }

		/// <summary>
		/// The number of phases that failed during the test session
		/// </summary>
		int PhasesFailedCount { get; }
	}

	/// <summary>
	/// The test sessions running in a given stream
	/// </summary>
	public interface ITestSessionStream
	{
		/// <summary>
		/// The stream id
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// Test ids of tests running in the stream
		/// </summary>
		IReadOnlyList<TestId> Tests { get; }

		/// <summary>
		/// Test metadata ids from running tests in the stream 
		/// </summary>
		IReadOnlyList<TestMetaId> Metadata { get; }

		/// <summary>
		/// Test tag ids from running tests in the stream
		/// </summary>
		IReadOnlyList<TestTagId> Tags { get; }
	}

	/// <summary>
	/// A Test phase outcome from a test session
	/// </summary>
	public interface ITestPhaseSession
	{
		/// <summary>
		/// The test phase session id
		/// </summary>
		TestPhaseSessionId Id { get; }

		/// <summary>
		/// The test phase reference id
		/// </summary>
		TestPhaseId PhaseRef { get; }

		/// <summary>
		/// The phase outcome
		/// </summary>
		TestPhaseOutcome Outcome { get; }

		/// <summary>
		/// The test session that run that phase
		/// </summary>
		TestSessionId SessionId { get; }

		/// <summary>
		/// The metadata associated with this test session
		/// </summary>
		TestMetaId Metadata { get; }

		/// <summary>
		/// The path to the event stream
		/// </summary>
		string? EventStreamPath { get; }

		/// <summary>
		/// Whether the test phase encountered at least one warning
		/// </summary>
		bool? HasWarning { get;  }

		/// <summary>
		/// The error fingerprint from the event stream
		/// </summary>
		string? ErrorFingerprint { get; }

		/// <summary>
		/// The associated stream
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The associated job
		/// </summary>
		JobId JobId { get; }

		/// <summary>
		/// The associated job step
		/// </summary>
		JobStepId StepId { get; }

		/// <summary>
		/// The build changelist upon which the test ran, may not correspond to the job changelist
		/// </summary>
		CommitIdWithOrder BuildCommitId { get; }

		/// <summary>
		/// How long the test phase session ran
		/// </summary>
		TimeSpan Duration { get; }

		/// <summary>
		/// Date time at which the test phase session was started
		/// </summary>
		DateTime StartDateTime { get; }
	}

	/// <summary>
	/// Stores information about the results of a test
	/// </summary>
	public interface ITestData
	{
		/// <summary>
		/// Unique id of the test data
		/// </summary>
		ObjectId Id { get; }

		/// <summary>
		/// Stream that generated the test data
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// The template reference id
		/// </summary>
		TemplateId TemplateRefId { get; }

		/// <summary>
		/// The job which produced the data
		/// </summary>
		JobId JobId { get; }

		/// <summary>
		/// The step that ran
		/// </summary>
		JobStepId StepId { get; }

		/// <summary>
		/// The commit that contained the data
		/// </summary>
		CommitIdWithOrder CommitId { get; }

		/// <summary>
		/// Key used to identify the particular data
		/// </summary>
		string Key { get; }

		/// <summary>
		/// The data stored for this test
		/// </summary>
		BsonDocument Data { get; }
	}
}
