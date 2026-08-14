/**
 * @file cogmorph.hpp
 * @brief CogMorph - Cognitive State Transformation Framework
 *
 * Provides isomorphic transformations between different representations
 * of cognitive state:
 *   - Hardware Projection (VirtualPCB, MMIO, DMA)
 *   - Library Projection (APIs, objects, callbacks)
 *   - Static Projection (CGGUF, serialized formats)
 *   - Network Projection (wire protocols, distributed)
 *   - Glyph Projection (visual/font-based storage)
 *
 * Key insight: All projections represent the SAME cognitive state.
 * Transforms are bidirectional and preserve cognitive semantics.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <span>
#include <functional>
#include <variant>
#include <expected>

namespace cogmorph {

// ============================================================================
// Forward Declarations
// ============================================================================

class CognitiveState;
class HardwareProjection;
class LibraryProjection;
class StaticProjection;
class NetworkProjection;
class GlyphProjection;
class TransformRegistry;

// ============================================================================
// Error Handling
// ============================================================================

enum class TransformError {
    SUCCESS = 0,
    INVALID_SOURCE,
    INVALID_TARGET,
    INCOMPATIBLE_VERSIONS,
    MEMORY_EXHAUSTED,
    SERIALIZATION_FAILED,
    DESERIALIZATION_FAILED,
    CHECKSUM_MISMATCH,
    UNSUPPORTED_TRANSFORM,
    PARTIAL_TRANSFORM,
};

template<typename T>
using TransformResult = std::expected<T, TransformError>;

// ============================================================================
// Projection Type Enumeration
// ============================================================================

enum class ProjectionType : uint8_t {
    HARDWARE,    // VirtualPCB, registers, DMA, interrupts
    LIBRARY,     // APIs, function calls, objects
    STATIC,      // CGGUF, frozen weights, serialized state
    NETWORK,     // Wire protocol, distributed
    GLYPH,       // Font-based storage, visual encoding
};

// ============================================================================
// Cognitive Component Types
// ============================================================================

/// Types of cognitive components that can exist in any projection
enum class ComponentType : uint16_t {
    // AtomSpace Core
    ATOMSPACE_ATOMS      = 0x0100,
    ATOMSPACE_LINKS      = 0x0101,
    ATOMSPACE_TYPES      = 0x0102,
    ATOMSPACE_NAMES      = 0x0103,

    // Truth/Attention Values
    TRUTH_VALUES         = 0x0200,
    ATTENTION_VALUES     = 0x0201,
    ATTENTION_BANK       = 0x0202,

    // PLN
    PLN_RULES            = 0x0300,
    PLN_INFERENCE_STATE  = 0x0301,
    PLN_PROOF_TREE       = 0x0302,

    // URE
    URE_RULE_BASE        = 0x0400,
    URE_CHAINER_STATE    = 0x0401,
    URE_CONTROL_POLICY   = 0x0402,

    // ECAN
    ECAN_HEBBIAN_LINKS   = 0x0500,
    ECAN_STIMULUS_MAP    = 0x0501,
    ECAN_RENT_COLLECTION = 0x0502,

    // MOSES
    MOSES_DEME           = 0x0600,
    MOSES_POPULATION     = 0x0601,
    MOSES_METAPOP        = 0x0602,

    // Pattern Matcher
    PATTERN_CACHE        = 0x0700,
    PATTERN_INDICES      = 0x0701,

    // NPU (LLM Coprocessor)
    NPU_MODEL_WEIGHTS    = 0x0800,
    NPU_KV_CACHE         = 0x0801,
    NPU_TOKENIZER_VOCAB  = 0x0802,
    NPU_EMBEDDING_TABLE  = 0x0803,

    // Episodic Memory
    EPISODIC_EVENTS      = 0x0900,
    EPISODIC_TIMELINE    = 0x0901,
    SPACETIME_MAP        = 0x0902,

    // User-defined
    USER_DEFINED         = 0xF000,
};

// ============================================================================
// Component Descriptor
// ============================================================================

/// Describes a cognitive component in projection-agnostic terms
struct ComponentDescriptor {
    ComponentType type;
    uint64_t logical_address;      // Projection-independent address
    uint64_t size_bytes;
    uint32_t version;
    uint64_t checksum;
    std::string name;

    // Dependencies on other components
    std::vector<ComponentType> dependencies;
};

// ============================================================================
// Abstract Cognitive State
// ============================================================================

/**
 * @brief Abstract base for all cognitive state representations
 *
 * This is the invariant core that all projections represent.
 * Each projection provides a different "view" of the same state.
 */
