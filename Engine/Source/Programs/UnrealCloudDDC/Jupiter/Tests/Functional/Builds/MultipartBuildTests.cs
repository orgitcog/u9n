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
using PutObjectResponse = Jupiter.Controllers.PutObjectResponse;

namespace Jupiter.FunctionalTests.Builds
{
	public abstract class MultipartBuildTests
	{
		protected MultipartBuildTests(string namespaceSuffix)
		{
			TestNamespace = new NamespaceId($"test-builds-{namespaceSuffix}");
		}
		
		private TestServer? _server;
		private HttpClient? _httpClient;
		protected NamespaceId TestNamespace { get; init; }
		protected BucketId BucketId { get; init; } = new BucketId("project.type.branch.platform");

		private byte[] _looseFileContents = null!;
		private byte[] _largeFileContents = null!;

		private BlobId _looseFileHash = null!;
		private BlobId _largeFileHash = null!;

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
			byte[] largeFileContents = await File.ReadAllBytesAsync("Builds/TestData/SampleBuild/large-file-multipart.bin");

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
				compressedBufferUtils.CompressContent(ms, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.Fast, largeFileContents);
				_largeFileContents = ms.ToArray();
			}
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
		public async Task GetPutDeleteNewBuildMultipartAsync()
		{
			CbObject multiPartBuildObject = GetMultiPartBuildObject();
			CbObject multipartBuildPartObject = GetMultipartBuildPartObject();
			string partName = "samplePart";
			CbObjectId buildId =  CbObjectId.NewObjectId();
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("d12ac1ba0000000150aba240");

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(multiPartBuildObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload small loose file
			{
				using HttpContent requestContent = new ByteArrayContent(_looseFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_looseFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}
			
			// multipart upload the large file

			string uploadId;
			List<MultipartPartDescription> partDescriptions;
			string blobName;
			// start the multipart upload
			{
				StartMultipartUploadRequest request = new StartMultipartUploadRequest {BlobLength = (ulong)_largeFileContents.LongLength};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType =  new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/startMultipartUpload", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				MultipartUploadIdResponse response = await result.Content.ReadAsAsync<MultipartUploadIdResponse>();
				uploadId = response.UploadId;
				partDescriptions = response.Parts;
				blobName = response.BlobName;
			}

			// upload parts
			// split the large file into the smallest chunks allowed
			foreach (MultipartPartDescription partDescription in partDescriptions)
			{
				int countOfBytes = (int)partDescription.LastByte - (int)partDescription.FirstByte;
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents, (int)partDescription.FirstByte, countOfBytes);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/uploadMultipart{partDescription.QueryString}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// complete the multipart upload
			{
				CompleteMultipartUploadRequest request = new CompleteMultipartUploadRequest() {BlobName = blobName, UploadId = uploadId, IsCompressed = true, PartIds = partDescriptions.Select(d => d.PartId).ToList()};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/completeMultipart", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}
			
			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(multipartBuildPartObject.GetView().ToArray());
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

				CollectionAssert.AreEqual(o.ToList(), multipartBuildPartObject.ToList());
			}

			// verify the blob contents
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				byte[] content = await result.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(_largeFileContents, content);
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
		/// uploads and fetches a new build that does not exist
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task GetPutDeleteNewBuildRedirectMultipartAsync()
		{
			CbObject multiPartBuildObject = GetMultiPartBuildObject();
			CbObject multipartBuildPartObject = GetMultipartBuildPartObject();
			string partName = "samplePart";
			CbObjectId buildId =  CbObjectId.NewObjectId();
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("d12ac1ba0000000150aba240");

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(multiPartBuildObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// upload small loose file
			{
				using HttpContent requestContent = new ByteArrayContent(_looseFileContents);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_looseFileHash}", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			
			// multipart upload the large file

			string uploadId;
			List<MultipartPartDescription> partDescriptions;
			string blobName;
			// start the multipart upload
			{
				StartMultipartUploadRequest request = new StartMultipartUploadRequest {BlobLength = (ulong)_largeFileContents.LongLength};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/startMultipartUpload", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				MultipartUploadIdResponse response = await result.Content.ReadAsAsync<MultipartUploadIdResponse>();
				uploadId = response.UploadId;
				partDescriptions = response.Parts;
				blobName = response.BlobName;
			}

			// upload parts
			// split the large file into the smallest chunks allowed

			foreach (MultipartPartDescription partDescription in partDescriptions)
			{
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents, (int)partDescription.FirstByte, (int)partDescription.LastByte - (int)partDescription.FirstByte);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/uploadMultipart{partDescription.QueryString}&supportsRedirect=true", UriKind.Relative), requestContent);
				// verify that we got a redirect uri
				Assert.AreEqual(HttpStatusCode.Found, result.StatusCode);
				Assert.IsNotNull(result.Headers.Location);
				Uri redirectUri = result.Headers.Location;
				// create a real http client that can reach services outside the current tests
				using HttpClient realHttpClient = new HttpClient();
				using HttpResponseMessage resultRedirected = await realHttpClient!.PutAsync(redirectUri, requestContent);
				resultRedirected.EnsureSuccessStatusCode();
			}

			// complete the multipart upload
			{
				CompleteMultipartUploadRequest request = new CompleteMultipartUploadRequest() {BlobName = blobName, UploadId = uploadId, IsCompressed = true, PartIds = partDescriptions.Select(d => d.PartId).ToList()};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/completeMultipart", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}
			
			// verify that all files in the part is uploaded
			{
				using HttpContent requestContent = new ByteArrayContent(multipartBuildPartObject.GetView().ToArray());
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

				CollectionAssert.AreEqual(o.ToList(), multipartBuildPartObject.ToList());
			}

			// verify the blob contents
			{
				using HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				byte[] content = await result.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(_largeFileContents, content);
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
		/// uploads a new build but fails to upload all parts
		/// </summary>
		/// <returns></returns>
		[TestMethod]
		public async Task PutNewBuildMultipartMissingPartAsync()
		{
			CbObject multiPartBuildObject = GetMultiPartBuildObject();
			CbObjectId buildId =  CbObjectId.NewObjectId();
			
			// we hardcode the partId so that we always overwrite it, thus we do not need to cleanup the db between runs of the test
			CbObjectId objectId = CbObjectId.Parse("d12ac1ba0000000151aba240");

			// register the build
			{
				using HttpContent requestContent = new ByteArrayContent(multiPartBuildObject.GetView().ToArray());
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}.json", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			// multipart upload the large file

			string uploadId;
			List<MultipartPartDescription> partDescriptions;
			string blobName;
			// start the multipart upload
			{
				StartMultipartUploadRequest request = new StartMultipartUploadRequest {BlobLength = (ulong)_largeFileContents.LongLength};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType =  new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/startMultipartUpload", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
				MultipartUploadIdResponse response = await result.Content.ReadAsAsync<MultipartUploadIdResponse>();
				uploadId = response.UploadId;
				partDescriptions = response.Parts;
				blobName = response.BlobName;
			}

			int i = 0;
			foreach (MultipartPartDescription partDescription in partDescriptions)
			{
				if (i == 2)
				{
					i++;
					// skip uploading one part which we should be told is missing
					continue;
				}
				using HttpContent requestContent = new ByteArrayContent(_largeFileContents, (int)partDescription.FirstByte, (int)partDescription.LastByte - (int)partDescription.FirstByte);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);

				using HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/uploadMultipart{partDescription.QueryString}&supportsRedirect=true", UriKind.Relative), requestContent);
				// verify that we got a redirect uri
				Assert.AreEqual(HttpStatusCode.Found, result.StatusCode);
				Assert.IsNotNull(result.Headers.Location);
				Uri redirectUri = result.Headers.Location;
				// create a real http client that can reach services outside the current tests
				using HttpClient realHttpClient = new HttpClient();
				using HttpResponseMessage resultRedirected = await realHttpClient!.PutAsync(redirectUri, requestContent);
				resultRedirected.EnsureSuccessStatusCode();
				i++;
			}

			// complete the multipart upload
			{
				CompleteMultipartUploadRequest request = new CompleteMultipartUploadRequest() {BlobName = blobName, UploadId = uploadId, IsCompressed = true, PartIds = partDescriptions.Select(d => d.PartId).ToList()};
				using HttpContent requestContent = JsonContent.Create(request);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

				using HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v2/builds/{TestNamespace}/{BucketId}/{buildId}/blobs/{_largeFileHash}/completeMultipart", UriKind.Relative), requestContent);
				Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);

				CompleteMultipartUploadResponse response = await result.Content.ReadAsAsync<CompleteMultipartUploadResponse>();
				Assert.AreEqual(1, response.MissingParts.Count);
				Assert.AreEqual(partDescriptions[2].PartId, response.MissingParts[0]);
			}
		}

		static CbObject GetMultiPartBuildObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("project", "test-project");
			writer.WriteString("branch", "branch");
			writer.WriteString("baselineBranch", "dev-main");
			writer.WriteString("platform", "windows");
			writer.WriteString("name", "multipart-build");
			writer.EndObject();

			return writer.ToObject();
		}

		CbObject GetMultipartBuildPartObject()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("looseFile", _looseFileHash.AsIoHash());
			writer.WriteBinaryAttachment("largeFile", _largeFileHash.AsIoHash());
			writer.EndObject();

			return writer.ToObject();
		}
	}

	// Azure blob storage support for multipart is not implemented yet
	/*[TestClass]
	public class AzureMultipartBuildTests : MultipartBuildTests
	{
		public AzureMultipartBuildTests() : base("azure")
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
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.Azure.ToString()),

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
	}*/
	
	[TestClass]
	public class S3MultipartBuildTests : MultipartBuildTests
	{
		public S3MultipartBuildTests() : base("scylla")
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
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("S3:BucketName", TestNamespace.ToString())
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
