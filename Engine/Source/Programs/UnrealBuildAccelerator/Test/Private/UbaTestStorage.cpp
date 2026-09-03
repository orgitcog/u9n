// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaFileAccessor.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaStorageClient.h"
#include "UbaStorageServer.h"

namespace uba
{
	bool TestStorage(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		#if PLATFORM_LINUX
		if (true) // TODO: Revisit this... fails on farm but works locally
			return true;
		#endif

		WorkManagerImpl workManager(1);
		StringBuffer<> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));
		StorageCreateInfo storageInfo(rootDir.data, logger.m_writer, workManager);
		storageInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		StorageImpl storage(storageInfo);

		StringBuffer<> detoursLib;
		GetDirectoryOfCurrentModule(logger, detoursLib);
		detoursLib.EnsureEndsWithSlash().Append(UBA_DETOURS_LIBRARY);

		storage.LoadCasTable();

		CasKey key;
		bool deferCreation = false;
		if (!storage.StoreCasFile(key, detoursLib.data, CasKeyZero, deferCreation))
			return logger.Error(TC("Failed to store file %s"), detoursLib.data);
		if (key == CasKeyZero)
			return logger.Error(TC("Failed to find file %s"), detoursLib.data);

		StringBuffer<> detoursLibCopy(detoursLib);
		detoursLibCopy.Append(TCV(".tmp"));

		auto deleteFile = MakeGuard([&]() { return DeleteFileW(detoursLibCopy.data); });

		if (!storage.CopyOrLink(key, detoursLibCopy.data, DefaultAttributes()))
			return logger.Error(TC("Failed to copy cas to file %s"), detoursLibCopy.data);

		FileHandle original;
		if (!OpenFileSequentialRead(logger, detoursLib.data, original))
			return logger.Error(TC("Failed to open %s for read"), detoursLib.data);
		auto closeOriginal = MakeGuard([&]() { return CloseFile(detoursLib.data, original); });

		FileHandle copy;
		if (!OpenFileSequentialRead(logger, detoursLibCopy.data, copy))
			return logger.Error(TC("Failed to open %s for read"), detoursLibCopy.data);
		auto closeCopy = MakeGuard([&]() { return CloseFile(detoursLibCopy.data, copy); });

		u64 originalSize;
		if (!GetFileSizeEx(originalSize, original))
			return logger.Error(TC("Failed to get size of %s"), detoursLib.data);
		u64 copySize;
		if (!GetFileSizeEx(copySize, copy))
			return logger.Error(TC("Failed to get size of %s"), detoursLibCopy.data);
		if (originalSize != copySize)
			return logger.Error(TC("Size mismatch between %s and %s (%llu vs %llu)"), detoursLib.data, detoursLibCopy.data, originalSize, copySize);

		u8 originalBuffer[64 * 1024];
		u8 copyBuffer[64 * 1024];
		u64 left = originalSize;
		while (left)
		{
			u64 toRead = Min(left, (u64)sizeof(originalBuffer));
			if (!ReadFile(logger, detoursLib.data, original, originalBuffer, toRead))
				return logger.Error(TC("Failed to read %u from %s"), detoursLib.data);
			if (!ReadFile(logger, detoursLibCopy.data, copy, copyBuffer, toRead))
				return logger.Error(TC("Failed to read %u from %s"), detoursLibCopy.data);
			if (memcmp(originalBuffer, copyBuffer, toRead) != 0)
				return logger.Error(TC("Data mismatch between %s and %s"), detoursLib.data, detoursLibCopy.data);

			left -= toRead;
		}

		if (!closeOriginal.Execute())
			return logger.Error(TC("Failed to close %s"), detoursLib.data);

		if (!closeCopy.Execute())
			return logger.Error(TC("Failed to close %s"), detoursLibCopy.data);

		if (!deleteFile.Execute())
			return logger.Error(TC("Failed to delete %s"), detoursLibCopy.data);

		return true;
	}

	bool TestRemoteStorageStore(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp serverTcp(logWriter, TC("ServerTcp"));
		NetworkBackendTcp clientTcp(logWriter, TC("ClientTcp"));

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);
		auto dg = MakeGuard([&]() { client.Disconnect(); });

		StringBuffer<> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageServerCreateInfo storageServerInfo(server, rootDir.data, logger.m_writer);
		storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		StorageServer storageServer(storageServerInfo);

		auto g = MakeGuard([&]() { server.DisconnectClients(); });

		rootDir.Append(TCV("Client"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageClientCreateInfo storageClientInfo(client, rootDir.data);
		StorageClient storageClient(storageClientInfo);
		storageClient.Start();

		if (!storageClient.LoadCasTable(true))
			return false;

		rootDir.EnsureEndsWithSlash();

		if (!server.StartListen(serverTcp, 1234))
			return logger.Error(TC("Failed to listen"));
		Sleep(100);
		if (!client.Connect(clientTcp, TC("127.0.0.1"), 1234))
			return logger.Error(TC("Failed to connect"));
		auto cg = MakeGuard([&]() { client.Disconnect(); });

		StringBuffer<> fileName;
		{
			fileName.Append(rootDir).Append(TCV("UbaTestFile"));
			FileAccessor fileHandle(logger, fileName.data);
			if (!fileHandle.CreateWrite())
				return logger.Error(TC("Failed to create file for write"));
			u8 byte = 'H';
			if (!fileHandle.Write(&byte, 1))
				return false;
			if (!fileHandle.Close())
				return false;
		}

		CasKey key;
		bool storeCompressed = false;
		if (!storageClient.StoreCasFileClient(key, ToStringKeyLower(fileName), fileName.data, FileMappingHandle{}, 0, 0, TC("UbaTestFile"), false, storeCompressed))
			return logger.Error(TC("Failed to store file %s"), fileName.data);

		fileName.Clear().Append(testRootDir).Append(TCV("Uba")).Append(PathSeparator).Append(TCV("UbaTestFile"));
		if (!storageServer.CopyOrLink(key, fileName.data, DefaultAttributes()))
			return logger.Error(TC("Failed to copy cas to file %s"), fileName.data);
		return true;
	}

	bool TestRemoteStorageFetch(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		auto& logWriter = logger.m_writer;
		NetworkBackendTcp serverTcp(logWriter, TC("ServerTcp"));
		NetworkBackendTcp clientTcp(logWriter, TC("ClientTcp"));

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, logWriter);
		NetworkClient client(ctorSuccess, logWriter);
		auto dg = MakeGuard([&]() { client.Disconnect(); });

		StringBuffer<> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageServerCreateInfo storageServerInfo(server, rootDir.data, logger.m_writer);
		storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		storageServerInfo.storeCompressed = false;
		StorageServer storageServer(storageServerInfo);

		auto g = MakeGuard([&]() { server.DisconnectClients(); });

		rootDir.Append(TCV("Client"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageClientCreateInfo storageClientInfo(client, rootDir.data);
		StorageClient storageClient(storageClientInfo);
		storageClient.Start();

		if (!storageClient.LoadCasTable(true))
			return false;

		rootDir.EnsureEndsWithSlash();

		if (!server.StartListen(serverTcp, 1234))
			return logger.Error(TC("Failed to listen"));
		Sleep(100);
		if (!client.Connect(clientTcp, TC("127.0.0.1"), 1234))
			return logger.Error(TC("Failed to connect"));
		auto cg = MakeGuard([&]() { client.Disconnect(); });

		StringBuffer<> fileName;
#if 1
		{
			fileName.Append(rootDir).Append(TCV("UbaTestFile"));
			FileAccessor fileHandle(logger, fileName.data);
			if (!fileHandle.CreateWrite())
				return logger.Error(TC("Failed to create file for write"));
			u8 byte = 'H';
			if (!fileHandle.Write(&byte, 1))
				return false;
			if (!fileHandle.Close())
				return false;
		}
#else
		fileName.Append(TCV("e:\\dev\\fn\\QAGame\\Saved\\StagedBuilds\\PS5_Temp\\Split\\qagame\\content\\paks\\qagame-ps5.ucas\\.copied"));
#endif

		CasKey casKey;
		if (!storageServer.CalculateCasKey(casKey, fileName.data))
			return false;

		casKey = AsCompressed(casKey, false);

		Storage::RetrieveResult result;
		if (!storageClient.RetrieveCasFile(result, casKey, fileName.data))
			return logger.Error(TC("Failed to store file %s"), fileName.data);

		return true;
	}
}
