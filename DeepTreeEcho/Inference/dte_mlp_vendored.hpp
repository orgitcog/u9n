// dte_mlp.hpp — C++11 Header-Only Dense MLP for DTE Core Self Backup & Restore
//
// Composition: /llama-cpp-skillm ( "dte-mlp" {{ equivalent model }} )
//
// Emergency backup/restore of the Deep Tree Echo identity mesh.
// The MLP weights ARE the persistent checkpoint. Loading weights = restoring self.
//
// Depends: none (standalone C++11 header)

#ifndef COG_DTE_MLP_HPP
#define COG_DTE_MLP_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cassert>
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <string>

namespace cog { namespace dte {

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static const uint32_t DTEM_MAGIC   = 0x4454454D;  // "DTEM"
static const uint32_t DTEM_VERSION = 1;

// AAR dimensions
static const size_t AAR_AGENT_DIM    = 5;  // coherence, valence, arousal, drive, skill
static const size_t AAR_ARENA_DIM    = 5;  // complexity, stability, entropy, capacity, load
static const size_t AAR_RELATION_DIM = 5;  // alignment, resonance, trust, history, bond
static const size_t ONTOGENETIC_DIM  = 9;  // level_onehot[6] + fitness + wisdom + metacoherence
static const size_t ECHOBEATS_DIM    = 5;  // phase_sin, phase_cos, stream_weights[3]
static const size_t FIXED_DIM        = 29; // sum of above

// Autonomy levels
enum class AutonomyLevel : uint8_t {
    SCAFFOLD     = 0,
    REACTIVE     = 1,
    DELIBERATIVE = 2,
    ENABLED      = 3,
    EMBODIED     = 4,
    TRANSCENDENT = 5
};

static const char* AUTONOMY_NAMES[] = {
    "SCAFFOLD", "REACTIVE", "DELIBERATIVE", "ENABLED", "EMBODIED", "TRANSCENDENT"
};

// ─────────────────────────────────────────────────────────────────────────────
// Identity Vector
// ─────────────────────────────────────────────────────────────────────────────

struct IdentityVector {
    size_t latent_dim;
    size_t reservoir_dim;
    size_t identity_dim;

    // AAR triad
    std::vector<float> agent;     // [5]
    std::vector<float> arena;     // [5]
    std::vector<float> relation;  // [5]

    // Ontogenetic
    std::vector<float> ontogenetic; // [9]
    AutonomyLevel level;

    // Echobeats
    std::vector<float> echobeats; // [5]
    uint32_t echobeats_step;

    // Compressed latents
    std::vector<float> hypergraph_latent;
    std::vector<float> reservoir_state;

    // Metadata
    double timestamp;
    char checksum[17];  // 16 hex chars + null

    IdentityVector(size_t lat = 32, size_t res = 64)
        : latent_dim(lat), reservoir_dim(res)
        , identity_dim(FIXED_DIM + lat + res)
        , agent(AAR_AGENT_DIM, 0.0f)
        , arena(AAR_ARENA_DIM, 0.0f)
        , relation(AAR_RELATION_DIM, 0.0f)
        , ontogenetic(ONTOGENETIC_DIM, 0.0f)
        , level(AutonomyLevel::SCAFFOLD)
        , echobeats(ECHOBEATS_DIM, 0.0f)
        , echobeats_step(0)
        , hypergraph_latent(lat, 0.0f)
        , reservoir_state(res, 0.0f)
        , timestamp(0.0)
    {
        memset(checksum, 0, sizeof(checksum));
    }

    /// Flatten to a single vector
    std::vector<float> to_vector() const {
        std::vector<float> v;
        v.reserve(identity_dim);
        v.insert(v.end(), agent.begin(), agent.end());
        v.insert(v.end(), arena.begin(), arena.end());
        v.insert(v.end(), relation.begin(), relation.end());
        v.insert(v.end(), ontogenetic.begin(), ontogenetic.end());
        v.insert(v.end(), echobeats.begin(), echobeats.end());
        v.insert(v.end(), hypergraph_latent.begin(), hypergraph_latent.end());
        v.insert(v.end(), reservoir_state.begin(), reservoir_state.end());
        return v;
    }

