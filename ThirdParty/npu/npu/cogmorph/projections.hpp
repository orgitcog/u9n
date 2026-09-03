/**
 * @file projections.hpp
 * @brief Concrete projection implementations for CogMorph
 *
 * Each projection provides a complete view of cognitive state
 * through a different lens/interface.
 */

#pragma once

#include "cogmorph.hpp"
#include <fstream>
#include <sstream>

// Forward declarations from virtual-device framework
namespace ggnucash::vdev {
    class VirtualPCB;
    class DeviceDriver;
}

namespace cogmorph {

// ============================================================================
// Hardware Projection
// ============================================================================

/**
 * @brief Cognitive state as a virtual hardware system
 *
 * Represents cognitive architecture as:
 * - VirtualPCB with memory regions
 * - Device drivers for each subsystem
 * - MMIO registers for control/status
 * - DMA for data movement
 * - Interrupts for events
 */
class HardwareProjection : public CognitiveState {
public:
    HardwareProjection(const std::string& device_id);
    ~HardwareProjection() override;

    [[nodiscard]] ProjectionType projection_type() const override {
        return ProjectionType::HARDWARE;
    }

    [[nodiscard]] std::vector<ComponentDescriptor> list_components() const override;
    [[nodiscard]] bool has_component(ComponentType type) const override;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const override;
    [[nodiscard]] TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) override;
    [[nodiscard]] uint64_t compute_checksum() const override;
    [[nodiscard]] uint32_t version() const override { return 1; }

    // Hardware-specific interface
    [[nodiscard]] ggnucash::vdev::VirtualPCB* pcb() { return pcb_.get(); }
    [[nodiscard]] const ggnucash::vdev::VirtualPCB* pcb() const { return pcb_.get(); }

    /// Read from virtual memory
    [[nodiscard]] uint8_t read_memory(uint64_t address) const;
    [[nodiscard]] uint32_t read_memory32(uint64_t address) const;
    [[nodiscard]] uint64_t read_memory64(uint64_t address) const;

    /// Write to virtual memory
    void write_memory(uint64_t address, uint8_t value);
    void write_memory32(uint64_t address, uint32_t value);
    void write_memory64(uint64_t address, uint64_t value);

    /// Register device driver
    bool attach_driver(std::shared_ptr<ggnucash::vdev::DeviceDriver> driver);

    /// DMA transfer
    int start_dma_transfer(uint64_t src, uint64_t dst, size_t length);
    bool wait_dma_complete(int channel);

    /// Trigger interrupt
    void trigger_interrupt(uint32_t vector);

private:
    std::unique_ptr<ggnucash::vdev::VirtualPCB> pcb_;
    std::vector<std::shared_ptr<ggnucash::vdev::DeviceDriver>> drivers_;
    std::map<ComponentType, ComponentDescriptor> component_map_;
};

// ============================================================================
// Library Projection
// ============================================================================

/**
 * @brief Cognitive state as a friendly API
 *
 * Hides all hardware details behind clean interfaces.
 * For developers who don't need/want to think about registers.
 */
class LibraryProjection : public CognitiveState {
public:
    // Inner API classes that hide hardware complexity
    class AtomSpaceAPI;
    class InferenceAPI;
    class AttentionAPI;
    class PatternAPI;

    LibraryProjection();
    ~LibraryProjection() override;

    [[nodiscard]] ProjectionType projection_type() const override {
        return ProjectionType::LIBRARY;
    }

    [[nodiscard]] std::vector<ComponentDescriptor> list_components() const override;
    [[nodiscard]] bool has_component(ComponentType type) const override;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const override;
    [[nodiscard]] TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) override;
    [[nodiscard]] uint64_t compute_checksum() const override;
    [[nodiscard]] uint32_t version() const override { return 1; }

    // Library-style API access
    [[nodiscard]] AtomSpaceAPI& atomspace() { return *atomspace_; }
    [[nodiscard]] InferenceAPI& inference() { return *inference_; }
    [[nodiscard]] AttentionAPI& attention() { return *attention_; }
    [[nodiscard]] PatternAPI& patterns() { return *patterns_; }

private:
    std::unique_ptr<AtomSpaceAPI> atomspace_;
    std::unique_ptr<InferenceAPI> inference_;
    std::unique_ptr<AttentionAPI> attention_;
    std::unique_ptr<PatternAPI> patterns_;

    // Internal storage (could be backed by HardwareProjection)
    std::unique_ptr<HardwareProjection> backing_hw_;
};

// ============================================================================
// Library Projection - AtomSpace API
// ============================================================================

class LibraryProjection::AtomSpaceAPI {
public:
    // Friendly atom creation
    uint64_t add_node(uint16_t type, const std::string& name);
    uint64_t add_link(uint16_t type, const std::vector<uint64_t>& outgoing);

