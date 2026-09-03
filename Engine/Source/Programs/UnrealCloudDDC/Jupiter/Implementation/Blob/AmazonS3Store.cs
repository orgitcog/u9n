// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using Amazon.S3;
using Amazon.S3.Model;
using Amazon.S3.Transfer;
using Amazon.S3.Util;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Jupiter.Common.Utils;
using Jupiter.Implementation.Blob;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;
using Exception = System.Exception;
using KeyNotFoundException = System.Collections.Generic.KeyNotFoundException;

namespace Jupiter.Implementation
{
	public class AmazonS3Store : IBlobStore, IMultipartBlobStore
	{
		private readonly IAmazonS3 _amazonS3;
		private readonly IBlobIndex _blobIndex;
		private readonly INamespacePolicyResolver _namespacePolicyResolver;
		private readonly ILogger<AmazonS3Store> _logger;
		private readonly IServiceProvider _provider;
		private readonly S3Settings _settings;
		private readonly ConcurrentDictionary<NamespaceId, AmazonStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, AmazonStorageBackend>();

		public AmazonS3Store(IAmazonS3 amazonS3, IOptionsMonitor<S3Settings> settings, IBlobIndex blobIndex, INamespacePolicyResolver namespacePolicyResolver, ILogger<AmazonS3Store> logger, IServiceProvider provider)
		{
			_amazonS3 = amazonS3;
			_blobIndex = blobIndex;
			_namespacePolicyResolver = namespacePolicyResolver;
			_logger = logger;
			_provider = provider;
			_settings = settings.CurrentValue;
		}

		AmazonStorageBackend GetBackend(NamespaceId ns)
		{
			return _backends.GetOrAdd(ns, x => ActivatorUtilities.CreateInstance<AmazonStorageBackend>(_provider, GetBucketName(x)));
		}

		public async Task<Uri?> GetObjectByRedirectAsync(NamespaceId ns, BlobId identifier)
		{
			Uri? uri = await GetBackend(ns).GetReadRedirectAsync(identifier.AsS3Key());

			return uri;
		}

		public async Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId identifier)
		{
			BlobMetadata? result = await GetBackend(ns).GetMetadataAsync(identifier.AsS3Key());
			if (result == null)
			{
				throw new BlobNotFoundException(ns, identifier);
			}

			return result;
		}
		
