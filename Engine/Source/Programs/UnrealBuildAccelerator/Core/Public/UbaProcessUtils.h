// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaPlatform.h"

#if defined(__clang_analyzer__)
#define stackAlloc(x) malloc(x)
#define stackFree(x) free(x)
#elif PLATFORM_WINDOWS
#include <malloc.h>
#define stackAlloc(x) _malloca(x)
#define stackFree(x) _freea(x)
#else
#define stackAlloc(x) alloca(x)
#define stackFree(x)
#endif

namespace uba
{
	// func is void(const CharType* arg, u32 strArg)
	template<typename CharType, typename Func>
	bool ParseArguments(const CharType* arguments, u64 argumentsLen, const Func& argumentFunc)
	{
		if (argumentsLen == 0)
			return true;

		const CharType* argStart = arguments;
		bool isInArg = false;
		bool isInQuotes = false;
		bool isInBackslashQuote = false;
		bool isEnd = *arguments == 0;
		CharType currentChar = 0;
		CharType lastChar = 0;
		bool isBackslashOwned = false;

		CharType* argWriteBuffer = static_cast<CharType*>( stackAlloc(sizeof(CharType) * (argumentsLen + 1)) );
		auto g = MakeGuard([&]() { stackFree(argWriteBuffer); });

		for (const CharType* it = arguments; !isEnd; lastChar = currentChar, ++it)
		{
			bool pastEnd = u64(it - arguments) == argumentsLen;
			if (!pastEnd)
				currentChar = *it;
			isEnd = pastEnd || currentChar == 0;
			if (isEnd || currentChar == ' ' || currentChar == '\t' || currentChar == '\n')
			{
				if (isInQuotes || !isInArg)
					continue;

				const CharType* argIt = argStart;
				const CharType* argEnd = it;

				if ((pastEnd || *argEnd == '\n') && argStart != argEnd && *(argEnd-1) == '\r')
					--argEnd;

				CharType* argWriteIt = argWriteBuffer;
				CharType lastChar2 = 0;
				isBackslashOwned = false;
				while (argIt != argEnd)
				{
					if (*argIt == '\"')
					{
						if (lastChar2 == '\\' && !isBackslashOwned)
							argWriteIt[-1] = '\"';
						lastChar2 = 0;
						++argIt;
						continue;
					}

					if (*argIt == '\\' && lastChar2 == '\\')
						isBackslashOwned = !isBackslashOwned;
					else
						isBackslashOwned = false;

					*argWriteIt++ = *argIt;
					lastChar2 = *argIt;
					++argIt;
				}

				if (argWriteBuffer != argWriteIt)
				{
					*argWriteIt = 0;
					argumentFunc(argWriteBuffer, u32(argWriteIt - argWriteBuffer));
				}
				isInArg = false;
				isBackslashOwned = false;
				continue;
			}

			if (!isInArg)
			{
				isInArg = true;
				argStart = it;
				if (currentChar == '\"')
					isInQuotes = true;
				continue;
			}

			if (*it == '\"')
			{
				if (isInQuotes && lastChar == '\\' && (!isBackslashOwned && !isInBackslashQuote))
					continue;

				isInQuotes = !isInQuotes;
				isInBackslashQuote = isInQuotes && lastChar == '\\';
			}

			if (*it == '\\' && lastChar == '\\')
				isBackslashOwned = !isBackslashOwned;
			else
				isBackslashOwned = false;
		}
		return true;
	}

	template<typename Func>
	bool ParseArguments(const tchar* arguments, const Func& argumentFunc)
	{
		return ParseArguments(arguments, TStrlen(arguments), argumentFunc);
	}
}
