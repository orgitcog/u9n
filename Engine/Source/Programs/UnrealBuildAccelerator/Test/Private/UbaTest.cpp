// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTestAll.h"
#include "UbaFileAccessor.h"

namespace uba
{
	bool CreateTestFile(Logger& logger, StringView testRootDir, StringView fileName, StringView content, u32 attributes)
	{
		StringBuffer<> testFileName;
		return CreateTestFile(testFileName, logger, testRootDir, fileName, content, attributes);
	}

	bool CreateTestFile(StringBufferBase& outFile, Logger& logger, StringView testRootDir, StringView fileName, StringView content, u32 attributes)
	{
		outFile.Clear().Append(testRootDir).EnsureEndsWithSlash().Append(fileName).FixPathSeparators();

		if (fileName.Contains(PathSeparator) || fileName.Contains(NonPathSeparator))
		{
			StringBuffer<> testFileDir;
			testFileDir.AppendDir(outFile);
			DirectoryCache().CreateDirectory(logger, testFileDir.data);
		}

		FileAccessor file(logger, outFile.data);
		if (!file.CreateMemoryWrite(false, attributes, content.count))
			return logger.Error(TC("Failed to create file for write"));
		memcpy(file.GetData(), content.data, content.count);
		return file.Close();
	}

	bool DeleteTestFile(Logger& logger, StringView testRootDir, StringView fileName)
	{
		StringBuffer<> testFileName;
		testFileName.Append(testRootDir).EnsureEndsWithSlash().Append(fileName).FixPathSeparators();
		return DeleteFileW(testFileName.data);
	}

	bool FileExists(Logger& logger, StringView dir, StringView fileName)
	{
		StringBuffer<> testFileName;
		testFileName.Append(dir).EnsureEndsWithSlash().Append(fileName).FixPathSeparators();
		return FileExists(logger, testFileName.data);
	}

	bool WrappedMain(int argc, tchar* argv[])
	{
		AddExceptionHandler();
		return RunTests(argc, argv);
	}
}

#if PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[])
{
	return uba::WrappedMain(argc, argv) ? 0 : -1;
}
#else
int main(int argc, char* argv[])
{
	return uba::WrappedMain(argc, argv) ? 0 : -1;
}
#endif
