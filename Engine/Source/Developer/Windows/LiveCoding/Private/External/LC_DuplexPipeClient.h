// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#if LC_VERSION == 1

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD
#include "LC_DuplexPipe.h"


class DuplexPipeClient : public DuplexPipe
{
public:
	bool Connect(const wchar_t* name);
};


#endif // LC_VERSION