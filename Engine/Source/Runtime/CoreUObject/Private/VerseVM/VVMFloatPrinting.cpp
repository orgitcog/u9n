// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMFloatPrinting.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Misc/AssertionMacros.h"
#include "Misc/StringBuilder.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMNativeString.h"
#include "VerseVM/VVMUnreachable.h"

// TODO: replace the not-yet-implemented-everywhere std::to_chars with a portable implementation.
#define HAS_FP_CHARCONV (PLATFORM_WINDOWS || PLATFORM_LINUX)

#if HAS_FP_CHARCONV
#include <charconv>
#endif

namespace Verse
{
void AppendDecimalToString(FUtf8StringBuilderBase& Builder, VFloat Float, EFloatStringFormat Format)
{
#if !HAS_FP_CHARCONV
	Format = EFloatStringFormat::Legacy;
#endif

	if (Float.IsNaN())
	{
		Builder.Append(UTF8TEXT("NaN"));
	}
	else if (Float == VFloat::Infinity())
	{
		Builder.Append(UTF8TEXT("Inf"));
	}
	else if (Float == -VFloat::Infinity())
	{
		Builder.Append(UTF8TEXT("-Inf"));
	}
	else if (Format == EFloatStringFormat::Legacy)
	{
		// This reproduces the original behavior of Verse's ToString(:float) function, but isn't ideal:
		// - It prints no more than 6 digits after the decimal point, while 64-bit floats may require hundreds of digits after the decimal point for exact reproduction.
		// - It prints 6 digits regardless of whether they are needed.
		// - It doesn't always print a decimal point, which might be desirable to ensure the output is syntactically distinct from integers.
		Builder.Appendf(UTF8TEXT("%f"), Float.AsDouble());
	}
	else
	{
		check(Format == EFloatStringFormat::ShortestOfFixedOrScientific);

#if HAS_FP_CHARCONV
		// Use std::to_chars to convert the VFloat to a decimal string.
		// 32 characters is enough to losslessly represent any VFloat with the default "general" format that allows scientific notation.
		constexpr size_t NumBufferChars = 32;
		char Buffer[NumBufferChars];
		char* const BufferEnd = Buffer + NumBufferChars;
		std::to_chars_result Result = std::to_chars(Buffer, BufferEnd, Float.AsDouble());
		if (!ensure(Result.ec == std::errc{}))
		{
			Builder.Append(UTF8TEXT("<std::to_chars error>"));
			return;
		}

		// Check whether the resulting string has a decimal point or exponent.
		bool bHasDecimalPointOrExponent = false;
		for (char* Ptr = Buffer; Ptr != Result.ptr; ++Ptr)
		{
			if (*Ptr == '.' || *Ptr == 'e')
			{
				bHasDecimalPointOrExponent = true;
				break;
			}
		}

		// If not, append a ".0" to ensure the resulting string is syntactically distinct from an integer.
		if (!bHasDecimalPointOrExponent)
		{
			check(Result.ptr + 2 < BufferEnd);
			*Result.ptr++ = '.';
			*Result.ptr++ = '0';
		}

		*Result.ptr = 0;
		Builder.Append(FUtf8StringView(Buffer, IntCastChecked<int32>(Result.ptr - Buffer)));
#else
		VERSE_UNREACHABLE();
#endif
	}
}
} // namespace Verse

#undef HAS_FP_CHARCONV