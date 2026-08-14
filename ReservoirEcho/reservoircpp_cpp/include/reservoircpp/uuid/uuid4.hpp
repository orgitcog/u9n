#ifndef RESERVOIRCPP_UUID_UUID4_HPP
#define RESERVOIRCPP_UUID_UUID4_HPP

#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstdint>

namespace reservoircpp {
namespace uuid {

/**
 * @brief UUID4 class for generating random UUIDs
 * 
 * Generates RFC 4122 compliant version 4 UUIDs for
 * unique identification of reservoir nodes and components.
 */
class UUID4 {
public:
    UUID4() {
        generate();
    }

    explicit UUID4(const std::string& str) {
        parse(str);
    }

    // Generate a new random UUID
    void generate() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dist;

        uint64_t high = dist(gen);
        uint64_t low = dist(gen);

        // Set version to 4 (random)
        high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        // Set variant to RFC 4122
        low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

        data_[0] = static_cast<uint8_t>(high >> 56);
        data_[1] = static_cast<uint8_t>(high >> 48);
        data_[2] = static_cast<uint8_t>(high >> 40);
        data_[3] = static_cast<uint8_t>(high >> 32);
        data_[4] = static_cast<uint8_t>(high >> 24);
        data_[5] = static_cast<uint8_t>(high >> 16);
        data_[6] = static_cast<uint8_t>(high >> 8);
        data_[7] = static_cast<uint8_t>(high);
        data_[8] = static_cast<uint8_t>(low >> 56);
        data_[9] = static_cast<uint8_t>(low >> 48);
        data_[10] = static_cast<uint8_t>(low >> 40);
        data_[11] = static_cast<uint8_t>(low >> 32);
        data_[12] = static_cast<uint8_t>(low >> 24);
        data_[13] = static_cast<uint8_t>(low >> 16);
        data_[14] = static_cast<uint8_t>(low >> 8);
        data_[15] = static_cast<uint8_t>(low);
    }

    // Parse UUID from string
    void parse(const std::string& str) {
        if (str.length() != 36) {
            throw std::invalid_argument("Invalid UUID string length");
        }

        size_t idx = 0;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '-') continue;
            
            if (idx >= 32) {
                throw std::invalid_argument("Invalid UUID string format");
            }

            uint8_t high = hex_to_nibble(str[i]);
            uint8_t low = hex_to_nibble(str[++i]);
            data_[idx / 2] = (high << 4) | low;
            idx += 2;
        }
    }

    // Convert to string representation
    std::string to_string() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        
        for (size_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                oss << '-';
            }
            oss << std::setw(2) << static_cast<int>(data_[i]);
        }
        
        return oss.str();
    }

    // Comparison operators
    bool operator==(const UUID4& other) const {
        return data_ == other.data_;
    }

    bool operator!=(const UUID4& other) const {
        return !(*this == other);
    }

    bool operator<(const UUID4& other) const {
        return data_ < other.data_;
    }

    // Hash support
    std::size_t hash() const {
        std::size_t h = 0;
        for (auto byte : data_) {
            h ^= std::hash<uint8_t>{}(byte) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    // Raw data access
    const std::array<uint8_t, 16>& data() const { return data_; }

private:
    std::array<uint8_t, 16> data_{};

    static uint8_t hex_to_nibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character");
    }
};

// Factory function
inline UUID4 uuid4() {
    return UUID4();
}

} // namespace uuid
} // namespace reservoircpp

// Hash specialization for std::unordered_map
namespace std {
template<>
struct hash<reservoircpp::uuid::UUID4> {
    std::size_t operator()(const reservoircpp::uuid::UUID4& uuid) const {
        return uuid.hash();
    }
};
}

#endif // RESERVOIRCPP_UUID_UUID4_HPP
