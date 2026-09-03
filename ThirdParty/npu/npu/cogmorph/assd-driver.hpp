/**
 * @file assd-driver.hpp
 * @brief AtomSpace State Drive - Virtual Block Device for AtomSpace
 *
 * ASSD provides hardware-style block device interface optimized for
 * hypergraph storage patterns. Implements ASFS (AtomSpace File System).
 *
 * Memory Map (at hw::VASSD_BASE = 0x40004000):
 *   0x00 REG_CMD          - Command register
 *   0x04 REG_STATUS       - Status register
 *   0x08 REG_ATOM_ID      - Atom ID for atom operations
 *   0x10 REG_BLOCK_ADDR   - Block address for block operations
 *   0x18 REG_DMA_ADDR     - DMA buffer address
 *   0x20 REG_XFER_SIZE    - Transfer size in bytes
 *   0x28 REG_ATOM_COUNT   - Total atom count (read-only)
 *   0x30 REG_LINK_COUNT   - Total link count (read-only)
 *   0x38 REG_ERROR        - Error code register
 */

#pragma once

#include "cogmorph.hpp"
#include <vector>
#include <map>
#include <optional>
#include <mutex>

// Forward declaration
namespace ggnucash::vdev {
    class VirtualPCB;
    class DeviceDriver;
}

namespace cogmorph {

// ============================================================================
// ASSD Driver Implementation
// ============================================================================

class ASSDDriverImpl : public ggnucash::vdev::DeviceDriver {
public:
    // ------------------------------------------------------------------------
    // Register Layout (at hw::VASSD_BASE)
    // ------------------------------------------------------------------------
    static constexpr uint64_t REG_BASE = hw::VASSD_BASE;

    static constexpr uint64_t REG_CMD         = REG_BASE + assd::REG_CMD;
    static constexpr uint64_t REG_STATUS      = REG_BASE + assd::REG_STATUS;
    static constexpr uint64_t REG_ATOM_ID     = REG_BASE + assd::REG_ATOM_ID;
    static constexpr uint64_t REG_BLOCK_ADDR  = REG_BASE + assd::REG_BLOCK_ADDR;
    static constexpr uint64_t REG_DMA_ADDR    = REG_BASE + assd::REG_DMA_ADDR;
    static constexpr uint64_t REG_XFER_SIZE   = REG_BASE + assd::REG_XFER_SIZE;
    static constexpr uint64_t REG_ATOM_COUNT  = REG_BASE + assd::REG_ATOM_COUNT;
    static constexpr uint64_t REG_LINK_COUNT  = REG_BASE + assd::REG_LINK_COUNT;
    static constexpr uint64_t REG_ERROR       = REG_BASE + assd::REG_ERROR;

    // Error codes
    enum ErrorCode : uint32_t {
        ERR_NONE           = 0x00,
        ERR_INVALID_CMD    = 0x01,
        ERR_INVALID_ADDR   = 0x02,
        ERR_ATOM_NOT_FOUND = 0x03,
        ERR_OUT_OF_SPACE   = 0x04,
        ERR_NOT_FORMATTED  = 0x05,
        ERR_CHECKSUM       = 0x06,
        ERR_DMA_FAULT      = 0x07,
    };

    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------

    ASSDDriverImpl(size_t capacity_bytes, size_t block_size = 4096);
    ~ASSDDriverImpl() override;

    // ------------------------------------------------------------------------
    // DeviceDriver Interface
    // ------------------------------------------------------------------------

    bool load(ggnucash::vdev::VirtualPCB* pcb) override;
    bool initialize() override;
    bool probe() override;
    bool remove() override;

    // ------------------------------------------------------------------------
    // Block Device Operations
    // ------------------------------------------------------------------------

    /// Read a block by address
    [[nodiscard]] TransformError
    read_block(uint64_t block_addr, std::span<uint8_t> buffer);

    /// Write a block by address
    [[nodiscard]] TransformError
    write_block(uint64_t block_addr, std::span<const uint8_t> data);

    /// Read multiple contiguous blocks
    [[nodiscard]] TransformError
    read_blocks(uint64_t start_block, size_t count, std::span<uint8_t> buffer);

    /// Write multiple contiguous blocks
    [[nodiscard]] TransformError
    write_blocks(uint64_t start_block, size_t count, std::span<const uint8_t> data);

    // ------------------------------------------------------------------------
    // Atom-Optimized Operations
    // ------------------------------------------------------------------------