    // Retrieval
    std::optional<std::string> get_node_name(uint64_t atom_id) const;
    std::optional<std::vector<uint64_t>> get_outgoing(uint64_t atom_id) const;
    std::optional<uint16_t> get_type(uint64_t atom_id) const;

    // Truth values
    void set_truth_value(uint64_t atom_id, float strength, float confidence);
    std::pair<float, float> get_truth_value(uint64_t atom_id) const;

    // Attention values
    void set_attention(uint64_t atom_id, float sti, int16_t lti = 0);
    std::tuple<float, int16_t, uint8_t> get_attention(uint64_t atom_id) const;

    // Queries
    std::vector<uint64_t> get_atoms_by_type(uint16_t type) const;
    std::vector<uint64_t> get_incoming(uint64_t atom_id) const;
    size_t size() const;

private:
    friend class LibraryProjection;
    HardwareProjection* hw_ = nullptr;
};

// ============================================================================
// Static Projection (CGGUF Format)
// ============================================================================

/**
 * @brief Cognitive state as serialized CGGUF data
 *
 * Portable format for:
 * - Saving/loading cognitive state
 * - Distributing trained models
 * - Checkpoint/restore
 */
class StaticProjection : public CognitiveState {
public:
    StaticProjection(uint32_t version = cgguf::VERSION);
    ~StaticProjection() override;

    [[nodiscard]] ProjectionType projection_type() const override {
        return ProjectionType::STATIC;
    }

    [[nodiscard]] std::vector<ComponentDescriptor> list_components() const override;
    [[nodiscard]] bool has_component(ComponentType type) const override;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const override;
    [[nodiscard]] TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) override;
    [[nodiscard]] uint64_t compute_checksum() const override;
    [[nodiscard]] uint32_t version() const override { return header_.version; }

    // File I/O
    [[nodiscard]] TransformError load_from_file(const std::string& path);
    [[nodiscard]] TransformError save_to_file(const std::string& path) const;

    // Memory I/O
    [[nodiscard]] TransformError load_from_memory(std::span<const uint8_t> data);
    [[nodiscard]] TransformResult<std::vector<uint8_t>> save_to_memory() const;

    // Tensor access
    [[nodiscard]] std::vector<cgguf::TensorDescriptor> list_tensors() const;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_tensor_data(const std::string& name) const;
    [[nodiscard]] TransformError
        set_tensor_data(const std::string& name, cgguf::TensorType type,
                        std::span<const uint64_t> shape,
                        std::span<const uint8_t> data);

    // Metadata
    void set_metadata(const std::string& key, const std::string& value);
    [[nodiscard]] std::optional<std::string> get_metadata(const std::string& key) const;

private:
    cgguf::Header header_;
    std::vector<cgguf::TensorDescriptor> tensors_;
    std::vector<uint8_t> data_blob_;
    std::map<std::string, std::string> metadata_;
    std::map<ComponentType, size_t> component_to_tensor_;  // Maps component to tensor index
};

// ============================================================================
// Network Projection
// ============================================================================

/**
 * @brief Cognitive state as network-accessible resource
 *
 * Enables:
 * - Distributed cognitive processing
 * - Remote AtomSpace access
 * - Cognitive state migration
 */
class NetworkProjection : public CognitiveState {
public:
    enum class Protocol : uint8_t {
        TCP,
        UDP,
        WEBSOCKET,
        GRPC,
    };

    NetworkProjection(const std::string& endpoint, Protocol proto = Protocol::TCP);
    ~NetworkProjection() override;

    [[nodiscard]] ProjectionType projection_type() const override {
        return ProjectionType::NETWORK;
    }

    [[nodiscard]] std::vector<ComponentDescriptor> list_components() const override;
    [[nodiscard]] bool has_component(ComponentType type) const override;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const override;
    [[nodiscard]] TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) override;
    [[nodiscard]] uint64_t compute_checksum() const override;
    [[nodiscard]] uint32_t version() const override;

    // Connection management
    [[nodiscard]] TransformError connect();
    [[nodiscard]] TransformError disconnect();
    [[nodiscard]] bool is_connected() const { return connected_; }

    // Synchronization
    [[nodiscard]] TransformError sync_full();
    [[nodiscard]] TransformError sync_component(ComponentType type);
    [[nodiscard]] TransformError push_changes();
    [[nodiscard]] TransformError pull_changes();

private:
    std::string endpoint_;
    Protocol protocol_;
    bool connected_ = false;

    // Cached state (for offline operation)
    std::unique_ptr<StaticProjection> cache_;
};

// ============================================================================
// Glyph Projection
// ============================================================================

