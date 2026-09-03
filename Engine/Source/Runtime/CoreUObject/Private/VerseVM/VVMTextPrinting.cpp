// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMTextPrinting.h"
#include "Containers/Utf8String.h"
#include "Misc/AssertionMacros.h"
#include "Misc/StringBuilder.h"

static bool DecodeCodePointFromUTF8(const UTF8CHAR*& FirstCodeUnit, const UTF8CHAR* CodeUnitsEnd, UTF32CHAR& OutCodePoint)
{
	checkfSlow(FirstCodeUnit < CodeUnitsEnd, TEXT("Can't decode UTF-8 from empty string!"));

	// If ASCII, deal with it right here
	UTF32CHAR Byte1 = *FirstCodeUnit;
	if (Byte1 < 128)
	{
		++FirstCodeUnit;
		OutCodePoint = Byte1;
		return true;
	}
	else if (Byte1 >= 192 && Byte1 < 224) // two bytes
	{
		// Ensure our string has enough characters to read from
		if (FirstCodeUnit + 2 <= CodeUnitsEnd)
		{
			Byte1 -= (128 + 64);
			const UTF32CHAR Byte2 = FirstCodeUnit[1];
			if ((Byte2 & (128 + 64)) == 128) // Verify format 10xxxxxx
			{
				const UTF32CHAR CodePoint = ((Byte1 << 6) | (Byte2 - 128));
				if ((CodePoint >= 0x80) && (CodePoint <= 0x7FF))
				{
					FirstCodeUnit += 2;
					OutCodePoint = CodePoint;
					return true;
				}
			}
		}
	}
	else if (Byte1 < 240) // three bytes
	{
		// Ensure our string has enough characters to read from
		if (FirstCodeUnit + 3 <= CodeUnitsEnd)
		{
			Byte1 -= (128 + 64 + 32);
			const UTF32CHAR Byte2 = FirstCodeUnit[1];
			const UTF32CHAR Byte3 = FirstCodeUnit[2];
			if ((Byte2 & (128 + 64)) == 128 && (Byte3 & (128 + 64)) == 128) // Verify format 10xxxxxx
			{
				const UTF32CHAR CodePoint = (((Byte1 << 12)) | ((Byte2 - 128) << 6) | ((Byte3 - 128)));

				// UTF-8 characters cannot be in the UTF-16 surrogates range
				constexpr UTF32CHAR UTF16HighSurrogateStartCodePoint = 0xD800;
				constexpr UTF32CHAR UTF16HighSurrogateEndCodePoint = 0xDBFF;
				constexpr UTF32CHAR UTF16LowSurrogateStartCodePoint = 0xDC00;
				constexpr UTF32CHAR UTF16LowSurrogateEndCodePoint = 0xDFFF;
				if ((CodePoint < UTF16HighSurrogateStartCodePoint || CodePoint > UTF16HighSurrogateEndCodePoint)
					&& (CodePoint < UTF16LowSurrogateStartCodePoint || CodePoint > UTF16LowSurrogateEndCodePoint))
				{
					FirstCodeUnit += 3;
					OutCodePoint = CodePoint;
					return true;
				}
			}
		}
	}
	else if (Byte1 < 248) // four bytes
	{
		// Ensure our string has enough characters to read from
		if (FirstCodeUnit + 4 <= CodeUnitsEnd)
		{
			Byte1 -= (128 + 64 + 32 + 16);
			const UTF32CHAR Byte2 = FirstCodeUnit[1];
			const UTF32CHAR Byte3 = FirstCodeUnit[2];
			const UTF32CHAR Byte4 = FirstCodeUnit[3];
			if ((Byte2 & (128 + 64)) == 128 && (Byte3 & (128 + 64)) == 128 && (Byte4 & (128 + 64)) == 128) // Verify format 10xxxxxx
			{
				const UTF32CHAR CodePoint = (((Byte1 << 18)) | ((Byte2 - 128) << 12) | ((Byte3 - 128) << 6) | ((Byte4 - 128)));
				if ((CodePoint >= 0x10000) && (CodePoint <= 0x10FFFF))
				{
					FirstCodeUnit += 4;
					OutCodePoint = CodePoint;
					return true;
				}
			}
		}
	}

	return false;
}

