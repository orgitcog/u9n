/**
 * Engine/Source APL253 cognitive grip tests.
 *
 * Locks don't-move hubs, catalog lookup, EchoSelf packing clauses, and
 * no-Unreal-include policy. Existence asserts skip when Runtime/Core is absent.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Enaction/EngineGripMap.h"
#include "Enaction/EngineSourceGripAdapter.h"
#include "Engine/Source/EngineSourceFacade.h"

#ifndef UN9N_REPO_ROOT
#define UN9N_REPO_ROOT "."
#endif

namespace
{

std::string RepoRoot()
{
    return std::string(UN9N_REPO_ROOT);
}

std::filesystem::path RepoFile(std::string_view relative)
{
    return std::filesystem::path(RepoRoot()) / std::string(NormalizeEnginePath(relative));
}

bool EngineTreePresent()
{
    return dte::engine_grip::EngineTreePresent(RepoRoot());
}

std::string ReadText(std::string_view relative)
{
    std::ifstream in(RepoFile(relative), std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

bool ContainsForbiddenUeInclude(const std::string& text)
{
    return text.find("CoreMinimal.h") != std::string::npos
        || text.find("UObject/") != std::string::npos
        || text.find("GameFramework/") != std::string::npos;
}

float EchoSelfHeuristicSalience(std::string_view path)
{
    const std::string p = NormalizeEnginePath(path);
    const auto has = [&](std::string_view frag) {
        return p.find(frag) != std::string::npos;
    };
    if (has("DeepTreeEcho/Self/")) return 0.95f;
    if (has("DeepTreeEcho/Core/")) return 0.90f;
    if (has("DeepTreeEcho/Self/COGNITIVE_GRIP.md")) return 0.80f;
    if (has("Engine/Source/COGNITIVE_GRIP.md")) return 0.70f;
    if (has("Engine/Source/EngineSourceFacade.h")) return 0.70f;
    if (has("Engine/Source/ThirdParty")) return 0.15f;
    if (has("Engine/Plugins")) return 0.15f;
    if (has("Engine/Source/Runtime")) return 0.30f;
    if (has("Engine/Source/")) return 0.30f;
    return 0.50f;
}

} // namespace

TEST(EngineGripCatalog, UniquePathsAndNineTerms)
{
    std::set<std::string> paths;
    std::set<EEngineNestTerm> terms;
    for (const FEngineGripEntry& entry : kEngineGripCatalog)
    {
        EXPECT_TRUE(paths.insert(entry.path).second) << entry.path;
        terms.insert(entry.term);
        EXPECT_LE(entry.salience, 0.70f);
        EXPECT_GE(entry.salience, 0.15f);
    }
    EXPECT_TRUE(terms.count(EEngineNestTerm::CoreRuntime));
    EXPECT_TRUE(terms.count(EEngineNestTerm::EngineWorld));
    EXPECT_TRUE(terms.count(EEngineNestTerm::RenderReservoir));
    EXPECT_TRUE(terms.count(EEngineNestTerm::BodyRuntime));
    EXPECT_TRUE(terms.count(EEngineNestTerm::MemoryRuntime));
    EXPECT_TRUE(terms.count(EEngineNestTerm::EditorAttention));
    EXPECT_TRUE(terms.count(EEngineNestTerm::NetStreams));
    EXPECT_TRUE(terms.count(EEngineNestTerm::ActionRuntime));
    EXPECT_TRUE(terms.count(EEngineNestTerm::DevWisdom));
    EXPECT_TRUE(terms.count(EEngineNestTerm::MosaicThirdParty));
    EXPECT_EQ(terms.size(), 10u);
}

TEST(EngineGripCatalog, LongestPrefixBoundary)
{
    EXPECT_EQ(TermFor("Engine/Source/Runtime/Core"), EEngineNestTerm::CoreRuntime);
    EXPECT_EQ(TermFor("Engine/Source/Runtime/CoreUObject"), EEngineNestTerm::CoreRuntime);
    EXPECT_EQ(TermFor("Engine/Source/Runtime/Core/Public/Misc/CoreMisc.h"),
              EEngineNestTerm::CoreRuntime);
    EXPECT_NE(FindEngineGripEntry("Engine/Source/Runtime/CoreUObject")->path,
              std::string("Engine/Source/Runtime/Core"));
    EXPECT_FLOAT_EQ(SalienceFor("Engine/Source/Runtime/Core"), 0.70f);
    EXPECT_FLOAT_EQ(SalienceFor("Engine/Source/Runtime/UnknownModule"), 0.30f);
    EXPECT_FLOAT_EQ(SalienceFor("Engine/Source/ThirdParty"), 0.15f);
    EXPECT_FLOAT_EQ(SalienceFor("DeepTreeEcho/Self/EchoSelf/EchoSelfIntegration.h"),
                    kEngineUnknownSalience);
}

TEST(EngineGripAdapter, EngineUnknownIsNotDeepTreeEchoSelf)
{
    const float engineCore = dte::engine_grip::SalienceFor("Engine/Source/Runtime/Core");
    EXPECT_LT(engineCore, kDeepTreeEchoSelfSalience);
    EXPECT_GE(engineCore, 0.55f);
    EXPECT_LE(engineCore, 0.70f);
    const float nonEngine =
        dte::engine_grip::SalienceFor("DeepTreeEcho/Self/EchoSelf/EchoSelfIntegration.h");
    EXPECT_FLOAT_EQ(nonEngine, kEngineUnknownSalience);
    EXPECT_NE(nonEngine, kDeepTreeEchoSelfSalience);
}

TEST(EngineGripAdapter, AbsentRootIsEmpty)
{
    EXPECT_FALSE(dte::engine_grip::EngineTreePresent(""));
    EXPECT_FALSE(dte::engine_grip::EngineTreePresent("C:/definitely-missing-un9n-engine-root"));
    EXPECT_EQ(dte::engine_grip::UnclassifiedRuntimeCount(""), 0);
    const auto snap = dte::engine_grip::InspectDirectoryTerms("");
    EXPECT_FALSE(snap.treePresent);
}

TEST(EngineGripAdapter, InjectableMissingCoreSkipsTree)
{
    const auto snap = dte::engine_grip::InspectDirectoryTerms("C:/un9n-missing-core-hub");
    EXPECT_FALSE(snap.treePresent);
    EXPECT_EQ(snap.classifiedHubs, 0);
}

TEST(EngineGripEchoSelf, CognitiveGripFilesDiffer)
{
    const std::string echo = ReadText("DeepTreeEcho/Self/echoself.md");
    EXPECT_NE(echo.find("DeepTreeEcho/Self/COGNITIVE_GRIP.md"), std::string::npos);
    EXPECT_NE(echo.find("Engine/Source/COGNITIVE_GRIP.md"), std::string::npos);
    EXPECT_EQ(echo.find("((string-contains? path \"COGNITIVE_GRIP.md\") 0.8)"),
              std::string::npos);
    const float dte = EchoSelfHeuristicSalience("DeepTreeEcho/Self/COGNITIVE_GRIP.md");
    const float engine = EchoSelfHeuristicSalience("Engine/Source/COGNITIVE_GRIP.md");
    EXPECT_GT(dte, engine);
    EXPECT_FLOAT_EQ(engine, 0.70f);
}

TEST(EngineGripEchoSelf, PackingDoesNotRaiseHubLeaves)
{
    EXPECT_FLOAT_EQ(
        EchoSelfHeuristicSalience("Engine/Source/Runtime/Core/Public/Misc/CoreMisc.h"),
        0.30f);
    EXPECT_LE(EchoSelfHeuristicSalience("Engine/Source/ThirdParty/zlib/zlib.h"), 0.15f);
}

TEST(EngineGripIncludePolicy, NoUnrealPublicHeaders)
{
    for (const char* rel : {
             "DeepTreeEcho/Enaction/EngineGripMap.h",
             "DeepTreeEcho/Enaction/EngineSourceGripAdapter.h",
             "Engine/Source/EngineSourceFacade.h"})
    {
        const std::string text = ReadText(rel);
        ASSERT_FALSE(text.empty()) << rel;
        EXPECT_FALSE(ContainsForbiddenUeInclude(text)) << rel;
        EXPECT_EQ(text.find("#include \"CoreMinimal.h\""), std::string::npos) << rel;
    }
}

TEST(EngineGripMarkdown, ContainsEachHubPath)
{
    const std::string md = ReadText("Engine/Source/COGNITIVE_GRIP.md");
    ASSERT_FALSE(md.empty());
    for (const FEngineGripEntry& entry : kEngineGripCatalog)
    {
        EXPECT_NE(md.find(entry.path), std::string::npos) << entry.path;
    }
}

TEST(EngineGripExistence, IndependentRegionsAndHubs)
{
    if (!EngineTreePresent())
    {
        GTEST_SKIP() << "Engine/Source/Runtime/Core missing; catalog tests still ran";
    }

    for (const char* region : kEngineIndependentRegions)
    {
        EXPECT_TRUE(std::filesystem::is_directory(RepoFile(region))) << region;
    }
    for (const FEngineGripEntry& entry : kEngineGripCatalog)
    {
        const auto path = RepoFile(entry.path);
        EXPECT_TRUE(std::filesystem::exists(path)) << entry.path;
    }
    EXPECT_EQ(TermFor("Engine/Source/Runtime/Core"), EEngineNestTerm::CoreRuntime);

    const auto children = dte::engine_grip::ImmediateEngineSourceChildren(RepoRoot());
    const std::set<std::string> allowedDirs{
        "Runtime", "Editor", "Developer", "Programs", "ThirdParty"};
    for (const std::string& name : children)
    {
        const auto child = RepoFile(std::string("Engine/Source/") + name);
        if (std::filesystem::is_directory(child))
        {
            EXPECT_TRUE(allowedDirs.count(name)) << "unclassified Engine/Source dir: " << name;
        }
    }
}
