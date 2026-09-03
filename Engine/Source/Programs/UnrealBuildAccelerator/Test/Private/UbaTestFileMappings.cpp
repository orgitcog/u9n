// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaLogger.h"
#include "UbaFileMapping.h"

namespace uba
{
	bool TestFileMappingBuffer(LoggerWithWriter& logger, const StringBufferBase& rootDir)
	{

		FileMappingBuffer mappingBuffer(logger);

		mappingBuffer.AddTransient(TC("Temp"), false);

		MappedView view = mappingBuffer.AllocAndMapView(MappedView_Transient, 1024, 1, TC("Foo"));

		*(u64*)view.memory = 1337;
		u64 value = *(u64*)view.memory;
		if (value != 1337)
			return false;
		mappingBuffer.UnmapView(view, TC("Foo"));

		return true;
	}
}