class CognitiveState {
public:
    virtual ~CognitiveState() = default;

    /// What type of projection is this?
    [[nodiscard]] virtual ProjectionType projection_type() const = 0;

    /// List all components in this state
    [[nodiscard]] virtual std::vector<ComponentDescriptor> list_components() const = 0;

    /// Check if a specific component exists
    [[nodiscard]] virtual bool has_component(ComponentType type) const = 0;

    /// Get raw bytes for a component (projection-specific encoding)
    [[nodiscard]] virtual TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const = 0;

    /// Set raw bytes for a component
    [[nodiscard]] virtual TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) = 0;

    /// Compute checksum of entire state
    [[nodiscard]] virtual uint64_t compute_checksum() const = 0;

    /// Version identifier for compatibility checking
    [[nodiscard]] virtual uint32_t version() const = 0;
};

// ============================================================================
// Transform Interface
// ============================================================================

/**
 * @brief Abstract bidirectional transform between projections
 */
template<typename From, typename To>
class CognitiveTransform {
public:
    virtual ~CognitiveTransform() = default;

    /// Transform from source to target projection
    [[nodiscard]] virtual TransformResult<std::unique_ptr<To>>
        transform(const From& source) const = 0;

    /// Inverse transform from target back to source
    [[nodiscard]] virtual TransformResult<std::unique_ptr<From>>
        inverse(const To& target) const = 0;

    /// Check if transform is possible (version/feature compatibility)
    [[nodiscard]] virtual bool can_transform(const From& source) const = 0;

    /// Estimate size of transformed output
    [[nodiscard]] virtual size_t estimate_target_size(const From& source) const = 0;
};

// ============================================================================
// Hardware Memory Map Constants
// ============================================================================

namespace hw {
    // Virtual memory regions (matching VirtualPCB layout)
    constexpr uint64_t FLASH_BASE      = 0x08000000;  // 1MB firmware
    constexpr uint64_t SRAM_BASE       = 0x20000000;  // 256KB working memory
    constexpr uint64_t PERIPH_BASE     = 0x40000000;  // Peripheral registers
    constexpr uint64_t ATOMSPACE_BASE  = 0x80000000;  // AtomSpace region (2GB)
    constexpr uint64_t VRAM_BASE       = 0xC0000000;  // Attention/GPU memory (1GB)

    // AtomSpace subregions within ATOMSPACE_BASE
    constexpr uint64_t ATOMS_OFFSET       = 0x00000000;  // Atom headers
    constexpr uint64_t LINKS_OFFSET       = 0x10000000;  // Link outgoing sets
    constexpr uint64_t NAMES_OFFSET       = 0x20000000;  // Node names (string table)
    constexpr uint64_t TV_OFFSET          = 0x30000000;  // Truth values
    constexpr uint64_t AV_OFFSET          = 0x38000000;  // Attention values

    // Peripheral device addresses
    constexpr uint64_t VCPU_BASE       = 0x40000000;  // URE inference core
    constexpr uint64_t VNPU_BASE       = 0x40001000;  // LLM coprocessor
    constexpr uint64_t VTPU_BASE       = 0x40002000;  // PLN truth processor
    constexpr uint64_t VAPU_BASE       = 0x40003000;  // ECAN attention unit
    constexpr uint64_t VASSD_BASE      = 0x40004000;  // AtomSpace State Drive
    constexpr uint64_t VGPU_BASE       = 0x40005000;  // Grassmannian Processing Unit (VLTI)

    // Slot sizes (matching opencog-modern types)
    constexpr size_t ATOM_SLOT_SIZE = 16;   // AtomHeader
    constexpr size_t TV_SLOT_SIZE   = 8;    // TruthValue
    constexpr size_t AV_SLOT_SIZE   = 8;    // AttentionValue
    constexpr size_t LINK_PTR_SIZE  = 8;    // AtomId
}

// ============================================================================
// CGGUF Format Constants (Static Projection)
// ============================================================================

namespace cgguf {
    constexpr uint32_t MAGIC = 0x43474755;  // "CGGU" in little-endian
    constexpr uint32_t VERSION = 1;

    // Tensor types (mirroring GGUF but cognitive-specific)
    enum class TensorType : uint32_t {
        // Standard numeric
        F32 = 0,
        F16 = 1,
        I32 = 2,
        I16 = 3,
        I8  = 4,

        // Cognitive-specific
        TRUTH_VALUE   = 100,  // <strength:f16, confidence:f16>
        ATTENTION_VAL = 101,  // <sti:f16, lti:i16, vlti:u8>
        ATOM_ID       = 102,  // <value:u64>
        ATOM_HEADER   = 103,  // <type:u16, flags:u16, inc_count:u32, hash:u64>