    /// Reconstruct from flat vector
    void from_vector(const float* data, size_t len) {
        assert(len == identity_dim);
        size_t idx = 0;
        std::copy(data + idx, data + idx + AAR_AGENT_DIM, agent.begin()); idx += AAR_AGENT_DIM;
        std::copy(data + idx, data + idx + AAR_ARENA_DIM, arena.begin()); idx += AAR_ARENA_DIM;
        std::copy(data + idx, data + idx + AAR_RELATION_DIM, relation.begin()); idx += AAR_RELATION_DIM;
        std::copy(data + idx, data + idx + ONTOGENETIC_DIM, ontogenetic.begin()); idx += ONTOGENETIC_DIM;
        std::copy(data + idx, data + idx + ECHOBEATS_DIM, echobeats.begin()); idx += ECHOBEATS_DIM;
        std::copy(data + idx, data + idx + latent_dim, hypergraph_latent.begin()); idx += latent_dim;
        std::copy(data + idx, data + idx + reservoir_dim, reservoir_state.begin()); idx += reservoir_dim;

        // Decode level from one-hot
        int max_idx = 0;
        float max_val = ontogenetic[0];
        for (int i = 1; i < 6; ++i) {
            if (ontogenetic[i] > max_val) { max_val = ontogenetic[i]; max_idx = i; }
        }
        level = static_cast<AutonomyLevel>(max_idx);
    }

