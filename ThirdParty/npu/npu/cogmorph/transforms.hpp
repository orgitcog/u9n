/**
 * @file transforms.hpp
 * @brief Bidirectional transform implementations between projections
 *
 * These transforms preserve cognitive semantics while changing representation.
 * Each transform has an inverse, enabling round-trip conversion.
 */

#pragma once

#include "cogmorph.hpp"
#include "projections.hpp"

namespace cogmorph {

// ============================================================================
// Hardware <-> Static Transform
// ============================================================================

/**
 * @brief Transform between Hardware and CGGUF Static projections
 *
 * Hardware -> Static: Serialize VirtualPCB memory to CGGUF tensors
 * Static -> Hardware: Deserialize CGGUF tensors to VirtualPCB memory
 */
class HardwareStaticTransform : public CognitiveTransform<HardwareProjection, StaticProjection> {
public:
    [[nodiscard]] TransformResult<std::unique_ptr<StaticProjection>>
        transform(const HardwareProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<HardwareProjection>>
        inverse(const StaticProjection& target) const override;

    [[nodiscard]] bool can_transform(const HardwareProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const HardwareProjection& source) const override;

private:
    // Component serialization helpers
    [[nodiscard]] std::vector<uint8_t> serialize_atoms(const HardwareProjection& hw) const;
    [[nodiscard]] std::vector<uint8_t> serialize_truth_values(const HardwareProjection& hw) const;
    [[nodiscard]] std::vector<uint8_t> serialize_attention_values(const HardwareProjection& hw) const;
    [[nodiscard]] std::vector<uint8_t> serialize_npu_state(const HardwareProjection& hw) const;

    // Component deserialization helpers
    void deserialize_atoms(HardwareProjection& hw, std::span<const uint8_t> data) const;
    void deserialize_truth_values(HardwareProjection& hw, std::span<const uint8_t> data) const;
    void deserialize_attention_values(HardwareProjection& hw, std::span<const uint8_t> data) const;
    void deserialize_npu_state(HardwareProjection& hw, std::span<const uint8_t> data) const;
};

// ============================================================================
// Hardware <-> Library Transform
// ============================================================================

/**
 * @brief Transform between Hardware and Library projections
 *
 * Hardware -> Library: Wrap VirtualPCB with friendly APIs
 * Library -> Hardware: Extract VirtualPCB from backing storage
 */
class HardwareLibraryTransform : public CognitiveTransform<HardwareProjection, LibraryProjection> {
public:
    [[nodiscard]] TransformResult<std::unique_ptr<LibraryProjection>>
        transform(const HardwareProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<HardwareProjection>>
        inverse(const LibraryProjection& target) const override;

    [[nodiscard]] bool can_transform(const HardwareProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const HardwareProjection& source) const override;
};

// ============================================================================
// Hardware <-> Network Transform
// ============================================================================

/**
 * @brief Transform between Hardware and Network projections
 *
 * Hardware -> Network: Serialize for transmission
 * Network -> Hardware: Deserialize received state
 */
class HardwareNetworkTransform : public CognitiveTransform<HardwareProjection, NetworkProjection> {
public:
    explicit HardwareNetworkTransform(const std::string& endpoint);

    [[nodiscard]] TransformResult<std::unique_ptr<NetworkProjection>>
        transform(const HardwareProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<HardwareProjection>>
        inverse(const NetworkProjection& target) const override;

    [[nodiscard]] bool can_transform(const HardwareProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const HardwareProjection& source) const override;

private:
    std::string endpoint_;
};

// ============================================================================
// Hardware <-> Glyph Transform
// ============================================================================

/**
 * @brief Transform between Hardware and Glyph projections
 *
 * Hardware -> Glyph: Encode memory regions as font glyphs
 * Glyph -> Hardware: Decode glyphs back to memory
 */
class HardwareGlyphTransform : public CognitiveTransform<HardwareProjection, GlyphProjection> {
public:
    [[nodiscard]] TransformResult<std::unique_ptr<GlyphProjection>>
        transform(const HardwareProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<HardwareProjection>>
        inverse(const GlyphProjection& target) const override;

    [[nodiscard]] bool can_transform(const HardwareProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const HardwareProjection& source) const override;

private:
    // Glyph encoding helpers
    glyph::GlyphType classify_sector(std::span<const uint8_t> data) const;
    std::vector<uint8_t> encode_sector_to_glyph(std::span<const uint8_t> data) const;
    std::vector<uint8_t> decode_glyph_to_sector(const glyph::GlyphDescriptor& desc,
                                                  std::span<const uint8_t> payload) const;
};

// ============================================================================
// Static <-> Network Transform
// ============================================================================

/**
 * @brief Transform between CGGUF Static and Network projections
 *
 * Static -> Network: Upload CGGUF to remote
 * Network -> Static: Download remote state to CGGUF
 */
class StaticNetworkTransform : public CognitiveTransform<StaticProjection, NetworkProjection> {
public:
    explicit StaticNetworkTransform(const std::string& endpoint);

