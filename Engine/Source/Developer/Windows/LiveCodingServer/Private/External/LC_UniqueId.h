// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#if LC_VERSION == 1

// BEGIN EPIC MOD
#include "CoreTypes.h"
#include <string>
// END EPIC MOD

namespace uniqueId
{
	void Startup(void);
	void Shutdown(void);

	uint32_t Generate(const std::wstring& path);
}


#endif