/**
 * @brief Cognitive state as visual font-based storage
 *
 * Each "sector" of storage is rendered as a glyph.
 * The visual representation IS the data.
 *
 * Benefits:
 * - Human-inspectable storage
 * - Natural compression (similar data = similar glyphs)
 * - Potential for cognitive pattern recognition on storage itself
 */
class GlyphProjection : public CognitiveState {
public:
    GlyphProjection();
    ~GlyphProjection() override;

    [[nodiscard]] ProjectionType projection_type() const override {
        return ProjectionType::GLYPH;
    }

    [[nodiscard]] std::vector<ComponentDescriptor> list_components() const override;
    [[nodiscard]] bool has_component(ComponentType type) const override;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_component_data(ComponentType type) const override;
    [[nodiscard]] TransformError
        set_component_data(ComponentType type, std::span<const uint8_t> data) override;
    [[nodiscard]] uint64_t compute_checksum() const override;
    [[nodiscard]] uint32_t version() const override { return 1; }

    // Glyph operations
    [[nodiscard]] std::vector<glyph::GlyphDescriptor> list_glyphs() const;
    [[nodiscard]] TransformResult<std::vector<uint8_t>>
        get_glyph_payload(uint32_t glyph_id) const;
    [[nodiscard]] TransformError
        set_glyph_payload(uint32_t glyph_id, glyph::GlyphType type,
                          std::span<const uint8_t> payload);

    // Visual rendering
    [[nodiscard]] std::string render_to_svg() const;
    [[nodiscard]] TransformError render_to_file(const std::string& path) const;

    // Font operations
    [[nodiscard]] TransformError export_font(const std::string& path) const;
    [[nodiscard]] TransformError import_font(const std::string& path);

    // Logical address to glyph mapping
    [[nodiscard]] uint32_t address_to_glyph(uint64_t logical_address) const;
    [[nodiscard]] uint64_t glyph_to_address(uint32_t glyph_id) const;

private:
    std::map<uint32_t, glyph::GlyphDescriptor> glyphs_;
    std::map<uint32_t, std::vector<uint8_t>> payloads_;
    uint32_t next_glyph_id_ = 1;

    // Visual parameters
    struct VisualParams {
        float base_width = 100.0f;
        float base_height = 100.0f;
        std::string stroke_color = "#000000";
        std::string fill_color = "#ffffff";
    } visual_;
};

// ============================================================================
// ASSD Device Driver
// ============================================================================

/**
 * @brief AtomSpace State Drive - virtual block device for AtomSpace
 *
 * Implements ASFS filesystem with hardware-style interface.
 * Optimized for hypergraph access patterns.
 */
class ASSDDriver {
public:
    ASSDDriver(size_t capacity_bytes);
    ~ASSDDriver();

    // DeviceDriver-style interface
    bool load(ggnucash::vdev::VirtualPCB* pcb);
    bool initialize();
    bool probe();
    bool remove();

    // Block device operations
    [[nodiscard]] TransformError read_block(uint64_t block_addr, std::span<uint8_t> buffer);
    [[nodiscard]] TransformError write_block(uint64_t block_addr, std::span<const uint8_t> data);

    // Atom-optimized operations
    [[nodiscard]] TransformResult<asfs::AtomEntry> read_atom(uint64_t atom_id);
    [[nodiscard]] TransformError write_atom(uint64_t atom_id, const asfs::AtomEntry& entry);
    [[nodiscard]] TransformResult<std::vector<uint64_t>> query_by_type(uint16_t atom_type);
    [[nodiscard]] TransformResult<std::vector<uint64_t>> query_incoming(uint64_t atom_id);

    // Filesystem operations
    [[nodiscard]] TransformError format();
    [[nodiscard]] TransformError flush();
    [[nodiscard]] asfs::Superblock get_superblock() const { return superblock_; }

    // Statistics
    [[nodiscard]] size_t get_capacity() const { return capacity_; }
    [[nodiscard]] size_t get_used_bytes() const;
    [[nodiscard]] size_t get_atom_count() const { return superblock_.atom_count; }
    [[nodiscard]] size_t get_link_count() const { return superblock_.link_count; }

private:
    ggnucash::vdev::VirtualPCB* pcb_ = nullptr;
    size_t capacity_;
    size_t block_size_ = 4096;

    asfs::Superblock superblock_;
    std::vector<uint8_t> block_cache_;
    bool initialized_ = false;

    // Register access helpers
    void write_reg32(uint64_t offset, uint32_t value);
    uint32_t read_reg32(uint64_t offset) const;

    // Internal helpers
    uint64_t allocate_block();
    void free_block(uint64_t block_addr);
    uint64_t find_atom_block(uint64_t atom_id);
};

} // namespace cogmorph
