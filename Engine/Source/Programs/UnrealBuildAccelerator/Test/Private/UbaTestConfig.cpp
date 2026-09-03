// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaConfig.h"
#include "UbaLogger.h"
#include "UbaPlatform.h"

namespace uba
{
	bool TestLoadConfig(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		static const char* configText =
			"RootDir = \"e:\\foo\"\r\n"
			"[CacheClient]\r\n"
			"UseDirectoryPreparsing = true\r\n"
			"# Comment = true\r\n"
			"";

		Config config;
		if (!config.LoadFromText(logger, configText, strlen(configText)))
			return false;

		const ConfigTable* tablePtr = config.GetTable(TC("CacheClient"));
		if (!tablePtr)
			return false;
		const ConfigTable& table = *tablePtr;
		bool test = false;
		if (!table.GetValueAsBool(test, TC("UseDirectoryPreparsing")))
			return false;
		if (test != true)
			return false;
		const tchar* str = nullptr;
		if (!table.GetValueAsString(str, TC("RootDir")))
			return false;
		if (TStrcmp(str, TC("e:\\foo")) != 0)
			return false;
		if (table.GetValueAsBool(test, TC("Comment")))
			return false;
		return true;
	}

	bool TestSaveConfig(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{
		Vector<char> data;
		{
			Config config;
			ConfigTable& table = config.AddTable(TC("TestTable"));
			table.AddValue(TC("Foo"), 42);
			config.SaveToText(logger, data);
		}

		Config config;
		if (!config.LoadFromText(logger, data.data(), data.size()))
			return false;

		const ConfigTable* tablePtr = config.GetTable(TC("TestTable"));
		if (!tablePtr)
			return false;
		const ConfigTable& table = *tablePtr;
		int foo = 0;
		if (!table.GetValueAsInt(foo, TC("Foo")))
			return false;
		if (foo != 42)
			return false;
		return true;
	}
}