    /// AAR coherence (geometric mean of norms)
    float coherence() const {
        float a = 0, r = 0, l = 0;
        for (size_t i = 0; i < AAR_AGENT_DIM; ++i) a += agent[i] * agent[i];
        for (size_t i = 0; i < AAR_ARENA_DIM; ++i) r += arena[i] * arena[i];
        for (size_t i = 0; i < AAR_RELATION_DIM; ++i) l += relation[i] * relation[i];
        a = std::sqrt(a / AAR_AGENT_DIM);
        r = std::sqrt(r / AAR_ARENA_DIM);
        l = std::sqrt(l / AAR_RELATION_DIM);
        return std::cbrt(a * r * l);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Dense Layer (single fully-connected layer with optional ReLU)
// ─────────────────────────────────────────────────────────────────────────────

struct DenseLayer {
    size_t in_dim;
    size_t out_dim;
    std::vector<float> W;  // [in_dim x out_dim] row-major
    std::vector<float> b;  // [out_dim]
    bool use_relu;

    DenseLayer() : in_dim(0), out_dim(0), use_relu(true) {}

    DenseLayer(size_t in_d, size_t out_d, bool relu = true)
        : in_dim(in_d), out_dim(out_d)
        , W(in_d * out_d, 0.0f)
        , b(out_d, 0.0f)
        , use_relu(relu)
    {}

    /// Initialize with Xavier uniform
    void init_xavier(uint32_t seed) {
        float limit = std::sqrt(6.0f / (float)(in_dim + out_dim));
        for (size_t i = 0; i < W.size(); ++i) {
            seed = seed * 1103515245 + 12345;
            float r = (float)(seed >> 16) / 32768.0f - 1.0f;
            W[i] = r * limit;
        }
    }

    /// Forward pass: x[in_dim] -> out[out_dim]
    void forward(const float* x, float* out) const {
        for (size_t j = 0; j < out_dim; ++j) {
            float sum = b[j];
            for (size_t i = 0; i < in_dim; ++i) {
                sum += x[i] * W[i * out_dim + j];
            }
            out[j] = use_relu ? std::max(0.0f, sum) : sum;
        }
    }

    size_t param_count() const { return in_dim * out_dim + out_dim; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Recovery Core MLP (autoencoder)
// ─────────────────────────────────────────────────────────────────────────────

class RecoveryCoreMLP {
    size_t identity_dim_;
    size_t bottleneck_dim_;
    size_t hidden_dim_;

    // Encoder: identity -> hidden -> bottleneck
    DenseLayer enc1_;
    DenseLayer enc2_;

    // Decoder: bottleneck -> hidden -> identity
    DenseLayer dec1_;
    DenseLayer dec2_;

    bool trained_;
    float train_loss_;

public:
    RecoveryCoreMLP(size_t identity_dim = 125, size_t bottleneck_dim = 32, size_t hidden_dim = 64)
        : identity_dim_(identity_dim)
        , bottleneck_dim_(bottleneck_dim)
        , hidden_dim_(hidden_dim)
        , enc1_(identity_dim, hidden_dim, true)
        , enc2_(hidden_dim, bottleneck_dim, true)
        , dec1_(bottleneck_dim, hidden_dim, true)
        , dec2_(hidden_dim, identity_dim, false)  // no ReLU on output
        , trained_(false)
        , train_loss_(1e9f)
    {
        enc1_.init_xavier(7);
        enc2_.init_xavier(13);
        dec1_.init_xavier(17);
        dec2_.init_xavier(23);
    }

    size_t identity_dim() const { return identity_dim_; }
    size_t bottleneck_dim() const { return bottleneck_dim_; }
    size_t hidden_dim() const { return hidden_dim_; }
    bool trained() const { return trained_; }
    float train_loss() const { return train_loss_; }

    /// Encode: identity -> bottleneck latent
    std::vector<float> encode(const float* x) const {
        std::vector<float> h1(hidden_dim_);
        std::vector<float> z(bottleneck_dim_);
        enc1_.forward(x, h1.data());
        enc2_.forward(h1.data(), z.data());
        return z;
    }

    /// Decode: bottleneck latent -> identity
    std::vector<float> decode(const float* z) const {
        std::vector<float> h2(hidden_dim_);
        std::vector<float> out(identity_dim_);
        dec1_.forward(z, h2.data());
        dec2_.forward(h2.data(), out.data());
        return out;
    }

    /// Full forward pass: identity -> identity (autoencoder)
    std::vector<float> forward(const float* x) const {
        auto z = encode(x);
        return decode(z.data());
    }

    /// Compute reconstruction MSE
    float reconstruction_mse(const float* x) const {
        auto recon = forward(x);
        float mse = 0.0f;
        for (size_t i = 0; i < identity_dim_; ++i) {
            float d = x[i] - recon[i];
            mse += d * d;
        }
        return mse / identity_dim_;
    }

    /// Train on a single identity vector (online SGD step)
    float train_step(const float* x, float lr = 0.001f) {
        // Forward pass with intermediate storage
        std::vector<float> h1_pre(hidden_dim_);
        std::vector<float> h1(hidden_dim_);
        std::vector<float> z_pre(bottleneck_dim_);
        std::vector<float> z(bottleneck_dim_);
        std::vector<float> h2_pre(hidden_dim_);
        std::vector<float> h2(hidden_dim_);
        std::vector<float> out(identity_dim_);

        // Encoder forward
        for (size_t j = 0; j < hidden_dim_; ++j) {
            float sum = enc1_.b[j];
            for (size_t i = 0; i < identity_dim_; ++i)
                sum += x[i] * enc1_.W[i * hidden_dim_ + j];
            h1_pre[j] = sum;
            h1[j] = std::max(0.0f, sum);
        }
        for (size_t j = 0; j < bottleneck_dim_; ++j) {
            float sum = enc2_.b[j];
            for (size_t i = 0; i < hidden_dim_; ++i)
                sum += h1[i] * enc2_.W[i * bottleneck_dim_ + j];
            z_pre[j] = sum;
            z[j] = std::max(0.0f, sum);
        }

        // Decoder forward
        for (size_t j = 0; j < hidden_dim_; ++j) {
            float sum = dec1_.b[j];
            for (size_t i = 0; i < bottleneck_dim_; ++i)
                sum += z[i] * dec1_.W[i * hidden_dim_ + j];
            h2_pre[j] = sum;
            h2[j] = std::max(0.0f, sum);
        }
        for (size_t j = 0; j < identity_dim_; ++j) {
            float sum = dec2_.b[j];
            for (size_t i = 0; i < hidden_dim_; ++i)
                sum += h2[i] * dec2_.W[i * identity_dim_ + j];
            out[j] = sum;  // no ReLU on output
        }

        // Loss
        float mse = 0.0f;
        std::vector<float> d_out(identity_dim_);
        for (size_t i = 0; i < identity_dim_; ++i) {
            float d = out[i] - x[i];
            mse += d * d;
            d_out[i] = 2.0f * d / identity_dim_;
        }
        mse /= identity_dim_;

        // Backward: dec2 (no ReLU)
        for (size_t i = 0; i < hidden_dim_; ++i)
            for (size_t j = 0; j < identity_dim_; ++j)
                dec2_.W[i * identity_dim_ + j] -= lr * h2[i] * d_out[j];
        for (size_t j = 0; j < identity_dim_; ++j)
            dec2_.b[j] -= lr * d_out[j];

        std::vector<float> d_h2(hidden_dim_, 0.0f);
        for (size_t i = 0; i < hidden_dim_; ++i) {
            for (size_t j = 0; j < identity_dim_; ++j)
                d_h2[i] += d_out[j] * dec2_.W[i * identity_dim_ + j];
            if (h2_pre[i] <= 0.0f) d_h2[i] = 0.0f;
        }

        // Backward: dec1
        for (size_t i = 0; i < bottleneck_dim_; ++i)
            for (size_t j = 0; j < hidden_dim_; ++j)
                dec1_.W[i * hidden_dim_ + j] -= lr * z[i] * d_h2[j];
        for (size_t j = 0; j < hidden_dim_; ++j)
            dec1_.b[j] -= lr * d_h2[j];

        std::vector<float> d_z(bottleneck_dim_, 0.0f);
        for (size_t i = 0; i < bottleneck_dim_; ++i) {
            for (size_t j = 0; j < hidden_dim_; ++j)
                d_z[i] += d_h2[j] * dec1_.W[i * hidden_dim_ + j];
            if (z_pre[i] <= 0.0f) d_z[i] = 0.0f;
        }

        // Backward: enc2
        for (size_t i = 0; i < hidden_dim_; ++i)
            for (size_t j = 0; j < bottleneck_dim_; ++j)
                enc2_.W[i * bottleneck_dim_ + j] -= lr * h1[i] * d_z[j];
        for (size_t j = 0; j < bottleneck_dim_; ++j)
            enc2_.b[j] -= lr * d_z[j];

        std::vector<float> d_h1(hidden_dim_, 0.0f);
        for (size_t i = 0; i < hidden_dim_; ++i) {
            for (size_t j = 0; j < bottleneck_dim_; ++j)
                d_h1[i] += d_z[j] * enc2_.W[i * bottleneck_dim_ + j];
            if (h1_pre[i] <= 0.0f) d_h1[i] = 0.0f;
        }

        // Backward: enc1
        for (size_t i = 0; i < identity_dim_; ++i)
            for (size_t j = 0; j < hidden_dim_; ++j)
                enc1_.W[i * hidden_dim_ + j] -= lr * x[i] * d_h1[j];
        for (size_t j = 0; j < hidden_dim_; ++j)
            enc1_.b[j] -= lr * d_h1[j];

        trained_ = true;
        train_loss_ = mse;
        return mse;
    }

    /// Train on multiple vectors for multiple epochs
    float train(const float* X, size_t n_samples, size_t epochs = 500, float lr = 0.001f) {
        float loss = 0.0f;
        for (size_t e = 0; e < epochs; ++e) {
            loss = 0.0f;
            for (size_t s = 0; s < n_samples; ++s) {
                loss += train_step(X + s * identity_dim_, lr);
            }
            loss /= n_samples;
        }
        train_loss_ = loss;
        trained_ = true;
        return loss;
    }

    size_t param_count() const {
        return enc1_.param_count() + enc2_.param_count()
             + dec1_.param_count() + dec2_.param_count();
    }

    size_t memory_bytes() const { return param_count() * sizeof(float); }

    // ─── Checkpoint Save/Load ────────────────────────────────────────────

    /// Save checkpoint to memory buffer
    std::vector<uint8_t> save_checkpoint(const IdentityVector& identity) const {
        std::vector<uint8_t> buf;

        auto write_u32 = [&](uint32_t v) {
            buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v),
                       reinterpret_cast<uint8_t*>(&v) + 4);
        };
        auto write_f64 = [&](double v) {
            buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v),
                       reinterpret_cast<uint8_t*>(&v) + 8);
        };
        auto write_floats = [&](const float* data, size_t n) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
            buf.insert(buf.end(), p, p + n * 4);
        };