		public async Task<Uri?> PutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier)
		{
			Uri? uri = await GetBackend(ns).GetWriteRedirectAsync(identifier.AsS3Key());

			return uri;
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, ReadOnlyMemory<byte> content, BlobId objectName)
		{
			await using MemoryStream stream = new MemoryStream(content.ToArray());
			return await PutObjectAsync(ns, stream, objectName);
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, Stream stream, BlobId objectName)
		{
			await GetBackend(ns).WriteAsync(objectName.AsS3Key(), stream, CancellationToken.None);
			return objectName;
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] content, BlobId objectName)
		{
			await using MemoryStream stream = new MemoryStream(content);
			return await PutObjectAsync(ns, stream, objectName);
		}

		private string GetBucketName(NamespaceId ns)
		{
			try
			{
				NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
				string storagePool = policy.StoragePool;
				// if the bucket to use for the storage pool has been overriden we use the override
				if (_settings.StoragePoolBucketOverride.TryGetValue(storagePool, out string? containerOverride))
				{
					return containerOverride;
				}
				// by default we use the storage pool as a suffix to determine the bucket for that pool
				string storagePoolSuffix = string.IsNullOrEmpty(storagePool) ? "" : $"-{storagePool}";
				return $"{_settings.BucketName}{storagePoolSuffix}";
			}
			catch (KeyNotFoundException)
			{
				throw new NamespaceNotFoundException(ns);
			}
		}

		public async Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, bool supportsRedirectUri = false)
		{
			NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
			try
			{
				if (supportsRedirectUri && policies.AllowRedirectUris)
				{
					Uri? redirectUri = await GetBackend(ns).GetReadRedirectAsync(blob.AsS3Key());
					if (redirectUri != null)
					{
						return new BlobContents(redirectUri);
					}
				}

				BlobContents? contents = await GetBackend(ns).TryReadAsync(blob.AsS3Key(), flags);
				if (contents == null)
				{
					throw new BlobNotFoundException(ns, blob);
				}
				return contents;
			}
			catch (AmazonS3Exception e)
			{
				// log information about the failed request except for 404 as its valid to not find objects in S3
				if (e.StatusCode != HttpStatusCode.NotFound)
				{
					_logger.LogWarning("Exception raised from S3 {Exception}. {RequestId} {Id}", e, e.RequestId, e.AmazonId2);
				}

				// rethrow the exception, we just wanted to log more information about the failed request for further debugging
				throw;
			}
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BlobId blobIdentifier, bool forceCheck)
		{
			NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
			if (_settings.UseBlobIndexForExistsCheck && policies.UseBlobIndexForSlowExists && !forceCheck)
			{
				return await _blobIndex.BlobExistsInRegionAsync(ns, blobIdentifier);
			}
			else
			{
				return await GetBackend(ns).ExistsAsync(blobIdentifier.AsS3Key(), CancellationToken.None);
			}
		}

		public async Task DeleteNamespaceAsync(NamespaceId ns)
		{
			string bucketName = GetBucketName(ns);
			try
			{
				await _amazonS3.DeleteBucketAsync(bucketName);
			}
			catch (AmazonS3Exception e)
			{
				// if the bucket does not exist we get a not found status code
				if (e.StatusCode == HttpStatusCode.NotFound)
				{
					// deleting a none existent bucket is a success
					return;
				}

				// something else happened, lets just process it as usual
			}
		}

		public async IAsyncEnumerable<(BlobId, DateTime)> ListObjectsAsync(NamespaceId ns)
		{
			IStorageBackend backend = GetBackend(ns);
			await foreach ((string path, DateTime time) in backend.ListAsync())
			{
				// ignore objects in the temp prefix
				if (path.StartsWith("Temp", StringComparison.OrdinalIgnoreCase))
				{
					continue;
				}
				string identifierString = path.Substring(path.LastIndexOf("/", StringComparison.Ordinal) + 1);
				yield return (new BlobId(identifierString), time);
			}
		}

		public async Task DeleteObjectAsync(NamespaceId ns, BlobId blobIdentifier)
		{
			IStorageBackend backend = GetBackend(ns);
			await backend.DeleteAsync(blobIdentifier.AsS3Key());
		}

		public async Task DeleteObjectAsync(IEnumerable<NamespaceId> namespaces, BlobId blob)
		{
			List<NamespaceId> namespaceIds = namespaces.ToList();
			List<string> storagePools = namespaceIds.Select(ns => _namespacePolicyResolver.GetPoliciesForNs(ns).StoragePool).Distinct().ToList();

			Dictionary<string, NamespaceId> storagePoolsToClean = storagePools.ToDictionary(storagePool => storagePool, storagePool => namespaceIds.FirstOrDefault(id => _namespacePolicyResolver.GetPoliciesForNs(id).StoragePool == storagePool));

			foreach ((string _, NamespaceId ns) in storagePoolsToClean)
			{
				await GetBackend(ns).DeleteAsync(blob.AsS3Key(), CancellationToken.None);
			}
		}
		
		#region IMultipartBlobStore
		
		
		private const string TempMultipartPrefix = "Temp/Multipart";
		public Task<string> StartMultipartUploadAsync(NamespaceId ns, string blobName)
		{
			return GetBackend(ns).StartMultipartUploadAsync($"{TempMultipartPrefix}/{blobName}");
		}

		public Task CompleteMultipartUploadAsync(NamespaceId ns, string blobName, string uploadId, List<string> partIds)
		{
			return GetBackend(ns).CompleteMultipartUploadAsync($"{TempMultipartPrefix}/{blobName}", uploadId, partIds);
		}

		public Task<Uri?> GetWriteRedirectForPartAsync(NamespaceId ns, string blobName, string uploadId, string partIdentifier)
		{
			return Task.FromResult(GetBackend(ns).GetWriteRedirectForPart($"{TempMultipartPrefix}/{blobName}", uploadId, partIdentifier));
		}
		
		public async Task<BlobContents?> GetMultipartObjectByNameAsync(NamespaceId ns, string blobName)
		{
			return await GetBackend(ns).TryReadAsync($"{TempMultipartPrefix}/{blobName}", LastAccessTrackingFlags.SkipTracking);
		}

		public async Task RenameMultipartBlobAsync(NamespaceId ns, string blobName, BlobId blobId)
		{
			await GetBackend(ns).RenameMultipartBlobAsync($"{TempMultipartPrefix}/{blobName}", blobId.AsS3Key());
		}
		public Task PutMultipartPartAsync(NamespaceId ns, string blobName, string uploadId, string partIdentifier, byte[] blob)
		{
			return GetBackend(ns).PutMultipartPartAsync($"{TempMultipartPrefix}/{blobName}", uploadId, partIdentifier, blob);
		}

		public List<MultipartByteRange> GetMultipartRanges(NamespaceId ns, string uploadId, ulong blobLength)
		{
			ulong countOfChunks = (blobLength / IdealS3ChunkSize) + 1;

			if (Math.Min(countOfChunks, MaxS3Chunks) == MaxS3Chunks)
			{
				throw new Exception("Multipart blob would use more then max chunks in S3, this is not supported");
			}

			List<MultipartByteRange> parts = new();
			ulong firstByte = 0;
			ulong lastByte = IdealS3ChunkSize; // last byte is inclusive as per http range requests

			for (int i = 1; i < (int)countOfChunks+1; i++)
			{
				// s3 parts start at 1 and max is 10_000 (inclusive)
				// s3 part ids are simply incrementing numbers
				parts.Add(new MultipartByteRange() {FirstByte = firstByte, LastByte = lastByte, PartId=i.ToString()});

				firstByte += IdealS3ChunkSize;
				lastByte = Math.Min(lastByte + IdealS3ChunkSize, blobLength);
			}

			return parts;
		}

		public MultipartLimits GetMultipartLimits(NamespaceId ns)
		{
			return new MultipartLimits { IdealChunkSize = IdealS3ChunkSize, MaxCountOfChunks = (int)MaxS3Chunks, MinChunkSize = 8 * 1024 * 1024 };
		}

		private const ulong MaxS3Chunks = 10_000;
		private const int IdealS3ChunkSize = 32 * 1024 * 1024; // 32 MB parts - this means the largest file we can upload to S3 is 312GB

		#endregion
	}

	public class AmazonStorageBackend : IStorageBackend
	{
		private readonly IAmazonS3 _amazonS3;
		private readonly string _bucketName;
		private readonly IOptionsMonitor<S3Settings> _settings;
		private readonly Tracer _tracer;
		private readonly ILogger<AmazonStorageBackend> _logger;
		private readonly BufferedPayloadFactory _payloadFactory;
		private bool _bucketExistenceChecked;
		private bool _bucketAccessPolicyApplied;

		public AmazonStorageBackend(IAmazonS3 amazonS3, string bucketName, IOptionsMonitor<S3Settings> settings, Tracer tracer, ILogger<AmazonStorageBackend> logger, BufferedPayloadFactory payloadFactory)
		{
			_amazonS3 = amazonS3;
			_bucketName = bucketName;
			_settings = settings;
			_tracer = tracer;
			_logger = logger;
			_payloadFactory = payloadFactory;
		}

		public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			if (_settings.CurrentValue.CreateBucketIfMissing)
			{
				if (!_bucketExistenceChecked)
				{
					bool bucketExist = await AmazonS3Util.DoesS3BucketExistV2Async(_amazonS3, _bucketName);
					if (!bucketExist)
					{
						PutBucketRequest putBucketRequest = new PutBucketRequest
						{
							BucketName = _bucketName,
							UseClientRegion = true
						};

						try
						{
							await _amazonS3.PutBucketAsync(putBucketRequest, cancellationToken);
						}
						catch (AmazonS3Exception e)
						{
							if (e.StatusCode == HttpStatusCode.Conflict)
							{
								// bucket already exists so no need to try and create it, most likely a race condition with another write operation that already tried to create the bucket at the same time in this or another instance
								// as this will end up with the bucket existing we can just ignore the error
							}
							else
							{
								throw;
							}
						}
					}
					_bucketExistenceChecked = true;
				}
			}

			if (_settings.CurrentValue.SetBucketPolicies && !_bucketAccessPolicyApplied)
			{
				// block all public access to the bucket
				try
				{
					await _amazonS3.PutPublicAccessBlockAsync(new PutPublicAccessBlockRequest
					{
						BucketName = _bucketName,
						PublicAccessBlockConfiguration = new PublicAccessBlockConfiguration()
						{
							RestrictPublicBuckets = true,
							BlockPublicAcls = true,
							BlockPublicPolicy = true,
							IgnorePublicAcls = true,
						}
					}, cancellationToken);

					_bucketAccessPolicyApplied = true;
				}
				catch (AmazonS3Exception e)
				{
					// if a conflicting operation is being applied to the public access block we just ignore it, as it will get reset the next time we run
					if (e.StatusCode != HttpStatusCode.Conflict)
					{
						throw;
					}
				}
			}

			if (_settings.CurrentValue.UseMultiPartUpload)
			{
				await WriteMultipartAsync(path, stream, cancellationToken);
			}
			else
			{
				PutObjectRequest request = new PutObjectRequest
				{
					BucketName = _bucketName,
					Key = path,
					InputStream = stream,
					UseChunkEncoding = _settings.CurrentValue.UseChunkEncoding
				};

				try
				{
					await _amazonS3.PutObjectAsync(request, cancellationToken);
				}
				catch (AmazonS3Exception e)
				{
					// if the same object is added twice S3 will raise a error, as we are content addressed we can just accept whichever of the objects so we can ignore that error
					if (e.StatusCode == HttpStatusCode.Conflict)
					{
						return;
					}

					if (e.StatusCode == HttpStatusCode.TooManyRequests)
					{
						throw new ResourceHasToManyRequestsException(e);
					}

					throw;
				}
			}
		}

		private async Task WriteMultipartAsync(string path, Stream stream, CancellationToken cancellationToken)
		{
			FilesystemBufferedPayload? payload = null;
			try
			{
				string? filePath = null;
				if (stream is FileStream fileStream)
				{
					filePath = fileStream.Name;
				}
				// use multipart transfers when buffer is larger than 16 MB, which is also the S3 default
				else if (stream.Length > 16 * (long)Math.Pow(2, 20))
				{
					// will be chunked by TransferUtility
					using FilesystemBufferedPayloadWriter writer = _payloadFactory.CreateFilesystemBufferedPayloadWriter("s3-upload");
					{
						await using Stream writableStream = writer.GetWritableStream();
						await stream.CopyToAsync(writableStream, cancellationToken);
					}
					payload = writer.Done();

					filePath = payload.TempFile.FullName;
				}
				using TransferUtility utility = new TransferUtility(_amazonS3);
				try
				{
					if (filePath != null)
					{
						await utility.UploadAsync(filePath, _bucketName, path, cancellationToken);
					}
					else
					{
						await utility.UploadAsync(stream, _bucketName, path, cancellationToken);
					}
				}
				catch (AmazonS3Exception e)
				{
					// if the same object is added twice S3 will raise a error, as we are content addressed we can just accept whichever of the objects so we can ignore that error
					if (e.StatusCode == HttpStatusCode.Conflict)
					{
						return;
					}

					if (e.StatusCode == HttpStatusCode.TooManyRequests)
					{
						throw new ResourceHasToManyRequestsException(e);
					}

					throw;
				}
			}
			finally
			{
				payload?.Dispose();
			}
		}

		private async Task<BlobContents?> GetMultipartAsync(string path, CancellationToken cancellationToken)
		{
			try
			{
				GetObjectRequest request = new GetObjectRequest { BucketName = _bucketName, Key = path, PartNumber = 1};
				GetObjectResponse response = await _amazonS3.GetObjectAsync(request, cancellationToken);
				// parts are only set of blobs that was uploaded using multipart, otherwise part 1 is the whole file
				if (response.PartsCount > 1)
				{
					// object had parts, we download them in parallel
					// -1 because parts start at 1
					int partsCount = response.PartsCount.Value;

					byte[][] parts = new byte[partsCount][];
					long[] lengths = new long[partsCount];

					try
					{
						{
							parts[0] = ArrayPool<byte>.Shared.Rent((int)response.ContentLength);
							lengths[0] = response.ContentLength;
							await using MemoryStream ms = new MemoryStream(parts[0]);
							await response.ResponseStream.CopyToAsync(ms, cancellationToken);
						}

						// multipart object, download it in parts, we already have the first part
						await Parallel.ForEachAsync(Enumerable.Range(2, partsCount-1), cancellationToken, async (part, token) =>
						{
							GetObjectRequest partRequest = new GetObjectRequest { BucketName = _bucketName, Key = path, PartNumber = part };
							GetObjectResponse partResponse = await _amazonS3.GetObjectAsync(partRequest, cancellationToken);

							// the first part was retrieved in the initial request
							parts[part-1] = ArrayPool<byte>.Shared.Rent((int)partResponse.ContentLength);
							lengths[part-1] = partResponse.ContentLength;
							await using MemoryStream ms = new MemoryStream(parts[part-1]);
							await partResponse.ResponseStream.CopyToAsync(ms, cancellationToken);
						});

						long totalSize = lengths.Sum();
						if (totalSize < _settings.CurrentValue.MultiPartMaxMemoryBufferSize)
						{
							Stream s = new MemoryStream(new byte[(int)totalSize]);
							for (int i = 0; i < parts.Length; i++)
							{
								byte[] part = parts[i];
								await s.WriteAsync(part, 0, (int)lengths[i], cancellationToken);
							}

							s.Position = 0;
							return new BlobContents(s, totalSize);
						}
						else
						{
							// large payload - we have to buffer to file
							using FilesystemBufferedPayloadWriter writer = _payloadFactory.CreateFilesystemBufferedPayloadWriter("s3-download");

							{
								await using Stream s = writer.GetWritableStream();
								for (int i = 0; i < parts.Length; i++)
								{
									byte[] part = parts[i];
									await s.WriteAsync(part, 0, (int)lengths[i], cancellationToken);
								}
							}
						
							return new BlobContents(writer.Done());
						}
					}
					finally
					{
						for (int i = 0; i < partsCount; i++)
						{
							ArrayPool<byte>.Shared.Return(parts[i]);
						}
					}
				}
				else
				{
					return new BlobContents(response.ResponseStream, response.ContentLength);
				}
			}
			catch (AmazonS3Exception e)
			{
				if (e.ErrorCode == "NoSuchKey")
				{
					return null;
				}

				if (e.ErrorCode == "NoSuchBucket")
				{
					return null;
				}

				if (e.StatusCode == HttpStatusCode.TooManyRequests)
				{
					throw new ResourceHasToManyRequestsException(e);
				}

				throw;
			}
		}

		public async Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, CancellationToken cancellationToken = default)
		{
			if (_settings.CurrentValue.UseMultiPartDownload)
			{
				return await GetMultipartAsync(path, cancellationToken);
			}
			else
			{
				GetObjectResponse response;
				try
				{
					response = await _amazonS3.GetObjectAsync(_bucketName, path, cancellationToken);
				}
				catch (AmazonS3Exception e)
				{
					if (e.ErrorCode == "NoSuchKey")
					{
						return null;
					}

					if (e.ErrorCode == "NoSuchBucket")
					{
						return null;
					}
					throw;
				}
				return new BlobContents(response.ResponseStream, response.ContentLength);
			}
		}

		public async Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			try
			{
				await _amazonS3.GetObjectMetadataAsync(_bucketName, path, cancellationToken);
			}
			catch (AmazonS3Exception e)
			{
				// if the object does not exist we get a not found status code
				if (e.StatusCode == HttpStatusCode.NotFound)
				{
					return false;
				}

				// something else happened, lets just process it as usual
			}

			return true;
		}

		public async IAsyncEnumerable<(string, DateTime)> ListAsync([EnumeratorCancellation] CancellationToken cancellationToken)
		{
			if (_settings.CurrentValue.PerPrefixListing)
			{
				List<string> hashPrefixes = new List<string>(65536);
				int i = 0;
				for (int a = 0; a <= byte.MaxValue; a++)
				{
					for (int b = 0; b <= byte.MaxValue; b++)
					{
						hashPrefixes.Add(StringUtils.FormatAsHexString(new byte[] { (byte)a, (byte)b }));
						i++;
					}
				}

				hashPrefixes.Shuffle();

				if (!await AmazonS3Util.DoesS3BucketExistV2Async(_amazonS3, _bucketName))
				{
					yield break;
				}

				foreach (string hashPrefix in hashPrefixes)
				{
					ListObjectsV2Request request = new ListObjectsV2Request
					{
						BucketName = _bucketName,
						Prefix = hashPrefix,
						MaxKeys = _settings.CurrentValue.PerPrefixMaxKeys
					};

					ListObjectsV2Response response;
					do
					{
						response = await _amazonS3.ListObjectsV2Async(request, cancellationToken);
						foreach (S3Object obj in response.S3Objects)
						{
							yield return (obj.Key, obj.LastModified);
						}

						request.ContinuationToken = response.NextContinuationToken;
					} while (response.IsTruncated);
				}
			}
			else
			{
				if (!await AmazonS3Util.DoesS3BucketExistV2Async(_amazonS3, _bucketName))
				{
					yield break;
				}

				ListObjectsV2Request request = new ListObjectsV2Request
				{
					BucketName = _bucketName
				};

				ListObjectsV2Response response;
				do
				{
					response = await _amazonS3.ListObjectsV2Async(request, cancellationToken);
					foreach (S3Object obj in response.S3Objects)
					{
						yield return (obj.Key, obj.LastModified);
					}

					request.ContinuationToken = response.NextContinuationToken;
				} while (response.IsTruncated);
			}
		}

		public async Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			await _amazonS3.DeleteObjectAsync(_bucketName, path, cancellationToken);
		}

		public ValueTask<Uri?> GetReadRedirectAsync(string path)
		{
			return new ValueTask<Uri?>(GetPresignedUrl(path, HttpVerb.GET));
		}

		public ValueTask<Uri?> GetWriteRedirectAsync(string path)
		{
			return new ValueTask<Uri?>(GetPresignedUrl(path, HttpVerb.PUT));
		}

		/// <summary>
		/// Helper method to generate a presigned URL for a request
		/// </summary>
		Uri? GetPresignedUrl(string path, HttpVerb verb, int? partNumber = null, string? uploadId = null)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan("s3.BuildPresignedUrl")
				.SetAttribute("Path", path)
			;

			try
			{
				GetPreSignedUrlRequest signedUrlRequest = new GetPreSignedUrlRequest();
				signedUrlRequest.BucketName = _bucketName;
				signedUrlRequest.Key = path;
				signedUrlRequest.Verb = verb;
				signedUrlRequest.Protocol = _settings.CurrentValue.AssumeHttpForRedirectUri ? Protocol.HTTP : Protocol.HTTPS;
				signedUrlRequest.Expires = DateTime.UtcNow.AddHours(3.0);
				if (partNumber.HasValue)
				{
					signedUrlRequest.PartNumber = partNumber.Value;
				}
				if (uploadId != null)
				{
					signedUrlRequest.UploadId = uploadId;
				}

				string url = _amazonS3.GetPreSignedURL(signedUrlRequest);

				return new Uri(url);
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Unable to get presigned url for {Path} from S3", path);
				return null;
			}
		}

		public async Task<BlobMetadata?> GetMetadataAsync(string path)
		{
			try
			{
				GetObjectAttributesResponse? metadata = await _amazonS3.GetObjectAttributesAsync(new GetObjectAttributesRequest
				{
					BucketName = _bucketName, Key = path, ObjectAttributes = new List<ObjectAttributes>() { ObjectAttributes.ObjectSize }
				});
				if (metadata == null)
				{
					return null;
				}

				return new BlobMetadata(metadata.ObjectSize, metadata.LastModified);
			}
			catch (AmazonS3Exception e)
			{
				// if the object does not exist we get a not found status code
				if (e.StatusCode == HttpStatusCode.NotFound)
				{
					return null;
				}

				throw;
			}
			catch (XmlException)
			{
				// Multipart uploaded objects can cause invalid xml objects generated in S3
				return null;
			}
		}

		public async Task<string> StartMultipartUploadAsync(string path)
		{
			InitiateMultipartUploadResponse response = await _amazonS3.InitiateMultipartUploadAsync(new InitiateMultipartUploadRequest {BucketName = _bucketName, Key = path});

			return response.UploadId;
		}

		public async Task CompleteMultipartUploadAsync(string path, string uploadId, List<string> partIds)
		{
			Dictionary<int, PartETag> etags = new ();

			int? partNumberMarker = 0;
			for (int i = 0; i < 10; i++)
			{
				// s3 only allows you to list 1000 parts per request, but supports up to 10_000 parts so we need to potentially list this 10 times to get all parts
				ListPartsRequest listPartsRequest = new() {BucketName = _bucketName, Key = path, UploadId = uploadId, PartNumberMarker = partNumberMarker.ToString()};
				ListPartsResponse partList = await _amazonS3.ListPartsAsync(listPartsRequest);
				foreach (PartDetail? part in partList.Parts)
				{
					etags.Add(part.PartNumber, new PartETag(part.PartNumber, part.ETag));
				}
				if (!partList.IsTruncated)
				{
					break;
				}
				partNumberMarker = partList.NextPartNumberMarker;
			}

			// check if any part is missing
			List<string> missingPartIds = new();
			foreach (string partId in partIds)
			{
				if (!etags.ContainsKey(int.Parse(partId)))
				{
					missingPartIds.Add(partId);
				}
			}

			if (missingPartIds.Count > 0)
			{
				throw new MissingMultipartPartsException(missingPartIds!);
			}

			await _amazonS3.CompleteMultipartUploadAsync(new CompleteMultipartUploadRequest {BucketName = _bucketName, Key = path, UploadId = uploadId, PartETags = etags.Values.ToList()});
		}

		public async Task PutMultipartPartAsync(string path, string uploadId, string partIdentifier, byte[] data)
		{
			await _amazonS3.UploadPartAsync(new UploadPartRequest { BucketName = _bucketName, Key = path, UploadId = uploadId, PartNumber = int.Parse(partIdentifier), InputStream = new MemoryStream(data)});
		}

		public Uri? GetWriteRedirectForPart(string path, string uploadId, string partIdentifier)
		{
			Uri? uri = GetPresignedUrl(path, HttpVerb.PUT, partNumber: int.Parse(partIdentifier), uploadId: uploadId);

			if (uri != null)
			{
				return uri;
			}

			return null;
		}

		public async Task RenameMultipartBlobAsync(string blobName, string targetBlobName)
		{
			GetObjectMetadataResponse metadata = await _amazonS3.GetObjectMetadataAsync(new GetObjectMetadataRequest {BucketName = _bucketName, Key = blobName, PartNumber = 1} );

			InitiateMultipartUploadResponse startMultipartUpload = await _amazonS3.InitiateMultipartUploadAsync(_bucketName, targetBlobName);
			string uploadId = startMultipartUpload.UploadId;
			ConcurrentBag<PartETag> parts = new();
			long chunkSize = metadata.ContentLength;
			long objectLength = long.Parse(metadata.ContentRange.Split('/')[1]);

			if (!metadata.PartsCount.HasValue)
			{
				throw new Exception($"No parts found on temp S3 multipart object \'{blobName}\', was this actually a multipart object?");
			}

			await Parallel.ForAsync(1,  metadata.PartsCount.Value + 1, async(int i, CancellationToken token) =>
			{
				long startOffset = (i - 1) * chunkSize;
				long endOffset = (i * chunkSize)-1; // last byte is exclusive

				endOffset = Math.Min(endOffset, objectLength-1);
				CopyPartResponse response = await _amazonS3.CopyPartAsync(new CopyPartRequest()
				{
					SourceBucket = _bucketName,
					DestinationBucket = _bucketName,
					PartNumber = i,
					SourceKey = blobName,
					DestinationKey = targetBlobName,
					UploadId = uploadId,
					FirstByte = startOffset,
					LastByte = endOffset, 
				}, token);
				parts.Add(new PartETag(i, response.ETag));
			});

			await _amazonS3.CompleteMultipartUploadAsync(new CompleteMultipartUploadRequest {BucketName = _bucketName, UploadId = uploadId, Key = targetBlobName, PartETags = parts.ToList()});

			// delete the old multipart object
			await _amazonS3.DeleteObjectAsync(_bucketName, blobName);
		}
	}

	public static class BlobIdentifierExtensions
	{
		public static string AsS3Key(this BlobId blobIdentifier)
		{
			string s = blobIdentifier.ToString();
			string prefix = s.Substring(0, 4);
			return $"{prefix}/{s}";
		}
	}
}