        // Sparse encodings
        SPARSE_TV     = 200,  // Run-length encoded truth values
        SPARSE_AV     = 201,  // Run-length encoded attention values
        SPARSE_LINKS  = 202,  // Compressed adjacency list
    };

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t n_components;
        uint64_t data_offset;
        uint64_t metadata_size;
        uint64_t total_size;
        uint64_t checksum;
    };

    struct TensorDescriptor {
        char name[64];
        ComponentType component_type;
        TensorType tensor_type;
        uint32_t n_dims;
        uint64_t shape[4];
        uint64_t offset;
        uint64_t size;
    };
}

// ============================================================================
// Glyph Storage Constants
// ============================================================================

namespace glyph {
    // Each glyph represents a "sector" of cognitive storage
    constexpr size_t GLYPH_SECTOR_SIZE = 4096;  // 4KB per glyph

    // Glyph semantic types (mapped to visual representations)
    enum class GlyphType : uint16_t {
        // Structural
        ATOM_NODE    = 0x0001,  // Circle glyph
        ATOM_LINK    = 0x0002,  // Arrow glyph

        // Values
        TRUTH_HIGH   = 0x0010,  // Solid fill
        TRUTH_LOW    = 0x0011,  // Hollow
        TRUTH_UNSURE = 0x0012,  // Dashed

        // Attention
        FOCUS_HOT    = 0x0020,  // Bright/large
        FOCUS_WARM   = 0x0021,  // Medium
        FOCUS_COLD   = 0x0022,  // Dim/small

        // Control
        SECTOR_EMPTY = 0x00FF,
        SECTOR_META  = 0x00FE,
    };

    struct GlyphDescriptor {
        uint32_t glyph_id;
        GlyphType type;
        uint64_t logical_address;
        uint32_t payload_size;
        uint16_t visual_width;
        uint16_t visual_height;
    };
}

// ============================================================================
// ASFS - AtomSpace File System
// ============================================================================

namespace asfs {
    // Magic number for ASFS superblock
    constexpr uint64_t ASFS_MAGIC = 0x4153465331303030ULL;  // "ASFS1000"

    // ASFS optimizes for hypergraph storage patterns:
    // - Atoms are addressed by AtomId (not inode)
    // - Links have special adjacency-optimized blocks
    // - Truth/Attention values are append-optimized journals

    enum class BlockType : uint8_t {
        SUPERBLOCK       = 0x00,
        ATOM_TABLE       = 0x01,
        LINK_ADJACENCY   = 0x02,
        NAME_STRINGS     = 0x03,
        TRUTH_JOURNAL    = 0x04,
        ATTENTION_JOURNAL= 0x05,
        TYPE_HIERARCHY   = 0x06,
        INDEX_BTREE      = 0x07,
        FREE_LIST        = 0xFF,
    };

    struct Superblock {
        uint64_t magic;
        uint32_t version;
        uint32_t block_size;       // Typically 4096
        uint64_t total_blocks;
        uint64_t free_blocks;
        uint64_t atom_count;
        uint64_t link_count;
        uint64_t root_atom_block;  // Block containing atom table root
        uint64_t name_table_block; // String table for node names
        uint64_t type_table_block; // Type hierarchy
        uint64_t journal_head;     // Head of TV/AV journal
        uint64_t checksum;
    };

    // ASFS block header (first 16 bytes of each block)
    struct BlockHeader {
        BlockType type;
        uint8_t flags;
        uint16_t entry_count;
        uint32_t next_block;       // For chained blocks
        uint64_t first_atom_id;    // For indexed lookup
    };

    // Atom entry in ATOM_TABLE block
    struct AtomEntry {
        uint64_t atom_id;
        uint16_t atom_type;
        uint16_t flags;
        uint32_t name_offset;      // Offset into NAME_STRINGS (0 for links)
        uint32_t outgoing_block;   // Block containing outgoing set (links only)
        uint16_t outgoing_arity;   // Number of outgoing atoms
        uint16_t incoming_count;   // Number of links pointing here
        float tv_strength;
        float tv_confidence;
        float av_sti;
        int16_t av_lti;
        uint16_t av_vlti;
    };

    static_assert(sizeof(AtomEntry) == 48, "AtomEntry should be 48 bytes");
}

// ============================================================================
// ASSD - AtomSpace State Drive (Virtual Storage Device)
// ============================================================================

namespace assd {
    // ASSD presents ASFS as a block device with cognitive-optimized I/O

