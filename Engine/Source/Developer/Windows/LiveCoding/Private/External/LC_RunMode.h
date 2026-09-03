// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

#if LC_VERSION == 1

struct RunMode
{
	enum Enum
	{
		DEFAULT,
		EXTERNAL_BUILD_SYSTEM,

		INVALID
	};
};


#endif // LC_VERSION