    /// Read atom entry by ID (O(log n) via B-tree index)
    [[nodiscard]] TransformResult<asfs::AtomEntry>
    read_atom(uint64_t atom_id);

    /// Write/update atom entry by ID
    [[nodiscard]] TransformError
    write_atom(uint64_t atom_id, const asfs::AtomEntry& entry);

    /// Delete atom by ID
    [[nodiscard]] TransformError
    delete_atom(uint64_t atom_id);

    /// Query all atoms of a given type
    [[nodiscard]] TransformResult<std::vector<uint64_t>>
    query_by_type(uint16_t atom_type);

    /// Query all incoming links for an atom
    [[nodiscard]] TransformResult<std::vector<uint64_t>>
    query_incoming(uint64_t atom_id);

    /// Query atoms in attentional focus (STI > threshold)
    [[nodiscard]] TransformResult<std::vector<uint64_t>>
    query_attentional_focus(float sti_threshold = 0.0f);

    /// Query atoms by truth value range
    [[nodiscard]] TransformResult<std::vector<uint64_t>>
    query_by_truth_value(float min_strength, float max_strength,
                         float min_confidence = 0.0f);

    // ------------------------------------------------------------------------
    // Link-Specific Operations
    // ------------------------------------------------------------------------

    /// Get outgoing set for a link
    [[nodiscard]] TransformResult<std::vector<uint64_t>>
    get_outgoing_set(uint64_t link_id);

    /// Set outgoing set for a link
    [[nodiscard]] TransformError
    set_outgoing_set(uint64_t link_id, std::span<const uint64_t> outgoing);

    /// Update adjacency index when link is added/removed
    [[nodiscard]] TransformError
    update_adjacency(uint64_t link_id, std::span<const uint64_t> targets, bool add);

    // ------------------------------------------------------------------------
    // Filesystem Operations
    // ------------------------------------------------------------------------

    /// Format the drive with ASFS
    [[nodiscard]] TransformError format();

    /// Flush pending writes to storage
    [[nodiscard]] TransformError flush();

    /// Run consistency check
    [[nodiscard]] TransformError fsck();

    /// Get superblock
    [[nodiscard]] const asfs::Superblock& get_superblock() const { return superblock_; }

    // ------------------------------------------------------------------------
    // Journal Operations (for TV/AV updates)
    // ------------------------------------------------------------------------

    /// Append truth value update to journal
    [[nodiscard]] TransformError
    journal_tv_update(uint64_t atom_id, float strength, float confidence);

    /// Append attention value update to journal
    [[nodiscard]] TransformError
    journal_av_update(uint64_t atom_id, float sti, int16_t lti, uint8_t vlti);

    /// Replay journal to rebuild state
    [[nodiscard]] TransformError replay_journal();

    /// Checkpoint journal (merge into main storage)
    [[nodiscard]] TransformError checkpoint_journal();

    // ------------------------------------------------------------------------
    // Statistics & Diagnostics
    // ------------------------------------------------------------------------

    [[nodiscard]] size_t get_capacity() const { return capacity_; }
    [[nodiscard]] size_t get_block_size() const { return block_size_; }
    [[nodiscard]] size_t get_total_blocks() const { return capacity_ / block_size_; }
    [[nodiscard]] size_t get_used_blocks() const;
    [[nodiscard]] size_t get_free_blocks() const;
    [[nodiscard]] size_t get_atom_count() const { return superblock_.atom_count; }
    [[nodiscard]] size_t get_link_count() const { return superblock_.link_count; }

    [[nodiscard]] std::string get_status_string() const;
    [[nodiscard]] std::string get_diagnostics() const;

private:
    // PCB reference
    ggnucash::vdev::VirtualPCB* pcb_ = nullptr;

    // Storage parameters
    size_t capacity_;
    size_t block_size_;

    // ASFS state
    asfs::Superblock superblock_;
    bool formatted_ = false;
    bool initialized_ = false;

    // Block storage (simulated)
    std::vector<std::vector<uint8_t>> blocks_;

    // Indices for fast lookup
    std::map<uint64_t, uint64_t> atom_to_block_;     // atom_id -> block containing it
    std::map<uint16_t, std::vector<uint64_t>> type_index_;  // type -> atom_ids
    std::map<uint64_t, std::vector<uint64_t>> incoming_index_;  // atom_id -> incoming link_ids

    // Free block management
    std::vector<uint64_t> free_blocks_;