static bool IsPrintableASCII(UTF32CHAR C)
{
	return C >= 32 && C <= 126;
}

static bool IsPrintableAsEscapeCode(UTF32CHAR CodePoint, char& OutEscapedChar)
{
	switch (CodePoint)
	{
		case '\t':
			OutEscapedChar = 't';
			return true;
		case '\n':
			OutEscapedChar = 'n';
			return true;
		case '\r':
			OutEscapedChar = 'r';
			return true;
		case '"':
		case '#':
		case '&':
		case '\'':
		case '<':
		case '>':
		case '\\':
		case '{':
		case '}':
		case '~':
			OutEscapedChar = static_cast<char>(CodePoint);
			return true;
		default:
			return false;
	}
}

namespace Verse
{
void AppendVerseToString(FUtf8StringBuilderBase& Builder, UTF8CHAR Char)
{
	char EscapedChar = 0;
	if (IsPrintableAsEscapeCode(Char, EscapedChar))
	{
		Builder.Appendf(UTF8TEXT("'\\%c'"), EscapedChar);
	}
	else if (IsPrintableASCII(Char))
	{
		Builder.Appendf(UTF8TEXT("'%c'"), static_cast<char>(Char));
	}
	else
	{
		Builder.Appendf(UTF8TEXT("0o%.2x"), static_cast<unsigned int>(Char));
	}
}

void AppendVerseToString(FUtf8StringBuilderBase& Builder, UTF32CHAR Char)
{
	Builder.Appendf(UTF8TEXT("0u%x"), static_cast<unsigned int>(Char));
}

void AppendVerseToString(FUtf8StringBuilderBase& Builder, FUtf8StringView String)
{
	Builder.AppendChar(UTF8TEXT('\"'));

	const UTF8CHAR* NextCodeUnit = String.GetData();
	const UTF8CHAR* CodeUnitsEnd = NextCodeUnit + String.Len();
	while (NextCodeUnit < CodeUnitsEnd)
	{
		UTF32CHAR CodePoint;
		if (!DecodeCodePointFromUTF8(NextCodeUnit, CodeUnitsEnd, CodePoint))
		{
			// If UTF-8 decoding failed, just print the next code unit independently of the rest of the string.
			CodePoint = *NextCodeUnit++;
		}

		char EscapedChar = 0;
		if (IsPrintableAsEscapeCode(CodePoint, EscapedChar))
		{
			Builder.AppendChar(UTF8TEXT('\\'));
			Builder.AppendChar(EscapedChar);
		}
		else if (IsPrintableASCII(CodePoint))
		{
			Builder.AppendChar(static_cast<char>(CodePoint));
		}
		else if (CodePoint <= 0xff)
		{
			Builder.Appendf(UTF8TEXT("{0o%.2x}"), CodePoint);
		}
		else
		{
			Builder.Appendf(UTF8TEXT("{0u%x}"), CodePoint);
		}
	}
	Builder.AppendChar(UTF8TEXT('\"'));
}

FUtf8String ToVerseString(UTF8CHAR Char)
{
	TUtf8StringBuilder<8> Builder;
	AppendVerseToString(Builder, Char);
	return FUtf8String(MoveTemp(Builder));
}

FUtf8String ToVerseString(UTF32CHAR Char)
{
	TUtf8StringBuilder<8> Builder;
	AppendVerseToString(Builder, Char);
	return FUtf8String(MoveTemp(Builder));
}

FUtf8String ToVerseString(FUtf8StringView String)
{
	TUtf8StringBuilder<32> Builder;
	AppendVerseToString(Builder, String);
	return FUtf8String(MoveTemp(Builder));
}

} // namespace Verse
