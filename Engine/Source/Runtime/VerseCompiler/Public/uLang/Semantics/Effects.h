// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Compiler Public API

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/Common/Containers/RangeView.h"

namespace uLang
{

// If you change this enum macro, you must keep things in sync with both VVMVerseEffectSet.h and MakeEffectSet
#define VERSE_ENUM_EFFECTS(v) \
    v(suspends)     \
    v(decides)      \
    v(diverges)     \
    v(reads)        \
    v(writes)       \
    v(allocates)    \
    v(dictates)     \
    v(no_rollback)

enum class EEffect
{
#define VISIT_EFFECT(Name) Name,
    VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
};

#define VISIT(Name) 1 +
inline constexpr int32_t EEffectNum = VERSE_ENUM_EFFECTS(VISIT) 0;
#undef VISIT

struct SEffectInfo
{
    const char* _AttributeName;
    EEffect _Effect;
};

VERSECOMPILER_API TRangeView<SEffectInfo*, SEffectInfo*> AllEffectInfos();
VERSECOMPILER_API SEffectInfo GetEffectInfo(EEffect Effect);

struct SEffectSetBase {};

struct SEffectSet : private SEffectSetBase
{
    constexpr SEffectSet()
    : SEffectSetBase()
#define VISIT_EFFECT(Name) , _##Name(false)
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
    {}
    constexpr SEffectSet(EEffect SingleEffect)
    : SEffectSetBase()
#define VISIT_EFFECT(Name) , _##Name(SingleEffect == EEffect::Name)
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
    {}

    friend constexpr SEffectSet operator~(const SEffectSet Operand)
    {
        SEffectSet Result;
#define VISIT_EFFECT(Name) Result._##Name = !Operand._##Name;
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    SEffectSet& operator|=(SEffectSet Rhs)
    {
#define VISIT_EFFECT(Name) _##Name = _##Name || Rhs._##Name;
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return *this;
    }
    friend constexpr SEffectSet operator|(const SEffectSet Lhs, const SEffectSet Rhs)
    {
        SEffectSet Result;
#define VISIT_EFFECT(Name) Result._##Name = Lhs._##Name || Rhs._##Name;
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    SEffectSet& operator&=(SEffectSet Rhs)
    {
#define VISIT_EFFECT(Name) _##Name = _##Name && Rhs._##Name;
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return *this;
    }
    friend constexpr SEffectSet operator&(const SEffectSet Lhs, const SEffectSet Rhs)
    {
        SEffectSet Result;
#define VISIT_EFFECT(Name) Result._##Name = Lhs._##Name && Rhs._##Name;
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    SEffectSet& operator^=(SEffectSet Rhs)
    {
#define VISIT_EFFECT(Name) _##Name = (!_##Name && Rhs._##Name) || (_##Name && !Rhs._##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return *this;
    }
    friend constexpr SEffectSet operator^(const SEffectSet Lhs, const SEffectSet Rhs)
    {
        SEffectSet Result;
#define VISIT_EFFECT(Name) Result._##Name = (!Lhs._##Name && Rhs._##Name) || (Lhs._##Name && !Rhs._##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    bool operator[](EEffect Effect) const
    {
        return Get(Effect);
    }

    constexpr bool HasAny(SEffectSet Rhs) const
    {
        bool Result = false;
#define VISIT_EFFECT(Name) Result = Result || (_##Name && Rhs._##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    constexpr bool HasAll(SEffectSet Rhs) const
    {
        bool Result = true;
#define VISIT_EFFECT(Name) Result = Result && (!Rhs._##Name || _##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    friend constexpr bool operator==(const SEffectSet Lhs, const SEffectSet Rhs)
    {
        bool Result = true;
#define VISIT_EFFECT(Name) Result = Result && (Lhs._##Name == Rhs._##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    friend constexpr bool operator!=(const SEffectSet Lhs, const SEffectSet Rhs)
    {
        bool Result = false;
#define VISIT_EFFECT(Name) Result = Result || (Lhs._##Name != Rhs._##Name);
        VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        return Result;
    }

    SEffectSet With(EEffect SingleEffect, bool bEnable = true) const
    {
        SEffectSet Result = *this;
        Result.Set(SingleEffect, bEnable);
        return Result;
    }

    // The number of bits set - used for some ordering in effect decomposition
    int32_t Num() const
    {
        return 0
#define VISIT_EFFECT(Name) + (_##Name ? 1 : 0)
            VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
            ;
    }

private:
#define VISIT_EFFECT(Name) bool _##Name : 1;
    VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT

    bool Get(EEffect Effect) const
    {
        switch (Effect)
        {
#define VISIT_EFFECT(Name) case EEffect::Name: return _##Name;
            VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        default: ULANG_UNREACHABLE();
        };
    }

    void Set(EEffect Effect, bool Enable)
    {
        switch(Effect)
        {
#define VISIT_EFFECT(Name) case EEffect::Name: _##Name = Enable; break;
            VERSE_ENUM_EFFECTS(VISIT_EFFECT)
#undef VISIT_EFFECT
        default: ULANG_UNREACHABLE();
        };
    }
};

constexpr SEffectSet operator|(const EEffect Lhs, const EEffect Rhs)
{
    return SEffectSet(Lhs) | SEffectSet(Rhs);
}

ULANG_FORCEINLINE uint32_t GetTypeHash(const SEffectSet& Effects)
{
    static_assert(EEffectNum <= sizeof(uint32_t) * 8);

    uint32_t Hash{};
    uint32_t I{};
#define VISIT(Name) if (Effects[EEffect::Name]) { Hash |= (1 << I); } ++I;
    VERSE_ENUM_EFFECTS(VISIT);
#undef VISIT
    return Hash;
}

namespace EffectSets
{
    // Singular effects
    constexpr SEffectSet Converges          = SEffectSet{};
    constexpr SEffectSet Suspends           = EEffect::suspends;
    constexpr SEffectSet Computes           = EEffect::diverges;
    constexpr SEffectSet NoRollback         = EEffect::no_rollback;
    constexpr SEffectSet Decides            = EEffect::decides;
    constexpr SEffectSet Dictates           = EEffect::dictates;

    // Aggregate effects
    constexpr SEffectSet Reads              = EEffect::reads | EEffect::dictates;
    constexpr SEffectSet Writes             = EEffect::writes | EEffect::dictates;
    constexpr SEffectSet Allocates          = EEffect::allocates | EEffect::dictates;
    constexpr SEffectSet Transacts          = EEffect::diverges | EEffect::reads | EEffect::writes | EEffect::allocates | EEffect::dictates;
    constexpr SEffectSet VariesDeprecated   = Transacts;

    // Contextual defaults
    constexpr SEffectSet ClassAndInterfaceDefault = Transacts;
    constexpr SEffectSet FunctionDefault    = Transacts | EEffect::no_rollback;
    constexpr SEffectSet ModuleDefault      = EEffect::diverges;
}

}