    // Journal
    struct JournalEntry {
        enum Type : uint8_t { TV_UPDATE, AV_UPDATE };
        Type type;
        uint64_t atom_id;
        union {
            struct { float strength; float confidence; } tv;
            struct { float sti; int16_t lti; uint8_t vlti; } av;
        };
    };
    std::vector<JournalEntry> journal_;
    size_t journal_checkpoint_ = 0;

    // Concurrency
    mutable std::mutex mutex_;

    // Register access helpers
    void write_reg32(uint64_t offset, uint32_t value);
    void write_reg64(uint64_t offset, uint64_t value);
    [[nodiscard]] uint32_t read_reg32(uint64_t offset) const;
    [[nodiscard]] uint64_t read_reg64(uint64_t offset) const;

    void set_status(uint32_t bits);
    void clear_status(uint32_t bits);
    void set_error(ErrorCode error);

    // Block management
    [[nodiscard]] uint64_t allocate_block();
    void free_block(uint64_t block_addr);
    [[nodiscard]] bool is_block_free(uint64_t block_addr) const;

    // Index management
    void add_to_type_index(uint64_t atom_id, uint16_t type);
    void remove_from_type_index(uint64_t atom_id, uint16_t type);
    void add_to_incoming_index(uint64_t link_id, uint64_t target_id);
    void remove_from_incoming_index(uint64_t link_id, uint64_t target_id);

    // Serialization helpers
    [[nodiscard]] std::vector<uint8_t> serialize_atom_entry(const asfs::AtomEntry& entry) const;
    [[nodiscard]] asfs::AtomEntry deserialize_atom_entry(std::span<const uint8_t> data) const;
};

// ============================================================================
// ASSD Factory Functions
// ============================================================================

/// Create an ASSD with default settings
[[nodiscard]] inline std::unique_ptr<ASSDDriverImpl>
create_assd(size_t capacity_mb) {
    return std::make_unique<ASSDDriverImpl>(capacity_mb * 1024 * 1024);
}

/// Create an ASSD and attach to a VirtualPCB
[[nodiscard]] inline std::unique_ptr<ASSDDriverImpl>
create_and_attach_assd(ggnucash::vdev::VirtualPCB* pcb, size_t capacity_mb) {
    auto assd = create_assd(capacity_mb);
    if (assd->load(pcb) && assd->initialize()) {
        return assd;
    }
    return nullptr;
}

// ============================================================================
// ASSD <-> AtomSpace Bridge
// ============================================================================

/**
 * @brief Bridge between ASSD storage and AtomSpace API
 *
 * Provides higher-level AtomSpace operations backed by ASSD storage.
 */
class ASSDAtomSpaceBridge {
public:
    explicit ASSDAtomSpaceBridge(ASSDDriverImpl* assd);

    // AtomSpace-style operations
    [[nodiscard]] uint64_t add_node(uint16_t type, const std::string& name,
                                     float strength = 1.0f, float confidence = 0.0f);

    [[nodiscard]] uint64_t add_link(uint16_t type, const std::vector<uint64_t>& outgoing,
                                     float strength = 1.0f, float confidence = 0.0f);

    [[nodiscard]] bool remove_atom(uint64_t atom_id);

    [[nodiscard]] std::optional<std::string> get_name(uint64_t atom_id) const;
    [[nodiscard]] std::optional<uint16_t> get_type(uint64_t atom_id) const;
    [[nodiscard]] std::optional<std::vector<uint64_t>> get_outgoing(uint64_t atom_id) const;
    [[nodiscard]] std::vector<uint64_t> get_incoming(uint64_t atom_id) const;

    // Truth value operations
    void set_tv(uint64_t atom_id, float strength, float confidence);
    [[nodiscard]] std::pair<float, float> get_tv(uint64_t atom_id) const;

    // Attention value operations
    void set_av(uint64_t atom_id, float sti, int16_t lti = 0, uint8_t vlti = 0);
    [[nodiscard]] std::tuple<float, int16_t, uint8_t> get_av(uint64_t atom_id) const;

    // Queries
    [[nodiscard]] std::vector<uint64_t> get_by_type(uint16_t type) const;
    [[nodiscard]] std::vector<uint64_t> get_attentional_focus(float threshold = 0.0f) const;

    // Statistics
    [[nodiscard]] size_t atom_count() const;
    [[nodiscard]] size_t link_count() const;

private:
    ASSDDriverImpl* assd_;
    uint64_t next_atom_id_ = 1;

    // Name interning
    std::map<std::string, uint64_t> name_to_atom_;
    std::map<uint64_t, std::string> atom_to_name_;
};

} // namespace cogmorph