    [[nodiscard]] TransformResult<std::unique_ptr<NetworkProjection>>
        transform(const StaticProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<StaticProjection>>
        inverse(const NetworkProjection& target) const override;

    [[nodiscard]] bool can_transform(const StaticProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const StaticProjection& source) const override;

private:
    std::string endpoint_;
};

// ============================================================================
// Static <-> Glyph Transform
// ============================================================================

/**
 * @brief Transform between CGGUF Static and Glyph projections
 *
 * Static -> Glyph: Visual encoding of tensor data
 * Glyph -> Static: Reconstruct tensors from glyphs
 */
class StaticGlyphTransform : public CognitiveTransform<StaticProjection, GlyphProjection> {
public:
    [[nodiscard]] TransformResult<std::unique_ptr<GlyphProjection>>
        transform(const StaticProjection& source) const override;

    [[nodiscard]] TransformResult<std::unique_ptr<StaticProjection>>
        inverse(const GlyphProjection& target) const override;

    [[nodiscard]] bool can_transform(const StaticProjection& source) const override;
    [[nodiscard]] size_t estimate_target_size(const StaticProjection& source) const override;
};

// ============================================================================
// Transform Registration
// ============================================================================

/**
 * @brief Register all standard transforms with the registry
 *
 * Call this at program startup to enable all transform paths.
 */
void register_standard_transforms();

/**
 * @brief Register a custom transform
 */
template<typename From, typename To>
void register_transform(std::unique_ptr<CognitiveTransform<From, To>> transform);

// ============================================================================
// Transform Chain
// ============================================================================

/**
 * @brief Compose multiple transforms into a chain
 *
 * When no direct transform exists between two projections,
 * the registry finds the shortest path through intermediate projections.
 */
class TransformChain {
public:
    void add_step(ProjectionType from, ProjectionType to);

    [[nodiscard]] TransformResult<std::unique_ptr<CognitiveState>>
        execute(const CognitiveState& source) const;

    [[nodiscard]] size_t length() const { return steps_.size(); }
    [[nodiscard]] bool empty() const { return steps_.empty(); }

private:
    std::vector<std::pair<ProjectionType, ProjectionType>> steps_;
};

/**
 * @brief Find shortest transform path between two projection types
 */
[[nodiscard]] std::optional<TransformChain>
find_transform_path(ProjectionType from, ProjectionType to);

// ============================================================================
// Convenience Transform Functions
// ============================================================================

/// Transform hardware state to CGGUF file
[[nodiscard]] inline TransformError
save_cognitive_state(const HardwareProjection& hw, const std::string& path) {
    HardwareStaticTransform transform;
    auto result = transform.transform(hw);
    if (!result) return result.error();
    return result.value()->save_to_file(path);
}

/// Load CGGUF file into hardware state
[[nodiscard]] inline TransformResult<std::unique_ptr<HardwareProjection>>
load_cognitive_state(const std::string& path) {
    auto static_proj = std::make_unique<StaticProjection>();
    if (auto err = static_proj->load_from_file(path); err != TransformError::SUCCESS) {
        return std::unexpected(err);
    }
    HardwareStaticTransform transform;
    return transform.inverse(*static_proj);
}

/// Export hardware state as visual font
[[nodiscard]] inline TransformError
export_cognitive_font(const HardwareProjection& hw, const std::string& path) {
    HardwareGlyphTransform transform;
    auto result = transform.transform(hw);
    if (!result) return result.error();
    return result.value()->export_font(path);
}

/// Sync hardware state to remote endpoint
[[nodiscard]] inline TransformError
sync_to_remote(const HardwareProjection& hw, const std::string& endpoint) {
    HardwareNetworkTransform transform(endpoint);
    auto result = transform.transform(hw);
    if (!result) return result.error();
    return result.value()->sync_full();
}

// ============================================================================
// Diff and Patch Operations
// ============================================================================

/**
 * @brief Compute difference between two cognitive states
 */
struct CognitiveDiff {
    std::vector<ComponentType> added;
    std::vector<ComponentType> removed;
    std::vector<ComponentType> modified;

    // Per-component diffs (for modified components)
    std::map<ComponentType, std::vector<uint8_t>> patches;
};

[[nodiscard]] TransformResult<CognitiveDiff>
compute_diff(const CognitiveState& from, const CognitiveState& to);

[[nodiscard]] TransformError
apply_diff(CognitiveState& target, const CognitiveDiff& diff);

// ============================================================================
// Snapshot and Restore
// ============================================================================

/**
 * @brief Create a point-in-time snapshot of cognitive state
 */
struct CognitiveSnapshot {
    uint64_t timestamp;
    uint64_t checksum;
    std::unique_ptr<StaticProjection> state;
    std::string description;
};

[[nodiscard]] TransformResult<CognitiveSnapshot>
create_snapshot(const CognitiveState& source, const std::string& description = "");

[[nodiscard]] TransformError
restore_snapshot(CognitiveState& target, const CognitiveSnapshot& snapshot);

} // namespace cogmorph
