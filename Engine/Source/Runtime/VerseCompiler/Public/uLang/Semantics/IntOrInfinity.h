// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/Common/Misc/Optional.h"
#include <cstdint>

namespace uLang
{
// Encodes either an integer or positive/negative infinity.
enum class EInfinitySign
{
    Negative,
    Positive
};
constexpr EInfinitySign operator-(EInfinitySign Sign)
{
    return Sign == EInfinitySign::Negative ? EInfinitySign::Positive : EInfinitySign::Negative;
}

template <EInfinitySign InfinitySign>
struct TIntOrInfinity
{
    TIntOrInfinity(int64_t InFiniteInt)
        : MaybeFiniteInt(InFiniteInt)
    {
    }

    static TIntOrInfinity Infinity() { return TIntOrInfinity(); }

    friend bool operator==(int64_t Lhs, const TIntOrInfinity& Rhs)
    {
        return Rhs.IsFinite() && Lhs == Rhs.GetFiniteInt();
    }
    friend bool operator<(int64_t Lhs, const TIntOrInfinity& Rhs)
    {
        return Rhs.IsInfinity()
            ? InfinitySign == EInfinitySign::Positive
            : Lhs < Rhs.GetFiniteInt();
    }
    friend bool operator>(int64_t Lhs, const TIntOrInfinity& Rhs)
    {
        return Rhs.IsInfinity()
            ? InfinitySign == EInfinitySign::Negative
            : Lhs > Rhs.GetFiniteInt();
    }
    friend bool operator<=(int64_t Lhs, const TIntOrInfinity& Rhs) { return Lhs == Rhs || Lhs < Rhs; }
    friend bool operator>=(int64_t Lhs, const TIntOrInfinity& Rhs) { return Lhs == Rhs || Lhs > Rhs; }

    friend bool operator<(const TIntOrInfinity& Lhs, int64_t Rhs) { return Rhs > Lhs; }
    friend bool operator>(const TIntOrInfinity& Lhs, int64_t Rhs) { return Rhs < Lhs; }
    friend bool operator<=(const TIntOrInfinity& Lhs, int64_t Rhs) { return Rhs == Lhs || Rhs > Lhs; }
    friend bool operator>=(const TIntOrInfinity& Lhs, int64_t Rhs) { return Rhs == Lhs || Rhs < Lhs; }

    bool IsInfinity() const { return !IsFinite(); }
    bool IsFinite() const { return MaybeFiniteInt.IsSet(); }
    int64_t GetFiniteInt() const { return MaybeFiniteInt.GetValue(); }

    bool IsSafeToNegate() const
    {
        if (MaybeFiniteInt)
        {
            return *MaybeFiniteInt != INT64_MIN;
        }
        return true;
    }

private:
    TOptional<int64_t> MaybeFiniteInt;

    TIntOrInfinity()
    {
    }
};

using FIntOrNegativeInfinity = TIntOrInfinity<EInfinitySign::Negative>;
using FIntOrPositiveInfinity = TIntOrInfinity<EInfinitySign::Positive>;

template<EInfinitySign LhsSign, EInfinitySign RhsSign>
inline bool operator==(const TIntOrInfinity<LhsSign>& Lhs, const TIntOrInfinity<RhsSign>& Rhs)
{
    if (Lhs.IsInfinity())
    {
        return Rhs.IsInfinity() && LhsSign == RhsSign;
    }
    else
    {
        return Rhs.IsFinite() && Lhs.GetFiniteInt() == Rhs.GetFiniteInt();
    }
}

template<EInfinitySign LhsSign, EInfinitySign RhsSign>
inline bool operator<(const TIntOrInfinity<LhsSign>& Lhs, const TIntOrInfinity<RhsSign>& Rhs)
{
    if (Lhs.IsInfinity() && Rhs.IsInfinity())
    {
        return LhsSign == EInfinitySign::Negative
            && RhsSign == EInfinitySign::Positive;
    }
    else if (Lhs.IsInfinity())
    {
        return LhsSign == EInfinitySign::Negative;
    }
    else if (Rhs.IsInfinity())
    {
        return RhsSign == EInfinitySign::Positive;
    }
    else
    {
        return Lhs.GetFiniteInt() < Rhs.GetFiniteInt();
    }
}
template<EInfinitySign LhsSign, EInfinitySign RhsSign>
inline bool operator<=(const TIntOrInfinity<LhsSign>& Lhs, const TIntOrInfinity<RhsSign>& Rhs)
{
    return Lhs == Rhs || Lhs < Rhs;
}
template<EInfinitySign LhsSign, EInfinitySign RhsSign>
inline bool operator>(const TIntOrInfinity<LhsSign>& Lhs, const TIntOrInfinity<RhsSign>& Rhs)
{
    return Rhs < Lhs;
}
template<EInfinitySign LhsSign, EInfinitySign RhsSign>
inline bool operator>=(const TIntOrInfinity<LhsSign>& Lhs, const TIntOrInfinity<RhsSign>& Rhs)
{
    return Lhs == Rhs || Rhs < Lhs;
}

template<EInfinitySign OperandSign>
inline TIntOrInfinity<-OperandSign> operator-(const TIntOrInfinity<OperandSign>& Operand)
{
    if (Operand.IsInfinity())
    {
        return TIntOrInfinity<-OperandSign>::Infinity();
    }
    else
    {
        ULANG_ASSERTF(Operand.GetFiniteInt() != INT64_MIN, "Can't negate INT64_MIN");
        return TIntOrInfinity<-OperandSign>(-Operand.GetFiniteInt());
    }
}
}