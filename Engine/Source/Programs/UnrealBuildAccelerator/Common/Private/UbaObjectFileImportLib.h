// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaObjectFile.h"

namespace uba
{
	class ObjectFileImportLib : public ObjectFile
	{
	public:
		virtual bool Parse(Logger& logger, ObjectFileParseMode parseMode, const tchar* hint) override;
		virtual const char* GetLibName() override;

	private:
		std::string m_libName;
	};


	bool IsImportLib(const u8* data, u64 dataSize);
}