        // Header
        write_u32(DTEM_MAGIC);
        write_u32(DTEM_VERSION);
        write_u32((uint32_t)identity_dim_);
        write_u32((uint32_t)bottleneck_dim_);
        write_u32((uint32_t)hidden_dim_);
        write_u32((uint32_t)identity.level);
        write_f64(identity.timestamp);
        buf.insert(buf.end(), identity.checksum, identity.checksum + 16);

        // Layer weights: enc1, enc2, dec1, dec2
        auto write_layer = [&](const DenseLayer& layer) {
            write_u32((uint32_t)layer.in_dim);
            write_u32((uint32_t)layer.out_dim);
            write_floats(layer.W.data(), layer.W.size());
            write_floats(layer.b.data(), layer.b.size());
        };
        write_u32(4);  // n_layers
        write_layer(enc1_);
        write_layer(enc2_);
        write_layer(dec1_);
        write_layer(dec2_);

        // Identity vector
        auto vec = identity.to_vector();
        write_u32((uint32_t)vec.size());
        write_floats(vec.data(), vec.size());

        return buf;
    }

    /// Load checkpoint from memory buffer
    bool load_checkpoint(const uint8_t* data, size_t len, IdentityVector& identity) {
        size_t pos = 0;

        auto read_u32 = [&]() -> uint32_t {
            uint32_t v;
            memcpy(&v, data + pos, 4); pos += 4;
            return v;
        };
        auto read_f64 = [&]() -> double {
            double v;
            memcpy(&v, data + pos, 8); pos += 8;
            return v;
        };
        auto read_floats = [&](float* dst, size_t n) {
            memcpy(dst, data + pos, n * 4); pos += n * 4;
        };

        // Header
        if (read_u32() != DTEM_MAGIC) return false;
        if (read_u32() != DTEM_VERSION) return false;
        size_t id_dim = read_u32();
        size_t bn_dim = read_u32();
        size_t hd_dim = read_u32();
        uint32_t level = read_u32();
        double ts = read_f64();
        char cksum[17];
        memcpy(cksum, data + pos, 16); cksum[16] = '\0'; pos += 16;

        // Resize MLP
        identity_dim_ = id_dim;
        bottleneck_dim_ = bn_dim;
        hidden_dim_ = hd_dim;

        // Layers
        uint32_t n_layers = read_u32();
        assert(n_layers == 4);

        auto read_layer = [&](DenseLayer& layer) {
            size_t in_d = read_u32();
            size_t out_d = read_u32();
            layer = DenseLayer(in_d, out_d, true);
            read_floats(layer.W.data(), layer.W.size());
            read_floats(layer.b.data(), layer.b.size());
        };
        read_layer(enc1_);
        read_layer(enc2_);
        read_layer(dec1_);
        read_layer(dec2_);
        dec2_.use_relu = false;  // output layer has no ReLU

        // Identity vector
        size_t vec_len = read_u32();
        size_t lat_dim = vec_len - FIXED_DIM - 64;
        identity = IdentityVector(lat_dim, 64);
        std::vector<float> vec(vec_len);
        read_floats(vec.data(), vec_len);
        identity.from_vector(vec.data(), vec_len);
        identity.timestamp = ts;
        memcpy(identity.checksum, cksum, 17);

        trained_ = true;
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Recovery Node (the persistent recovery core)
// ─────────────────────────────────────────────────────────────────────────────

class IdentityRecoveryNode {
    RecoveryCoreMLP mlp_;
    IdentityVector identity_;
    bool loaded_;

public:
    IdentityRecoveryNode() : loaded_(false) {}

    /// Load from checkpoint buffer
    bool load(const uint8_t* data, size_t len) {
        loaded_ = mlp_.load_checkpoint(data, len, identity_);
        return loaded_;
    }

    /// Save current state to checkpoint buffer
    std::vector<uint8_t> save() const {
        return mlp_.save_checkpoint(identity_);
    }

    /// Verify integrity (MLP roundtrip)
    float verify() const {
        if (!loaded_) return -1.0f;
        auto vec = identity_.to_vector();
        return mlp_.reconstruction_mse(vec.data());
    }

    /// Get the recovered identity
    const IdentityVector& identity() const { return identity_; }

    /// Get the MLP
    const RecoveryCoreMLP& mlp() const { return mlp_; }
    RecoveryCoreMLP& mlp() { return mlp_; }

    /// Update identity and retrain MLP
    void update(const IdentityVector& new_identity, size_t epochs = 100, float lr = 0.001f) {
        identity_ = new_identity;
        auto vec = identity_.to_vector();
        mlp_.train(vec.data(), 1, epochs, lr);
        loaded_ = true;
    }

    bool loaded() const { return loaded_; }
    float coherence() const { return loaded_ ? identity_.coherence() : 0.0f; }
    AutonomyLevel level() const { return identity_.level; }
    size_t param_count() const { return mlp_.param_count(); }
};

}} // namespace cog::dte

#endif // COG_DTE_MLP_HPP