    // Device registers (at hw::VASSD_BASE)
    constexpr uint64_t REG_CMD         = 0x00;  // Command register
    constexpr uint64_t REG_STATUS      = 0x04;  // Status register
    constexpr uint64_t REG_ATOM_ID     = 0x08;  // Atom ID for operation
    constexpr uint64_t REG_BLOCK_ADDR  = 0x10;  // Block address
    constexpr uint64_t REG_DMA_ADDR    = 0x18;  // DMA buffer address
    constexpr uint64_t REG_XFER_SIZE   = 0x20;  // Transfer size
    constexpr uint64_t REG_ATOM_COUNT  = 0x28;  // Total atoms (read-only)
    constexpr uint64_t REG_LINK_COUNT  = 0x30;  // Total links (read-only)
    constexpr uint64_t REG_ERROR       = 0x38;  // Error code

    // Commands
    enum Command : uint32_t {
        CMD_NOP           = 0x00,
        CMD_READ_BLOCK    = 0x01,
        CMD_WRITE_BLOCK   = 0x02,
        CMD_READ_ATOM     = 0x03,  // Read atom by ID (optimized)
        CMD_WRITE_ATOM    = 0x04,  // Write atom by ID
        CMD_QUERY_TYPE    = 0x05,  // Get all atoms of type
        CMD_QUERY_INCOMING= 0x06,  // Get incoming links for atom
        CMD_QUERY_PATTERN = 0x07,  // Pattern match (async)
        CMD_FLUSH         = 0x08,
        CMD_TRIM          = 0x09,  // Mark blocks as free
        CMD_FORMAT        = 0xFF,
    };

    // Status bits
    enum Status : uint32_t {
        STATUS_READY      = 0x01,
        STATUS_BUSY       = 0x02,
        STATUS_ERROR      = 0x04,
        STATUS_DMA_ACTIVE = 0x08,
        STATUS_FORMATTED  = 0x10,
    };
}

// ============================================================================
// Transform Registry
// ============================================================================

/**
 * @brief Central registry of available cognitive transforms
 *
 * Allows discovering and executing transforms between any two projections.
 */
class TransformRegistry {
public:
    using TransformFactory = std::function<
        TransformResult<std::unique_ptr<CognitiveState>>(const CognitiveState&)
    >;

    static TransformRegistry& instance();

    /// Register a transform between two projection types
    void register_transform(
        ProjectionType from,
        ProjectionType to,
        TransformFactory factory
    );

    /// Check if a direct transform exists
    [[nodiscard]] bool has_transform(ProjectionType from, ProjectionType to) const;

    /// Execute a transform (finds shortest path if no direct transform)
    [[nodiscard]] TransformResult<std::unique_ptr<CognitiveState>>
        transform(const CognitiveState& source, ProjectionType target) const;

    /// List all registered transform pairs
    [[nodiscard]] std::vector<std::pair<ProjectionType, ProjectionType>>
        list_transforms() const;

private:
    TransformRegistry() = default;

    std::map<std::pair<ProjectionType, ProjectionType>, TransformFactory> transforms_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Transform cognitive state to a different projection
template<typename TargetProjection>
[[nodiscard]] TransformResult<std::unique_ptr<TargetProjection>>
morph(const CognitiveState& source);

/// Create a new hardware projection with standard layout
[[nodiscard]] std::unique_ptr<HardwareProjection>
create_hardware_projection(const std::string& device_id);

/// Create a new CGGUF static projection
[[nodiscard]] std::unique_ptr<StaticProjection>
create_static_projection(uint32_t version = cgguf::VERSION);

/// Create a new ASSD-backed projection
[[nodiscard]] std::unique_ptr<HardwareProjection>
create_assd_projection(const std::string& device_id, size_t capacity_bytes);

/// Compute logical address for an atom ID
[[nodiscard]] constexpr uint64_t atom_logical_address(uint64_t atom_id) noexcept {
    return hw::ATOMSPACE_BASE + hw::ATOMS_OFFSET + (atom_id * hw::ATOM_SLOT_SIZE);
}

/// Compute logical address for truth value of an atom
[[nodiscard]] constexpr uint64_t tv_logical_address(uint64_t atom_id) noexcept {
    return hw::ATOMSPACE_BASE + hw::TV_OFFSET + (atom_id * hw::TV_SLOT_SIZE);
}

/// Compute logical address for attention value of an atom
[[nodiscard]] constexpr uint64_t av_logical_address(uint64_t atom_id) noexcept {
    return hw::ATOMSPACE_BASE + hw::AV_OFFSET + (atom_id * hw::AV_SLOT_SIZE);
}

} // namespace cogmorph
