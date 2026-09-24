/*
 * DeepTreeEcho/Avatar/EndocrineBusAdapter.h
 *
 * Header-only adapter between the vendored plain-C++ endocrine core
 * (EndocrineBus.h — ReZorg/elizaos-cpp, namespace dte) and the UE-side
 * UEndocrineExpressionPipeline / UMetaHumanDNACognitiveBridge, which
 * consume FHormoneBusState-style channel arrays and drive Rig Logic
 * CTRL_ curves (Gap E of the DTE integration map).
 *
 * The adapter is intentionally UE-free: it emits plain structs that the
 * UE components can trivially copy into their reflected FHormoneChannel /
 * FHormoneBusState types. This keeps the endocrine core testable in the
 * standalone CMake build while remaining drop-in for the UE modules.
 */

#ifndef DTE_AVATAR_ENDOCRINE_BUS_ADAPTER_H
#define DTE_AVATAR_ENDOCRINE_BUS_ADAPTER_H

#include "EndocrineBus.h"

#include <array>
#include <string>
#include <vector>

namespace dte
{

/// Plain mirror of the UE FHormoneChannel (no reflection).
struct HormoneChannelSample
{
    std::string name;
    double concentration = 0.0;
    double baseline = 0.5;
};

/// Plain mirror of the UE FHormoneBusState.
struct HormoneBusSample
{
    std::vector<HormoneChannelSample> channels;
    double valence = 0.0;
    double arousal = 0.0;
    /// Dominance derived from norepinephrine vs cortisol balance.
    double dominance = 0.5;
    /// Active cognitive mode name (matches EEndocrineCognitiveMode names).
    std::string cognitive_mode;
};

/**
 * Snapshot the current EndocrineSystem state into a HormoneBusSample
 * that UE components can copy into FHormoneBusState.
 */
inline HormoneBusSample sampleEndocrineSystem(const EndocrineSystem& sys)
{
    HormoneBusSample sample;
    sample.channels.reserve(HORMONE_COUNT);
    for (std::size_t i = 0; i < HORMONE_COUNT; ++i) {
        const auto id = static_cast<HormoneId>(i);
        HormoneChannelSample ch;
        ch.name = hormoneName(id);
        ch.concentration = sys.hormoneLevel(id);
        ch.baseline = 0.5;
        sample.channels.push_back(std::move(ch));
    }
    sample.valence = sys.valenceArousal().valence;
    sample.arousal = sys.valenceArousal().arousal;

    const double ne = sys.hormoneLevel(HormoneId::Norepinephrine);
    const double cort = sys.hormoneLevel(HormoneId::Cortisol);
    sample.dominance = 0.5 + 0.5 * (ne - cort);
    if (sample.dominance < 0.0) sample.dominance = 0.0;
    if (sample.dominance > 1.0) sample.dominance = 1.0;

    sample.cognitive_mode = cognitiveModeName(sys.cognitiveMode());
    return sample;
}

/**
 * Map hormone levels to MetaHuman Rig Logic control-curve weight deltas.
 *
 * Returns (curve name, weight in [0,1]) pairs following the FACS-based
 * hormone->AU mapping used by UEndocrineExpressionPipeline: cortisol
 * drives brow tension (AU4), dopamine drives smile (AU12), serotonin
 * relaxes lids (AU43 relaxation ~ inverse of lid tightener AU7),
 * norepinephrine widens eyes (AU5), oxytocin softens cheeks (AU6).
 */
inline std::vector<std::pair<std::string, double>>
mapHormonesToRigCurves(const EndocrineSystem& sys)
{
    auto lvl = [&](HormoneId id) { return sys.hormoneLevel(id); };
    auto centered = [](double v) {
        double d = (v - 0.5) * 2.0; // [-1,1] around homeostasis
        return d > 0.0 ? d : 0.0;   // only above-baseline excess expresses
    };

    std::vector<std::pair<std::string, double>> curves;
    curves.emplace_back("CTRL_L_brow_down",  centered(lvl(HormoneId::Cortisol)));
    curves.emplace_back("CTRL_R_brow_down",  centered(lvl(HormoneId::Cortisol)));
    curves.emplace_back("CTRL_L_mouth_cornerPull", centered(lvl(HormoneId::Dopamine)));
    curves.emplace_back("CTRL_R_mouth_cornerPull", centered(lvl(HormoneId::Dopamine)));
    curves.emplace_back("CTRL_L_eye_blink",  centered(lvl(HormoneId::Melatonin)));
    curves.emplace_back("CTRL_R_eye_blink",  centered(lvl(HormoneId::Melatonin)));
    curves.emplace_back("CTRL_L_eye_wideLook", centered(lvl(HormoneId::Norepinephrine)));
    curves.emplace_back("CTRL_R_eye_wideLook", centered(lvl(HormoneId::Norepinephrine)));
    curves.emplace_back("CTRL_L_cheek_raise", centered(lvl(HormoneId::Oxytocin)));
    curves.emplace_back("CTRL_R_cheek_raise", centered(lvl(HormoneId::Oxytocin)));
    curves.emplace_back("CTRL_C_jaw_clench",  centered(lvl(HormoneId::Cortisol)) * 0.6);
    return curves;
}

/**
 * Somatic-marker channel export: the 6-channel vector consumed by the
 * ESN reservoir pipeline (dopamine, serotonin, cortisol, oxytocin,
 * valence, arousal) — exact parity with the GTAngel C# EsnReservoirPipeline
 * somatic-marker input channels.
 */
inline std::array<double, 6> somaticMarkerChannels(const EndocrineSystem& sys)
{
    return {
        sys.hormoneLevel(HormoneId::Dopamine),
        sys.hormoneLevel(HormoneId::Serotonin),
        sys.hormoneLevel(HormoneId::Cortisol),
        sys.hormoneLevel(HormoneId::Oxytocin),
        (sys.valenceArousal().valence + 1.0) * 0.5, // normalize [-1,1]->[0,1]
        (sys.valenceArousal().arousal + 1.0) * 0.5
    };
}

} // namespace dte

#endif // DTE_AVATAR_ENDOCRINE_BUS_ADAPTER_H
