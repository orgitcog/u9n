/**
 * Engine/Source nest-4 cognitive grip catalog.
 *
 * Canonical constexpr table for EchoSelf. Does not #include Unreal headers.
 * Paths are repo-relative. Lookup is longest-prefix on slash-normalized paths
 * with a trailing-slash boundary so Runtime/Core does not match CoreUObject.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

enum class EEngineNestTerm : std::uint8_t
{
    CoreRuntime = 1,
    EngineWorld,
    RenderReservoir,
    BodyRuntime,
    MemoryRuntime,
    EditorAttention,
    NetStreams,
    ActionRuntime,
    DevWisdom,
    MosaicThirdParty,
    UnclassifiedRuntime
};

struct FEngineGripEntry
{
    EEngineNestTerm term;
    const char* path;
    float salience;
    int aplId;
};

inline constexpr float kEngineUnknownSalience = 0.5f;
inline constexpr float kEngineUnclassifiedRuntimeSalience = 0.30f;
inline constexpr float kDeepTreeEchoSelfSalience = 0.95f;

inline constexpr std::array<FEngineGripEntry, 36> kEngineGripCatalog{{
    {EEngineNestTerm::CoreRuntime, "Engine/Source/Runtime/Core", 0.70f, 1},
    {EEngineNestTerm::CoreRuntime, "Engine/Source/Runtime/CoreUObject", 0.70f, 1},
    {EEngineNestTerm::CoreRuntime, "Engine/Source/Runtime/Launch", 0.70f, 1},
    {EEngineNestTerm::CoreRuntime, "Engine/Source/Runtime/Projects", 0.70f, 1},
    {EEngineNestTerm::EngineWorld, "Engine/Source/Runtime/Engine", 0.68f, 30},
    {EEngineNestTerm::EngineWorld, "Engine/Source/Runtime/EngineSettings", 0.68f, 30},
    {EEngineNestTerm::RenderReservoir, "Engine/Source/Runtime/RenderCore", 0.66f, 30},
    {EEngineNestTerm::RenderReservoir, "Engine/Source/Runtime/RHI", 0.66f, 30},
    {EEngineNestTerm::RenderReservoir, "Engine/Source/Runtime/Renderer", 0.66f, 30},
    {EEngineNestTerm::RenderReservoir, "Engine/Source/Runtime/StateStream", 0.66f, 30},
    {EEngineNestTerm::BodyRuntime, "Engine/Source/Runtime/InputCore", 0.64f, 8},
    {EEngineNestTerm::BodyRuntime, "Engine/Source/Runtime/Slate", 0.64f, 8},
    {EEngineNestTerm::BodyRuntime, "Engine/Source/Runtime/SlateCore", 0.64f, 8},
    {EEngineNestTerm::BodyRuntime, "Engine/Source/Runtime/UMG", 0.64f, 8},
    {EEngineNestTerm::BodyRuntime, "Engine/Source/Runtime/ApplicationCore", 0.64f, 8},
    {EEngineNestTerm::MemoryRuntime, "Engine/Source/Runtime/Serialization", 0.62f, 95},
    {EEngineNestTerm::MemoryRuntime, "Engine/Source/Runtime/PakFile", 0.62f, 95},
    {EEngineNestTerm::MemoryRuntime, "Engine/Source/Runtime/AssetRegistry", 0.62f, 95},
    {EEngineNestTerm::EditorAttention, "Engine/Source/Editor", 0.60f, 107},
    {EEngineNestTerm::EditorAttention, "Engine/Source/Runtime/TypedElementFramework", 0.60f, 107},
    {EEngineNestTerm::EditorAttention, "Engine/Source/Runtime/GameplayDebugger", 0.60f, 107},
    {EEngineNestTerm::NetStreams, "Engine/Source/Runtime/Net", 0.58f, 52},
    {EEngineNestTerm::NetStreams, "Engine/Source/Runtime/Networking", 0.58f, 52},
    {EEngineNestTerm::NetStreams, "Engine/Source/Runtime/Sockets", 0.58f, 52},
    {EEngineNestTerm::NetStreams, "Engine/Source/Runtime/Online", 0.58f, 52},
    {EEngineNestTerm::NetStreams, "Engine/Source/Runtime/AudioMixer", 0.58f, 52},
    {EEngineNestTerm::ActionRuntime, "Engine/Source/Runtime/AIModule", 0.57f, 30},
    {EEngineNestTerm::ActionRuntime, "Engine/Source/Runtime/GameplayTasks", 0.57f, 30},
    {EEngineNestTerm::ActionRuntime, "Engine/Source/Runtime/NavigationSystem", 0.57f, 30},
    {EEngineNestTerm::DevWisdom, "Engine/Source/Developer", 0.55f, 95},
    {EEngineNestTerm::DevWisdom, "Engine/Source/Programs", 0.55f, 95},
    {EEngineNestTerm::DevWisdom, "Engine/Source/UnrealEditor.Target.cs", 0.55f, 95},
    {EEngineNestTerm::DevWisdom, "Engine/Source/UnrealGame.Target.cs", 0.55f, 95},
    {EEngineNestTerm::DevWisdom, "Engine/Source/UnrealServer.Target.cs", 0.55f, 95},
    {EEngineNestTerm::DevWisdom, "Engine/Source/UnrealClient.Target.cs", 0.55f, 95},
    {EEngineNestTerm::MosaicThirdParty, "Engine/Source/ThirdParty", 0.15f, 8},
}};

inline constexpr const char* kEngineTargetCs[] = {
    "Engine/Source/UnrealEditor.Target.cs",
    "Engine/Source/UnrealGame.Target.cs",
    "Engine/Source/UnrealServer.Target.cs",
    "Engine/Source/UnrealClient.Target.cs",
};

inline constexpr const char* kEngineIndependentRegions[] = {
    "Engine/Source/Runtime",
    "Engine/Source/Editor",
    "Engine/Source/Developer",
    "Engine/Source/Programs",
    "Engine/Source/ThirdParty",
};

inline std::string NormalizeEnginePath(std::string_view path)
{
    std::string out;
    out.reserve(path.size());
    for (char c : path)
    {
        out.push_back(c == '\\' ? '/' : c);
    }
    return out;
}

inline bool PrefixMatchesBoundary(std::string_view path, std::string_view prefix)
{
    if (path.size() < prefix.size())
    {
        return false;
    }
    if (path.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }
    if (path.size() == prefix.size())
    {
        return true;
    }
    return path[prefix.size()] == '/';
}

inline const FEngineGripEntry* FindEngineGripEntry(std::string_view path)
{
    const std::string normalized = NormalizeEnginePath(path);
    const FEngineGripEntry* best = nullptr;
    std::size_t bestLen = 0;
    for (const FEngineGripEntry& entry : kEngineGripCatalog)
    {
        const std::string_view prefix(entry.path);
        if (prefix.size() < bestLen)
        {
            continue;
        }
        if (PrefixMatchesBoundary(normalized, prefix) && prefix.size() >= bestLen)
        {
            best = &entry;
            bestLen = prefix.size();
        }
    }
    return best;
}

inline EEngineNestTerm TermFor(std::string_view path)
{
    if (const FEngineGripEntry* entry = FindEngineGripEntry(path))
    {
        return entry->term;
    }
    const std::string normalized = NormalizeEnginePath(path);
    if (PrefixMatchesBoundary(normalized, "Engine/Source/Runtime"))
    {
        return EEngineNestTerm::UnclassifiedRuntime;
    }
    return EEngineNestTerm::UnclassifiedRuntime;
}

inline float SalienceFor(std::string_view path)
{
    if (const FEngineGripEntry* entry = FindEngineGripEntry(path))
    {
        return entry->salience;
    }
    const std::string normalized = NormalizeEnginePath(path);
    if (PrefixMatchesBoundary(normalized, "Engine/Source/Runtime"))
    {
        return kEngineUnclassifiedRuntimeSalience;
    }
    if (PrefixMatchesBoundary(normalized, "Engine/Source"))
    {
        return kEngineUnknownSalience;
    }
    return kEngineUnknownSalience;
}
