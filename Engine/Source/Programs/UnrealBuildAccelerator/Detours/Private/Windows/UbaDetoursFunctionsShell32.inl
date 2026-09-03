// Copyright Epic Games, Inc. All Rights Reserved.


//typedef struct _MODLOAD_DATA {} *PMODLOAD_DATA;
using SHGetKnownFolderPathFunc = HRESULT(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);
SHGetKnownFolderPathFunc* True_SHGetKnownFolderPath;

HRESULT Detoured_SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath)
{
	if (g_runningRemote)
	{
		UBA_ASSERT(hToken == NULL);
		RPC_MESSAGE(SHGetKnownFolderPath, getFullFileName)
		writer.WriteBytes(&rfid, sizeof(KNOWNFOLDERID));
		writer.WriteU32(dwFlags);
		writer.Flush();
		BinaryReader reader;
		HRESULT res = reader.ReadU32();
		*ppszPath = NULL;
		if (res == S_OK)
		{
			StringBuffer<> path;
			reader.ReadString(path);
			u32 memSize = (path.count+1)*2;
			void* mem = CoTaskMemAlloc(memSize);
			UBA_ASSERT(mem);
			if (!mem)
				return E_FAIL;
			memcpy(mem, path.data, memSize);
			*ppszPath = (PWSTR)mem;
		}
		DEBUG_LOG_DETOURED(L"SHGetKnownFolderPath", L"(%ls) -> %ls", *ppszPath, ToString(res == S_OK));
		return res;
	}

	SuppressDetourScope _;
	HRESULT res = True_SHGetKnownFolderPath(rfid, dwFlags, hToken, ppszPath);
	DEBUG_LOG_TRUE(L"SHGetKnownFolderPath", L"(%ls) -> %ls", *ppszPath, ToString(res == S_OK));
	return res;
}
