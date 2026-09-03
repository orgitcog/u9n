// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDdcUtils.h"
#include "Cloud/MetaHumanCloudServicesSettings.h"

#include "DerivedDataCacheInterface.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheModule.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataRequestTypes.h"
#include "DerivedDataValueId.h"
#include "Settings/EditorProjectSettings.h"
#include "Memory/SharedBuffer.h"
#include "Async/Async.h"

namespace UE::MetaHuman
{
	bool CacheAvailable()
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
		if (Settings->DdcPolicy != EMetaHumanCloudServicesDdcPolicy::NoCache)
		{
			using namespace UE::DerivedData;
			return (TryGetCache() != nullptr);
		}
		return false;
	}

	FSharedBuffer TryCacheFetch(const FString& CacheKeyString)
	{
		FSharedBuffer HighFrequencyDataBuffer;
		if (CacheAvailable())
		{
			// check the DDC for this texture data
			using namespace UE::DerivedData;
			if (ICache* Cache = TryGetCache())
			{
				const FValueId CacheKeyValueId = FValueId::FromName(CacheKeyString);

				FCacheKey CacheKey;
				static const TCHAR* MetaHumanCacheBucketName = TEXT("MetaHumanCloudServices");
				CacheKey.Bucket = FCacheBucket(MetaHumanCacheBucketName);
				CacheKey.Hash = FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(CacheKeyString)));

				ECachePolicy QueryPolicy = ECachePolicy::Query;
				const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
				switch (Settings->DdcPolicy)
				{
				case EMetaHumanCloudServicesDdcPolicy::UseLocalDdc:
					QueryPolicy = ECachePolicy::QueryLocal;
					break;
				case EMetaHumanCloudServicesDdcPolicy::UseSharedDdc:
					QueryPolicy = ECachePolicy::QueryRemote;
					break;
				default:;
				}

				FCacheRecordPolicyBuilder PolicyBuilder(QueryPolicy);
				FCacheGetRequest Request;
				Request.Name = TEXT("MetaHumanServiceRequest");
				Request.Key = CacheKey;
				Request.Policy = PolicyBuilder.Build();

				FRequestOwner GetRequestOwner(EPriority::Blocking);
				Cache->Get(MakeArrayView(&Request, 1), GetRequestOwner,
					[&HighFrequencyDataBuffer, &CacheKeyValueId](FCacheGetResponse&& Response)
					{
						if (Response.Status == EStatus::Ok)
						{
							const FCompressedBuffer& CompressedBuffer = Response.Record.GetValue(CacheKeyValueId).GetData();
							HighFrequencyDataBuffer = CompressedBuffer.Decompress();
						}
					});
				GetRequestOwner.Wait();
			}
		}
		return HighFrequencyDataBuffer;
	}

	void UpdateCacheAsync(const FString& CacheKeyString, FSharedString InRequestName, FSharedBuffer InOutSharedBuffer)
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();
		if (Settings->DdcPolicy != EMetaHumanCloudServicesDdcPolicy::NoCache)
		{
			using namespace UE::DerivedData;
			if (ICache* Cache = TryGetCache())
			{
				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Cache, InOutSharedBuffer, CacheKeyString, InRequestName]()
					{
						const FValueId CacheKeyValueId = FValueId::FromName(CacheKeyString);
						FCacheKey CacheKey;
						CacheKey.Bucket = FCacheBucket(TEXT("MetaHumanCloudServices"));
						CacheKey.Hash = FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(CacheKeyString)));
						FCacheRecordBuilder RecordBuilder(CacheKey);
						FValue Value = FValue::Compress(InOutSharedBuffer);
						RecordBuilder.AddValue(CacheKeyValueId, Value);

						ECachePolicy StorePolicy = ECachePolicy::Default;
						FRequestOwner PutRequestOwner = FRequestOwner(EPriority::Normal);
						FCachePutRequest PutRequest = {
							InRequestName,
							RecordBuilder.Build(),
							StorePolicy
						};
						Cache->Put(MakeArrayView(&PutRequest, 1), PutRequestOwner, [](FCachePutResponse&& Response) {});
						PutRequestOwner.Wait();
					});
			}
		}
	}
}