// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Containers/AnsiString.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Experimental/ZenServerInterface.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IO/IoDispatcher.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

#define UE_API IOSTOREUTILITIES_API

class FCbPackage;
class FCbObject;

namespace UE {
namespace Zen {
	struct FZenHttpRequestPool;
	class FZenHttpRequest;
	enum class EContentType;

	namespace Internal {
		struct FZenHttpClientState;
	}
}

/**
 * HTTP protocol implementation of Zen Store client interface
 */
class FZenStoreHttpClient
{
public:
	UE_API FZenStoreHttpClient();
	UE_API FZenStoreHttpClient(FStringView HostName, uint16 Port);
	UE_API FZenStoreHttpClient(UE::Zen::FServiceSettings&& InSettings);
	UE_API ~FZenStoreHttpClient();

	UE_API bool TryCreateProject(FStringView InProjectId, FStringView InParentProjectId, FStringView InOplogId, FStringView ServerRoot, 
					FStringView EngineRoot, FStringView ProjectRoot,
					FStringView ProjectFilePath);
	UE_API bool TryCreateOplog(FStringView InProjectId, FStringView InOplogId, FStringView InOplogLifetimeMarkerPath);
	UE_API bool TryDeleteOplog(FStringView InProjectId, FStringView InOplogId);

	UE_API void InitializeReadOnly(FStringView InProjectId, FStringView InOplogId);

	UE_API bool IsConnected() const;

	UE_API void StartBuildPass();
	UE_API TIoStatusOr<uint64> EndBuildPass(FCbPackage OpEntry);

	UE_API TIoStatusOr<uint64> AppendOp(FCbPackage OpEntry);

	UE_API TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& Id);
	UE_API TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& Id, uint64 Offset = 0, uint64 Size = ~0ull);
	UE_API TIoStatusOr<FIoBuffer> ReadChunk(const FIoHash& RawHash, uint64 Offset = 0, uint64 Size = ~0ull);
	// TODO: Expand API to add partial chunk requests and ModTag field
	UE_API UE_INTERNAL void ReadChunks(TConstArrayView<FIoChunkId> Ids, TUniqueFunction<void(const FIoChunkId& Id, TIoStatusOr<FIoBuffer> Result)>&& ChunkCallback);
	UE_API UE_INTERNAL void ReadChunks(TConstArrayView<FIoHash> RawHashes, TUniqueFunction<void(const FIoHash& RawHash, TIoStatusOr<FIoBuffer> Result)>&& ChunkCallback);
	UE_API UE_INTERNAL void ReadChunksAsync(TConstArrayView<FIoChunkId> Ids, TUniqueFunction<void(const FIoChunkId& Id, TIoStatusOr<FIoBuffer> Result)>&& ChunkCallback);
	UE_API UE_INTERNAL void ReadChunksAsync(TConstArrayView<FIoHash> RawHashes, TUniqueFunction<void(const FIoHash& RawHash, TIoStatusOr<FIoBuffer> Result)>&& ChunkCallback);

	UE_DEPRECATED(5.6, "Use ReadChunk(FIoHash) instead")
	UE_API TIoStatusOr<FIoBuffer> ReadOpLogAttachment(FStringView Id);

#if UE_WITH_ZEN
	const TCHAR* GetHostName() const { return ZenService.GetInstance().GetHostName(); }
	uint16 GetPort() const { return ZenService.GetInstance().GetPort(); }
	const UE::Zen::FZenServiceInstance& GetZenServiceInstance() const { return ZenService.GetInstance(); }
	UE::Zen::FZenServiceInstance& GetZenServiceInstance() { return ZenService.GetInstance(); }
#else // Default to localhost:8558 for platforms where Zen wouldn't be supported yet
	const TCHAR* GetHostName() const { return TEXT("localhost"); }
	uint16 GetPort() const { return 8558; }
#endif

	UE_API TFuture<TIoStatusOr<FCbObject>> GetProjectInfo();
	UE_API TFuture<TIoStatusOr<FCbObject>> GetOplog(bool bTrimByReferencedset = true);
	UE_API TFuture<TIoStatusOr<FCbObject>> GetFiles();
	UE_API TFuture<TIoStatusOr<FCbObject>> GetChunkInfos();

	static UE_API const UTF8CHAR* FindOrAddAttachmentId(FUtf8StringView AttachmentText);
	static UE_API const UTF8CHAR* FindAttachmentId(FUtf8StringView AttachmentText);

private:
	UE_API TIoStatusOr<FIoBuffer> ReadOpLogUri(FStringBuilderBase& ChunkUri, uint64 Offset = 0, uint64 Size = ~0ull);
	UE_API bool Download(Zen::FZenHttpRequest& Request, FStringView Uri, TArray64<uint8>* Buffer, Zen::EContentType AcceptType);
	UE_API bool Post(Zen::FZenHttpRequest& Request, FStringView Uri, FCbObjectView Obj);
	UE_API bool Post(Zen::FZenHttpRequest& Request, FStringView Uri, FMemoryView Payload);
	UE_API void Rpc(FAnsiStringView Uri, FCbObject RpcRequestObject, FCbPackage& OutResponse);
	UE_API void RpcAsync(FAnsiStringView Uri, FCbObject RpcRequestObject, TUniqueFunction<void(const FCbPackage& Response)>&& ResponseCallback);
	UE_API bool Delete(Zen::FZenHttpRequest& Request, FStringView Uri);
	UE_API bool ShouldRecoverAndRetry(Zen::FZenHttpRequest& Request);

	template<bool bAsync, typename T>
	void ReadChunks_Internal(TConstArrayView<T> Requests, TUniqueFunction<void(const T& Id, TIoStatusOr<FIoBuffer> Result)>&& ChunkCallback);

	UE_API void InitCommon();

	static UE_API const uint32 PoolEntryCount;
	struct FSaltGenerator
	{
		FSaltGenerator();
		inline int32_t Next()
		{
			const uint32_t A = ++GOpCounter;
			return static_cast<int32_t>((A ^ (SaltBase + (A << 6) + (A >> 2))) & 0x7fffffffu);
		}
	private:
		static std::atomic<uint32> GOpCounter;
		const uint32_t SaltBase;
	};
#if UE_WITH_ZEN
	UE::Zen::FScopeZenService ZenService;
#endif
	// Legacy request pool
	TUniquePtr<Zen::FZenHttpRequestPool> RequestPool;

	// Newer connection pool and request queue
	TUniquePtr<Zen::Internal::FZenHttpClientState> ClientState;

	FSaltGenerator SaltGen;
	FString ProjectPath;
	FString OplogPath;
	FAnsiString OplogRpcPath;
	FString OplogNewEntryPath;
	FString OplogPrepNewEntryPath;
	FString TempDirPath;
	const uint64 StandaloneThresholdBytes = 1 * 1024 * 1024;
	bool bAllowRead = false;
	bool bAllowEdit = false;
	bool bConnectionSucceeded = false;
};

}

#undef UE_API
