// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReaderShared.h"
#include "Templates/SharedPointerFwd.h"

namespace psd
{
	struct Document;
}

namespace UE::PSDImporter::Private
{
	class FDocumentReader
	{
	public:
		bool Read(FReadContext& InContext);
	};
}
