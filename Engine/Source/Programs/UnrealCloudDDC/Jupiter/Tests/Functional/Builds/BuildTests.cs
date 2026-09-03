// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Builds
{
	public abstract class BuildTests
	{
		protected BuildTests(string namespaceSuffix)
		{
			TestNamespace = new NamespaceId($"test-builds-{namespaceSuffix}");
		}

		private static (byte[],byte[]) CreateBlockContents(params byte[][] parameters)
		{
			CbWriter indexWriter = new CbWriter();
			indexWriter.BeginObject();

			int lengthParams = parameters.Sum(bytes => bytes.Length);
			byte[] blockData = new byte[lengthParams];

			int offset = 0;
			foreach (byte[] parameter in parameters)
			{
				Array.Copy(parameter, 0, blockData, offset, parameter.Length);

				offset += parameter.Length;
			}

			indexWriter.BeginUniformArray("rawHashes", CbFieldType.Hash);
			foreach (byte[] parameter in parameters)
			{
				indexWriter.WriteHashValue(IoHash.Compute(parameter));
			}
			indexWriter.EndUniformArray();

			indexWriter.BeginObject("metadata");
			indexWriter.WriteString("createdBy", "jupiter-tests");
			indexWriter.EndObject();
			indexWriter.EndObject();
			byte[] indexBuffer = indexWriter.ToByteArray();

			return (blockData, indexBuffer);
		}

		private TestServer? _server;
		private HttpClient? _httpClient;
		protected NamespaceId TestNamespace { get; init; }
		protected BucketId BucketId { get; init; } = new BucketId("project.type.branch.platform");
		protected BucketId SearchBucketId { get; init; } = new BucketId("searchProject.type.branch.platform");

		private byte[] _looseFileContents = null!;
		private byte[] _smallFile1Contents = null!;
		private byte[] _smallFile2Contents = null!;
		private byte[] _smallFile3Contents = null!;
		private byte[] _largeFileContents = null!;
		
		private byte[] _block0Contents = null!;
		private BlobId _block0Hash = null!;
		private byte[] _block0Index = null!;

		private byte[] _block1Contents = null!;
		private BlobId _block1Hash = null!;
		private byte[] _block1Index = null!;

		private BlobId _looseFileHash = null!;
		private BlobId _largeFileHash = null!;

		private byte[] _existingBlockContents = null!;
		private BlobId _existingBlockBlobId = null!;
		private byte[] _existingBlockIndex = null!;

		// base line for when builds got created so we can do comparision search
		private DateTime _creationBaseline;

		private CbObjectId _searchBuild0Id;
		private CbObjectId _searchBuild1Id;
		private CbObjectId _searchBuild2Id;
		private CbObjectId _searchBuild3Id;
		private CbObjectId _searchBuild4Id;
		private CbObjectId _searchBuild5Id;

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(GetSettings())
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			TestServer server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.UseStartup<JupiterStartup>()
			);
			_httpClient = server.CreateClient();
			_server = server;

			// Seed storage
			await SeedAsync(_server.Services);
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettings();

		protected virtual async Task SeedAsync(IServiceProvider services)
		{
			CompressedBufferUtils compressedBufferUtils = services.GetService<CompressedBufferUtils>()!;

			// read the raw contents
			byte[] looseFileContents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/loose-file.bin");
			byte[] smallFile1Contents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/small-file-1.bin");
			byte[] smallFile2Contents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/small-file-2.bin");
			byte[] smallFile3Contents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/small-file-3.bin");
			byte[] largeFileContents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/large-file.bin");

			// calculate hash of the uncompressed content
			_looseFileHash = BlobId.FromBlob(looseFileContents);
			_largeFileHash = BlobId.FromBlob(largeFileContents);

			// compress the content and use that
			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, looseFileContents);
				_looseFileContents = ms.ToArray();
			}

			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, smallFile1Contents);
				_smallFile1Contents = ms.ToArray();
			}

			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, smallFile2Contents);
				_smallFile2Contents = ms.ToArray();
			}

			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, smallFile3Contents);
				_smallFile3Contents = ms.ToArray();
			}

			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, largeFileContents);
				_largeFileContents = ms.ToArray();
			}

			(_block0Contents, _block0Index) = CreateBlockContents(_smallFile1Contents, _smallFile2Contents);
			_block0Hash = BlobId.FromBlob(_block0Contents);

			(byte[] block1Contents, _block1Index) = CreateBlockContents(_smallFile1Contents, _smallFile2Contents, smallFile3Contents);
			_block1Hash = BlobId.FromBlob(block1Contents);
			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, block1Contents);
				_block1Contents = ms.ToArray();
			}

			byte[] file1Contents = await File.ReadAllBytesAsync("Builds/TestData/ExistingBlocks/file-1.bin");
			byte[] file2Contents = await File.ReadAllBytesAsync("Builds/TestData/ExistingBlocks/file-2.bin");

			(_existingBlockContents, _existingBlockIndex) = CreateBlockContents(file1Contents, file2Contents);
			_existingBlockBlobId = BlobId.FromBlob(_existingBlockContents);
			BlobId blockMetadataId = BlobId.FromBlob(_existingBlockIndex);

			IBlobService blobService = services.GetService<IBlobService>()!;
			await blobService.PutObjectAsync(TestNamespace, _existingBlockContents, _existingBlockBlobId);
			await blobService.PutObjectAsync(TestNamespace, _existingBlockIndex, blockMetadataId);

			IBlockStore blockStore = services.GetService<IBlockStore>()!;
			{
				BlockContext blockContext = new BlockContext("test-project-blocks.branch-blocks.windows");
				await blockStore.PutBlockMetadataAsync(TestNamespace, _existingBlockBlobId, blockMetadataId);
				await blockStore.AddBlockToContextAsync(TestNamespace, blockContext, blockMetadataId);
			}

			{
				BlockContext blockContext = new BlockContext("test-project-blocks.branch-cb-blocks.windows");
				await blockStore.AddBlockToContextAsync(TestNamespace, blockContext, blockMetadataId);
			}

			(CbObjectId, CbObject) GetSearchBuild0(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				writer.WriteInteger("changelist", 40091037);
				writer.WriteString("platform", "windows");
				writer.WriteDateTime("createdAt", baseline);
				writer.WriteString("name", "search-build-0");

				writer.EndObject();
				return (CbObjectId.Parse("0e29888300000001f13983ed"), writer.ToObject());
			}

			(CbObjectId, CbObject) GetSearchBuild1(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				// is a double on purpose to verify that we can handle implicit conversions between int and double
				writer.WriteDouble("changelist", 40091040);
				writer.WriteString("platform", "windows");
				writer.WriteDateTime("createdAt", baseline.AddHours(1.1));
				writer.WriteString("name", "search-build-1");

				writer.EndObject();
				return (CbObjectId.Parse("0e29888900000002f13983ed"), writer.ToObject());
			}

			(CbObjectId, CbObject) GetSearchBuild2(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				writer.WriteInteger("changelist", 40091045);
				writer.WriteString("platform", "windows");
				writer.WriteDateTime("createdAt", baseline.AddHours(2.1));
				writer.WriteString("name", "search-build-2");

				writer.EndObject();
				return (CbObjectId.Parse("0e29888b00000003f13983ed"), writer.ToObject());
			}

			(CbObjectId, CbObject) GetSearchBuild3(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				writer.WriteInteger("changelist", 40091050);
				writer.WriteString("platform", "windows");
				writer.WriteDateTime("createdAt", baseline.AddHours(3.1));
				writer.WriteString("name", "search-build-3");

				writer.EndObject();
				return (CbObjectId.Parse("0e29888c00000004f13983ed"), writer.ToObject());
			}

			(CbObjectId, CbObject) GetSearchBuild4(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				// this is string on purpose to validate that we can handle fields accidentally written as strings
				writer.WriteString("changelist", "40091055"); 
				writer.WriteString("platform", "windows");
				writer.WriteDateTime("createdAt", baseline.AddHours(4.1));
				writer.WriteString("name", "search-build-4");

				writer.EndObject();
				return (CbObjectId.Parse("0e29888e00000005f13983ed"), writer.ToObject());
			}

			(CbObjectId, CbObject) GetSearchBuild5(DateTime baseline)
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();

				writer.WriteInteger("changelist", 40091060);
				writer.WriteString("platform", "linux");
				writer.WriteDateTime("createdAt", baseline.AddHours(5.1));
				writer.WriteString("name", "search-build-5");

				writer.EndObject();
				return (CbObjectId.Parse("0e29889000000006f13983ed"), writer.ToObject());
			}

			_creationBaseline = DateTime.Now.AddDays(-1);
			IBuildStore buildStore = services.GetService<IBuildStore>()!;
			uint ttl = 5000u;
			// build 0 is the oldest build while build 5 is the newest build
			(_searchBuild0Id, CbObject searchBuild0Object) = GetSearchBuild0(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild0Id, searchBuild0Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild0Id, ttl);

			(_searchBuild1Id, CbObject searchBuild1Object) = GetSearchBuild1(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild1Id, searchBuild1Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild1Id, ttl);

			(_searchBuild2Id, CbObject searchBuild2Object) = GetSearchBuild2(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild2Id, searchBuild2Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild2Id, ttl);

			(_searchBuild3Id, CbObject searchBuild3Object) = GetSearchBuild3(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild3Id, searchBuild3Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild3Id, ttl);
			
			(_searchBuild4Id, CbObject searchBuild4Object) = GetSearchBuild4(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild4Id, searchBuild4Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild4Id, ttl);

			(_searchBuild5Id, CbObject searchBuild5Object) = GetSearchBuild5(_creationBaseline);
			await buildStore.PutBuildAsync(TestNamespace, SearchBucketId, _searchBuild5Id, searchBuild5Object, ttl);
			await buildStore.FinalizeBuildAsync(TestNamespace, SearchBucketId, _searchBuild5Id, ttl);
		}

		protected abstract Task Teardown(IServiceProvider provider);

		[TestCleanup]
		public async Task MyTeardownAsync()
		{
			await Teardown(_server!.Services);
		}

		/// <summary>
		/// uploads and fetches a new build that does not exist
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task GetPutDeleteNewBuildAsync()
		{
			CbObject sampleBuild = GetSampleBuildObject();
			CbObject sampleBuildPartObject = GetSampleBuildPartObject();
			string partName = "samplePart";
			CbObjectId buildId =  CbObjectId.NewObjectId();
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("d12ac1ba0000000150aca240");

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuild.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the first build part
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all files should be missing
				Assert.AreEqual(3, response.Needs.Length);
			}

			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_looseFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_looseFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			
			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block
			{
				using HttpContent requestContent = new ByteArrayContent(_block0Contents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block0Hash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block metadata
			{
				using HttpContent requestContent = new ByteArrayContent(_block0Index);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block0Hash}/metadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all parts are now present
				Assert.AreEqual(0, response.Needs.Length);
			}

			// finish the build upload
			{
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/finalize", UriKind.Relative), null);
				result.EnsureSuccessStatusCode();
			}

			// fetch the submitted build
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.uecb", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] response = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(response);

				Assert.AreEqual("windows", o["platform"].AsString());
				Assert.AreEqual("test-project", o["project"].AsString());
				Assert.AreEqual("branch", o["branch"].AsString());

				// verify that the parts of the build are listed
				Assert.AreNotEqual(CbField.Empty, o.Find("parts"), "parts field missing");
				CbObject parts = o["parts"].AsObject();
				Assert.AreNotEqual(CbObject.Empty, parts);
				Assert.AreEqual(1, parts.Count());
				CbField part = parts.First();
				Utf8String foundPartName = part.Name;
				CbObjectId partId = part.AsObjectId();

				Assert.AreEqual("samplePart", foundPartName.ToString());
				Assert.AreEqual(objectId, partId);
			}

			// fetch the submitted build part
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] response = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(response);

				CollectionAssert.AreEqual(o.ToList(), sampleBuildPartObject.ToList());
			}

			// copy the build
			BucketId copyBuildBucketId = new BucketId("project.type.copy-branch.platform");
			CbObjectId copyBuildId;
			{
				CopyBuildRequest copyBuildRequest = new CopyBuildRequest() {BucketId = copyBuildBucketId, BuildId = buildId, NewBranch = "copy-branch"};
				using HttpContent content = JsonContent.Create(copyBuildRequest);
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/copyBuild", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", MediaTypeNames.Application.Json);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				CopyBuildResponse? response = await result.Content.ReadFromJsonAsync<CopyBuildResponse>();

				Assert.IsNotNull(response);
				copyBuildId = response.BuildId;
			}

			// delete the original build
			{
				using HttpResponseMessage result = await _httpClient!.DeleteAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
			}

			// make sure the build and part can not be found
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}

			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
			
			// delete the copied build
			{
				using HttpResponseMessage result = await _httpClient!.DeleteAsync(new Uri($"api/v2/builds/{TestNamespace}/{copyBuildBucketId}/{copyBuildId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
			}

			// make sure the build and part can not be found for the copied build
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{copyBuildBucketId}/{copyBuildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}

			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{copyBuildBucketId}/{copyBuildId}/parts/{objectId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
		}

		/// <summary>
		/// uploads and fetches a new build that does not exist
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task GetPutDeleteBuildCompressedBufferAsync()
		{
			CbObject sampleBuild = GetSampleCompressedBufferBuildObject();
			CbObject sampleBuildPartObject = GetSampleCompressedBufferBuildPartObject();
			string partName = "samplePartCbBuffer";
			CbObjectId buildId =  CbObjectId.NewObjectId();
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("e23ac1ba0000000150aca240");

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuild.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the first build part
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all files should be missing
				Assert.AreEqual(3, response.Needs.Length);
			}

			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_looseFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_looseFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			
			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block
			{
				using HttpContent requestContent = new ByteArrayContent(_block1Contents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block1Hash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block metadata
			{
				using HttpContent requestContent = new ByteArrayContent(_block1Index);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block1Hash}/metadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all parts are now present
				Assert.AreEqual(0, response.Needs.Length);
			}

			// finish the build upload
			{
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/finalize", UriKind.Relative), null);
				result.EnsureSuccessStatusCode();
			}

			// fetch the submitted build
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.uecb", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] response = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(response);

				Assert.AreEqual("windows", o["platform"].AsString());
				Assert.AreEqual("test-project", o["project"].AsString());
				Assert.AreEqual("branch", o["branch"].AsString());

				// verify that the parts of the build are listed
				Assert.AreNotEqual(CbField.Empty, o.Find("parts"), "parts field missing");
				CbObject parts = o["parts"].AsObject();
				Assert.AreNotEqual(CbObject.Empty, parts);
				Assert.AreEqual(1, parts.Count());
				CbField part = parts.First();
				Utf8String foundPartName = part.Name;
				CbObjectId partId = part.AsObjectId();

				Assert.AreEqual("samplePartCbBuffer", foundPartName.ToString());
				Assert.AreEqual(objectId, partId);
			}

			// fetch the submitted build part
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] response = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(response);

				CollectionAssert.AreEqual(o.ToList(), sampleBuildPartObject.ToList());
			}

			// delete the build
			{
				using HttpResponseMessage result = await _httpClient!.DeleteAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
			}

			// make sure the build and part can not be found
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}

			// fetch the submitted build part
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
		}

		/// <summary>
		/// lists blocks to find a block to use and submits new blocks
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task ListAndUseBlocksAsync()
		{
			// register a build so we can reason about its block context
			// this build references an existing block as well as a new one
			CbObject blockBuild = GetBlockBuildObject("block-build", "branch-blocks");
			CbObjectId buildId = CbObjectId.Parse("d12c8f4100000002aea6602c");
			(byte[] newBlockContents, byte[] newBlockIndex) = CreateBlockContents(_smallFile2Contents);
			BlobId newBlockHash = BlobId.FromBlob(newBlockContents);

			CbObject blockBuildPartObject = GetBlockBuildPartObject(newBlockHash);
			CbObjectId objectId = CbObjectId.Parse("d12c8f4100000001aea6602c");
			string partName = "block-build";

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(blockBuild.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// list blocks, one block should already exist
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/listBlocks", UriKind.Relative));
				if (result.StatusCode == HttpStatusCode.InternalServerError)
				{
					string errorMsg = await result.Content.ReadAsStringAsync();
					Assert.Fail($"500 response with error: {errorMsg}");
				}
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(1, blocks.Count);
				// Check that the one block that did exist was the one we expected
				foreach (CbField field in blocks)
				{
					CollectionAssert.AreEqual(_existingBlockIndex, field.AsObject().GetView().ToArray());	
				}
			}

			// upload the new block
			{
				using HttpContent requestContent = new ByteArrayContent(newBlockContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{newBlockHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the new block metadata
			{
				using HttpContent requestContent = new ByteArrayContent(newBlockIndex);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{newBlockHash}/metadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(blockBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all parts are now present
				Assert.AreEqual(0, response.Needs.Length);
			}

			// finish the build upload
			{
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/finalize", UriKind.Relative), null);
				result.EnsureSuccessStatusCode();
			}

			// list blocks again, make sure that the new block is found
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/listBlocks", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(2, blocks.Count);
				List<CbField> arrayFields = blocks.ToList();
				ReadOnlyMemory<byte> view0;
				Assert.IsTrue(arrayFields[0].TryGetView(out view0));
				byte[] bytes0 = view0.ToArray();

				ReadOnlyMemory<byte> view1;
				Assert.IsTrue(arrayFields[1].TryGetView(out view1));
				byte[] bytes1 = view1.ToArray();

				// the blocks were just updated in the finalize, thus the sorting is not strictly defined (as they are both used in the lastest build and thus their use time is now)
				CollectionAssert.AreEqual(bytes0.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes0);
				CollectionAssert.AreEqual(bytes1.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes1);
			}

			// fetch the blocks metadata and verify its correct
			{
				CbWriter blockMetadataRequestWriter = new CbWriter();
				blockMetadataRequestWriter.BeginObject();
				blockMetadataRequestWriter.BeginArray("blocks");
				blockMetadataRequestWriter.WriteHashValue(_existingBlockBlobId.AsIoHash());
				blockMetadataRequestWriter.WriteHashValue(newBlockHash.AsIoHash());
				blockMetadataRequestWriter.EndArray();
				blockMetadataRequestWriter.EndObject();
				using HttpContent requestContent = new ByteArrayContent(blockMetadataRequestWriter.ToByteArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/getBlockMetadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(2, blocks.Count);
				List<CbField> arrayFields = blocks.ToList();
				ReadOnlyMemory<byte> view0;
				Assert.IsTrue(arrayFields[0].TryGetView(out view0));
				byte[] bytes0 = view0.ToArray();

				ReadOnlyMemory<byte> view1;
				Assert.IsTrue(arrayFields[1].TryGetView(out view1));
				byte[] bytes1 = view1.ToArray();

				// verify that we found the two blocks that we requested
				CollectionAssert.AreEqual(bytes0.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes0);
				CollectionAssert.AreEqual(bytes1.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes1);
			}
		}

		
		/// <summary>
		/// lists blocks to find a block to use and submits new blocks
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task ListAndUseBlocksCompressedBufferAsync()
		{
			CompressedBufferUtils compressedBufferUtils = _server!.Services.GetService<CompressedBufferUtils>()!;

			// register a build so we can reason about its block context
			// this build references an existing block as well as a new one
			CbObject blockBuild = GetBlockBuildObject("block-build-compressed-buffer", "branch-cb-blocks");
			CbObjectId buildId = CbObjectId.Parse("d12c9e5100000002aea6602c");
			
			(byte[] newBlockContents, byte[] newBlockIndex) = CreateBlockContents(_smallFile3Contents);
			BlobId newBlockHash = BlobId.FromBlob(newBlockContents);

			{
				using MemoryStream ms = new MemoryStream();
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, newBlockContents);
				newBlockContents = ms.ToArray();
			}

			CbObject blockBuildPartObject = GetBlockBuildPartObject(newBlockHash);
			CbObjectId objectId = CbObjectId.Parse("d12c8f4100000001aea6602c");
			string partName = "samplePart";

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(blockBuild.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// list blocks, one block should already exist
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/listBlocks", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(1, blocks.Count);
				// Check that the one block that did exist was the one we expected
				foreach (CbField field in blocks)
				{
					CollectionAssert.AreEqual(_existingBlockIndex, field.AsObject().GetView().ToArray());	
				}
			}

			// upload the new block
			{
				using HttpContent requestContent = new ByteArrayContent(newBlockContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{newBlockHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the new block metadata
			{
				using HttpContent requestContent = new ByteArrayContent(newBlockIndex);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{newBlockHash}/metadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(blockBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all parts are now present
				Assert.AreEqual(0, response.Needs.Length);
			}

			// finish the build upload
			{
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/finalize", UriKind.Relative), null);
				result.EnsureSuccessStatusCode();
			}

			// list blocks again, make sure that the new block is found
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/listBlocks", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(2, blocks.Count);
				List<CbField> arrayFields = blocks.ToList();
				ReadOnlyMemory<byte> view0;
				Assert.IsTrue(arrayFields[0].TryGetView(out view0));
				byte[] bytes0 = view0.ToArray();

				ReadOnlyMemory<byte> view1;
				Assert.IsTrue(arrayFields[1].TryGetView(out view1));
				byte[] bytes1 = view1.ToArray();

				// the blocks were just updated in the finalize, thus the sorting is not strictly defined (as they are both used in the lastest build and thus their use time is now)
				CollectionAssert.AreEqual(bytes0.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes0);
				CollectionAssert.AreEqual(bytes1.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes1);
			}

			// fetch the blocks metadata and verify its correct
			{
				CbWriter blockMetadataRequestWriter = new CbWriter();
				blockMetadataRequestWriter.BeginObject();
				blockMetadataRequestWriter.BeginArray("blocks");
				blockMetadataRequestWriter.WriteHashValue(_existingBlockBlobId.AsIoHash());
				blockMetadataRequestWriter.WriteHashValue(newBlockHash.AsIoHash());
				blockMetadataRequestWriter.EndArray();
				blockMetadataRequestWriter.EndObject();
				using HttpContent requestContent = new ByteArrayContent(blockMetadataRequestWriter.ToByteArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/getBlockMetadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();

				byte[] listBlocksResponse = await result.Content.ReadAsByteArrayAsync();
				CbObject o = new CbObject(listBlocksResponse);
				Assert.AreNotEqual(CbField.Empty, o["blocks"]);
				CbArray blocks = o["blocks"].AsArray();

				Assert.AreEqual(2, blocks.Count);
				List<CbField> arrayFields = blocks.ToList();
				ReadOnlyMemory<byte> view0;
				Assert.IsTrue(arrayFields[0].TryGetView(out view0));
				byte[] bytes0 = view0.ToArray();

				ReadOnlyMemory<byte> view1;
				Assert.IsTrue(arrayFields[1].TryGetView(out view1));
				byte[] bytes1 = view1.ToArray();

				// verify that we found the two blocks that we requested
				CollectionAssert.AreEqual(bytes0.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes0);
				CollectionAssert.AreEqual(bytes1.Length == newBlockIndex.Length ? newBlockIndex : _existingBlockIndex, bytes1);
			}
		}

		/// <summary>
		/// given a couple of builds do searches for them
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task SearchBuildJsonAsync()
		{
			// Greater then search
			{
				using StringContent content = new StringContent("{ \"query\": { \"changelist\": { \"$gt\": 40091040 }}}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(4, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);
			}

			// Greater then search with max results
			{
				using StringContent content = new StringContent("{ \"query\": { \"changelist\": { \"$gt\": 40091040} }, \"options\": { \"limit\": 1 }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			// Less then search
			{
				using StringContent content = new StringContent("{ \"query\": { \"changelist\": { \"$lt\": 40091040} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild0Id, searchResult.Results[0].BuildId);
			}

			// Less then or equal search
			{
				using StringContent content = new StringContent("{ \"query\": { \"changelist\": { \"$lte\": 40091040} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(2, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild1Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[1].BuildId);
			}

			// Equal search integer
			{
				using StringContent content = new StringContent("{ \"query\": { \"changelist\": { \"$eq\": 40091040} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild1Id, searchResult.Results[0].BuildId);
			}

			// Equal search
			{
				using StringContent content = new StringContent("{ \"query\": { \"platform\": { \"$eq\": \"windows\"} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(5, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild4Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[3].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[4].BuildId);
			}

			// Not Equals search
			{
				using StringContent content = new StringContent("{ \"query\": { \"platform\": { \"$neq\": \"windows\"} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			// In search
			{
				using StringContent content = new StringContent("{ \"query\": { \"platform\": { \"$in\": [\"windows\", \"linux\"]} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(6, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[4].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[5].BuildId);
			}

			// Not in search
			{
				using StringContent content = new StringContent("{ \"query\": { \"platform\": { \"$nin\": [\"windows\"]} }}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			// Date Time search greater then
			{
				using StringContent content = new StringContent($"{{ \"query\": {{ \"createdAt\": {{ \"$gt\": \"{_creationBaseline.AddHours(2)}\"}} }}}}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(4, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);
			}

			// Date Time search between
			{
				using StringContent content = new StringContent($"{{ \"query\": {{ \"createdAt\": {{ \"$gt\": \"{_creationBaseline.AddHours(2)}\", \"$lt\": \"{_creationBaseline.AddHours(4)}\"}} }}}}");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(2, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild3Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[1].BuildId);
			}

			// Or Search
			{
				using StringContent content = new StringContent("{ \"query\": { \"$or\": { \"changelist\": { \"$lte\": 40091040 }, \"platform\": { \"$eq\": \"linux\" } } } }");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(3, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[2].BuildId);
			}
		}

		/// <summary>
		/// uploads a new build, checks its ttl and attempts to fetch it
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task BuildTTLAsync()
		{
			CbObject sampleBuild = GetSampleBuildObject();
			CbObject sampleBuildPartObject = GetSampleBuildPartObject();

			CbObjectId buildId = CbObjectId.Parse("d12ac1ba0000000250aca240");
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("d12ac1ce0000000150aca240");
			string partName = "samplePart";

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuild.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the first build part
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all files should be missing
				Assert.AreEqual(3, response.Needs.Length);
			}

			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_looseFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_looseFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			
			// upload each blob
			{
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block
			{
				using HttpContent requestContent = new ByteArrayContent(_block0Contents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block0Hash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload the block metadata
			{
				using HttpContent requestContent = new ByteArrayContent(_block0Index);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blocks/{_block0Hash}/metadata", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(sampleBuildPartObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/parts/{objectId}/{partName}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				PutObjectResponse response = await result.Content.ReadAsAsync<PutObjectResponse>();

				// all parts are now present
				Assert.AreEqual(0, response.Needs.Length);
			}

			// finish the build upload
			{
				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/finalize", UriKind.Relative), null);
				result.EnsureSuccessStatusCode();
			}

			// fetch the submitted build
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.uecb", UriKind.Relative));
				result.EnsureSuccessStatusCode();
			}

			// fetch the submitted builds ttl
			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/ttl", UriKind.Relative));
				request.Headers.Add("Accept", MediaTypeNames.Application.Json);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				GetTTLResponse? ttlResponse = await result.Content.ReadAsAsync<GetTTLResponse>();
				// default ttl is 14 days or 1209600 seconds
				Assert.IsTrue(ttlResponse.TTL <= 1209600u);
				Assert.IsTrue(ttlResponse.TTL >= 1209000u);
			}

			// update the ttl
			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/updateTTL", UriKind.Relative));
				request.Headers.Add("Accept", MediaTypeNames.Application.Json);
				using StringContent content = new StringContent("{ \"ttl\": 1338 }");
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);
				request.Content = content;
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();
			}

			// fetch the ttl to make sure its updated
			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/ttl", UriKind.Relative));
				request.Headers.Add("Accept", MediaTypeNames.Application.Json);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				GetTTLResponse? ttlResponse = await result.Content.ReadAsAsync<GetTTLResponse>();
				// ttl starts to countdown when its submitted above so give it some fudge to vary
				Assert.IsTrue(ttlResponse.TTL <= 1338u);
				Assert.IsTrue(ttlResponse.TTL >= 1300u);
			}
		}

		/// <summary>
		/// given a couple of builds do searches for them with queries being compact binary
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task SearchBuildCompactBinaryAsync()
		{
			// Greater then search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("changelist");
				writer.WriteInteger("$gt", 40091040);
				writer.EndObject(); // changelist
				writer.EndObject(); // query
				writer.EndObject();
				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				byte[] b = await result.Content.ReadAsByteArrayAsync();
				SearchResult searchResult = CbSerializer.Deserialize<SearchResult>(b);
				Assert.AreEqual(4, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);

				CbObject searchResultObject = new CbObject(b);
				CbField resultField = searchResultObject["results"];
				Assert.AreNotEqual(CbField.Empty, resultField);
				// check to see that we serialize this as a uniform array
				Assert.IsTrue(resultField.IsArray());
				Assert.IsTrue(CbFieldUtils.HasUniformFields(resultField.TypeWithFlags));
			}

			// Greater then search with max results
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("changelist");
				writer.WriteInteger("$gt", 40091040);
				writer.EndObject(); // changelist
				writer.EndObject(); // query
				writer.BeginObject("options");
				writer.WriteInteger("limit", 1);
				writer.EndObject(); // options
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			
			// Less then search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("changelist");
				writer.WriteInteger("$lt", 40091040);
				writer.EndObject(); // changelist
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild0Id, searchResult.Results[0].BuildId);
			}

			// Less then or equal search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("changelist");
				writer.WriteInteger("$lte", 40091040);
				writer.EndObject(); // changelist
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(2, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild1Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[1].BuildId);
			}

			// Equal search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("platform");
				writer.WriteString("$eq", "windows");
				writer.EndObject(); // platform
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(5, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild4Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[3].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[4].BuildId);
			}

			// Not Equals search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("platform");
				writer.WriteString("$neq", "windows");
				writer.EndObject(); // platform
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			// In search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("platform");
				writer.BeginArray("$in");
				writer.WriteStringValue("windows");
				writer.WriteStringValue("linux");
				writer.EndArray(); // in
				writer.EndObject(); // platform
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(6, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[4].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[5].BuildId);
			}

			// Not in search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("platform");
				writer.BeginArray("$nin");
				writer.WriteStringValue("windows");
				writer.EndArray(); // in
				writer.EndObject(); // platform
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(1, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
			}

			// Date Time search greater then
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("createdAt");
				writer.WriteDateTime("$gt", _creationBaseline.AddHours(2));
				writer.EndObject(); // createdAt
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(4, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild4Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild3Id, searchResult.Results[2].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[3].BuildId);

			}

			// Date Time search between
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("createdAt");
				writer.WriteDateTime("$gt", _creationBaseline.AddHours(2));
				writer.WriteDateTime("$lt", _creationBaseline.AddHours(4));
				writer.EndObject(); // createdAt
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(2, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild3Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild2Id, searchResult.Results[1].BuildId);
			}

			// Or Search
			{
				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.BeginObject("query");
				writer.BeginObject("$or");
				writer.BeginObject("changelist");
				writer.WriteInteger("$lte", 40091040);
				writer.EndObject(); // changelist
				writer.BeginObject("platform");
				writer.WriteString("$eq", "linux");
				writer.EndObject(); // platform
				writer.EndObject(); // or
				writer.EndObject(); // query
				writer.EndObject();

				using ByteArrayContent content = new ByteArrayContent(writer.ToByteArray());
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"api/v2/builds/{TestNamespace}/{SearchBucketId}/search", UriKind.Relative));
				request.Content = content;
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
				using HttpResponseMessage result = await _httpClient!.SendAsync(request);
				result.EnsureSuccessStatusCode();

				SearchResult searchResult = await result.Content.ReadAsCompactBinaryAsync<SearchResult>();
				Assert.AreEqual(3, searchResult.Results.Count);

				Assert.AreEqual(_searchBuild5Id, searchResult.Results[0].BuildId);
				Assert.AreEqual(_searchBuild1Id, searchResult.Results[1].BuildId);
				Assert.AreEqual(_searchBuild0Id, searchResult.Results[2].BuildId);
			}
		}

		/// <summary>
		/// verifies that per bucket auth works as expected
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task TestBucketAuthAsync()
		{
			CbObjectId buildId = CbObjectId.NewObjectId();
			// fetch build in a namespace we do not have access to
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/test-namespace-no-access/{BucketId}/{buildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.Forbidden, result.StatusCode);
			}

			// fetch build in a namespace we have access to but a bucket we do not have access to
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/project.type.branch.secret-platform/{buildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.Forbidden, result.StatusCode);
			}

			// fetch build in a namespace we have access and a bucket we have access to
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/project.type.branch.platform/{buildId}", UriKind.Relative));
				// not found result because we haven't actually submitted this build
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}

			// fetch build in a namespace that uses a wildcard for its scope
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/test-builds-wildcard/{BucketId}/{buildId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
		}

		private static CbObject GetBlockBuildObject(string name, string branch)
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("project", "test-project-blocks");
			writer.WriteString("branch", branch);
			writer.WriteString("baselineBranch", "dev-main-blocks");
			writer.WriteString("platform", "windows");
			writer.WriteString("name", name);
			writer.EndObject();

			return writer.ToObject();
		}

		CbObject GetBlockBuildPartObject(BlobId newBlockHash)
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("existingBlock", _existingBlockBlobId.AsIoHash());
			writer.WriteBinaryAttachment("newBlock", newBlockHash.AsIoHash());
			writer.EndObject();

			return writer.ToObject();
		}

		static CbObject GetSampleBuildObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("project", "test-project");
			writer.WriteString("branch", "branch");
			writer.WriteString("baselineBranch", "dev-main");
			writer.WriteString("platform", "windows");
			writer.WriteString("name", "sample-build");
			writer.EndObject();

			return writer.ToObject();
		}

		CbObject GetSampleBuildPartObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("looseFile", _looseFileHash.AsIoHash());
			writer.WriteBinaryAttachment("block0", _block0Hash.AsIoHash());
			writer.WriteBinaryAttachment("largeFile", _largeFileHash.AsIoHash());
			writer.EndObject();

			return writer.ToObject();
		}

		static CbObject GetSampleCompressedBufferBuildObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("project", "test-project");
			writer.WriteString("branch", "branch");
			writer.WriteString("baselineBranch", "dev-main");
			writer.WriteString("platform", "windows");
			writer.WriteString("name", "sample-build-compressed-buffer");
			writer.EndObject();

			return writer.ToObject();
		}

		CbObject GetSampleCompressedBufferBuildPartObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("looseFile", _looseFileHash.AsIoHash());
			writer.WriteBinaryAttachment("block1", _block1Hash.AsIoHash());
			writer.WriteBinaryAttachment("largeFile", _largeFileHash.AsIoHash());
			writer.EndObject();

			return writer.ToObject();
		}
	}

	[TestClass]
	public class MemoryBuildTests : BuildTests
	{
		public MemoryBuildTests() : base("memory")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Memory.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ContentIdStoreImplementation", UnrealCloudDDCSettings.ContentIdStoreImplementations.Memory.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Memory.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:BuildStoreImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Memory.ToString()),
			};
		}

		protected override Task SeedAsync(IServiceProvider provider)
		{
			return base.SeedAsync(provider);
		}

		protected override Task Teardown(IServiceProvider provider)
		{
			return Task.CompletedTask;
		}
	}
	
	[TestClass]
	public class ScyllaBuildTests : BuildTests
	{
		public ScyllaBuildTests() : base("scylla")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ContentIdStoreImplementation", UnrealCloudDDCSettings.ContentIdStoreImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:BuildStoreImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()),
			};
		}

		protected override Task SeedAsync(IServiceProvider provider)
		{
			return base.SeedAsync(provider);
		}

		protected override Task Teardown(IServiceProvider provider)
		{
			return Task.CompletedTask;
		}
	}
}
