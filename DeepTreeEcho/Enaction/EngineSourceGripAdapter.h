/**
 * DTE-side adapter over EngineGripMap.h.
 *
 * Header-only. No Unreal public includes. Filesystem checks use an injectable
 * repo root; missing Engine/Source/Runtime/Core yields empty coverage.
 */
#pragma once

#include "EngineGripMap.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dte
{
namespace engine_grip
{

inline constexpr const char* kCoreHubRelative = "Engine/Source/Runtime/Core";

struct FDirectoryTermSnapshot
{
    int classifiedHubs = 0;
    int unclassifiedRuntime = 0;
    bool treePresent = false;
};

inline std::filesystem::path RepoPath(std::string_view repoRoot, std::string_view relative)
{
    std::filesystem::path root{std::string(repoRoot)};
    return root / std::string(NormalizeEnginePath(relative));
}

inline bool EngineTreePresent(std::string_view repoRoot)
{
    if (repoRoot.empty())
    {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_directory(RepoPath(repoRoot, kCoreHubRelative), ec);
}

inline const auto& Entries()
{
    return kEngineGripCatalog;
}

inline float SalienceFor(std::string_view path)
{
    return ::SalienceFor(path);
}

inline EEngineNestTerm TermFor(std::string_view path)
{
    return ::TermFor(path);
}

inline int UnclassifiedRuntimeCount(std::string_view repoRoot)
{
    if (!EngineTreePresent(repoRoot))
    {
        return 0;
    }
    const std::filesystem::path runtime = RepoPath(repoRoot, "Engine/Source/Runtime");
    std::error_code ec;
    if (!std::filesystem::is_directory(runtime, ec))
    {
        return 0;
    }
    int unclassified = 0;
    for (const auto& child : std::filesystem::directory_iterator(runtime, ec))
    {
        if (ec)
        {
            break;
        }
        std::error_code dirEc;
        if (!child.is_directory(dirEc) || dirEc)
        {
            continue;
        }
        const std::string relative =
            std::string("Engine/Source/Runtime/") + child.path().filename().string();
        if (FindEngineGripEntry(relative) == nullptr)
        {
            ++unclassified;
        }
    }
    return unclassified;
}

inline std::vector<std::string> ImmediateEngineSourceChildren(std::string_view repoRoot)
{
    std::vector<std::string> names;
    if (repoRoot.empty())
    {
        return names;
    }
    const std::filesystem::path source = RepoPath(repoRoot, "Engine/Source");
    std::error_code ec;
    if (!std::filesystem::is_directory(source, ec))
    {
        return names;
    }
    for (const auto& child : std::filesystem::directory_iterator(source, ec))
    {
        if (ec)
        {
            break;
        }
        names.push_back(child.path().filename().string());
    }
    return names;
}

inline FDirectoryTermSnapshot InspectDirectoryTerms(std::string_view repoRoot)
{
    FDirectoryTermSnapshot snap;
    snap.treePresent = EngineTreePresent(repoRoot);
    if (!snap.treePresent)
    {
        return snap;
    }
    snap.classifiedHubs = static_cast<int>(kEngineGripCatalog.size());
    snap.unclassifiedRuntime = UnclassifiedRuntimeCount(repoRoot);
    return snap;
}

} // namespace engine_grip
} // namespace dte
