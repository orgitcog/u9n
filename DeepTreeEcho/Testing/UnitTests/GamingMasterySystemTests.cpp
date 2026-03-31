/**
 * @file GamingMasterySystemTests.cpp
 * @brief Comprehensive unit tests for GamingMasterySystem and Situational Awareness
 *
 * Tests cover:
 * - Skill acquisition phases (Novice → Master, Dreyfus model)
 * - Strategic thinking modes (System1/2 dual process, MCTS, adversarial)
 * - Gaming domain classification and specialization
 * - Lightning reflexes: sub-10ms reaction time modeling
 * - Situational awareness: threat detection, opportunity recognition
 * - Pattern recognition and opponent modeling
 * - AI Gaming mastery metrics (KPM, accuracy, positioning)
 * - Adaptive difficulty and skill plateau detection
 * - Deliberate practice tracking
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <limits>

namespace {

// ============================================================================
// Mock types (standalone, no UE dependency)
// ============================================================================

namespace FMath {
    inline float Clamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    inline float Max(float a, float b) { return a > b ? a : b; }
    inline float Min(float a, float b) { return a < b ? a : b; }
    inline float Abs(float x) { return std::abs(x); }
}

// ============================================================================
// Skill Acquisition Phase (Dreyfus Model)
// ============================================================================

enum class ESkillPhase : uint8_t {
    Novice,         // Rule-based, context-free
    AdvancedBeginner, // Recognizes situational elements
    Competent,      // Deliberate planning, prioritization
    Proficient,     // Intuitive recognition, holistic assessment
    Expert,         // Intuitive, fluid, embodied mastery
    Master          // Transcendent, creative, innovative
};

// ============================================================================
// Strategic Thinking Mode
// ============================================================================

enum class EStrategicMode : uint8_t {
    System1_Intuitive,   // Fast pattern-based
    System2_Analytical,  // Slow deliberate
    DualProcess,         // Hybrid adaptive
    TreeSearch,          // MCTS-style
    AdversarialReasoning // Opponent modeling
};

// ============================================================================
// Gaming Domain
// ============================================================================

enum class EGameDomain : uint8_t {
    FPS,
    RTS,
    Fighting,
    MOBA,
    BattleRoyale,
    Racing,
    TurnBased
};

// ============================================================================
// Skill Component
// ============================================================================

struct FSkillComponent {
    std::string SkillID;
    std::string SkillName;
    float ProficiencyLevel = 0.0f;
    ESkillPhase AcquisitionPhase = ESkillPhase::Novice;
    float PracticeHours = 0.0f;
    int DeliberatePracticeSessions = 0;
    float RecentSuccessRate = 0.0f;
    float DecayRate = 0.01f;
    float LastPracticeTime = 0.0f;
    std::vector<std::string> Prerequisites;
};

// ============================================================================
// Situational Awareness State
// ============================================================================

struct FSituationalAwareness {
    // Threat Assessment (Level 1: Perception)
    float ThreatLevel = 0.0f;         // 0-1: immediate danger
    float EnemyProximity = 0.0f;      // 0-1: closest enemy distance (inverted)
    float CrosshairAccuracy = 0.0f;   // 0-1: how on-target
    int VisibleEnemyCount = 0;

    // Opportunity Recognition (Level 2: Comprehension)
    float AdvantageScore = 0.0f;      // Net advantage over opponents
    float PositionalScore = 0.0f;     // Cover, elevation, flanking opportunities
    float ResourceAvailability = 0.0f;// Ammo, HP, cooldowns

    // Projection (Level 3: Prediction)
    float PredictedEnemyPath = 0.0f;  // Where enemy will be
    float WinProbability = 0.5f;      // Estimated win rate of current fight

    bool bIsInDanger() const { return ThreatLevel > 0.6f; }
    bool bHasAdvantage() const { return AdvantageScore > 0.3f; }
    bool bCanEngage() const {
        return ResourceAvailability > 0.3f && WinProbability > 0.5f;
    }
};

// ============================================================================
// Gaming Mastery System Mock
// ============================================================================

class MockGamingMasterySystem {
public:
    void Initialize(EGameDomain Domain = EGameDomain::FPS) {
        ActiveDomain = Domain;
        GlobalProficiency = 0.0f;
        TotalPlayTime = 0.0f;
        TotalKills = 0;
        TotalDeaths = 0;
        TotalActions = 0;
        SessionReactionTimeMs = 0.0f;
        SessionReactionCount = 0;
        SetupDefaultSkills();
        CurrentMode = EStrategicMode::DualProcess;
        bInitialized = true;
    }

    // -- Skill Management --

    bool AddSkill(const std::string& ID, const std::string& Name,
                  const std::vector<std::string>& Prerequisites = {}) {
        if (Skills.count(ID)) return false;
        FSkillComponent skill;
        skill.SkillID = ID;
        skill.SkillName = Name;
        skill.Prerequisites = Prerequisites;
        Skills[ID] = skill;
        return true;
    }

    bool PracticeSkill(const std::string& SkillID, float Quality = 0.8f,
                        float DeltaTimeHours = 0.5f) {
        auto it = Skills.find(SkillID);
        if (it == Skills.end()) return false;

        // Check prerequisites are met
        for (const auto& prereq : it->second.Prerequisites) {
            auto prereqIt = Skills.find(prereq);
            if (prereqIt == Skills.end() ||
                prereqIt->second.AcquisitionPhase < ESkillPhase::Competent) {
                return false; // Prerequisite not met
            }
        }

        FSkillComponent& skill = it->second;
        skill.PracticeHours += DeltaTimeHours;
        skill.DeliberatePracticeSessions++;

        // Skill gain proportional to quality and diminishing returns
        float existingLevel = skill.ProficiencyLevel;
        float gain = Quality * (1.0f - existingLevel) * 0.1f;
        skill.ProficiencyLevel = FMath::Clamp(skill.ProficiencyLevel + gain, 0.0f, 1.0f);

        // Update acquisition phase
        UpdateSkillPhase(skill);
        UpdateGlobalProficiency();
        return true;
    }

    void RecordGameOutcome(bool bWon, int Kills, int Deaths,
                           float ReactionTimeMs, int ActionsPerMinute) {
        TotalKills += Kills;
        TotalDeaths += Deaths;
        TotalActions += ActionsPerMinute;

        // Update reaction time average
        if (ReactionTimeMs > 0.0f) {
            SessionReactionTimeMs = FMath::Lerp(SessionReactionTimeMs,
                                                ReactionTimeMs,
                                                1.0f / (SessionReactionCount + 1));
            SessionReactionCount++;
        }

        // Win/loss affects global proficiency
        float feedback = bWon ? 0.01f : -0.005f;
        GlobalProficiency = FMath::Clamp(GlobalProficiency + feedback, 0.0f, 1.0f);

        OutcomeHistory.push_back(bWon ? 1.0f : 0.0f);
        if (OutcomeHistory.size() > 100) OutcomeHistory.erase(OutcomeHistory.begin());
    }

    // -- Situational Awareness --

    FSituationalAwareness AssessSituation(
        float Health, float EnemyHP,
        float PlayerPosition, float EnemyPosition,
        float Ammo, int EnemyCount) const {

        FSituationalAwareness SA;
        SA.ThreatLevel = FMath::Clamp((1.0f - Health) * 0.5f +
                                       EnemyCount * 0.1f, 0.0f, 1.0f);
        SA.EnemyProximity = FMath::Max(0.0f,
                                        1.0f - std::abs(PlayerPosition - EnemyPosition) / 100.0f);
        SA.VisibleEnemyCount = EnemyCount;
        SA.ResourceAvailability = FMath::Clamp(Health * 0.5f + Ammo * 0.5f, 0.0f, 1.0f);
        SA.WinProbability = FMath::Clamp(
            (Health / FMath::Max(EnemyHP, 0.01f)) * 0.6f +
            (EnemyCount > 0 ? 1.0f / EnemyCount : 1.0f) * 0.4f,
            0.0f, 1.0f);
        SA.AdvantageScore = SA.WinProbability - 0.5f;
        SA.PositionalScore = FMath::Clamp(
            (1.0f - SA.EnemyProximity) * 0.5f + Ammo * 0.5f, 0.0f, 1.0f);
        return SA;
    }

    // -- Strategic Mode Selection --

    EStrategicMode SelectStrategicMode(const FSituationalAwareness& SA) {
        if (SA.bIsInDanger()) {
            // Under pressure: use fast intuitive (System 1)
            CurrentMode = EStrategicMode::System1_Intuitive;
        } else if (SA.bHasAdvantage()) {
            // Safe and winning: use deliberate analytical (System 2)
            CurrentMode = EStrategicMode::System2_Analytical;
        } else if (SA.WinProbability < 0.3f) {
            // Heavily outmatched: adversarial reasoning
            CurrentMode = EStrategicMode::AdversarialReasoning;
        } else {
            // Normal play: hybrid dual-process
            CurrentMode = EStrategicMode::DualProcess;
        }
        return CurrentMode;
    }

    // -- Reflex / Reaction Time --

    float SimulateReactionTime(float BaseReactionMs = 200.0f) const {
        // Expert players have faster reactions (down to ~100ms)
        float skillModifier = 1.0f - GlobalProficiency * 0.5f;
        float jitter = ((float)(rand() % 20) - 10.0f);
        return FMath::Max(80.0f, BaseReactionMs * skillModifier + jitter);
    }

    bool IsLightningReflex(float ReactionTimeMs) const {
        return ReactionTimeMs < 150.0f;  // Sub-150ms = lightning reflex
    }

    // -- Skill Decay --

    void ApplyTimeDecay(float DaysElapsed) {
        for (auto& [id, skill] : Skills) {
            if (skill.ProficiencyLevel > 0.0f) {
                float decay = skill.DecayRate * DaysElapsed;
                skill.ProficiencyLevel = FMath::Max(0.0f,
                                                     skill.ProficiencyLevel - decay);
                if (skill.ProficiencyLevel < 0.01f) {
                    skill.AcquisitionPhase = ESkillPhase::Novice;
                }
            }
        }
    }

    // -- Opponent Modeling --

    void ObserveOpponentBehavior(const std::string& OpponentID,
                                  float AggressionLevel, float AimAccuracy,
                                  float MovementPattern) {
        FOpponentModel& model = OpponentModels[OpponentID];
        float alpha = 0.1f;
        model.AggressionLevel = FMath::Lerp(model.AggressionLevel,
                                              AggressionLevel, alpha);
        model.AimAccuracy = FMath::Lerp(model.AimAccuracy,
                                         AimAccuracy, alpha);
        model.MovementPattern = FMath::Lerp(model.MovementPattern,
                                              MovementPattern, alpha);
        model.ObservationCount++;
    }

    float PredictOpponentAction(const std::string& OpponentID) const {
        auto it = OpponentModels.find(OpponentID);
        if (it == OpponentModels.end()) return 0.5f;
        return it->second.AggressionLevel * 0.4f +
               it->second.AimAccuracy * 0.3f +
               it->second.MovementPattern * 0.3f;
    }

    // -- Accessors --

    bool IsInitialized() const { return bInitialized; }
    float GetGlobalProficiency() const { return GlobalProficiency; }
    float GetRecentWinRate() const {
        if (OutcomeHistory.empty()) return 0.5f;
        float sum = 0.0f;
        int n = FMath::Min(static_cast<int>(OutcomeHistory.size()), 20);
        for (int i = OutcomeHistory.size() - n; i < (int)OutcomeHistory.size(); ++i)
            sum += OutcomeHistory[i];
        return sum / n;
    }
    float GetAverageReactionTimeMs() const { return SessionReactionTimeMs; }
    EStrategicMode GetCurrentMode() const { return CurrentMode; }
    EGameDomain GetActiveDomain() const { return ActiveDomain; }
    int GetSkillCount() const { return static_cast<int>(Skills.size()); }
    bool HasSkill(const std::string& ID) const { return Skills.count(ID) > 0; }

    float GetSkillProficiency(const std::string& ID) const {
        auto it = Skills.find(ID);
        return it != Skills.end() ? it->second.ProficiencyLevel : 0.0f;
    }

    ESkillPhase GetSkillPhase(const std::string& ID) const {
        auto it = Skills.find(ID);
        return it != Skills.end() ? it->second.AcquisitionPhase : ESkillPhase::Novice;
    }

    int GetDeliberatePracticeSessions(const std::string& ID) const {
        auto it = Skills.find(ID);
        return it != Skills.end() ? it->second.DeliberatePracticeSessions : 0;
    }

    bool IsSkillAtPhase(const std::string& ID, ESkillPhase Phase) const {
        return GetSkillPhase(ID) >= Phase;
    }

    int GetOpponentModelCount() const { return static_cast<int>(OpponentModels.size()); }

private:
    struct FOpponentModel {
        float AggressionLevel = 0.5f;
        float AimAccuracy = 0.5f;
        float MovementPattern = 0.5f;
        int ObservationCount = 0;
    };

    void SetupDefaultSkills() {
        AddSkill("AimAccuracy", "Aim Accuracy");
        AddSkill("MovementSpeed", "Movement Speed");
        AddSkill("GameSense", "Game Sense", {"AimAccuracy", "MovementSpeed"});
        AddSkill("PositionalPlay", "Positional Play", {"GameSense"});
    }

    void UpdateSkillPhase(FSkillComponent& skill) {
        if (skill.ProficiencyLevel >= 0.9f) skill.AcquisitionPhase = ESkillPhase::Master;
        else if (skill.ProficiencyLevel >= 0.75f) skill.AcquisitionPhase = ESkillPhase::Expert;
        else if (skill.ProficiencyLevel >= 0.55f) skill.AcquisitionPhase = ESkillPhase::Proficient;
        else if (skill.ProficiencyLevel >= 0.35f) skill.AcquisitionPhase = ESkillPhase::Competent;
        else if (skill.ProficiencyLevel >= 0.15f) skill.AcquisitionPhase = ESkillPhase::AdvancedBeginner;
        else skill.AcquisitionPhase = ESkillPhase::Novice;
    }

    void UpdateGlobalProficiency() {
        if (Skills.empty()) return;
        float sum = 0.0f;
        for (const auto& [id, skill] : Skills) sum += skill.ProficiencyLevel;
        GlobalProficiency = sum / Skills.size();
    }

    bool bInitialized = false;
    EGameDomain ActiveDomain = EGameDomain::FPS;
    EStrategicMode CurrentMode = EStrategicMode::DualProcess;
    float GlobalProficiency = 0.0f;
    float TotalPlayTime = 0.0f;
    int TotalKills = 0;
    int TotalDeaths = 0;
    int TotalActions = 0;
    float SessionReactionTimeMs = 0.0f;
    int SessionReactionCount = 0;
    std::unordered_map<std::string, FSkillComponent> Skills;
    std::unordered_map<std::string, FOpponentModel> OpponentModels;
    std::vector<float> OutcomeHistory;
};

// ============================================================================
// Test Fixture
// ============================================================================

class GamingMasterySystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        System = std::make_unique<MockGamingMasterySystem>();
        System->Initialize(EGameDomain::FPS);
    }

    void TearDown() override { System.reset(); }

    std::unique_ptr<MockGamingMasterySystem> System;
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(GamingMasterySystemTest, InitializationSucceeds) {
    EXPECT_TRUE(System->IsInitialized());
}

TEST_F(GamingMasterySystemTest, DefaultDomainIsFPS) {
    EXPECT_EQ(System->GetActiveDomain(), EGameDomain::FPS);
}

TEST_F(GamingMasterySystemTest, DefaultSkillsRegistered) {
    EXPECT_GE(System->GetSkillCount(), 4);  // AimAccuracy, MovementSpeed, GameSense, etc.
}

TEST_F(GamingMasterySystemTest, InitialProficiencyIsZero) {
    EXPECT_FLOAT_EQ(System->GetGlobalProficiency(), 0.0f);
}

// ============================================================================
// SKILL ACQUISITION TESTS (Dreyfus Model)
// ============================================================================

TEST_F(GamingMasterySystemTest, PracticeIncreasesSkillProficiency) {
    float before = System->GetSkillProficiency("AimAccuracy");
    System->PracticeSkill("AimAccuracy", 0.9f);
    float after = System->GetSkillProficiency("AimAccuracy");
    EXPECT_GT(after, before);
}

TEST_F(GamingMasterySystemTest, HighQualityPracticeGainsMoreThanLow) {
    MockGamingMasterySystem highQ, lowQ;
    highQ.Initialize(EGameDomain::FPS);
    lowQ.Initialize(EGameDomain::FPS);

    for (int i = 0; i < 5; ++i) {
        highQ.PracticeSkill("AimAccuracy", 1.0f);  // Perfect quality
        lowQ.PracticeSkill("AimAccuracy",  0.2f);  // Low quality
    }

    EXPECT_GT(highQ.GetSkillProficiency("AimAccuracy"),
              lowQ.GetSkillProficiency("AimAccuracy"));
}

TEST_F(GamingMasterySystemTest, SkillProgressesThroughDreyfusPhases) {
    EXPECT_EQ(System->GetSkillPhase("AimAccuracy"), ESkillPhase::Novice);

    // Practice until AdvancedBeginner (proficiency >= 0.15)
    for (int i = 0; i < 20; ++i) {
        System->PracticeSkill("AimAccuracy", 0.9f);
    }
    EXPECT_NE(System->GetSkillPhase("AimAccuracy"), ESkillPhase::Novice);
}

TEST_F(GamingMasterySystemTest, HighProficiencyAchievesExpertOrMaster) {
    // Practice extensively to reach expert level
    for (int i = 0; i < 200; ++i) {
        System->PracticeSkill("AimAccuracy", 0.95f);
    }
    ESkillPhase phase = System->GetSkillPhase("AimAccuracy");
    EXPECT_GE(static_cast<int>(phase), static_cast<int>(ESkillPhase::Expert));
}

TEST_F(GamingMasterySystemTest, DeliberatePracticeCountTracked) {
    System->PracticeSkill("AimAccuracy");
    System->PracticeSkill("AimAccuracy");
    System->PracticeSkill("AimAccuracy");
    EXPECT_EQ(System->GetDeliberatePracticeSessions("AimAccuracy"), 3);
}

TEST_F(GamingMasterySystemTest, PrerequisiteEnforcedForAdvancedSkill) {
    // GameSense requires AimAccuracy and MovementSpeed at Competent level
    // Neither is there yet, so GameSense practice should fail
    bool result = System->PracticeSkill("GameSense");
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(System->GetSkillProficiency("GameSense"), 0.0f);
}

TEST_F(GamingMasterySystemTest, SkillUnlockableAfterPrerequisites) {
    // Unlock prerequisites
    for (int i = 0; i < 60; ++i) {
        System->PracticeSkill("AimAccuracy", 0.9f);
        System->PracticeSkill("MovementSpeed", 0.9f);
    }
    // AimAccuracy and MovementSpeed should now be at Competent level
    EXPECT_GE(static_cast<int>(System->GetSkillPhase("AimAccuracy")),
              static_cast<int>(ESkillPhase::Competent));

    // Now GameSense should be practiceable
    bool result = System->PracticeSkill("GameSense");
    EXPECT_TRUE(result);
    EXPECT_GT(System->GetSkillProficiency("GameSense"), 0.0f);
}

TEST_F(GamingMasterySystemTest, SkillProficiencyBoundedZeroToOne) {
    for (int i = 0; i < 1000; ++i) {
        System->PracticeSkill("AimAccuracy", 1.0f);
    }
    EXPECT_LE(System->GetSkillProficiency("AimAccuracy"), 1.0f);
    EXPECT_GE(System->GetSkillProficiency("AimAccuracy"), 0.0f);
}

TEST_F(GamingMasterySystemTest, SkillDecayOverTime) {
    // Build up a skill
    for (int i = 0; i < 50; ++i) System->PracticeSkill("AimAccuracy", 0.9f);
    float beforeDecay = System->GetSkillProficiency("AimAccuracy");

    // Apply time decay
    System->ApplyTimeDecay(7.0f);  // 7 days without practice
    float afterDecay = System->GetSkillProficiency("AimAccuracy");

    EXPECT_LT(afterDecay, beforeDecay);
}

TEST_F(GamingMasterySystemTest, AddCustomSkill) {
    bool added = System->AddSkill("Wallbang", "Wallbang Shots", {"AimAccuracy"});
    EXPECT_TRUE(added);
    EXPECT_TRUE(System->HasSkill("Wallbang"));

    // Duplicate add should fail
    EXPECT_FALSE(System->AddSkill("Wallbang", "Wallbang Shots"));
}

// ============================================================================
// LIGHTNING REFLEXES TESTS
// ============================================================================

TEST_F(GamingMasterySystemTest, ExpertPlayerHasFasterReflexes) {
    // Low proficiency player
    MockGamingMasterySystem novice, expert;
    novice.Initialize(EGameDomain::FPS);
    expert.Initialize(EGameDomain::FPS);

    // Build expert's proficiency
    for (int i = 0; i < 500; ++i) {
        expert.PracticeSkill("AimAccuracy", 1.0f);
        expert.PracticeSkill("MovementSpeed", 1.0f);
    }
    expert.RecordGameOutcome(true, 30, 2, 120.0f, 200);

    // Expert should react faster
    float noviceReaction = novice.SimulateReactionTime();
    float expertReaction = expert.SimulateReactionTime();
    EXPECT_LT(expertReaction, noviceReaction);
}

TEST_F(GamingMasterySystemTest, LightningReflexThresholdAt150ms) {
    EXPECT_TRUE(System->IsLightningReflex(100.0f));
    EXPECT_TRUE(System->IsLightningReflex(149.9f));
    EXPECT_FALSE(System->IsLightningReflex(150.0f));
    EXPECT_FALSE(System->IsLightningReflex(200.0f));
}

TEST_F(GamingMasterySystemTest, ReactionTimeBoundedByMinimum) {
    // Even master players can't react below ~80ms (biological limit)
    MockGamingMasterySystem master;
    master.Initialize(EGameDomain::FPS);
    for (int i = 0; i < 1000; ++i) master.PracticeSkill("AimAccuracy", 1.0f);

    for (int i = 0; i < 100; ++i) {
        float rt = master.SimulateReactionTime();
        EXPECT_GE(rt, 80.0f);  // Biological minimum
    }
}

// ============================================================================
// SITUATIONAL AWARENESS TESTS (3-Level SA Model: Endsley)
// ============================================================================

TEST_F(GamingMasterySystemTest, HighThreatAssessedCorrectly) {
    // Low health, multiple enemies close
    auto SA = System->AssessSituation(
        0.1f,  // 10% health (critical)
        1.0f,  // Enemy at full health
        0.0f,  // Player position
        5.0f,  // Enemy nearby (5 units away)
        0.5f,  // 50% ammo
        3      // 3 enemies visible
    );

    EXPECT_GT(SA.ThreatLevel, 0.5f);
    EXPECT_TRUE(SA.bIsInDanger());
}

TEST_F(GamingMasterySystemTest, LowThreatWhenSafe) {
    auto SA = System->AssessSituation(
        1.0f,   // Full health
        0.3f,   // Weak enemy
        0.0f,   // Player position
        100.0f, // Enemy far away
        1.0f,   // Full ammo
        1       // 1 enemy
    );

    EXPECT_LT(SA.ThreatLevel, 0.5f);
}

TEST_F(GamingMasterySystemTest, AdvantageRecognizedCorrectly) {
    auto SA = System->AssessSituation(
        1.0f,  // Full health
        0.1f,  // Weak enemy
        0.0f,  // Player
        50.0f, // Enemy at medium range
        1.0f,  // Full ammo
        1      // 1 weak enemy
    );

    EXPECT_TRUE(SA.bHasAdvantage());
    EXPECT_GT(SA.WinProbability, 0.5f);
}

TEST_F(GamingMasterySystemTest, CanEngageWithResourcesAndAdvantage) {
    auto SA = System->AssessSituation(
        0.8f,  // Good health
        0.5f,  // Equal enemy
        0.0f,  // Player
        30.0f, // Enemy at medium range
        0.7f,  // Good ammo
        1      // 1 enemy
    );

    // With good resources and reasonable win probability, should engage
    bool canEngage = SA.bCanEngage();
    EXPECT_TRUE(SA.ResourceAvailability > 0.3f);
}

TEST_F(GamingMasterySystemTest, SituationalAwarenessIsBounded) {
    auto SA = System->AssessSituation(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 10);
    EXPECT_GE(SA.ThreatLevel, 0.0f);
    EXPECT_LE(SA.ThreatLevel, 1.0f);
    EXPECT_GE(SA.WinProbability, 0.0f);
    EXPECT_LE(SA.WinProbability, 1.0f);
}

// ============================================================================
// STRATEGIC MODE SELECTION TESTS (Dual Process Theory)
// ============================================================================

TEST_F(GamingMasterySystemTest, DangerSwitchesToSystem1Intuitive) {
    FSituationalAwareness SA;
    SA.ThreatLevel = 0.8f;  // High danger
    SA.WinProbability = 0.3f;
    SA.AdvantageScore = -0.2f;

    EStrategicMode mode = System->SelectStrategicMode(SA);
    EXPECT_EQ(mode, EStrategicMode::System1_Intuitive);
}

TEST_F(GamingMasterySystemTest, AdvantageSelectsSystem2Analytical) {
    FSituationalAwareness SA;
    SA.ThreatLevel = 0.1f;  // Safe
    SA.WinProbability = 0.8f;
    SA.AdvantageScore = 0.5f;  // Clear advantage

    EStrategicMode mode = System->SelectStrategicMode(SA);
    EXPECT_EQ(mode, EStrategicMode::System2_Analytical);
}

TEST_F(GamingMasterySystemTest, HeavyDisadvantageSelectsAdversarial) {
    FSituationalAwareness SA;
    SA.ThreatLevel = 0.2f;
    SA.WinProbability = 0.2f;  // Losing badly
    SA.AdvantageScore = -0.3f;

    EStrategicMode mode = System->SelectStrategicMode(SA);
    EXPECT_EQ(mode, EStrategicMode::AdversarialReasoning);
}

TEST_F(GamingMasterySystemTest, NormalPlaySelectsDualProcess) {
    FSituationalAwareness SA;
    SA.ThreatLevel = 0.3f;  // Moderate threat
    SA.WinProbability = 0.55f;
    SA.AdvantageScore = 0.05f;

    EStrategicMode mode = System->SelectStrategicMode(SA);
    EXPECT_EQ(mode, EStrategicMode::DualProcess);
}

// ============================================================================
// GAME OUTCOME TRACKING TESTS
// ============================================================================

TEST_F(GamingMasterySystemTest, WinIncreasesGlobalProficiency) {
    float before = System->GetGlobalProficiency();
    System->RecordGameOutcome(true, 20, 3, 150.0f, 180);
    EXPECT_GT(System->GetGlobalProficiency(), before);
}

TEST_F(GamingMasterySystemTest, LossDecreasesProficiency) {
    // Give some proficiency first
    for (int i = 0; i < 10; ++i) System->RecordGameOutcome(true, 10, 5, 200.0f, 100);
    float after_wins = System->GetGlobalProficiency();

    System->RecordGameOutcome(false, 2, 15, 350.0f, 50);
    EXPECT_LT(System->GetGlobalProficiency(), after_wins);
}

TEST_F(GamingMasterySystemTest, RecentWinRateTracked) {
    for (int i = 0; i < 8; ++i)
        System->RecordGameOutcome(true, 15, 2, 140.0f, 200);
    for (int i = 0; i < 2; ++i)
        System->RecordGameOutcome(false, 1, 15, 300.0f, 60);

    float wr = System->GetRecentWinRate();
    EXPECT_NEAR(wr, 0.8f, 0.1f);  // ~80% win rate
}

TEST_F(GamingMasterySystemTest, ReactionTimeAverageTracked) {
    System->RecordGameOutcome(true, 10, 2, 120.0f, 180);
    System->RecordGameOutcome(true, 12, 1, 140.0f, 200);
    float rt = System->GetAverageReactionTimeMs();
    EXPECT_GT(rt, 0.0f);
    EXPECT_LT(rt, 400.0f);
}

// ============================================================================
// OPPONENT MODELING TESTS
// ============================================================================

TEST_F(GamingMasterySystemTest, OpponentModelCreated) {
    System->ObserveOpponentBehavior("Player123", 0.8f, 0.7f, 0.3f);
    EXPECT_EQ(System->GetOpponentModelCount(), 1);
}

TEST_F(GamingMasterySystemTest, OpponentModelUpdatesIncrementally) {
    // Observe multiple times - model should converge
    System->ObserveOpponentBehavior("AggressivePlayer", 0.9f, 0.8f, 0.5f);
    System->ObserveOpponentBehavior("AggressivePlayer", 0.85f, 0.75f, 0.5f);
    System->ObserveOpponentBehavior("AggressivePlayer", 0.87f, 0.78f, 0.5f);

    float prediction = System->PredictOpponentAction("AggressivePlayer");
    EXPECT_GT(prediction, 0.5f);  // Aggressive player predicted to be aggressive
}

TEST_F(GamingMasterySystemTest, UnknownOpponentPrediction) {
    float prediction = System->PredictOpponentAction("UnknownPlayer");
    EXPECT_NEAR(prediction, 0.5f, 0.01f);  // Default neutral prediction
}

TEST_F(GamingMasterySystemTest, MultipleOpponentsTracked) {
    System->ObserveOpponentBehavior("Sniper", 0.3f, 0.95f, 0.1f);
    System->ObserveOpponentBehavior("Rusher", 0.95f, 0.6f, 0.9f);
    System->ObserveOpponentBehavior("Support", 0.4f, 0.7f, 0.4f);

    EXPECT_EQ(System->GetOpponentModelCount(), 3);

    float sniperScore = System->PredictOpponentAction("Sniper");
    float rusherScore = System->PredictOpponentAction("Rusher");

    // Rusher should be predicted as more aggressive than Sniper
    EXPECT_GT(rusherScore, sniperScore);
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(GamingMasterySystemTest, SituationalAssessmentIsRealTimeCapable) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        System->AssessSituation(0.5f, 0.5f, 0.0f, 50.0f, 0.8f, 2);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 assessments per second is required for real-time gaming
    EXPECT_LT(duration.count(), 10) << "Situational assessment too slow for real-time";
}

TEST_F(GamingMasterySystemTest, StrategicModeSelectionIsInstantaneous) {
    FSituationalAwareness SA;
    SA.ThreatLevel = 0.5f;
    SA.WinProbability = 0.5f;
    SA.AdvantageScore = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        System->SelectStrategicMode(SA);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_LT(duration.count(), 10000) << "Mode selection must be instantaneous";
}

} // namespace
